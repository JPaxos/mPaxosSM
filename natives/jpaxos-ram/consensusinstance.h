#ifndef CONSENSUSINSTANCE_H
#define CONSENSUSINSTANCE_H

#include <cstdint>
#ifndef __GNUC__
    #include <bitset>
#endif
#include <boost/endian/conversion.hpp>

#include "jpaxos-common.hpp"

#include <atomic>

#include "headers/lsr_paxos_storage_PersistentConsensusInstance.h"

enum LogEntryState : uint8_t {
    UNKNOWN = lsr_paxos_storage_PersistentConsensusInstance_ENUM_LOGENTRYSTATE_UNKNOWN,
    KNOWN = lsr_paxos_storage_PersistentConsensusInstance_ENUM_LOGENTRYSTATE_KNOWN,
    RESET = lsr_paxos_storage_PersistentConsensusInstance_ENUM_LOGENTRYSTATE_RESET,
    DECIDED = lsr_paxos_storage_PersistentConsensusInstance_ENUM_LOGENTRYSTATE_DECIDED
};

class ConsensusInstance{
    /*const*/ jint id;
    jint lastSeenView    {-1};
    jint lastVotedView   {-1};
    LogEntryState state  {UNKNOWN};
    uint32_t accepts     {0};
    size_t valueLength   {0};
    jbyte * value        {nullptr};
    
    bool isReadyToBeDecieded(){return state==KNOWN && isMajority();}
    
    void deleteValueUnchecked() {
        #ifdef DEBUG_DISAPPEARING_CI_VALUE
        fprintf(debugLogFile, "Erasing value for inst %d (this: %p, value: %p, %luB) thread: %d\n", id, this, value, valueLength, gettid());
        #endif
        
        assert(valueLength!=0 && value!=nullptr);
        delete [] value;
        value = nullptr;
        valueLength = 0;
    }
    
public:
    ConsensusInstance():id(-1){}
    ConsensusInstance(jint id):id(id){}
    jint getLastSeenView() const {return lastSeenView;}
    jint getLastVotedView() const {return lastVotedView;}
    jbyteArray getValue(JNIEnv * env) const {
        #ifdef DEBUG_DISAPPEARING_CI_VALUE
        fprintf(debugLogFile, "Getting value for inst %d (this: %p, value: %p, %luB) thread: %d\n", id, this, value, valueLength, gettid());
        #endif
        if(!value)
            return nullptr;
        jbyteArray ba = env->NewByteArray(valueLength);
        env->SetByteArrayRegion(ba, 0, valueLength, value);
        return ba;
    }
    LogEntryState getState() const {return state;}
    bool isMajority() const {
        #ifdef __GNUC__
        return __builtin_popcountl(accepts) >= majority();
        #else
        return std::bitset<32>(accepts).count() >= majority();
        #endif
    }
    void setDecided() {
        #ifdef DEBUG_DISAPPEARING_CI_VALUE
        fprintf(debugLogFile, "Deciding instance %d (this: %p, value: %p, %luB) thread: %d\n", id, this, value, valueLength, gettid());
        #endif
        state = DECIDED;
    }
    
    bool updateStateFromPropose(JNIEnv* env, jint proposeSender, jint view, jbyteArray newValue);
    bool updateStateFromAccept(jint view, jint acceptSender);
    void updateStateFromDecision(JNIEnv * env, jint view, jbyteArray newValue);
    
    // Writing nad packing must be consistent with the java version!
    size_t byteSize() const { return 4 /*id*/ + 4 /*view*/ + 4 /*state*/ + 4 /*val len*/ + valueLength;}
    void writeTo(jbyte* target) const {
        *((uint32_t*)target) = boost::endian::native_to_big(static_cast<uint32_t>(id));
        target+=4;
        *((uint32_t*)target) = boost::endian::native_to_big(static_cast<uint32_t>(lastVotedView));
        target+=4;
        *((uint32_t*)target) = boost::endian::native_to_big(static_cast<uint32_t>(state!=RESET ? state : KNOWN));
        target+=4;
        *((uint32_t*)target) = boost::endian::native_to_big(static_cast<uint32_t>(state==UNKNOWN ? -1 : valueLength));
        target+=4;
        if(value){
            assert(state != UNKNOWN);
            memcpy(target, value, valueLength);
        }
    }
    
    void freeMemory(){
        if(value){
            delete [] value;
            #ifdef DEBUG_DISAPPEARING_CI_VALUE
            fprintf(debugLogFile, "Erasing value for inst %d (this: %p, value: %p, %luB) thread: %d\n", id, this, value, valueLength, gettid());
            #endif
        }
    }
};

#endif // CONSENSUSINSTANCE_H
