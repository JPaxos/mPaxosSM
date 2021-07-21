#!/bin/zsh

# This is a script that traces operations on decided and waitning execution queue

until pgrep -f 'GenericJniService 1'; do :; done
bpftrace -e '
	uprobe:/tmp/jpaxos_1/libjpaxos-pmem.so:Java_lsr_paxos_replica_storage_PersistentReplicaStorage_addDecidedWaitingExecution     {printf("%12d[%d] add(%d)\n", elapsed, tid, arg2);}
	uprobe:/tmp/jpaxos_1/libjpaxos-pmem.so:Java_lsr_paxos_replica_storage_PersistentReplicaStorage_isDecidedWaitingExecution      {printf("%12d[%d]  is(%d)\n", elapsed, tid, arg2);}
     uretprobe:/tmp/jpaxos_1/libjpaxos-pmem.so:Java_lsr_paxos_replica_storage_PersistentReplicaStorage_isDecidedWaitingExecution      {printf("%12d[%d]  is->%s\n", elapsed, tid, retval ? "t" : "f");}
	uprobe:/tmp/jpaxos_1/libjpaxos-pmem.so:Java_lsr_paxos_replica_storage_PersistentReplicaStorage_releaseDecidedWaitingExecution {printf("%12d[%d] rel(%d)\n", elapsed, tid, arg2);}
' > dwe
