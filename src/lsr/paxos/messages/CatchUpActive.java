package lsr.paxos.messages;

import java.io.DataInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;

/**
 * Sent by a peer replica when it prepares CU snapshot to inform that it handles
 * the query and one should wait instead of sending new queries
 */

public class CatchUpActive extends Message {
    private static final long serialVersionUID = 1L;

    private final long requestTime;
    private final long suspendCatchupQueryDelay;

    public CatchUpActive(int view, long requestTime, long suspendDelay) {
        super(view);
        this.requestTime = requestTime;
        suspendCatchupQueryDelay = suspendDelay;
    }

    public CatchUpActive(DataInputStream input) throws IOException {
        super(input);
        requestTime = input.readLong();
        suspendCatchupQueryDelay = input.readLong();
    }

    public CatchUpActive(ByteBuffer bb) {
        super(bb);
        requestTime = bb.getLong();
        suspendCatchupQueryDelay = bb.getLong();
    }

    @Override
    public MessageType getType() {
        return MessageType.CatchUpActive;
    }

    @Override
    protected void write(ByteBuffer bb) {
        bb.putLong(requestTime);
        bb.putLong(suspendCatchupQueryDelay);
    }

    public long getSuspendCatchupQueryDelay() {
        return suspendCatchupQueryDelay;
    }

    public long getRequestTime() {
        return requestTime;
    }

    @Override
    public int byteSize() {
        return super.byteSize() + 16;
    }

    @Override
    public String toString() {
        return "CatchUpActive (" + super.toString() + ") ts: " + requestTime +
               " delay: " + suspendCatchupQueryDelay;
    }

}
