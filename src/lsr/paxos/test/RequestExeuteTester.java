package lsr.paxos.test;

import java.lang.Thread.UncaughtExceptionHandler;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.Random;
import java.util.concurrent.Callable;
import java.util.concurrent.Future;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import lsr.common.Configuration;
import lsr.common.ProcessDescriptor;
import lsr.common.SingleThreadDispatcher;
import lsr.paxos.core.Paxos;
import lsr.paxos.network.UdpNetwork;
import lsr.paxos.replica.DecideCallback;
import lsr.paxos.replica.DecideCallbackImpl;
import lsr.paxos.replica.Replica;
import lsr.paxos.storage.ConsensusInstance;
import lsr.paxos.storage.Log;
import lsr.paxos.storage.PersistentStorage;

/*-
rm -rf /mnt/pmem/*.0; java -Dlogback.configurationFile=../jpaxos/scenarios/COMMON/logback-benchmark.xml -Djava.library.path=natives/jpaxos-pmem/build-debug:natives/service/build-debug  -cp lib/logback-classic-1.2.3.jar:lib/logback-core-1.2.3.jar:lib/slf4j-api-1.7.26.jar:bin lsr.paxos.test.RequestExeuteTester
-*/

public class RequestExeuteTester {

    public native void init(int localId);

    public final int BATCH_MAX_SIZE;
    public final int REQ_SIZE;
    public final int CLIENTS;
    public final int KEYSPACE;

    public final int REQUEST_PAIRS_IN_BATCH;
    public final int BATCH_SIZE;

    public final int SAMPLING_INST = 256;

    final HashMap<Long, Integer> cliToRequest;

    final Random rng = new Random(System.nanoTime());

    final Log log;
    final DecideCallback decideCallback;
    private Method decideCallback_executeRequestsInternal_m;
    final Replica replica;

    int instance = 0;

    public static void main(String[] _args) throws Exception {
        LinkedList<String> args = new LinkedList<>(Arrays.asList(_args));
        int batchMaxSize = 65536;
        int reqSize = 4096;
        int clients = 1000;
        int keyspace = 1000;

        int warmup = 5;
        int sampleTime = 10;
        int sampleCount = -1;

        while (!args.isEmpty()) {
            String token = args.pop();
            if (token.matches("-b|--batchMaxSize"))
                batchMaxSize = Integer.parseInt(args.pop());
            else if (token.matches("-k|--keyspace"))
                keyspace = Integer.parseInt(args.pop());
            else if (token.matches("-r|--reqSize"))
                reqSize = Integer.parseInt(args.pop());
            else if (token.matches("-c|--clients"))
                clients = Integer.parseInt(args.pop());
            else if (token.matches("-w|--warmup"))
                warmup = Integer.parseInt(args.pop());
            else if (token.matches("-s|--samplingTime"))
                sampleTime = Integer.parseInt(args.pop());
            else if (token.matches("-S|--sampleCount"))
                sampleCount = Integer.parseInt(args.pop());
            else
                throw new IllegalArgumentException(token);
        }

        final RequestExeuteTester ret = new RequestExeuteTester(batchMaxSize, reqSize, clients,
                keyspace);

        if (warmup > 0)
            ret.loop(System.currentTimeMillis() + warmup * 1000);

        while (sampleCount != 0) {
            int startInst = ret.instance;
            long deadline = System.currentTimeMillis() + sampleTime * 1000;
            long startTime = System.nanoTime();
            ret.loop(deadline);
            long nanoDuration = System.nanoTime() - startTime;
            int instancesCompleted = ret.instance - startInst;

            double bytePerNanosecond = ret.BATCH_SIZE * instancesCompleted / (double) nanoDuration;
            double instancesPerSecond = instancesCompleted * 1e9 / (double) nanoDuration;
            double requestsPerSecond = instancesPerSecond * ret.REQUEST_PAIRS_IN_BATCH * 2;
            System.out.print("Requ/s " + requestsPerSecond + "\n");
            System.out.print("Inst/s " + instancesPerSecond + "\n");
            System.out.print("Gbit/s " + bytePerNanosecond * 8 + "\n");

            if (sampleCount > 0)
                sampleCount--;
        }
    }

    public RequestExeuteTester(int batchMaxSize, int reqSize, int clients, int keyspace)
            throws Exception {
        BATCH_MAX_SIZE = batchMaxSize;
        REQ_SIZE = reqSize;
        CLIENTS = clients;
        KEYSPACE = keyspace;

        REQUEST_PAIRS_IN_BATCH = (BATCH_MAX_SIZE - 4) / ((16 + REQ_SIZE) + 25);
        BATCH_SIZE = 4 + REQUEST_PAIRS_IN_BATCH * ((16 + REQ_SIZE) + 25);

        cliToRequest = new HashMap<Long, Integer>(CLIENTS);
        for (long i = 0; i < CLIENTS; ++i)
            cliToRequest.put(i, 0);

        Configuration config = new Configuration();
        config.getProperties().setProperty(ProcessDescriptor.NETWORK, "UDP");
        GenericJniService service = new GenericJniService(0);
        replica = new Replica(config, 0, service);
        decideCallback = new DecideCallbackImpl(replica);

        Paxos paxos = new Paxos(new PersistentStorage(), replica);
        // killing dispatcher and network threads autostarted by Paxos
        paxos.getDispatcher().shutdown();
        UdpNetwork net = (UdpNetwork) paxos.getNetwork();
        Field net_thread_f = net.getClass().getDeclaredField("readThread");
        net_thread_f.setAccessible(true);
        Thread net_thread = (Thread) net_thread_f.get(net);
        net_thread.setUncaughtExceptionHandler(new UncaughtExceptionHandler() {
            public void uncaughtException(Thread arg0, Throwable arg1) {
            }
        });
        net_thread.interrupt();
        // setting paxos in Replica
        Field replica_paxos_f = replica.getClass().getDeclaredField("paxos");
        replica_paxos_f.setAccessible(true);
        replica_paxos_f.set(replica, paxos);
        // setting batcher in Replica
        Field replica_batcher_f = replica.getClass().getDeclaredField("batcher");
        replica_batcher_f.setAccessible(true);
        replica_batcher_f.set(replica, paxos.getBatcher());

        // Replace catch-up dispatcher with a fake one that does not call
        // anything
        Field catchup_dispatcher_f = paxos.getCatchup().getClass().getDeclaredField(
                "paxosDispatcher");
        catchup_dispatcher_f.setAccessible(true);
        catchup_dispatcher_f.set(paxos.getCatchup(), new SingleThreadDispatcher("fakeDispatcher") {
            public <T> Future<T> submit(Callable<T> task) {
                try {
                    task.call();
                } catch (Exception e) {
                }
                return null;
            }

            public Future<?> submit(Runnable task) {
                task.run();
                return null;
            }
        });

        log = paxos.getStorage().getLog();

        decideCallback_executeRequestsInternal_m = decideCallback.getClass().getDeclaredMethod(
                "executeRequestsInternal");
        decideCallback_executeRequestsInternal_m.setAccessible(true);
    }

    public ConsensusInstance makeCI() {
        ConsensusInstance ci = log.append();

        ByteBuffer bb = ByteBuffer.allocate(BATCH_SIZE);
        bb.putInt(REQUEST_PAIRS_IN_BATCH * 2); /*-                4 -*/
        for (int i = 0; i < REQUEST_PAIRS_IN_BATCH; ++i) {
            int key = rng.nextInt(KEYSPACE);
            long clientId = rng.nextInt(CLIENTS);
            int sequenceId = cliToRequest.get(clientId);
            cliToRequest.put(clientId, sequenceId + 1);

            bb.putLong(clientId); /*-                             8 -*/
            bb.putInt(sequenceId); /*-                            4 -*/
            bb.putInt(REQ_SIZE); /*-                              4 -*/
            bb.put((byte) 'P'); /*-                           1 \   -*/
            bb.putInt(4); /*-                                 4  |  -*/
            bb.putInt(key); /*-                               4  |N -*/
            bb.position(bb.position() + REQ_SIZE - 9); /*-  N-9 /   -*/
            /*-                                                ==== -*/
            /*-                                                16+N -*/

            key = rng.nextInt(KEYSPACE);
            clientId = rng.nextInt(CLIENTS);
            sequenceId = cliToRequest.get(clientId);
            cliToRequest.put(clientId, sequenceId + 1);

            bb.putLong(clientId); /*-                             8 -*/
            bb.putInt(sequenceId); /*-                            4 -*/
            bb.putInt(9); /*-                                     4 -*/
            bb.put((byte) 'G'); /*-                               1 -*/
            bb.putInt(4); /*-                                     4 -*/
            bb.putInt(key); /*-                                   4 -*/
            /*-                                                  == -*/
            /*-                                                  25 -*/
        }
        ci.updateStateFromDecision(0, bb.array());
        ci.setDecided();
        return ci;
    }

    public void loop(long deadline) throws Exception {
        while (true) {
            makeCI();
            replica.getReplicaStorage().addDecidedWaitingExecution(instance, null);
            decideCallback_executeRequestsInternal_m.invoke(decideCallback);
            instance++;
            if (0 == instance % SAMPLING_INST)
                if (deadline <= System.currentTimeMillis())
                    return;
        }
    }

    static final Logger logger = LoggerFactory.getLogger(RequestExeuteTester.class);
}
