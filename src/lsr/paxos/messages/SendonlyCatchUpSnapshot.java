package lsr.paxos.messages;

import java.nio.ByteBuffer;

import lsr.paxos.Snapshot;

/**
 * This class is a replacement for CatchUpSnapshot that saves copying data
 * around by Java, but can be used only to send the message (getters throw)
 * 
 * @author JK
 */
public class SendonlyCatchUpSnapshot extends CatchUpSnapshot {
    private static final long serialVersionUID = 1L;

    final ByteBuffer bb;

    public SendonlyCatchUpSnapshot(int view, long requestTime, Snapshot snapshot) {
        super(view, requestTime, snapshot);

        bb = ByteBuffer.allocateDirect(
                /* total size */Integer.BYTES +
                                       /* common Message class headers */ 1 + 4 + 8 +
                                       /* CatchUpSnapshot data */ 8 +
                                       snapshot.upperByteSizeEstimate());
        // size placeholder
        bb.putInt(0);
        // message type
        bb.put((byte) getType().ordinal());
        // view & ts
        bb.putInt(view);
        bb.putLong(sentTime);
        // CatchUpSnapshot contents
        bb.putLong(requestTime);
        snapshot.writeTo(bb);
        // set limit at position, and write the real size to the beginning
        bb.flip();
        bb.putInt(0, bb.limit() - 4);
    }

    public ByteBuffer packMessageToBBWithSize() {
        return bb;
    }

    @Override
    public int byteSize() {
        return bb.limit();
    }

    @Override
    protected void write(ByteBuffer bb) {
        throw new RuntimeException("Invalid use of this class");
    }

    public long getRequestTime() {
        throw new RuntimeException("Invalid use of this class");
    }

    public Snapshot getSnapshot() {
        throw new RuntimeException("Invalid use of this class");
    }

}
