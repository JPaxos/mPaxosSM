package lsr.service;

import java.util.List;

import lsr.paxos.replica.Replica;

/**
 * This interface represents state machine. It is used by {@link Replica} to
 * execute commands from clients, for getting the state as a snapshot, and also
 * to update the state from other snapshot (received from an other replica).
 * <p>
 * All methods are called from the same thread, so it is not necessary to
 * synchronize them.
 * 
 * @see Replica
 * @see SnapshotListener
 */
public interface Service {

    /**
     * Executes one command from client on this state machine. This method will
     * be called by {@link Replica} in proper order.
     * 
     * @param seqNo - the sequential number of this command (starting at 0)
     * @param value - value of instance to execute on this service
     * @return generated reply which will be sent back to client
     */
    byte[] execute(long seqNo, byte[] value);

    /**
     * Called when a peer replica needs state to catch up (or recover). Shall
     * return the paths to files that contain the state. Service must ensure
     * that the files are up to date and must not make any changes to the files
     * until releaseSnapshotFiles is called.
     */
    List<String> getAndLockSnapshotFiles();

    /**
     * see getAndLockSnapshotFiles
     */
    void releaseSnapshotFiles();

    /**
     * Restores the service state from snapshot
     * 
     * @param snapshotFiles the files a peer replica returned from
     *            getAndLockSnapshotFiles
     */
    void updateToSnapshotFiles(List<String> snapshotFiles);
}