package lsr.paxos.replica.storage;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import lsr.common.Reply;
import lsr.paxos.Snapshot;
import lsr.paxos.storage.ConsensusInstance;
import lsr.paxos.storage.PersistentConsensusInstance;

public class PersistentReplicaStorage implements ReplicaStorage {

    private static native int getExecuteUB_();

    volatile int executeUBCache = getExecuteUB_();
    int unpackUB = getExecuteUB_();
    
    @Override
    public int getExecuteUB() {
        return executeUBCache;
    }

    private static native void setExecuteUB_(int executeUB);

    @Override
    public void setExecuteUB(int executeUB) {
        setExecuteUB_(executeUB);
        executeUBCache = executeUB;
        unpackUB = executeUB;
    }

    private static native void incrementExecuteUB_();

    @Override
    public void incrementExecuteUB() {
        incrementExecuteUB_();
        executeUBCache++;
    }
    
    @Override
    public int getUnpackUB() {
        return unpackUB;
    }

    @Override
    public void incrementUnpackUB() {
        unpackUB++;
    }

    private static native void addDecidedWaitingExecution(int instanceId);

    @Override
    public void addDecidedWaitingExecution(Integer instanceId, ConsensusInstance ci) {
        addDecidedWaitingExecution(instanceId);
    }

    private static native boolean isDecidedWaitingExecution(int instanceId);

    @Override
    public ConsensusInstance getDecidedWaitingExecution(Integer instanceId) {
        if (!isDecidedWaitingExecution(instanceId))
            return null;
        return new PersistentConsensusInstance(instanceId);
    }

    @Override
    public native void releaseDecidedWaitingExecutionUpTo(int instanceId);

    @Override
    public native void releaseDecidedWaitingExecution(int instanceId);

    @Override
    public native int decidedWaitingExecutionCount();

    private static native void setLastReplyForClient(long client, long clientSeqNo, byte[] reply);

    @Override
    public void setLastReplyForClient(Long client, Reply reply) {
        setLastReplyForClient(client, reply.getRequestId().getSeqNumber(), reply.getValue());

    }

    /** Returns -1 if there was no reply */
    private static native int getLastReplySeqNoForClient_(long client);

    @Override
    public Integer getLastReplySeqNoForClient(Long client) {
        int seqNo = getLastReplySeqNoForClient_(client);
        return seqNo == -1 ? null : seqNo;
    }

    private static native Reply getLastReplyForClient_(long client);

    @Override
    public Reply getLastReplyForClient(Long client) {
        return getLastReplyForClient_(client);
    }

    private static native void dropLastReplyForClient();

    @Override
    public void restoreFromSnapshot(Snapshot snapshot) {
        dropLastReplyForClient();

        for (Reply reply : snapshot.getLastReplyForClient()) {
            long clientId = reply.getRequestId().getClientId();
            int clientSeqNo = reply.getRequestId().getSeqNumber();
            // FIXME: This below works, but is potentially __very__ slow.
            // Profile perf it.
            setLastReplyForClient(clientId, clientSeqNo, reply.getValue());
        }

        /*- TODO: this is also called by DecideCallbackImpl.atRestoringStateFromSnapshot - investigate -*/
        setExecuteUB(snapshot.getNextInstanceId());

        setServiceSeqNo(snapshot.getNextServiceSeqNo());

        releaseDecidedWaitingExecutionUpTo(snapshot.getNextInstanceId());
    }

    private static native Reply[] getAllReplies();

    @Override
    public void makeSnapshot(Snapshot snapshot) {
        snapshot.setNextInstanceId(executeUBCache);
        snapshot.setNextServiceSeqNo(getServiceSeqNo());
        snapshot.setLastReplyForClient(getAllReplies());
    }

    private static native void setServiceSnapshotToRestore(String serviceFiles[]);

    @Override
    public void setServiceSnapshotToRestore(List<String> serviceFiles) {
        setServiceSnapshotToRestore(serviceFiles.toArray(new String[serviceFiles.size()]));
    }

    @Override
    public native void armServiceSnapshotToRestore();

    @Override
    public native void removeServiceSnapshotToRestore();

    private static native String[] getServiceSnapshotToRestore_();

    @Override
    public ArrayList<String> getServiceSnapshotToRestore() {
        String[] array = getServiceSnapshotToRestore_();
        if (array == null)
            return null;
        ArrayList<String> list = new ArrayList<String>();
        list.addAll(Arrays.asList(array));
        return list;
    }

    @Override
    public native long getServiceSeqNo();

    @Override
    public native void setServiceSeqNo(long newServiceSeqNo);

    @Override
    public native long incServiceSeqNo();

}
