package lsr.paxos.test;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.ByteBuffer;
import java.util.Random;
import java.util.Vector;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.Semaphore;
import java.util.concurrent.atomic.AtomicInteger;

import lsr.common.Configuration;
import lsr.paxos.client.Client;
import lsr.paxos.client.ReplicationException;

public class HashMapMultiClient {
    private Vector<ClientThread> clients = new Vector<ClientThread>();

    private AtomicInteger runningClients = new AtomicInteger(0);
    private final Semaphore finishedLock = new Semaphore(1);
    private long startTime;
    private int lastRequestCount;

    private final Random rnd = new Random();

    private final int requestSize;
    private final int keyspaceSize;

    private final Configuration configuration;

    public static void main(String[] args) throws IOException, ReplicationException,
            InterruptedException {
        if (args.length != 2) {
            showUsage();
            System.exit(1);
        }
        printUsage();
        int requestSize = Integer.parseInt(args[0]);
        int keyspaceSize = Integer.parseInt(args[1]);
        HashMapMultiClient client = new HashMapMultiClient(requestSize, keyspaceSize);
        client.run();
    }

    private static void showUsage() {
        System.out.println(
                HashMapMultiClient.class.getCanonicalName() + " <requestSize> <keySpace>");
    }

    private static void printUsage() {
        System.out.println("bye  -- quits (killing clients if necessay)");
        System.out.println("kill  -- stops all clients");
        System.out.println("<clientCount> <requestsPerClient> [<any string>] -- sends requests");
    }

    class ClientThread extends Thread {
        private final byte[] put;
        private final byte[] get;
        final Client client;
        private ArrayBlockingQueue<Integer> sends;

        public ClientThread() throws IOException {
            setDaemon(true);
            client = new Client(configuration);
            sends = new ArrayBlockingQueue<Integer>(128);
            put = new byte[requestSize];
            get = new byte[1 + 4 + 4];
        }

        @Override
        public void run() {
            try {
                client.connect();

                while (true) {
                    Integer count;
                    count = sends.take();
                    for (int i = 0; i < count; i++) {
                        if (Thread.interrupted()) {
                            break;
                        }

                        if (rnd.nextBoolean()) {
                            int key = Math.abs(rnd.nextInt()) % keyspaceSize;
                            ByteBuffer bb = ByteBuffer.wrap(put);
                            bb.put((byte) 'P');
                            bb.putInt(4);
                            bb.putInt(key);
                            // only some byte of the request get randomized
                            bb.putLong(rnd.nextLong());

                            @SuppressWarnings("unused")
                            byte[] response;
                            response = client.execute(put);
                        } else {
                            int key = Math.abs(rnd.nextInt()) % keyspaceSize;
                            ByteBuffer bb = ByteBuffer.wrap(get);
                            bb.put((byte) 'G');
                            bb.putInt(4);
                            bb.putInt(key);

                            @SuppressWarnings("unused")
                            byte[] response;
                            response = client.execute(get);
                        }
                    }
                    int stillActive = runningClients.decrementAndGet();
                    if (stillActive == 0) {
                        finishedSend();
                    }
                }
            } catch (ReplicationException e) {
                System.err.println(e.getLocalizedMessage());
                System.exit(1);
            } catch (InterruptedException e) {
                int stillActive = runningClients.decrementAndGet();
                if (stillActive == 0) {
                    finishedSend();
                }
            }
        }

        public void execute(int count) throws InterruptedException {
            sends.put(count);
        }

    }

    public HashMapMultiClient(int requestSize, int keyspaceSize) throws IOException {
        this.configuration = new Configuration();
        this.requestSize = requestSize;
        this.keyspaceSize = keyspaceSize;
    }

    public void run() throws IOException, ReplicationException, InterruptedException {

        BufferedReader reader = new BufferedReader(new InputStreamReader(System.in));
        while (true) {
            String line = reader.readLine();
            if (line == null)
                // EOF
                break;

            String[] args = line.split(" ");

            if (args[0].equals("bye")) {
                for (ClientThread client : clients)
                    client.interrupt();
                break;
            }

            if (args[0].equals("kill")) {
                for (ClientThread client : clients)
                    client.interrupt();
                continue;
            }

            if (args.length < 2) {
                System.err.println("Wrong command length! Expected:");
                printUsage();
                continue;
            }

            int clientCount;
            int requests;

            try {
                clientCount = Integer.parseInt(args[0]);
                requests = Integer.parseInt(args[1]);
            } catch (NumberFormatException e) {
                System.err.println("Wrong argument! Expected:");
                printUsage();
                continue;
            }

            execute(clientCount, requests);
        }
    }

    public void finishedSend() {

        long duration = System.currentTimeMillis() - startTime;
        System.err.println(String.format("Finished %d %4.2f\n", duration,
                (double) lastRequestCount / duration));

        finishedLock.release();
    }

    private void execute(int clientCount, int requests)
            throws ReplicationException, IOException, InterruptedException {

        finishedLock.acquire();

        for (int i = clients.size(); i < clientCount; i++) {
            ClientThread client = new ClientThread();
            client.start();
            clients.add(client);
        }

        runningClients.addAndGet(clientCount);

        startTime = System.currentTimeMillis();
        lastRequestCount = clientCount * requests;

        for (int i = 0; i < clientCount; i++) {
            clients.get(i).execute(requests);
        }
    }
}
