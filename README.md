mPaxosSM /master/
=================

Description
-----------

mPaxosSM is a library and runtime system for building efficient replicated
state machines (highly-available services). It supports the crash-recovery
model of failure and tolerates message loss and communication delays.
It is based on JPaxos (https://github.com/JPaxos/JPaxos).

mPaxosSM assumes that the system is equipped with persistent memory (pmem),
also known as Non-Volatile Memory (NVM). However, mPaxosSM can also work with
the emulated pmem (https://nvdimm.wiki.kernel.org/). For more on pmem, see:
https://www.snia.org/technology-focus/persistent-memory .

In mPaxosSM all critical data are written to pmem to enable fast failure
recovery with little performance overhead during failure-free operation.
The replicated state machine no longer needs to create state snapshots
periodically, but in return it must persist its state in pmem. This brings
performance benefits whenever creating snapshots is costly.

While the core of mPaxosSM is implemented in Java, the code responsible for 
storing the vital data is written in C++, so that the PMDK library can be
leveraged for accessing pmem efficiently.

mPaxosSM has a sibling version, called mPaxos, which does not require that the
state machine keeps its state in pmem. So it has to periodically create 
snapshots. The source code of mPaxos is available at:
https://github.com/JPaxos/JPaxos

You are free to use mPaxosSM as the experimental platform for doing research 
on software-based replication and for any other purposes, provided that the
LGPL 3.0 licence is respected, moreover, any research papers which are
presenting the results obtained with the use of this system must reference
the paper given in the LICENCE file and below.

Research Papers
---------------

* Failure Recovery from Persistent Memory in Paxos-based State Machine
  Replication. Jan Kończak, Paweł T. Wojciechowski. 40th International
  Symposium on Reliable Distributed Systems (SRDS 2021)

Version
-------

The gitub repository may not contain the most recent version of our systems.
Please query the authors for more recent code, especially if the last commit
is far in the past.


License
-------

This software is distributed under the LGPL 3.0 licence. For license details
please read the LICENCE file.


Contact and authors
-------------------

mPaxosSM has been developed at Poznan University of Technology (PUT).
It is based on JPaxos, which was a joint work of PUT and Distributed System
Laboratory (LSR-EPFL)

PUT institutional page: http://www.cs.put.poznan.pl/persistentdatastore/

Contributors:

* Jan Kończak
* Paweł T. Wojciechowski
