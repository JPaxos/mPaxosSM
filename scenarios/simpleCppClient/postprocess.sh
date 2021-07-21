#!/bin/bash

echo -e "checking decisions.log.X"
let i=0;
head -qn-1 /tmp/jpaxos_?/jpaxosLogs/?/decisions.log.? |\
        sort -n | uniq |\
        while read seqNo hash
        do
            if ((seqNo != i))
            then
                echo -e "\n\033[01;05;41mERROR AT SEQNO $seqNo\033[00m\n"
                break
            fi
        let i++
        done
echo -e "decisions.log.X consistent"
