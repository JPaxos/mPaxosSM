package lsr.paxos.messages;

import java.io.DataInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;

import lsr.paxos.Snapshot;

public class CatchUpSnapshot extends Message {
    private static final long serialVersionUID = 1L;

    /** Forwards the time of request, allowing dynamic timeouts for catch-up */
    private long requestTime;

    private ByteBuffer snapshotBB = null;
    private DataInputStream snapshotIS = null;
    private Snapshot snapshot = null;

    public CatchUpSnapshot(int view, long requestTime, Snapshot snapshot) {
        super(view);
        this.requestTime = requestTime;
        this.snapshot = snapshot;
    }

    public CatchUpSnapshot(DataInputStream input) throws IOException {
        super(input);
        requestTime = input.readLong();
        snapshotIS = input;
    }

    public CatchUpSnapshot(ByteBuffer bb) {
        super(bb);
        requestTime = bb.getLong();
        snapshotBB = bb;
    }

    public long getRequestTime() {
        return requestTime;
    }

    public Snapshot getSnapshot() {
        if (snapshot == null) {
            if (snapshotBB != null)
                snapshot = new Snapshot(snapshotBB);
            else if (snapshotIS != null)
                try {
                    snapshot = new Snapshot(snapshotIS);
                } catch (IOException e) {
                    throw new RuntimeException(e);
                }
            else
                throw new NullPointerException(
                        "BB, IS and snapshot are all null, while one should not be");
        }
        return snapshot;
    }

    public MessageType getType() {
        return MessageType.CatchUpSnapshot;
    }

    public int byteSize() {
        // return super.byteSize() + 8 + snapshot.byteSize();
        throw new UnsupportedOperationException("This class does not support byte size");
    }

    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("CatchUpSnapshot (").append(super.toString()).append(") ");
        if (snapshot != null) {
            sb.append("nextInstaceID: ").append(snapshot.getNextInstanceId());
        } else if (snapshotBB != null) {
            sb.append("yet unpacked (in ByteBuffer, size ").append(snapshotBB.remaining()).append(')');
        } else if (snapshotIS != null) {
            sb.append("yet unpacked (in InputStream, available ");
            try {
                sb.append(snapshotIS.available()).append(')');
            } catch (IOException e) {
                sb.append("ERROR: ").append(e).append(')');
            }
        } else {
            sb.append("INVALID - MISSING SNAPSHOT VALUE");
        }

        return sb.toString();
    }

    protected void write(ByteBuffer bb) {
        bb.putLong(requestTime);
        snapshot.writeTo(bb);
    }
}
