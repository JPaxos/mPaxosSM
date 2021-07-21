package lsr.paxos.replica;

import static lsr.common.ProcessDescriptor.processDescriptor;

import java.io.IOException;
import java.nio.file.AccessDeniedException;
import java.util.ArrayList;
import java.util.concurrent.BrokenBarrierException;
import java.util.concurrent.CyclicBarrier;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import lsr.common.ClientRequest;
import lsr.common.Configuration;
import lsr.common.CrashModel;
import lsr.common.Pair;
import lsr.common.ProcessDescriptor;
import lsr.common.Reply;
import lsr.common.RequestId;
import lsr.common.SingleThreadDispatcher;
import lsr.paxos.Batcher;
import lsr.paxos.Snapshot;
import lsr.paxos.NATIVE.PersistentMemory;
import lsr.paxos.core.Paxos;
import lsr.paxos.messages.CatchUpQuery;
import lsr.paxos.messages.CatchUpSnapshot;
import lsr.paxos.messages.Message;
import lsr.paxos.messages.SendonlyCatchUpSnapshot;
import lsr.paxos.recovery.PmemRecovery;
import lsr.paxos.recovery.RecoveryAlgorithm;
import lsr.paxos.recovery.RecoveryListener;
import lsr.paxos.replica.storage.PersistentReplicaStorage;
import lsr.paxos.replica.storage.ReplicaStorage;
import lsr.paxos.storage.Storage;
import lsr.service.Service;

/**
 * Manages replication of a service. Receives requests from the client, orders
 * them using Paxos, executes the ordered requests and sends the reply back to
 * the client.
 * <p>
 * Example of usage:
 * <p>
 * <blockquote>
 * 
 * <pre>
 * public static void main(String[] args) throws IOException {
 *  int localId = Integer.parseInt(args[0]);
 *  Replica replica = new Replica(localId, new MapService());
 *  replica.start();
 * }
 * </pre>
 * 
 * </blockquote>
 */
public class Replica {

    // // // // // // // // // // // // // // // //
    // External modules accessed by the replica. //
    // // // // // // // // // // // // // // // //

    private Paxos paxos;
    private NioClientManager clientManager;
    private ClientRequestManager requestManager;
    /** Represent the deterministic state machine (service) itself */
    private final ServiceProxy serviceProxy;
    private DecideCallbackImpl decideCallback;
    /** Client set exposed to the world */
    private InternalClient intCli = null;
    private Batcher batcher;

    // // // // // // // // // //
    // Miscellaneous variables //
    // // // // // // // // // //

    private final ReplicaStorage replicaStorage;

    /** Location for files that should survive crashes */
    private String stableStoragePath;

    /** Thread for handling events connected to the replica */
    private final SingleThreadDispatcher replicaDispatcher;
    private ClientRequestForwarder requestForwarder;

    // // // // // // //
    // Public methods //
    // // // // // // //

    /**
     * Initializes new instance of <code>Replica</code> class.
     * <p>
     * This constructor doesn't start the replica and Paxos protocol. In order
     * to run it the {@link #start()} method should be called.
     * 
     * @param config - the configuration of the replica
     * @param localId - the id of replica to create
     * @param service - the state machine to execute request on
     */
    public Replica(Configuration config, int localId, Service service) {
        ProcessDescriptor.initialize(config, localId);
        if (logger.isWarnEnabled(processDescriptor.logMark_Benchmark2019))
            logger.warn(processDescriptor.logMark_Benchmark2019, "START");

        stableStoragePath = processDescriptor.logPath + '/' + localId;

        switch (processDescriptor.crashModel) {
            case CrashStop:
            case FullSS:
            case ViewSS:
            case EpochSS:
                throw new RuntimeException(
                        "Unsupported crash recovery model - only pmem allowed with this JPaxos version");
            case Pmem:
                String pmemFile = processDescriptor.nvmDirectory + '/' + "jpaxos." +
                                  String.valueOf(localId);
                try {
                    PersistentMemory.loadLib(pmemFile);
                } catch (UnsatisfiedLinkError e) {
                    throw new RuntimeException(
                            "Shared library (.so/.dll) for pmem missing or invalid", e);
                } catch (AccessDeniedException e) {
                    throw new RuntimeException("Cannot create pmem file " + pmemFile, e);
                }
                replicaStorage = new PersistentReplicaStorage();

                break;
            default:
                throw new UnsupportedOperationException();
        }

        replicaDispatcher = new SingleThreadDispatcher("Replica");

        serviceProxy = new ServiceProxy(service, replicaDispatcher);

        ArrayList<String> lostSnapshot = replicaStorage.getServiceSnapshotToRestore();
        if (lostSnapshot != null)
            serviceProxy.updateToSnapshot(lostSnapshot);
        replicaStorage.removeServiceSnapshotToRestore();
    }

    /**
     * Starts the replica.
     * 
     * First the recovery phase is started and after that the replica joins the
     * Paxos protocol and starts the client manager and the underlying service.
     * 
     * @throws IOException if some I/O error occurs
     */
    public void start() throws IOException {
        logger.info(processDescriptor.logMark_Benchmark, "Recovery phase started.");

        replicaDispatcher.start();

        decideCallback = new DecideCallbackImpl(this);

        RecoveryAlgorithm recovery = createRecoveryAlgorithm(processDescriptor.crashModel);

        paxos = recovery.getPaxos();
        paxos.setDecideCallback(decideCallback);

        batcher = paxos.getBatcher();

        requestForwarder = new ClientRequestForwarder(paxos);
        requestForwarder.start();

        if (logger.isWarnEnabled(processDescriptor.logMark_Benchmark2019)) {
            paxos.reportDPS();
        }

        paxos.startPassivePaxos();

        recovery.addRecoveryListener(new InnerRecoveryListener());

        if (logger.isWarnEnabled(processDescriptor.logMark_Benchmark2019))
            logger.warn(processDescriptor.logMark_Benchmark2019, "REC B");

        recovery.start();
    }

    private RecoveryAlgorithm createRecoveryAlgorithm(CrashModel crashModel) throws IOException {
        switch (crashModel) {
            case Pmem:
                return new PmemRecovery(this, serviceProxy,
                        replicaDispatcher, decideCallback);
            default:
                throw new RuntimeException("Unknown crash model: " + crashModel);
        }
    }

    public ReplicaStorage getReplicaStorage() {
        return replicaStorage;
    }

    public void forceExit() {
        // TODO (JK) hm... implement this?
        replicaDispatcher.shutdownNow();
    }

    /**
     * Sets the path to directory where all stable storage logs will be saved.
     * 
     * @param path to directory where the stable storage logs will be saved
     */
    public void setStableStoragePath(String path) {
        stableStoragePath = path;
    }

    /**
     * Gets the path to directory where all stable storage logs will be saved.
     * 
     * @return path
     */
    public String getStableStoragePath() {
        return stableStoragePath;
    }

    /**
     * Adds the request to the set of requests be executed. If called e(A) e(B),
     * the delivery will be either d(A) d(B) or d(B) d(A).
     * 
     * If the replica crashes before the request is delivered, the request may
     * get lost.
     * 
     * @param requestValue - the exact request that will be delivered to the
     *            Service execute method
     * @throws IllegalStateException if the method is called before the recovery
     *             has finished
     */
    public void executeNonFifo(byte[] requestValue) throws IllegalStateException {
        if (intCli == null)
            throw new IllegalStateException(
                    "Request cannot be executed before recovery has finished");
        intCli.executeNonFifo(requestValue);
    }

    /** Returns the current view */
    public int getView() {
        if (paxos == null)
            throw new IllegalStateException("Replica must be started prior to this call");
        return paxos.getStorage().getView();
    }

    /** Returns the ID of current leader */
    public int getLeader() {
        if (paxos == null)
            throw new IllegalStateException("Replica must be started prior to this call");
        return paxos.getLeaderId();
    }

    /**
     * Adds a listener for leader changes. Allowed after the replica has been
     * started.
     */
    public boolean addViewChangeListener(Storage.ViewChangeListener listener) {
        if (listener == null)
            throw new IllegalArgumentException("The listener cannot be null");
        if (paxos == null)
            throw new IllegalStateException("Replica must be started prior to adding a listener");
        return paxos.getStorage().addViewChangeListener(listener);
    }

    /**
     * Removes a listener previously added by
     * {@link #addViewChangeListener(Storage.ViewChangeListener)}
     */
    public boolean removeViewChangeListener(Storage.ViewChangeListener listener) {
        return paxos.getStorage().removeViewChangeListener(listener);
    }

    // // // // // // // // // // // //
    // Callback's for JPaxos modules //
    // // // // // // // // // // // //

    /* package access */void executeClientBatchAndWait(final int instance,
                                                       final ClientRequest[] requests) {
        innerExecuteClientBatch(instance, requests);
    }

    /* package access */void instanceExecuted(final int instance,
                                              final ClientRequest[] requests) {
        innerInstanceExecuted(instance, requests);
    }

    /* package access */SingleThreadDispatcher getReplicaDispatcher() {
        return replicaDispatcher;
    }

    // // // // // // // // // // // //
    // Internal methods and classes. //
    // // // // // // // // // // // //

    /**
     * Called by the RequestManager when it has the ClientRequest that should be
     * executed next.
     * 
     * @param instance
     * @param bInfo
     */
    private void innerExecuteClientBatch(int instance, ClientRequest[] requests) {
        assert replicaDispatcher.amIInDispatcher() : "Wrong thread: " +
                                                     Thread.currentThread().getName();

        if (logger.isTraceEnabled(processDescriptor.logMark_Benchmark2019nope))
            logger.trace(processDescriptor.logMark_Benchmark2019nope, "IX {}", instance);

        for (ClientRequest cRequest : requests) {
            RequestId rID = cRequest.getRequestId();
            Integer lastSequenceNumberFromClient = replicaStorage.getLastReplySeqNoForClient(
                    rID.getClientId());
            if (lastSequenceNumberFromClient != null) {

                // Do not execute the same request several times.
                if (rID.getSeqNumber() <= lastSequenceNumberFromClient) {
                    // with Pmem this message is normal for the first
                    // instance after recovery
                    logger.warn(
                            "Request ordered multiple times. inst: {}, req: {}, lastSequenceNumberFromClient: ",
                            instance, cRequest, lastSequenceNumberFromClient);

                    // (JK) FIXME: investigate if the client could get the
                    // response multiple times here.

                    // Send the cached reply back to the client
                    if (rID.getSeqNumber() == lastSequenceNumberFromClient) {
                        // req manager can be null on fullss disk read
                        if (requestManager != null)
                            requestManager.onRequestExecuted(cRequest,
                                    replicaStorage.getLastReplyForClient(rID.getClientId()));
                    }
                    continue;
                }
                // else there is a cached reply, but for a past request
                // only.
            }

            // Executing the request (at last!)
            // Here the replica thread is given to Service.
            byte[] result = serviceProxy.execute(replicaStorage.getServiceSeqNo(), cRequest);

            Reply reply = new Reply(cRequest.getRequestId(), result);

            PersistentMemory.startThreadLocalTx();
            replicaStorage.setLastReplyForClient(rID.getClientId(), reply);
            replicaStorage.incServiceSeqNo();
            PersistentMemory.commitThreadLocalTx();

            // req manager can be null on fullss disk read
            if (requestManager != null)
                requestManager.onRequestExecuted(cRequest, reply);

        }
    }

    private void innerInstanceExecuted(final int instance, final ClientRequest[] requests) {
        replicaDispatcher.checkInDispatcher();
        assert replicaStorage.getExecuteUB() == instance : replicaStorage.getExecuteUB() + " " +
                                                           instance;
        logger.info("Instance finished: {}", instance);
        paxos.getProposer().instanceExecuted(instance);
        batcher.instanceExecuted(instance, requests);
        paxos.getCatchup().instanceExecuted(instance);
    }

    /**
     * Listener called after recovery algorithm is finished and paxos can be
     * started.
     */
    private class InnerRecoveryListener implements RecoveryListener {

        public void recoveryFinished() {
            requestManager = new ClientRequestManager(Replica.this, decideCallback,
                    requestForwarder, paxos);
            paxos.setClientRequestManager(requestManager);
            if (processDescriptor.redirectClientsFromLeader && paxos.isLeader()) {
                requestManager.setFendOffClients(true);
            }
            requestForwarder.setClientRequestManager(requestManager);

            intCli = new InternalClient(replicaDispatcher, requestManager);

            try {
                NioClientProxy.createIdGenerator(paxos.getStorage());
                clientManager = new NioClientManager(requestManager);
                clientManager.start();
            } catch (IOException e) {
                throw new RuntimeException("Could not prepare the socket for clients! Aborting.",
                        e);
            }

            logger.info(processDescriptor.logMark_Benchmark,
                    "Recovery phase finished. Starting paxos protocol.");
            if (logger.isWarnEnabled(processDescriptor.logMark_Benchmark2019))
                logger.warn(processDescriptor.logMark_Benchmark2019, "REC E");
            paxos.startActivePaxos();
        }
    }

    public void handleSnapshot(final CatchUpSnapshot msg, final Runnable onSnapshotHandled) {
        // called at least by CatchUp from Paxos dispatcher

        if (logger.isDebugEnabled()) {
            logger.info("New snapshot received: " + msg.toString() +
                        " (at " + Thread.currentThread().getName() + ")");
        } else
            logger.info("New snapshot received: " + msg.toString());

        replicaDispatcher.execute(() -> {
            logger.debug("New snapshot received: " + msg.toString() +
                         " (at " + Thread.currentThread().getName() + ")");
            paxos.getStorage().acquireSnapshotMutex();
            handleSnapshotInternal(msg, onSnapshotHandled);
            paxos.getStorage().releaseSnapshotMutex();
        });
    }

    /**
     * Restoring state from a snapshot
     * 
     * @param snapshot
     * @param onSnapshotHandled
     */
    private void handleSnapshotInternal(CatchUpSnapshot msg, final Runnable onSnapshotHandled) {
        assert replicaDispatcher.amIInDispatcher();

        Snapshot snapshot = msg.getSnapshot();

        if (snapshot.getNextInstanceId() < replicaStorage.getExecuteUB()) {
            logger.error("Received snapshot is older than current state. {}, executeUB: {}",
                    snapshot.getNextInstanceId(), replicaStorage.getExecuteUB());
            return;
        }

        if (logger.isDebugEnabled(processDescriptor.logMark_Benchmark2019))
            logger.debug(processDescriptor.logMark_Benchmark2019, "CS U");

        replicaStorage.setServiceSnapshotToRestore(snapshot.getSnapshotFiles());

        /*
         * (JK) FIXME - now, due to the changes for efficiency, when the replica
         * crashes after line "Snapshot snapshot = msg.getSnapshot();" a couple
         * lines above, but before one line above, it will not delete upon
         * recovery the files it already unpacked on disk. This is bad.      
         * Reason: To know what file names are in snapshot, stupid java needs to
         * parse whole input with ZipInputStream, and that takes lots of time.
         * Normally it's sufficient to read file list located at the end of ZIP
         * file, and we know where the ZIP file ends, but in Java there is no
         * built-in that can use this data.
         */

        logger.warn("Updating machine state from {}", snapshot);

        if (processDescriptor.crashModel == CrashModel.Pmem)
            PersistentMemory.startThreadLocalTx();

        replicaStorage.armServiceSnapshotToRestore();

        replicaStorage.restoreFromSnapshot(snapshot);

        CyclicBarrier barrier = new CyclicBarrier(2);
        paxos.getDispatcher().submit(() -> {
            try {
                barrier.await();
                // here replica thread, in its pmem tx, recovers paxos
                barrier.await();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            } catch (BrokenBarrierException e) {
                throw new RuntimeException(e);
            }
        });

        try {
            barrier.await();
            paxos.updateToSnapshot(snapshot);
            barrier.await();
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        } catch (BrokenBarrierException e) {
            throw new RuntimeException(e);
        }

        if (processDescriptor.crashModel == CrashModel.Pmem)
            PersistentMemory.commitThreadLocalTx();

        if (onSnapshotHandled != null)
            onSnapshotHandled.run();

        serviceProxy.updateToSnapshot(snapshot.getSnapshotFiles());

        replicaStorage.removeServiceSnapshotToRestore();
    }

    /* package access */boolean hasUnexecutedRequests(ClientRequest[] requests) {
        for (ClientRequest req : requests) {
            RequestId reqId = req.getRequestId();
            Integer prevReply = replicaStorage.getLastReplySeqNoForClient(reqId.getClientId());
            if (prevReply == null)
                return true;
            if (prevReply < reqId.getSeqNumber())
                return true;
        }
        return false;
    }

    public ClientRequestManager getRequestManager() {
        return requestManager;
    }

    /** Returns CatchUpSnapshot message and the snapshot nextInstanceId. */
    // keep in mind that message must be constructed when dispatchers are held
    public Pair<Message, Integer> makeSnapshot(final CatchUpQuery query) {
        assert paxos.getDispatcher().amIInDispatcher();

        final Pair<Message, Integer> response = new Pair<Message, Integer>();

        replicaDispatcher.executeAndWait(() -> {
            // here we have both replica and paxos threads(!)
            // TODO (JK): make sure that executeAndWait on replica is called
            // only from paxos, never the other way round

            Snapshot snapshot = new Snapshot();

            serviceProxy.makeSnapshot(snapshot);
            replicaStorage.makeSnapshot(snapshot);

            Message m = new SendonlyCatchUpSnapshot(paxos.getStorage().getView(),
                    query.getSentTime(), snapshot);

            response.setKey(m);
            response.setValue(snapshot.getNextInstanceId());

            // Paxos storage can be just empty
            // next instance is same as next uncommited
            // view from CatchUpResponse is sufficient
        });

        return response;
    }

    private final static Logger logger = LoggerFactory.getLogger(Replica.class);

}
