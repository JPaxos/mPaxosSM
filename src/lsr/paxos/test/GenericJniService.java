package lsr.paxos.test;

import static lsr.common.ProcessDescriptor.processDescriptor;

import java.util.Collections;
import java.util.List;
import java.util.concurrent.RejectedExecutionHandler;
import java.util.concurrent.ScheduledThreadPoolExecutor;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import lsr.common.Configuration;
import lsr.common.KillOnExceptionHandler;
import lsr.paxos.replica.Replica;
import lsr.service.Service;

public class GenericJniService implements Service {

    public native void init(int localId);

    public GenericJniService(int localId) {
        System.loadLibrary("jpaxos-service");
        init(localId);
    }

    public static void main(String[] args) throws Exception {
        int localId = Integer.parseInt(args[0]);
        Configuration config = new Configuration();
        GenericJniService service = new GenericJniService(localId);
        Replica replica = new Replica(config, localId, service);
        startRpsReporter();
        replica.start();
        System.in.read();
        System.exit(-1);
    }

    protected static native byte[] execute_(long seqNo, byte[] value);

    @Override
    public byte[] execute(long seqNo, byte[] value) {
        byte[] resp = execute_(seqNo, value);
        executedCtr.incrementAndGet();
        return resp;
    }

    protected native static String getSnapshotFile();

    @Override
    public List<String> getAndLockSnapshotFiles() {
        return Collections.singletonList(getSnapshotFile());
    }

    protected native static void releaseSnapshotFile();

    @Override
    public void releaseSnapshotFiles() {
        releaseSnapshotFile();
    }

    protected native void updateToSnapshot(String file);

    @Override
    public void updateToSnapshotFiles(List<String> snapshotFiles) {
        updateToSnapshot(snapshotFiles.get(0));
    }

    private static AtomicInteger executedCtr = new AtomicInteger(0);
    private final static long SAMPLING_MS = 100;

    protected static void startRpsReporter() {
        // Creates an executor with named thread that runs task printing stats
        new ScheduledThreadPoolExecutor(1, new ThreadFactory() {
            boolean startled = false;

            public Thread newThread(Runnable r) {
                if (startled)
                    throw new RuntimeException();
                startled = true;
                Thread thread = new Thread(r, "RPS_Reporter");
                thread.setDaemon(true);
                thread.setPriority(Thread.MAX_PRIORITY);
                thread.setUncaughtExceptionHandler(new KillOnExceptionHandler());
                return thread;
            }
        }, new RejectedExecutionHandler() {
            public void rejectedExecution(Runnable r, ThreadPoolExecutor executor) {
                throw new RuntimeException("" + r + " " + executor);
            }
        }).scheduleAtFixedRate(new Runnable() {
            private long lastSeenCtr = executedCtr.get();
            private long lastSeenTime;
            {
                lastSeenTime = System.currentTimeMillis();
                lastSeenTime -= lastSeenTime % SAMPLING_MS;
            }

            public void run() {
                if (processDescriptor == null)
                    // this happens upon startup
                    return;

                // Hrm.... Java and the behavior of 'scheduleAtFixedRate' are
                // far far from what one expects...
                long elapsed;
                while ((elapsed = System.currentTimeMillis() - lastSeenTime) < SAMPLING_MS)
                    try {
                        Thread.sleep(SAMPLING_MS - elapsed);
                    } catch (InterruptedException e) {
                    }

                long ctr = executedCtr.get();

                if (logger.isInfoEnabled(processDescriptor.logMark_Benchmark))
                    logger.info(processDescriptor.logMark_Benchmark, "RPS: {}",
                            (ctr - lastSeenCtr) * (1000.0 / elapsed));

                if (logger.isWarnEnabled(processDescriptor.logMark_Benchmark2019))
                    logger.warn(processDescriptor.logMark_Benchmark2019, "RPS {}",
                            (ctr - lastSeenCtr) * (1000.0 / elapsed));

                lastSeenCtr = ctr;
                lastSeenTime += elapsed;
                lastSeenTime -= lastSeenTime % SAMPLING_MS;
            }
        }, SAMPLING_MS - (System.currentTimeMillis() % SAMPLING_MS), SAMPLING_MS,
                TimeUnit.MILLISECONDS);
    }

    private static final Logger logger = LoggerFactory.getLogger(GenericJniService.class);
}
