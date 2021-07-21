package lsr.paxos.replica.storage;

import java.util.ArrayList;
import java.util.List;

import lsr.common.Reply;
import lsr.paxos.Snapshot;
import lsr.paxos.storage.ConsensusInstance;

/**
 * Class containing data that must not be lost upon crash/recovery for
 * correctness of the state machine replication
 */

public interface ReplicaStorage {

    /** Instance number such that all lower are already executed */
    int getExecuteUB();

    /** @see #getExecuteUB() */
    void setExecuteUB(int executeUB);

    /** @see #getExecuteUB() */
    void incrementExecuteUB();

    /** Instance number such that all lower are already unpacked form batch */
    int getUnpackUB();
    
    /** @see #getUnpackUB() */
    void incrementUnpackUB();
    
    /** Decided instances wait here for execution */
    void addDecidedWaitingExecution(Integer instanceId, ConsensusInstance ci);

    /** Decided instances are taken for execution */
    ConsensusInstance getDecidedWaitingExecution(Integer instanceId);

    /** Executed instances in range [0, instanceId) are thrown away */
    void releaseDecidedWaitingExecutionUpTo(int instanceId);

    /** Executed instance instanceId are thrown away */
    void releaseDecidedWaitingExecution(int instanceId);

    /**
     * Returns the number of decided instances that wait for being executed by
     * the Service
     */
    int decidedWaitingExecutionCount();

    /**
     * Stores the reply for the client; used in case the reply is lost and
     * client sends the request again, and in case when a duplicated stale
     * request arrives
     * 
     * called by Replica thread
     */
    void setLastReplyForClient(Long client, Reply reply);

    /**
     * @see #setLastReplyForClient(int, int, Reply)
     *
     *      called by multiple threads (at least Replica, ClientManager)
     */
    Integer getLastReplySeqNoForClient(Long client);

    /**
     * @see #setLastReplyForClient(int, int, Reply)
     * 
     *      called by multiple threads (at least Replica, ClientManager)
     */
    Reply getLastReplyForClient(Long client);

    /**
     * Called upon restoring state from snapshot in order to bring the
     * ReplicaStorage to the state from snapshot
     */
    void restoreFromSnapshot(Snapshot snapshot);

    void makeSnapshot(Snapshot snapshot);

    /**
     * Flow:
     * <ol>
     * <li>replica receives a snapshot to update from</li>
     * <li>replica reads file names from the snapshot using
     * setServiceSnapshotToRestore (and stores them in pmem)</li>
     * <li>replica unpacks the files from snapshot to filesystem</li>
     * <li>replica applies the snapshot and arms the files with
     * armServiceSnapshotToRestore</li>
     * <li>replica bids the service to update</li>
     * <li>removeServiceSnapshotToRestore is called</li>
     * </ol>
     * so getServiceSnapshotToRestore shall return files that were set by
     * setServiceSnapshotToRestore and armed, but not yet removed
     */
    void setServiceSnapshotToRestore(List<String> list);

    /** see {@link setServiceSnapshotToRestore} */
    void armServiceSnapshotToRestore();

    /** see {@link setServiceSnapshotToRestore} */
    ArrayList<String> getServiceSnapshotToRestore();

    /** see {@link setServiceSnapshotToRestore} */
    void removeServiceSnapshotToRestore();

    public long getServiceSeqNo();

    void setServiceSeqNo(long newServiceSeqNo);

    public long incServiceSeqNo();
}
