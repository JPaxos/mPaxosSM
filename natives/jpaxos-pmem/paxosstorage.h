#ifndef PAXOSSTORAGE_H
#define PAXOSSTORAGE_H

#include "jpaxos-common.hpp"
#include "headers/lsr_paxos_core_Proposer.h"


class PaxosStorage
{
    pm::p<jint> view = 0;
    pm::p<jint> firstUncommited = 0;
    
    pm::p<jint> runUniqueId = 0;
    
    pm::p<jbyte> proposerState = lsr_paxos_core_Proposer_ENUM_PROPOSERSTATE_INACTIVE;
    
public:
    PaxosStorage(){}
    
    jint getFirstUncommited(){return firstUncommited;}
    jint updateFirstUncommited(jint snapshotNextId = -1);

    jint getView(){return view;}
    void setView(jint newView){pm::transaction::run(*pop,[&]{view = newView;});}
    
    jint getRunUniqueId() {return runUniqueId;}
    void incRunUniqueId() {pm::transaction::run(*pop,[&]{runUniqueId++;});}
    
    jbyte getProposerState() {return proposerState;}
    void setProposerState(jbyte state) {pm::transaction::run(*pop,[&]{proposerState = state;});}
    
    void dump(FILE* out) const;
};

#endif // PAXOSSTORAGE_H
