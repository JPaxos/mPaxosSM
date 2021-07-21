package lsr.paxos.replica;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;

import lsr.common.RequestType;

final public class ClientBatchID implements RequestType {
    protected final int uniqueRunId;
    protected final int sn;

    public ClientBatchID(int uniqueRunId, int sequenceNumber) {
        if (uniqueRunId < 0 || sequenceNumber < 0)
            throw new IllegalArgumentException("Arguments must be non-negative. " +
                                               "Received: <replicaID:" + uniqueRunId +
                                               ", sequenceNumber:" + sequenceNumber);
        this.uniqueRunId = uniqueRunId;
        this.sn = sequenceNumber;
    }

    public ClientBatchID(DataInputStream input) throws IOException {
        this.uniqueRunId = input.readInt();
        this.sn = input.readInt();
    }

    public ClientBatchID(ByteBuffer buffer) {
        this.uniqueRunId = buffer.getInt();
        this.sn = buffer.getInt();
    }

    public int byteSize() {
        return 4 + 4;
    }

    public void writeTo(DataOutputStream dos) throws IOException {
        dos.writeInt(uniqueRunId);
        dos.writeInt(sn);
    }

    public void writeTo(ByteBuffer bb) {
        bb.putInt(uniqueRunId);
        bb.putInt(sn);
    }

    @Override
    public String toString() {
        return uniqueRunId + ":" + sn;
    }

    @Override
    public boolean equals(Object other) {
        /* Adapted from Effective Java, Item 8 */
        if (!(other instanceof ClientBatchID))
            return false;
        ClientBatchID rid = (ClientBatchID) other;
        return rid.uniqueRunId == uniqueRunId && rid.sn == sn;
    }

    @Override
    public int hashCode() {
        /* Adapted from Effective Java, Item 9 */
        int result = 17;
        result = 31 * result + uniqueRunId;
        result = 31 * result + sn;
        return result;
    }
}
