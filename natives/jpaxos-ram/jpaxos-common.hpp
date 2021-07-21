#ifndef JPAXOS_COMMON_H
#define JPAXOS_COMMON_H

#include <unistd.h>
#include <cstdio>
#include <algorithm>
#include <cassert>

#include <jni.h>


//#define DEBUG_DISAPPEARING_CI_VALUE
#undef DEBUG_DISAPPEARING_CI_VALUE

#ifdef DEBUG_DISAPPEARING_CI_VALUE
#define DEBUGFILE 1
#endif

class PaxosStorage;
class ReplicaStorage;
class ConsensusLog;

extern ConsensusLog * consensusLog;
extern PaxosStorage * paxosStorage;
extern ReplicaStorage * replicaStorage;

const unsigned char & numReplicas();
const unsigned char & majority();
const unsigned char & localId();

#ifdef DEBUGFILE
    #include <string>
    using namespace std::literals;
    extern FILE * debugLogFile;
#endif

// #include "paxosstorage.h"
// #include "replicastorage.h" 
// #include "consensusinstance.h"
// #include "consensuslog.h"

#endif // JPAXOS_COMMON_H
