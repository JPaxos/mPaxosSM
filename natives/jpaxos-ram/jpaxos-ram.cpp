#include <unistd.h>

#include "jpaxos-common.hpp"
#include "replicastorage.h"
#include "paxosstorage.h"
#include "consensuslog.h"

#include "headers/lsr_paxos_NATIVE_PersistentMemory.h"

#ifndef NDEBUG
#include <signal.h>
void (*original_sigabrt)(int);
void on_SIGABRT(int){
    // java handles abrt by aborting self, and handles sevg by dumping usefull info and calling abort
    // and assert calls abrt
    // so we catch first abrt and change it to segv, then resore abrt handler.
    // This does not work well, but at least good enough to see what went wrong.
    signal(SIGABRT, original_sigabrt);
    raise(SIGSEGV);
}

#endif

PaxosStorage *paxosStorage;
ReplicaStorage *replicaStorage;
ConsensusLog *consensusLog;
unsigned char numReplicas_;
unsigned char majority_;
unsigned char localId_;
const unsigned char & numReplicas(){return numReplicas_;}
const unsigned char & majority(){return majority_;}
const unsigned char & localId(){return localId_;}

#ifdef DEBUGFILE
    FILE * debugLogFile;
#endif

__attribute__((constructor))
void init() {
}

#ifdef __cplusplus
extern "C" {
#endif
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *, void *){
    return JNI_VERSION_1_8;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *, void *){
}

JNIEXPORT void JNICALL Java_lsr_paxos_NATIVE_PersistentMemory_init (JNIEnv * jnienv, jclass, jstring, jlong, jint numReplicas, jint localId){
    #ifndef NDEBUG
    original_sigabrt = signal(SIGABRT, on_SIGABRT);
    #endif
    if(numReplicas > 32)
        jnienv->ThrowNew(jnienv->FindClass("java/lang/IllegalArgumentException"), "JPaxos PM natives use 32-bit integer for acceptor bitset - you have too many replicas!");
    
    paxosStorage = new PaxosStorage();
    replicaStorage = new ReplicaStorage();
    consensusLog = new ConsensusLog();
    
    ::numReplicas_ = numReplicas;
    ::majority_ = (numReplicas+1)/2;
    ::localId_ = localId;
    
    jniGlue::prepareReplicaStorageGlue(jnienv);
    
    #ifdef DEBUGFILE
    debugLogFile = fopen("ramNativesDebug.log", "a");
    if(!debugLogFile)
        jnienv->ThrowNew(jnienv->FindClass("java/lang/RuntimeException"), ("Cannot create debug log file "s + strerror(errno)).c_str());
    setbuf(debugLogFile, nullptr);
    #endif
}

JNIEXPORT void JNICALL Java_lsr_paxos_NATIVE_PersistentMemory_startThreadLocalTx (JNIEnv *, jclass){
}

JNIEXPORT void JNICALL Java_lsr_paxos_NATIVE_PersistentMemory_commitThreadLocalTx (JNIEnv *, jclass){
}

#ifdef __cplusplus
}
#endif
