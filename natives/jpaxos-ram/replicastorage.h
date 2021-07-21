#ifndef REPLICASTORAGE_H
#define REPLICASTORAGE_H

#include "jpaxos-common.hpp"

#include <list>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>


struct ClientReply {
    jlong clientId {0};
    jint seqNo {-1};
    
    signed char* value {nullptr};
    size_t valueLength {0};
    
    void freeValue() {
        if(value) {
            delete [] value;
            value = nullptr;
        }
    }
};

class ReplicaStorage
{
    jint executeUB {0};
    jlong serviceSeqNo {0};
    std::unordered_set<jint> decidedWaitingExecution = std::unordered_set<jint>(128);
    
    
    std::unordered_map<jlong, ClientReply> lastReplyForClient = std::unordered_map<jlong, ClientReply>(2<<14 /* 16384 */);
    
    mutable std::shared_mutex lastReplyForClient_mutex;
    
    std::vector<std::string> serviceSnapshotToRestore;
    bool serviceSnapshotToRestore_armed {false};
    
public:
    ReplicaStorage(){}
    
    jint getExcuteUB() const {return executeUB;}
    void setExcuteUB(jint _executeUB) {executeUB=_executeUB;}
    void incrementExcuteUB(){++executeUB;}
    
    jlong getServiceSeqNo() const {return serviceSeqNo;}
    void setServiceSeqNo(jlong _serviceSeqNo) {serviceSeqNo=_serviceSeqNo;}
    jlong incServiceSeqNo(){++serviceSeqNo; return serviceSeqNo;}
    
    void addDecidedWaitingExecution(jint instanceId) {
        #ifdef DEBUG_DISAPPEARING_CI_VALUE
        fprintf(debugLogFile, "Adding DWE %d thread: %d\n", instanceId, gettid());
        #endif
        decidedWaitingExecution.insert(instanceId);
    }
    jboolean isDecidedWaitingExecution(jint instanceId) const {
        #ifdef DEBUG_DISAPPEARING_CI_VALUE
        fprintf(debugLogFile, "Checking DWE %d thread: %d\n", instanceId, gettid());
        #endif
        return (decidedWaitingExecution.find(instanceId) != decidedWaitingExecution.end()) ? JNI_TRUE : JNI_FALSE;
    }
    void releaseDecidedWaitingExecution(jint instanceId) {
        #ifdef DEBUG_DISAPPEARING_CI_VALUE
        fprintf(debugLogFile, "Releasing DWE %d thread: %d\n", instanceId, gettid());
        #endif
        decidedWaitingExecution.erase(instanceId);
    }
    void releaseDecidedWaitingExecutionUpTo(jint instanceId) {
        std::list<jint> instancesToRemove;
        for(auto iid: decidedWaitingExecution){
            if(iid < instanceId)
                instancesToRemove.push_back(iid);
        }
        for(auto iid: instancesToRemove)
            decidedWaitingExecution.erase(iid);
    }
    size_t decidedWaitingExecutionCount() const {return decidedWaitingExecution.size();}
    
    /// called from within a transaction
    void setLastReplyForClient(jlong clientId, jint clientSeqNo, signed char * value, size_t valueLength){
        std::unique_lock<std::shared_mutex> lock(lastReplyForClient_mutex);
        ClientReply & reply = lastReplyForClient[clientId];
        reply.freeValue();
        
        reply.clientId    = clientId;
        reply.seqNo       = clientSeqNo;
        reply.value       = value;
        reply.valueLength = valueLength;

        lock.unlock();
    }
    
    jint getLastReplySeqNoForClient(jlong clientId) const {
        std::shared_lock<std::shared_mutex> lock(lastReplyForClient_mutex);
        auto reply = lastReplyForClient.find(clientId);
        if(reply != lastReplyForClient.end())
            return reply->second.seqNo;
        return -1;
    }
    
    jobject getLastReplyForClient(jlong clientId, JNIEnv * env) const;
    
    std::pair<const std::unordered_map<jlong, ClientReply>&, std::shared_lock<std::shared_mutex>> getRepliesMap() {
        return {
            lastReplyForClient,
            std::shared_lock<std::shared_mutex>(lastReplyForClient_mutex)
        };
    }
    
    void dropAllLastRepliesForClients(){
        std::unique_lock<std::shared_mutex> lock(lastReplyForClient_mutex);
        for(auto & p: lastReplyForClient)
            p.second.freeValue();
        lastReplyForClient.clear();
        lock.unlock();
    }
    
    void setServiceSnapshotToRestore(std::vector<std::string> paths) {
        serviceSnapshotToRestore = paths;
    }
    
    void armServiceSnapshotToRestore() {
        serviceSnapshotToRestore_armed = true;
    }
    
    void removeServiceSnapshotToRestore(){
            serviceSnapshotToRestore_armed = false;
            for(const std::string & path : serviceSnapshotToRestore)
                unlink(path.c_str());
            serviceSnapshotToRestore.clear();
    }
    
    std::vector<const char *> getServiceSnapshotToRestore() const {
        if(!serviceSnapshotToRestore_armed)
            return {};
        std::vector<const char *> result;
        for(const std::string & path : serviceSnapshotToRestore)
            result.emplace_back(path.c_str());
        return result;
    }
    
    #ifdef DEBUG_LASTREPLYFORCLIENT
        void dumpLastClientReply(){
            fprintf(debugLogFile, "Dumping client respose IDs:\n");
            std::shared_lock<std::shared_mutex> lock(lastReplyForClient_live_mutex);
            for(auto pair : lastReplyForClient_live)
                fprintf(debugLogFile, "%ld:%d\n", pair.first, pair.second.get_ro().seqNo);
            lock.unlock();
            fprintf(debugLogFile, "Client respose IDs dumped\n");
        }
    #endif
    
    void dump(FILE* out) const;
};

namespace jniGlue {
    // TODO: classes seem to be not cachable?
    // extern jclass lsr_common_Reply;
    jclass lsr_common_Reply();
    extern jmethodID lsr_common_Reply__constructor;
    void prepareReplicaStorageGlue(JNIEnv * env);
}

#endif // REPLICASTORAGE_H
