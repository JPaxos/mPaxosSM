#!/bin/bash

#
# This script runs Replica thread of mPaxosSM.
# 
# It was used to profile the thread. Keep in mind that native binaries must 
# use frame pointer (i.e. compiled with -fno-omit-frame-pointer in gcc/clang)
#

TARGET=/tmp/jpaxos_test
PMEM=/mnt/pmem1
NUMACTL="numactl -N1"

rm -rf "$TARGET" "$PMEM"/jpaxos*

mkdir -p "$TARGET"

echo "path=$PMEM/jpaxos_test"   > "$TARGET"/service.conf
echo "size=$((512*1024*1024))"  >> "$TARGET"/service.conf

echo "process.0 = localhost:2001:3001"   > "$TARGET"/paxos.properties
echo "CrashModel = Pmem"                >> "$TARGET"/paxos.properties
echo "NvmBaseDir = $PMEM"               >> "$TARGET"/paxos.properties
echo "NvmPoolSize = $((512*1024*1024))" >> "$TARGET"/paxos.properties

cp ../jpaxos/scenarios/COMMON/logback-benchmark.xml            "$TARGET"/logback.xml
cp natives/jpaxos-pmem/build-release-fp-ggdb/libjpaxos-pmem.so "$TARGET"
cp natives/service/build-release-fp-ggdb/libjpaxos-service.so  "$TARGET"
cp jpaxos.jar                                                  "$TARGET"
cp -r lib                                                      "$TARGET"

(
    cd "$TARGET"
    $NUMACTL \
    java \
        -cp lib/slf4j-api-1.7.26.jar:lib/logback-core-1.2.3.jar:lib/logback-classic-1.2.3.jar:jpaxos.jar \
        -Djava.library.path=. \
        -Dlogback.configurationFile=logback.xml \
        -XX:+UnlockExperimentalVMOptions -XX:+UseZGC \
        -Xms8G -Xmx8G \
        lsr.paxos.test.RequestExeuteTester "$@"
)

rm -rf "$PMEM"/jpaxos*
