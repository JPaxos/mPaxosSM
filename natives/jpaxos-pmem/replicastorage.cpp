#include "replicastorage.h"

namespace jniGlue {
    //jclass lsr_common_Reply;
    jmethodID lsr_common_Reply__constructor;
    jclass lsr_common_Reply(JNIEnv * env) {
        return env->FindClass("lsr/common/Reply");
    }
    
    void prepareReplicaStorageGlue(JNIEnv * env){
        //lsr_common_Reply = env->FindClass("lsr/common/Reply");
        lsr_common_Reply__constructor = env->GetMethodID(lsr_common_Reply(env), "<init>", "(JI[B)V");
    }
    
    jobject reply_to_reply(JNIEnv * env, const ClientReply& replyC, jclass lsr_common_Reply){
        jbyteArray valueJ = env->NewByteArray(replyC.valueLength);
        if(replyC.valueLength)
            env->SetByteArrayRegion(valueJ, 0, replyC.valueLength, replyC.value.get());
        return env->NewObject(lsr_common_Reply, lsr_common_Reply__constructor, replyC.clientId, replyC.seqNo, valueJ);
    }
}

jobject ReplicaStorage::getLastReplyForClient(jlong clientId, JNIEnv * env) const {
    std::shared_lock<std::shared_mutex> lock(lastReplyForClient_mutex);
    auto reply = lastReplyForClient.get_if_exists(clientId);
    return reply ? jniGlue::reply_to_reply(env, *reply, jniGlue::lsr_common_Reply(env)) : nullptr;
}

void ReplicaStorage::dump(FILE* out) const {
    fprintf(out, "ExecuteUB: %d\nDecided waiting for execution:", executeUB.get_ro());
    
    std::vector<int> dwe;
    dwe.reserve(decidedWaitingExecution.count());
    for(auto i : decidedWaitingExecution)
        dwe.push_back(i);

    std::sort(dwe.begin(), dwe.end());
    for(auto i : dwe)
        fprintf(out, " %d", i);
    fprintf(out, "\n");
    
    fprintf(out, "Map of last replies:\n");
    std::shared_lock<std::shared_mutex> lock(lastReplyForClient_mutex);
    for(auto p : lastReplyForClient)
        fprintf(out, "%ld:%d ", p.first, p.second.seqNo);
    lock.unlock();
    fprintf(out, "\n");
    
    fprintf(out, "There are %zu service snapshot files to restore (%s)\n",
                 serviceSnapshotToRestore.count(),
                 serviceSnapshotToRestore_armed ? "ARMED" : "unarmed"
           );
    for(const auto &[ptr,size]: serviceSnapshotToRestore){
        fprintf(out, "  * ");
        fwrite(ptr.get(), 1, size, out);
        fprintf(out, "\n");
    }
}

#ifdef __cplusplus
extern "C" {
#endif
    
JNIEXPORT jint JNICALL Java_lsr_paxos_replica_storage_PersistentReplicaStorage_getExecuteUB_1 (JNIEnv *, jclass){
    return replicaStorage->getExcuteUB();
}

JNIEXPORT void JNICALL Java_lsr_paxos_replica_storage_PersistentReplicaStorage_setExecuteUB_1 (JNIEnv *, jclass, jint executeUB){
    return replicaStorage->setExcuteUB(executeUB);
}

JNIEXPORT void JNICALL Java_lsr_paxos_replica_storage_PersistentReplicaStorage_incrementExecuteUB_1 (JNIEnv *, jclass){
    return replicaStorage->incrementExcuteUB();
}

JNIEXPORT jlong JNICALL Java_lsr_paxos_replica_storage_PersistentReplicaStorage_getServiceSeqNo (JNIEnv *, jobject){
    return replicaStorage->getServiceSeqNo();
}

JNIEXPORT void JNICALL Java_lsr_paxos_replica_storage_PersistentReplicaStorage_setServiceSeqNo (JNIEnv *, jobject, jlong ssn){
    replicaStorage->setServiceSeqNo(ssn);
}

JNIEXPORT jlong JNICALL Java_lsr_paxos_replica_storage_PersistentReplicaStorage_incServiceSeqNo (JNIEnv *, jobject) {
    return replicaStorage->incServiceSeqNo();
}

JNIEXPORT void JNICALL Java_lsr_paxos_replica_storage_PersistentReplicaStorage_addDecidedWaitingExecution (JNIEnv *, jclass, jint instanceId){
    replicaStorage->addDecidedWaitingExecution(instanceId);
}

JNIEXPORT jboolean JNICALL Java_lsr_paxos_replica_storage_PersistentReplicaStorage_isDecidedWaitingExecution (JNIEnv *, jclass, jint instanceId){
    return replicaStorage->isDecidedWaitingExecution(instanceId);
}
    
JNIEXPORT void JNICALL Java_lsr_paxos_replica_storage_PersistentReplicaStorage_releaseDecidedWaitingExecution (JNIEnv *, jobject, jint instanceId){
    replicaStorage->releaseDecidedWaitingExecution(instanceId);
}

JNIEXPORT void JNICALL Java_lsr_paxos_replica_storage_PersistentReplicaStorage_releaseDecidedWaitingExecutionUpTo (JNIEnv *, jobject, jint instanceId){
    replicaStorage->releaseDecidedWaitingExecutionUpTo(instanceId);
}

JNIEXPORT jint JNICALL Java_lsr_paxos_replica_storage_PersistentReplicaStorage_decidedWaitingExecutionCount (JNIEnv *, jobject){
    return replicaStorage->decidedWaitingExecutionCount();
}

JNIEXPORT void JNICALL Java_lsr_paxos_replica_storage_PersistentReplicaStorage_setLastReplyForClient (JNIEnv * env, jclass, jlong clientId, jint clientSeqNo, jbyteArray valueJ){
    pmem::obj::transaction::automatic tx(*pop);
    size_t valueLength = (size_t) env->GetArrayLength(valueJ);
    pmx::self_relative_ptr<signed char[]> valueC;
    if(valueLength) {
        valueC = blockReuser->pop(valueLength);
        if (valueC == nullptr){
            valueC = make_persistent<signed char[]>(valueLength);
            env->GetByteArrayRegion(valueJ, 0, valueLength, valueC.get());
        } else {
            env->GetByteArrayRegion(valueJ, 0, valueLength, valueC.get());
            pmem_persist(valueC.get(), valueLength);
        }
    } else {
        valueC = nullptr;
    }
    replicaStorage->setLastReplyForClient(clientId, clientSeqNo, valueC, valueLength);
}

JNIEXPORT jint JNICALL Java_lsr_paxos_replica_storage_PersistentReplicaStorage_getLastReplySeqNoForClient_1 (JNIEnv *, jclass, jlong clientId){
    return replicaStorage->getLastReplySeqNoForClient(clientId);
}

JNIEXPORT jobject JNICALL Java_lsr_paxos_replica_storage_PersistentReplicaStorage_getLastReplyForClient_1 (JNIEnv * env, jclass, jlong clientId){
    return replicaStorage->getLastReplyForClient(clientId, env);
}

JNIEXPORT jobjectArray JNICALL Java_lsr_paxos_replica_storage_PersistentReplicaStorage_getAllReplies (JNIEnv * env, jclass){
    auto [repliesC, lock] = replicaStorage->getRepliesMap();
    jclass lsr_common_Reply = jniGlue::lsr_common_Reply(env);
    auto count = repliesC.count();
    jobjectArray repliesJ = env->NewObjectArray(count, lsr_common_Reply, nullptr);
    jsize num = 0;
    for(const auto& pair : repliesC){
        const auto& reply = pair.second;
        env->SetObjectArrayElement(repliesJ, num++, jniGlue::reply_to_reply(env, reply, lsr_common_Reply));
    }
    return repliesJ;
}

JNIEXPORT void JNICALL Java_lsr_paxos_replica_storage_PersistentReplicaStorage_dropLastReplyForClient (JNIEnv *, jclass){
    replicaStorage->dropAllLastRepliesForClients();
}

JNIEXPORT void JNICALL Java_lsr_paxos_replica_storage_PersistentReplicaStorage_setServiceSnapshotToRestore (JNIEnv * env, jclass, jobjectArray jPaths){
    int count = env->GetArrayLength(jPaths);
    std::vector<std::string> cPaths;
    cPaths.reserve(count);
    for(int i = 0; i < count; ++i){
        jstring jPath = static_cast<jstring>(env->GetObjectArrayElement(jPaths, i));
        auto length = env->GetStringUTFLength(jPath);
        auto chars = env->GetStringUTFChars(jPath, nullptr);
        cPaths.emplace_back(chars, length);
        env->ReleaseStringUTFChars(jPath, chars);
    }
    replicaStorage->setServiceSnapshotToRestore(cPaths);
}

JNIEXPORT void JNICALL Java_lsr_paxos_replica_storage_PersistentReplicaStorage_armServiceSnapshotToRestore (JNIEnv *, jobject){
    replicaStorage->armServiceSnapshotToRestore();
}

JNIEXPORT void JNICALL Java_lsr_paxos_replica_storage_PersistentReplicaStorage_removeServiceSnapshotToRestore (JNIEnv *, jobject){
    replicaStorage->removeServiceSnapshotToRestore();
}

JNIEXPORT jobjectArray JNICALL Java_lsr_paxos_replica_storage_PersistentReplicaStorage_getServiceSnapshotToRestore_1 (JNIEnv * env, jclass){
    auto cPaths = replicaStorage->getServiceSnapshotToRestore();
    if(cPaths.empty())
        return nullptr;
    auto jPaths = env->NewObjectArray(cPaths.size(), env->FindClass("java/lang/String"), nullptr);
    int idx=0;
    for(const auto path : cPaths)
        env->SetObjectArrayElement(jPaths, idx++, env->NewStringUTF(path));
    return jPaths;
}


#ifdef __cplusplus
}
#endif
