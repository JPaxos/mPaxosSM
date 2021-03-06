package lsr.common;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.slf4j.Marker;
import org.slf4j.MarkerFactory;

/**
 * Contains all the information describing the local process, including the
 * local id and the configuration of the system.
 * 
 * @author Nuno Santos (LSR)
 */
public final class ProcessDescriptor {
    public final Configuration config;

    /*-._.-*
      level:
      ↑HI   LO↓
      W I D T     Warn, Info, Debug, Trace
      W         ✓ replica starts             START
               
          D     ✓ prepare sent               P1A S <view>
          D     ✓ prepare rcvd               P1A R <view>
      W         ✓ view number changed        VIEW <view>
          D     ✓ prepareok sent             P1B S <view>
          D     ✓ prepareok rcvd             P1B R <view> <sender>
          D     ✓ new view ready             PREP <view>
               
            T   ✓ instance proposed          IP <id> <request count>
            T   ✓ instance decided           ID <id>
            T   ✓ instance execution starts  IX <id>
            T   ✓ request executed !!!¹      XX <seqno>
               
      W         ✓ recovery starts            REC B                     ESS VSS
        I       ✓ rec msg sent               R1 S <uid>                 ✓   ✓
        I       ✓ rec msg rcvd               R1 R <uid> <sender>        ✓   ✓
        I       ✓ recOK msg sent             R2 S <uid> <rcpt>          ✓   ✓
        I       ✓ recOK msg rcvd             R2 R <uid> <sender>        ✓   ✓
      W         ✓ recovery ends              REC E
      W         ✓ recovery catchup triggered CRB
      W         ✓ recovery catchup finished  CRE
               
      W         ✓ catchup triggered          CB
        I       ✓ query sent                 CQ S <rcpt>
        I       ✓ query rcvd                 CQ R <sender>
        I       ✓ snap sent                  CS S <rcpt>
        I       ✓ snap rcvd                  CS R <sender>
        I       ✓ snap applied               CS X
        I       ✓ resp sent                  CR S <rcpt>
        I       ✓ resp rcvd                  CR R <sender>
      W         ✓ catchup finished           CE
      
      W I D T   Warn, Info, Debug, Trace
      
      sent messages are logged right after calling network.send()
      rcvd messages are logged right after reaching their proper handler
      this means that inter-thread communication is added to message transmission delay
      ________________________________________________________________________
      ¹ request executed might be too strong.
      
     *-'^'-*/
    public final Marker logMark_Benchmark2019 = MarkerFactory.getMarker("BENCHMARK2019");
    public final Marker logMark_Benchmark2019nope = MarkerFactory.getMarker("BENCHMARK2019NOPE");

    public final Marker logMark_Benchmark = MarkerFactory.getMarker("BENCHMARK");
    public final Marker logMark_OldBenchmark = MarkerFactory.getMarker("OLD_BENCH");

    /*---------------------------------------------
     * The following properties are read from the 
     * paxos.properties file  
     *---------------------------------------------*/
    /**
     * Defines the default window size - that is, the maximum number of
     * concurrently proposed instances.
     */
    public static final String WINDOW_SIZE = "WindowSize";
    public static final int DEFAULT_WINDOW_SIZE = 2;

    /**
     * Maximum UDP packet size in java is 65507. Higher than that and the send
     * method throws an exception.
     * 
     * In practice, most networks have a lower limit on the maximum packet size
     * they can transmit. If this limit is exceeded, the lower layers will
     * usually fragment the packet, but in some cases there's a limit over which
     * large packets are simply dropped or raise an error.
     * 
     * A safe value is the maximum ethernet frame: 1500 - maximum Ethernet
     * payload 20/40 - ipv4/6 header 8 - UDP header.
     * 
     * Usually values up to 8KB are safe.
     */
    public static final String MAX_UDP_PACKET_SIZE = "MaxUDPPacketSize";
    public static final int DEFAULT_MAX_UDP_PACKET_SIZE = 8 * 1024;

    /**
     * Protocol to use between replicas. TCP, UDP or Generic, which combines
     * both
     */
    public static final String NETWORK = "Network";
    public static final String DEFAULT_NETWORK = "TCP";

    /**
     * When sending a huge message (snapshot), this speed is used to estimate
     * the time it takes to transmit the message. Unit: bits per second.
     * Defaults to 10G.
     */
    public static final String ESTIMATED_NET_SPEED = "EstimatedNetSpeedBitsPerSecond";
    public static final long DEFAULT_ESTIMATED_NET_SPEED = 10_000_000_000L;

    /**
     * The maximum size of batched request.
     */
    public static final String BATCH_SIZE = "BatchSize";
    public static final int DEFAULT_BATCH_SIZE = 65507;

    /** How long to wait until suspecting the leader. In milliseconds */
    public static final String FD_SUSPECT_TO = "FDSuspectTimeout";
    public static final int DEFAULT_FD_SUSPECT_TO = 1000;

    /** Interval between sending heartbeats. In milliseconds */
    public final static String FD_SEND_TO = "FDSendTimeout";
    public static final int DEFAULT_FD_SEND_TO = 500;

    /**
     * Interval between sending CatchUpAlive (while preparing snapshots).
     * Defaults to FDSendTimeout if unspecified. Upon no response (interval*2.5)
     * the replica performing the catch-up sends a query to another replica.
     * Unit: milliseconds.
     */
    public final static String CATCHUP_ALIVE_RESEND = "CatchupAliveResend";

    /**
     * The crash model used. For valid entries see {@link CrashModel}
     */
    public static final String CRASH_MODEL = "CrashModel";
    public static final CrashModel DEFAULT_CRASH_MODEL = CrashModel.FullSS;

    /**
     * Location of the stable storage (JPaxos logs) or NVM file
     */
    public static final String LOG_PATH = "LogPath";
    public static final String DEFAULT_LOG_PATH = "jpaxosLogs";

    /**
     * Footprint of the NVM
     */
    public static final String NVM_POOL_SIZE = "NvmPoolSize";
    public static final long DEFAULT_NVM_POOL_SIZE = 256 * 1024 * 1024;

    /**
     * Footprint of the NVM
     */
    public static final String NVM_DIRECTORY = "NvmBaseDir";
    public static final String DEFAULT_NVM_DIRECTORY = "/mnt/pmem";

    /**
     * Maximum time in ms that a batch can be delayed before being proposed.
     * Used to aggregate several requests on a single proposal, for greater
     * efficiency. (Naggle's algorithm for state machine replication).
     */
    public static final String MAX_BATCH_DELAY = "MaxBatchDelay";
    public static final int DEFAULT_MAX_BATCH_DELAY = 10;

    public static final String DECIDED_BUT_NOT_EXECUTED_THRESHOLD = "DecidedButNotExecutedThreshold";
    public static final int DEFAULT_DECIDED_BUT_NOT_EXECUTED_THRESHOLD = 128;

    public static final String CLIENT_ID_GENERATOR = "ClientIDGenerator";
    public static final String DEFAULT_CLIENT_ID_GENERATOR = "ViewEpoch";

    /** Enable or disable collecting of statistics */
    public static final String BENCHMARK_RUN_REPLICA = "BenchmarkRunReplica";
    public static final boolean DEFAULT_BENCHMARK_RUN_REPLICA = false;

    /**
     * Before any snapshot was made, we need to have an estimate of snapshot
     * size. Value given as for now is 1 KB
     */
    public static final String FIRST_SNAPSHOT_SIZE_ESTIMATE = "FirstSnapshotEstimateBytes";
    public static final int DEFAULT_FIRST_SNAPSHOT_SIZE_ESTIMATE = 1024;

    /** Minimum size of the log before a snapshot is attempted */
    public static final String SNAPSHOT_MIN_LOG_SIZE = "MinLogSizeForRatioCheckBytes";
    public static final int DEFAULT_SNAPSHOT_MIN_LOG_SIZE = 100 * 1024;

    /** Ratio = \frac{log}{snapshot}. How bigger the log must be to ask */
    public static final String SNAPSHOT_ASK_RATIO = "SnapshotAskRatio";
    public static final double DEFAULT_SNAPSHOT_ASK_RATIO = 1;

    /** Ratio = \frac{log}{snapshot}. How bigger the log must be to force */
    public static final String SNAPSHOT_FORCE_RATIO = "SnapshotForceRatio";
    public static final double DEFAULT_SNAPSHOT_FORCE_RATIO = 2;

    /** Minimum number of instances for checking ratios */
    public static final String MIN_SNAPSHOT_SAMPLING = "MinimumInstancesForSnapshotRatioSample";
    public static final int DEFAULT_MIN_SNAPSHOT_SAMPLING = 50;

    /**
     * When a process gets a Propose, then it checks if it's in window to see if
     * the process is not late. With N≥5 this can sometimes be violated. Setting
     * this will stop catch-up in such case.
     **/
    public static final String CU_WS_VIOLATION_ALLOWANCE = "CatchUpWindowSizeViolationAllowance";
    public static final int DEFAULT_CU_WS_VIOLATION_ALLOWANCE = 0;

    /** If true, then ZipOptputStream.setMethod is DEFLATED, else STORED */
    public static final String SNAPSHOT_COMPRESS = "snapshotCompress";
    public static final boolean DEFAULT_SNAPSHOT_COMPRESS = false;

    /** Value passed to ZipOptputStream.setLevel() when deflating snapshots */
    public static final String SNAPSHOT_COMPRESSION_LEVEL = "snapshotCompressionLevel";
    public static final int DEFAULT_SNAPSHOT_COMPRESSION_LEVEL = 3;

    public static final String NUMBER_OF_EXECUTED_INST_IN_LOG = "NumberOfExecutedInstInLog";
    public static final int DEFAULT_NUMBER_OF_EXECUTED_INST_IN_LOG = DEFAULT_WINDOW_SIZE * 2;

    public static final String RETRANSMIT_TIMEOUT = "RetransmitTimeoutMilisecs";
    public static final long DEFAULT_RETRANSMIT_TIMEOUT = 1000;

    /** If a TCP connection fails, how much to wait for another try */
    public static final String TCP_RECONNECT_TIMEOUT = "TcpReconnectMilisecs";
    public static final long DEFAULT_TCP_RECONNECT_TIMEOUT = 1000;

    /** ??? Corresponds to a ethernet frame */
    public final static String FORWARD_MAX_BATCH_SIZE = "replica.ForwardMaxBatchSize";
    public final static int DEFAULT_FORWARD_MAX_BATCH_SIZE = 1450;

    /** ??? In milliseconds */
    public final static String FORWARD_MAX_BATCH_DELAY = "replica.ForwardMaxBatchDelay";
    public final static int DEFAULT_FORWARD_MAX_BATCH_DELAY = 50;

    /** How many selector threads to use */
    public static final String SELECTOR_THREADS = "replica.SelectorThreads";
    public static final int DEFAULT_SELECTOR_THREADS = -1;

    /**
     * Size of a buffer for reading client requests; larger requests than this
     * size will cause extra memory allocation and freeing at each such request.
     * This variable impacts memory usage, as each client connection
     * pre-allocates such buffer.
     */
    public static final String CLIENT_REQUEST_BUFFER_SIZE = "replica.ClientRequestBufferSize";
    public static final int DEFAULT_CLIENT_REQUEST_BUFFER_SIZE = 8 * 1024 +
                                                                 ClientCommand.HEADERS_SIZE;

    /**
     * How long can the proposer / catch-up wait for batch values during view
     * change / catching up, in milliseconds
     */
    private static final String MAX_BATCH_FETCHING_TIME_MS = "TimeoutFetchBatchValue";
    private static final int DEFAULT_MAX_BATCH_FETCHING_TIME_MS = 2500;

    private static final String MULTICAST_PORT = "MulticastPort";
    private static final int DEFAULT_MULTICAST_PORT = 3000;

    private static final String MULTICAST_IP_ADDRESS = "MulticastIpAddress";
    private static final String DEFAULT_MULTICAST_IP_ADDRESS = "224.0.0.144";

    private static final String MTU = "NetworkMtuSize";
    private static final int DEFAULT_MTU = 1492;

    private static final String REDIRECT_CLIENTS_FROM_LEADER = "RedirectClientsFromLeader";
    private static final boolean DEFAULT_REDIRECT_CLIENTS_FROM_LEADER = false;

    private static final String AUGMENTED_PAXOS = "AugmentedPaxos";
    private static final boolean DEFAULT_AUGMENTED_PAXOS = false;

    /*
     * Exposing fields is generally not good practice, but here they are made
     * final, so there is no danger of exposing them. Advantage: less
     * boilerplate code.
     */
    public final int localId;
    public final int numReplicas;
    public final int windowSize;
    public final int batchingLevel;
    public final int maxUdpPacketSize;
    public final int maxBatchDelay;
    public final String clientIDGenerator;
    public final String network;
    public final CrashModel crashModel;
    public final String logPath;
    public final long nvmPoolSize;
    public final String nvmDirectory;
    public final long estimatedNetSpeed;

    public final int firstSnapshotSizeEstimate;
    public final int snapshotMinLogSize;
    public final double snapshotAskRatio;
    public final double snapshotForceRatio;
    public final int minSnapshotSampling;
    public final int cuWSViolationAllowance;

    public final boolean snapshotCompress;
    public final int snapshotCompressionLevel;

    public final long retransmitTimeout;
    public final long tcpReconnectTimeout;
    public final int fdSuspectTimeout;
    public final int fdSendTimeout;
    public final int catchupAliveResend;

    public final int forwardBatchMaxSize;
    public final int forwardBatchMaxDelay;

    public final int selectorThreadCount;

    public final int clientRequestBufferSize;

    /** ⌊(n+1)/2⌋ */
    public final int majority;

    public final long maxBatchFetchingTimeoutMs;

    public final int multicastPort;

    public final String multicastIpAddress;

    public final int mtu;

    public final boolean augmentedPaxos;

    public int decidedButNotExecutedThreshold;

    public final boolean redirectClientsFromLeader;

    public final int numberOfExecutedInstInLog;

    /**
     * The singleton instance of process descriptor. Must be initialized before
     * use.
     */
    public static ProcessDescriptor processDescriptor = null;

    public static void initialize(Configuration config, int localId) {
        ProcessDescriptor.processDescriptor = new ProcessDescriptor(config, localId);
    }

    private ProcessDescriptor(Configuration config, int localId) {
        this.localId = localId;
        this.config = config;

        this.numReplicas = config.getN();

        this.windowSize = config.getIntProperty(
                WINDOW_SIZE, DEFAULT_WINDOW_SIZE);
        this.batchingLevel = config.getIntProperty(
                BATCH_SIZE, DEFAULT_BATCH_SIZE);
        this.maxUdpPacketSize = config.getIntProperty(
                MAX_UDP_PACKET_SIZE, DEFAULT_MAX_UDP_PACKET_SIZE);
        this.maxBatchDelay = config.getIntProperty(
                MAX_BATCH_DELAY, DEFAULT_MAX_BATCH_DELAY);
        this.clientIDGenerator = config.getProperty(
                CLIENT_ID_GENERATOR, DEFAULT_CLIENT_ID_GENERATOR);
        this.network = config.getProperty(
                NETWORK, DEFAULT_NETWORK);
        this.logPath = config.getProperty(
                LOG_PATH, DEFAULT_LOG_PATH);
        this.nvmPoolSize = config.getLongProperty(
                NVM_POOL_SIZE, DEFAULT_NVM_POOL_SIZE);
        this.nvmDirectory = config.getProperty(
                NVM_DIRECTORY, DEFAULT_NVM_DIRECTORY);
        this.firstSnapshotSizeEstimate = config.getIntProperty(
                FIRST_SNAPSHOT_SIZE_ESTIMATE, DEFAULT_FIRST_SNAPSHOT_SIZE_ESTIMATE);

        this.snapshotMinLogSize = Math.max(1, config.getIntProperty(
                SNAPSHOT_MIN_LOG_SIZE, DEFAULT_SNAPSHOT_MIN_LOG_SIZE));
        this.snapshotAskRatio = config.getDoubleProperty(
                SNAPSHOT_ASK_RATIO, DEFAULT_SNAPSHOT_ASK_RATIO);
        this.snapshotForceRatio = config.getDoubleProperty(
                SNAPSHOT_FORCE_RATIO, DEFAULT_SNAPSHOT_FORCE_RATIO);
        this.minSnapshotSampling = config.getIntProperty(
                MIN_SNAPSHOT_SAMPLING, DEFAULT_MIN_SNAPSHOT_SAMPLING);
        this.cuWSViolationAllowance = config.getIntProperty(
                CU_WS_VIOLATION_ALLOWANCE, DEFAULT_CU_WS_VIOLATION_ALLOWANCE);

        this.snapshotCompress = config.getBooleanProperty(
                SNAPSHOT_COMPRESS, DEFAULT_SNAPSHOT_COMPRESS);
        this.snapshotCompressionLevel = config.getIntProperty(
                SNAPSHOT_COMPRESSION_LEVEL, DEFAULT_SNAPSHOT_COMPRESSION_LEVEL);

        this.numberOfExecutedInstInLog = Math.max(0, config.getIntProperty(
                NUMBER_OF_EXECUTED_INST_IN_LOG, DEFAULT_NUMBER_OF_EXECUTED_INST_IN_LOG));
        this.retransmitTimeout = config.getLongProperty(
                RETRANSMIT_TIMEOUT, DEFAULT_RETRANSMIT_TIMEOUT);
        this.tcpReconnectTimeout = config.getLongProperty(
                TCP_RECONNECT_TIMEOUT, DEFAULT_TCP_RECONNECT_TIMEOUT);
        this.fdSuspectTimeout = config.getIntProperty(
                FD_SUSPECT_TO, DEFAULT_FD_SUSPECT_TO);
        this.fdSendTimeout = config.getIntProperty(
                FD_SEND_TO, DEFAULT_FD_SEND_TO);

        this.catchupAliveResend = config.getIntProperty(CATCHUP_ALIVE_RESEND, fdSendTimeout);
        this.estimatedNetSpeed = config.getLongProperty(
                ESTIMATED_NET_SPEED, DEFAULT_ESTIMATED_NET_SPEED);

        this.forwardBatchMaxDelay = config.getIntProperty(
                FORWARD_MAX_BATCH_DELAY,
                DEFAULT_FORWARD_MAX_BATCH_DELAY);
        this.forwardBatchMaxSize = config.getIntProperty(FORWARD_MAX_BATCH_SIZE,
                DEFAULT_FORWARD_MAX_BATCH_SIZE);

        this.decidedButNotExecutedThreshold = config.getIntProperty(
                DECIDED_BUT_NOT_EXECUTED_THRESHOLD, DEFAULT_DECIDED_BUT_NOT_EXECUTED_THRESHOLD);

        this.selectorThreadCount = config.getIntProperty(SELECTOR_THREADS,
                DEFAULT_SELECTOR_THREADS);

        this.clientRequestBufferSize = config.getIntProperty(
                CLIENT_REQUEST_BUFFER_SIZE,
                DEFAULT_CLIENT_REQUEST_BUFFER_SIZE);

        this.maxBatchFetchingTimeoutMs = config.getIntProperty(
                MAX_BATCH_FETCHING_TIME_MS,
                DEFAULT_MAX_BATCH_FETCHING_TIME_MS);

        this.multicastPort = config.getIntProperty(MULTICAST_PORT, DEFAULT_MULTICAST_PORT);

        this.multicastIpAddress = config.getProperty(MULTICAST_IP_ADDRESS,
                DEFAULT_MULTICAST_IP_ADDRESS);

        this.mtu = config.getIntProperty(MTU, DEFAULT_MTU);

        this.redirectClientsFromLeader = config.getBooleanProperty(REDIRECT_CLIENTS_FROM_LEADER,
                DEFAULT_REDIRECT_CLIENTS_FROM_LEADER);

        this.augmentedPaxos = config.getBooleanProperty(AUGMENTED_PAXOS,
                DEFAULT_AUGMENTED_PAXOS);

        String crash = config.getProperty(
                CRASH_MODEL, DEFAULT_CRASH_MODEL.toString());
        CrashModel crashModel;
        try {
            crashModel = CrashModel.valueOf(crash);
        } catch (IllegalArgumentException e) {
            throw new RuntimeException(
                    "Config file contains unknown crash model \"" + crash + "\"");
        }
        this.crashModel = crashModel;

        majority = (numReplicas + 1) / 2;

        printProcessDescriptor(config, crashModel);

        logMark_Benchmark.add(logMark_OldBenchmark);
    }

    private void printProcessDescriptor(Configuration config, CrashModel crashModel) {
        logger.info(config.toString());

        logger.info(WINDOW_SIZE + "=" + windowSize);
        logger.info(BATCH_SIZE + "=" + batchingLevel);
        logger.info(MAX_BATCH_DELAY + "=" + maxBatchDelay);
        logger.info(MAX_UDP_PACKET_SIZE + "=" + maxUdpPacketSize);
        logger.info(NETWORK + "=" + network);
        logger.info(CLIENT_ID_GENERATOR + "=" + clientIDGenerator);
        logger.info(FD_SEND_TO + " = " + fdSendTimeout);
        logger.info(FD_SUSPECT_TO + "=" + fdSuspectTimeout);
        logger.info("Crash model: " + crashModel + ", LogPath: " + logPath);
        if (crashModel == CrashModel.Pmem)
            logger.info(NVM_POOL_SIZE + "=" + nvmPoolSize);
        logger.info(FIRST_SNAPSHOT_SIZE_ESTIMATE + "=" + firstSnapshotSizeEstimate);
        logger.info(SNAPSHOT_MIN_LOG_SIZE + "=" + snapshotMinLogSize);
        logger.info(SNAPSHOT_ASK_RATIO + "=" + snapshotAskRatio);
        logger.info(SNAPSHOT_FORCE_RATIO + "=" + snapshotForceRatio);
        logger.info(MIN_SNAPSHOT_SAMPLING + "=" + minSnapshotSampling);
        logger.info(SNAPSHOT_COMPRESS + "=" + snapshotCompress);
        logger.info(SNAPSHOT_COMPRESSION_LEVEL + "=" + snapshotCompressionLevel);
        logger.info(RETRANSMIT_TIMEOUT + "=" + retransmitTimeout);
        logger.info(TCP_RECONNECT_TIMEOUT + "=" + tcpReconnectTimeout);
        logger.info(FORWARD_MAX_BATCH_DELAY + "=" + forwardBatchMaxDelay);
        logger.info(FORWARD_MAX_BATCH_SIZE + "=" + forwardBatchMaxSize);
        logger.info(SELECTOR_THREADS + "=" + forwardBatchMaxSize);
        logger.info(CLIENT_REQUEST_BUFFER_SIZE + "=" + clientRequestBufferSize);
        logger.info(MAX_BATCH_FETCHING_TIME_MS + "=" + maxBatchFetchingTimeoutMs);
        logger.info(MULTICAST_PORT + "=" + multicastPort);
        logger.info(MULTICAST_IP_ADDRESS + "=" + multicastIpAddress);
        logger.info(MTU + "=" + mtu);
    }

    /**
     * @return the local process
     */
    public PID getLocalProcess() {
        return config.getProcess(localId);
    }

    public int getLeaderOfView(int view) {
        return view % numReplicas;
    }

    public boolean isLocalProcessLeader(int view) {
        return getLeaderOfView(view) == localId;
    }

    private final static Logger logger = LoggerFactory.getLogger(ProcessDescriptor.class);

    /**
     * Next replica ID in lexical order, other than local replica.
     */
    public int nextReplica(int nextReplicaToAsk) {
        nextReplicaToAsk++;
        nextReplicaToAsk %= numReplicas;
        if (nextReplicaToAsk == localId) {
            nextReplicaToAsk++;
            return nextReplicaToAsk % numReplicas;
        }
        return nextReplicaToAsk;
    }
}
