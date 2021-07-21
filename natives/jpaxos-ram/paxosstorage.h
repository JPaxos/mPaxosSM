#ifndef PAXOSSTORAGE_H
#define PAXOSSTORAGE_H


#include "jpaxos-common.hpp"
#include "headers/lsr_paxos_core_Proposer.h"


class PaxosStorage
{
    jint view = 0;
    jint firstUncommited = 0;
    
    jint runUniqueId = 0;
    
    jbyte proposerState = lsr_paxos_core_Proposer_ENUM_PROPOSERSTATE_INACTIVE;
    
public:
    PaxosStorage(){}
    
    jint getFirstUncommited(){return firstUncommited;}
    jint updateFirstUncommited(jint snapshotNextId = -1);

    jint getView(){return view;}
    void setView(jint newView){view = newView;}
    
    jint getRunUniqueId() {return runUniqueId;}
    void incRunUniqueId() {runUniqueId++;}
    
    jbyte getProposerState() {return proposerState;}
    void setProposerState(jbyte state) {proposerState = state;}
};

#endif // PAXOSSTORAGE_H
