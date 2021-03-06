package lsr.paxos.core;

import static lsr.common.ProcessDescriptor.processDescriptor;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import lsr.paxos.messages.Accept;
import lsr.paxos.messages.Prepare;
import lsr.paxos.messages.PrepareOK;
import lsr.paxos.messages.Propose;
import lsr.paxos.network.Network;
import lsr.paxos.storage.ConsensusInstance;
import lsr.paxos.storage.ConsensusInstance.LogEntryState;
import lsr.paxos.storage.Log;
import lsr.paxos.storage.Storage;

/**
 * Represents part of paxos which is responsible for responding on the
 * <code>Prepare</code> message, and also sending <code>Accept</code> after
 * receiving proper <code>Propose</code>.
 */
class Acceptor {
    private final Paxos paxos;
    private final Storage storage;
    private final Network network;

    /**
     * Initializes new instance of <code>Acceptor</code>.
     * 
     * @param paxos - the paxos the acceptor belong to
     * @param storage - data associated with the paxos
     * @param network - used to send responses
     * 
     */
    public Acceptor(Paxos paxos, Storage storage, Network network) {
        this.paxos = paxos;
        this.storage = storage;
        this.network = network;
    }

    /**
     * Promises not to accept a proposal numbered less than message view. Sends
     * the proposal with the highest number less than message view that it has
     * accepted if any. If message view equals current view, then it may be a
     * retransmission or out-of-order delivery. If the process already accepted
     * this proposal, then the proposer doesn't need anymore the prepareOK
     * message. Otherwise it might need the message, so resent it.
     * 
     * @param msg received prepare message
     * @see Prepare
     */
    public void onPrepare(Prepare msg, int sender) {
        assert paxos.getDispatcher().amIInDispatcher() : "Thread should not be here: " +
                                                         Thread.currentThread();

        if (!paxos.isActive())
            return;

        // TODO: JK: When can we skip responding to a prepare message?
        // Is detecting stale prepare messages it worth it?

        if (logger.isDebugEnabled(processDescriptor.logMark_Benchmark2019))
            logger.debug(processDescriptor.logMark_Benchmark2019, "P1A R {}", msg.getView());

        logger.info("{} From {}", msg, sender);

        Log log = storage.getLog();

        if (msg.getFirstUncommitted() < log.getLowestAvailableId()) {
            // We're MUCH MORE up-to-date than the replica that sent Prepare
            paxos.startProposer();
            return;
        }

        ConsensusInstance[] v = new ConsensusInstance[Math.max(
                log.getNextId() - msg.getFirstUncommitted(), 0)];
        for (int i = msg.getFirstUncommitted(); i < log.getNextId(); i++) {
            v[i - msg.getFirstUncommitted()] = log.getInstance(i);
        }

        PrepareOK m = new PrepareOK(msg.getView(), v, storage.getEpoch());
        logger.info("Sending {}", m);

        network.sendMessage(m, sender);
        if (logger.isDebugEnabled(processDescriptor.logMark_Benchmark2019))
            logger.debug(processDescriptor.logMark_Benchmark2019, "P1B S {}", m.getView());
    }

    /**
     * Accepts proposals higher or equal than the current view.
     * 
     * @param message - received propose message
     * @param sender - the id of replica that send the message
     */
    public void onPropose(final Propose message, final int sender) {
        assert message.getView() == storage.getView() : "Msg.view: " + message.getView() +
                                                        ", view: " + storage.getView();
        assert paxos.getDispatcher().amIInDispatcher();

        ConsensusInstance instance = storage.getLog().getInstance(message.getInstanceId());
        // The propose is so old, that it's log has already been erased
        if (instance == null) {
            logger.debug("Ignoring old message: {}", message);
            return;
        }

        if (logger.isDebugEnabled()) {
            logger.debug("onPropose. View:instance: {}:{}", message.getView(),
                    message.getInstanceId());
        }

        // In FullSS, updating state leads to setting new value if needed, which
        // syncs to disk
        boolean isMajority = instance.updateStateFromPropose(sender, message.getView(),
                message.getValue());

        // leader will not send the accept message;
        if (!paxos.isLeader()) {

            if (storage.getFirstUncommitted() +
                (processDescriptor.windowSize * 3) < message.getInstanceId()) {
                // the instance is so new that we must be out of date.
                paxos.getCatchup().forceCatchup();
            }

            if (paxos.isActive())
                network.sendToOthers(new Accept(message));
        }

        // we could have decided the instance earlier (and now we get a
        // duplicated propose)
        if (instance.getState() == LogEntryState.DECIDED) {
            logger.trace("Instance already decided: {}", message.getInstanceId());
            return;
        }

        // Check if we can decide (n<=3 or if some accepts overtook propose)
        if (isMajority) {
            paxos.decide(instance.getId());
        }
    }

    private final static Logger logger = LoggerFactory.getLogger(Acceptor.class);
}
