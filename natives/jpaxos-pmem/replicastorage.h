#ifndef REPLICASTORAGE_H
#define REPLICASTORAGE_H

#include "jpaxos-common.hpp"

#include <libpmemobj++/persistent_ptr.hpp>
#include <list>

#include <libpmemobj.h>

using namespace pmem::obj;

struct ClientReply {
    jlong clientId {0};
    jint seqNo {-1};
    
    persistent_ptr<signed char[]> value {nullptr};
    size_t valueLength {0};
    
    void freeValue() {
        if(value) {
            delete_persistent<signed  char[]>(value, valueLength);
            value = nullptr;
        }
    }
};

class ReplicaStorage
{
    pmem::obj::p<jint> executeUB {0};
    pmem::obj::p<jlong> serviceSeqNo {0};
    hashset_persistent<jint> decidedWaitingExecution = hashset_persistent<jint>(*pop, 128);
    
    hashmap_persistent<jlong, ClientReply> lastReplyForClient = hashmap_persistent<jlong, ClientReply>(*pop, 2<<14 /* 16384 */);
    
    mutable std::shared_mutex lastReplyForClient_mutex;
    
    linkedqueue_persistent<std::pair<pm::persistent_ptr<char []>, int>> serviceSnapshotToRestore;
    pm::p<bool> serviceSnapshotToRestore_armed {false};
    
public:
    ReplicaStorage(){}
    
    jint getExcuteUB() const {return executeUB;}
    void setExcuteUB(jint _executeUB) {executeUB=_executeUB; pop->persist(executeUB);}
    void incrementExcuteUB(){++executeUB; pop->persist(executeUB);}
    
    jlong getServiceSeqNo() const {return serviceSeqNo;}
    void setServiceSeqNo(jlong _serviceSeqNo) {serviceSeqNo=_serviceSeqNo; pop->persist(serviceSeqNo);}
    jlong incServiceSeqNo(){++serviceSeqNo; pop->persist(serviceSeqNo); return serviceSeqNo;}
    
    void addDecidedWaitingExecution(jint instanceId) {decidedWaitingExecution.add(*pop, instanceId);}
    jboolean isDecidedWaitingExecution(jint instanceId) const {return decidedWaitingExecution.contains(instanceId) ? JNI_TRUE : JNI_FALSE;}
    void releaseDecidedWaitingExecution(jint instanceId) {decidedWaitingExecution.erase(*pop, instanceId);}
    void releaseDecidedWaitingExecutionUpTo(jint instanceId) {
        std::list<jint> instancesToRemove;
        for(auto iid: decidedWaitingExecution){
            if(iid < instanceId)
                instancesToRemove.push_back(iid);
        }
        pmem::obj::transaction::run(*pop,[&]{
            for(auto iid: instancesToRemove)
                decidedWaitingExecution.erase(*pop, iid);
        });
    }
    size_t decidedWaitingExecutionCount() const {return decidedWaitingExecution.count();}
    
    /// called from within a transaction
    void setLastReplyForClient(jlong clientId, jint clientSeqNo, persistent_ptr<signed char[]> value, size_t valueLength){
        std::unique_lock<std::shared_mutex> lock(lastReplyForClient_mutex);
        ClientReply & reply = lastReplyForClient.get(*pop, clientId).get_rw();
        reply.freeValue();
        
        reply.clientId    = clientId;
        reply.seqNo       = clientSeqNo;
        reply.value       = value;
        reply.valueLength = valueLength;

        lock.unlock();
        
        #ifdef DEBUG_LASTREPLYFORCLIENT
            fprintf(debugLogFile, "Adding reply: %ld:%d\n", clientId, clientSeqNo);
        #endif
    }
    
    jint getLastReplySeqNoForClient(jlong clientId) const {
        std::shared_lock<std::shared_mutex> lock(lastReplyForClient_mutex);
        auto reply = lastReplyForClient.get_if_exists(clientId);
        return reply ? reply->get_ro().seqNo : -1;
    }
    
    jobject getLastReplyForClient(jlong clientId, JNIEnv * env) const;
    
    std::pair<const hashmap_persistent<jlong, ClientReply>&, std::shared_lock<std::shared_mutex>> getRepliesMap() {
        return {
            lastReplyForClient,
            std::shared_lock<std::shared_mutex>(lastReplyForClient_mutex)
        };
    }
    
    void dropAllLastRepliesForClients(){
        pmem::obj::transaction::run(*pop,[&]{
            std::unique_lock<std::shared_mutex> lock(lastReplyForClient_mutex);
            for(auto p: lastReplyForClient)
                p.second.get_rw().freeValue();
            lastReplyForClient.clear(*pop);
            lock.unlock();
        });
    }
    
    void setServiceSnapshotToRestore(std::vector<std::string> paths) {
        pmem::obj::transaction::run(*pop,[&]{
            for(const auto & path : paths){
                auto len = path.length()+1;
                pm::persistent_ptr<char[]> ppath = pm::make_persistent<char[]>(len);
                memcpy(ppath.get(), path.c_str(), len);
                serviceSnapshotToRestore.push_back(*pop, {ppath, len});
            }
        });
    }
    
    void armServiceSnapshotToRestore() {
        serviceSnapshotToRestore_armed = true;
        pop->persist(serviceSnapshotToRestore_armed);
    }
    
    void removeServiceSnapshotToRestore(){
        pmem::obj::transaction::run(*pop,[&]{
            serviceSnapshotToRestore_armed = false;
            for(const auto & p : serviceSnapshotToRestore){
                const auto & [path, len] = p.get_ro();
                unlink(path.get());
                delete_persistent<char[]>(path, len);
            }
            serviceSnapshotToRestore.clear(*pop);
        });
    }
    
    std::vector<const char *> getServiceSnapshotToRestore() const {
        if(!serviceSnapshotToRestore_armed)
            return {};
        std::vector<const char *> result;
        for(const auto & p : serviceSnapshotToRestore)
            result.emplace_back(p.get_ro().first.get());
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
