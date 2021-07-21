#ifndef JPAXOS_COMMON_H
#define JPAXOS_COMMON_H

#undef DEBUG_LASTREPLYFORCLIENT
#undef DEBUG_TX

#if defined(DEBUG_LASTREPLYFORCLIENT) || defined(DEBUG_TX)
    #include <cstdio>
    #include <stdlib.h>
    #include <time.h>
    #include <sys/syscall.h>
    #define gettid() syscall(SYS_gettid)
    extern FILE * debugLogFile;
#endif

#include <unistd.h>
#include <cstdio>

#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/transaction.hpp>

#include <jni.h>

#include "hashmap_persistent_synchronized.hpp"
#include "hashset_persistent.hpp"
#include "linkedqueue_persistent.hpp"

using namespace std::literals;
namespace pm = pmem::obj;

struct root;
class PaxosStorage;
class ReplicaStorage;
class ConsensusLog;

extern pm::pool<root> * pop;

extern ConsensusLog * consensusLog;
extern PaxosStorage * paxosStorage;
extern ReplicaStorage * replicaStorage;

const unsigned char & numReplicas();
const unsigned char & majority();
const unsigned char & localId();

#include "paxosstorage.h"
#include "replicastorage.h" 
#include "consensuslog.h"

extern "C" {
    void dumpJpaxosPmem(char * pmemFile, FILE * outFile);
}

#endif // JPAXOS_COMMON_H
