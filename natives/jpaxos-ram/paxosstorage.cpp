#include "paxosstorage.h"
#include "consensusinstance.h"
#include "consensuslog.h"


jint PaxosStorage::updateFirstUncommited(jint snapshotNextId){
    firstUncommited = std::max(snapshotNextId, firstUncommited);
    while(true){
        const ConsensusInstance * ci = consensusLog->getInstanceIfExists(firstUncommited);
        if(!ci || ci->getState() != DECIDED )
            break;
        firstUncommited++;
    }   
    return firstUncommited;
}

#ifdef __cplusplus
extern "C" {
#endif
JNIEXPORT jint JNICALL Java_lsr_paxos_storage_PersistentStorage_getFirstUncommitted_1 (JNIEnv *, jclass){
    return paxosStorage->getFirstUncommited();
}

JNIEXPORT jint JNICALL Java_lsr_paxos_storage_PersistentStorage_updateFirstUncommitted_1__ (JNIEnv *, jclass){
    return paxosStorage->updateFirstUncommited();
}

JNIEXPORT jint JNICALL Java_lsr_paxos_storage_PersistentStorage_updateFirstUncommitted_1__I (JNIEnv *, jclass, jint snapshotNextId){
    return paxosStorage->updateFirstUncommited(snapshotNextId);
}

JNIEXPORT jint JNICALL Java_lsr_paxos_storage_PersistentStorage_getView_1 (JNIEnv *, jclass){
    return paxosStorage->getView();
}

JNIEXPORT void JNICALL Java_lsr_paxos_storage_PersistentStorage_setView_1 (JNIEnv *, jclass, jint newView){
    paxosStorage->setView(newView);
}

JNIEXPORT jlong JNICALL Java_lsr_paxos_storage_PersistentStorage_getRunUniqueId (JNIEnv *, jobject){
    return paxosStorage->getRunUniqueId();
}

JNIEXPORT jbyte JNICALL Java_lsr_paxos_storage_PersistentStorage_getProposerState_1 (JNIEnv *, jobject){
    return paxosStorage->getProposerState();
}

JNIEXPORT void JNICALL Java_lsr_paxos_storage_PersistentStorage_setProposerState (JNIEnv *, jobject, jbyte state){
    paxosStorage->setProposerState(state);
}


#ifdef __cplusplus
}
#endif
