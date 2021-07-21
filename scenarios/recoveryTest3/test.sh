#!/bin/bash
mkdir /tmp/res || exit 1
let x=1
while [[ "`squeue -u $UID --noheader -w pmem-[2-6]`" ]]
do
   xw=`printf "%03d" $x`
	echo "################################### $xw ###################################";
	./runTest.sh ../jpaxos_pmem/scenarios/recoveryTest
	mkdir /tmp/res/$xw
	mv /tmp/jpaxos_* /tmp/res/$xw
	mv jpdb.sqlite3 /tmp/res/$xw
	let x++
done
