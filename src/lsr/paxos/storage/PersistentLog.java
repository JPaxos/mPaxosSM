package lsr.paxos.storage;

import java.util.Collections;
import java.util.SortedMap;
import java.util.TreeMap;

public class PersistentLog implements Log {

    protected TreeMap<Integer, PersistentConsensusInstance> instanceMapView = new TreeMap<Integer, PersistentConsensusInstance>();
    protected int nextIdCache;

    private static native int[] getExistingInstances();

    public PersistentLog() {
        for (int id : getExistingInstances()) {
            instanceMapView.put(id, new PersistentConsensusInstance(id));
        }
        nextIdCache = getNextId_();
    }

    @Override
    public SortedMap<Integer, PersistentConsensusInstance> getInstanceMap() {
        return Collections.unmodifiableSortedMap(instanceMapView);
    }

    private static native void appendUpTo(int instanceId);

    @Override
    public PersistentConsensusInstance getInstance(int instanceId) {
        boolean resized = (instanceId >= nextIdCache);
        while (instanceId >= nextIdCache) {
            instanceMapView.put(nextIdCache, new PersistentConsensusInstance(nextIdCache));
            nextIdCache++;
        }

        if (resized) {
            appendUpTo(instanceId);
        }
        return instanceMapView.get(instanceId);
    }

    private static native int append_();

    @Override
    public PersistentConsensusInstance append() {
        int id = append_();
        PersistentConsensusInstance ci = new PersistentConsensusInstance(id);
        instanceMapView.put(id, ci);
        nextIdCache = id + 1;
        return ci;
    }

    private static native int getNextId_();

    @Override
    public int getNextId() {
        return nextIdCache;
    }

    @Override
    public native int getLowestAvailableId();

    private static native void truncateBelow_(int instanceId);

    @Override
    public void truncateBelow(int instanceId) {
        truncateBelow_(instanceId);
        while (!instanceMapView.isEmpty() && instanceMapView.firstKey() < instanceId)
            instanceMapView.pollFirstEntry();
    }

    /// returns the list of removed instances
    private static native int[] clearUndecidedBelow_(int instanceId);

    @Override
    public void clearUndecidedBelow(int instanceId) {
        for (int id : clearUndecidedBelow_(instanceId)) {
            instanceMapView.remove(id);
        }
    }
}
