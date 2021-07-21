package lsr.paxos.replica;

import static lsr.common.ProcessDescriptor.processDescriptor;

import java.util.List;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import lsr.common.ClientRequest;
import lsr.common.CrashModel;
import lsr.common.SingleThreadDispatcher;
import lsr.paxos.Snapshot;
import lsr.service.Service;

public class ServiceProxy {

    private final Service service;

    /**
     * Creates new <code>ServiceProxy</code> instance.
     * 
     * @param service - the service wrapped by this proxy
     * @param replicaStorage - the cache of responses from service
     * @param replicaDispatcher - the dispatcher used in replica
     */
    public ServiceProxy(Service service, SingleThreadDispatcher replicaDispatcher) {
        this.service = service;

        assert processDescriptor.crashModel == CrashModel.Pmem;
    }

    /**
     * Executes the request on underlying service with correct sequence number.
     * 
     * @param seqNo - the sequential number of this SM command (starting at 0)
     * @param request - the request to execute on service
     * @return the reply from service
     */
    public byte[] execute(long seqNo, ClientRequest request) {
        logger.debug("Executing request {} ({})", seqNo, request.getRequestId());
        return service.execute(seqNo, request.getValue());
    }

    /**
     * Gets the file names that will be created to update the service from.
     * 
     * @param snapshot - the snapshot with newer service state
     */
    /*- FIXME
    public ArrayList<String> getNamesFileToUpdateToSnapshot(Snapshot snapshot) {
        ZipInputStream zis = new ZipInputStream(new ByteArrayInputStream(snapshot.getValue()));
        ZipEntry ze;
        ArrayList<String> paths = new ArrayList<String>();
        try {
            while (null != (ze = zis.getNextEntry())) {
                String path = ze.getName() + "~";
                if (!ze.getName().startsWith("/"))
                    path = processDescriptor.nvmDirectory + "/" + path;
    
                paths.add(path);
            }
            zis.close();
        } catch (IOException e) {
            throw new RuntimeException("", e);
        }
        return paths;
    }
    -*/

    public void updateToSnapshot(List<String> list) {
        service.updateToSnapshotFiles(list);
    }

    public void makeSnapshot(Snapshot snapshot) {
        long serviceTime = 0;
        if (logger.isDebugEnabled())
            serviceTime = System.nanoTime();

        snapshot.setSnapshotFiles(service.getAndLockSnapshotFiles(), service::releaseSnapshotFiles);

        if (logger.isDebugEnabled())
            serviceTime = System.nanoTime() - serviceTime;

        logger.debug("Service provided snapshot in {}s", serviceTime / 1.e9);
    }

    private final static Logger logger = LoggerFactory.getLogger(ServiceProxy.class);

}
