package lsr.paxos;

import java.nio.ByteBuffer;

import lsr.common.ClientRequest;

public class UnBatcher {
    // Prevent construction
    private UnBatcher() {
    };

    /// returns number of client requests in a batch
    public static int countCR(byte[] source) {
        ByteBuffer bb = ByteBuffer.wrap(source);
        return bb.getInt();
    }

    public static ClientRequest[] unpackCR(byte[] source) {
        ByteBuffer bb = ByteBuffer.wrap(source);
        int count = bb.getInt();

        ClientRequest[] requests = new ClientRequest[count];

        for (int i = 0; i < count; ++i) {
            requests[i] = ClientRequest.create(bb);
        }

        assert bb.remaining() == 0 : "Packing/unpacking error";

        return requests;
    }
}
