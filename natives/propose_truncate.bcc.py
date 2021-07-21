#!/usr/bin/env python3

# This is a tracing script that tracks how long the log grows (i.e. when new
# consensus instances are appended, and when the old instances are truncated)

from bcc import BPF

b = BPF(text="""
    #include <uapi/linux/ptrace.h>

    /**************************************************/

    BPF_PERF_OUTPUT(proposes);
    
    struct onPropose_t {
        // void* thiz;
        // void* env;
        // int proposeSender;
        // int view;
        // void * newValue; // jbyteArray :-<

        int id;
    };

    int onPropose(struct pt_regs *ctx) {
        struct onPropose_t entry = {};

        void * thiz            = (void*) PT_REGS_PARM1(ctx);
        // entry.thiz          = (void*) PT_REGS_PARM1(ctx);
        entry.id               = *(int*) thiz;
        // entry.env           = (void*) PT_REGS_PARM2(ctx);
        // entry.proposeSender = PT_REGS_PARM3(ctx);
        // entry.view          = PT_REGS_PARM4(ctx);
        // entry.newValue      = (void*) PT_REGS_PARM5(ctx);
        
        proposes.perf_submit(ctx, &entry, sizeof(entry));

        return 0;
    }

    /**************************************************/

    BPF_PERF_OUTPUT(truncates);
    
    struct truncateBelow_t{
        int id;
    };
    
    int truncateBelow(struct pt_regs *ctx) {
        // args: JNIEnv *, jclass, jint id
        struct truncateBelow_t entry;
        entry.id = PT_REGS_PARM3(ctx);
        truncates.perf_submit(ctx, &entry, sizeof(entry));
        return 0;
    }
""")

#                                                ConsensusInstance::updateStateFromPropose(JNIEnv_*, int, int, _jbyteArray*)
b.attach_uprobe(name="./libjpaxos-pmem.so", sym="_ZN17ConsensusInstance22updateStateFromProposeEP7JNIEnv_iiP11_jbyteArray", fn_name="onPropose")

b.attach_uprobe(name="./libjpaxos-pmem.so", sym="Java_lsr_paxos_storage_PersistentLog_truncateBelow_1", fn_name="truncateBelow")

def print_propose(cpu, data, size):
    event = b["proposes"].event(data)
    #print("Propose V:", event.view, "I:", event.id);
    print("Propose  I:", event.id);
def print_truncate(cpu, data, size):
    event = b["truncates"].event(data)
    print("Truncate I:", event.id);
    

b["proposes"].open_perf_buffer(print_propose)
b["truncates"].open_perf_buffer(print_truncate)

while 1:
    try:
        b.perf_buffer_poll()
    except KeyboardInterrupt:
        exit()
        
