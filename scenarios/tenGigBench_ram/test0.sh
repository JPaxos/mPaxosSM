#!/bin/bash
#SBATCH -w pmem-[2-6],hpc-9 -t 8:0:0 --wait-all-nodes=1

if [ "$SLURM_JOB_ID" ]
then
	SCRIPT="$(scontrol show job $SLURM_JOBID | grep 'Command=' | cut -d'=' -f2- )"
else
	SCRIPT="$(readlink -e $0)"
fi

SCRIPTDIR="${SCRIPT%/*}"

cd ~/jpaxos

OUT="${SCRIPT%.sh}-$(date +%Y%m%d_%H%M).out"

echo -ne "reqsize clicount " > $OUT
HEADER=1 "$SCRIPTDIR"/sqlg.sh | tr -s '|\n' ' ' | sed 's/ $/\n/' >> $OUT

clientMachines=2

# safe values are up to 100
for reqPerClient in 25
do
    # 10 entries: 50 100 200 300 400 600 800 1000 1300 1600 2000 2500
    # safe values for cliCount are up to 96
    for cliCount in 1 2 4 6 8 12 16 20 26 32 40 50
    do

        # 32 entries: 128 256 384 512 640 768 896 1024 1280 1536 1792 2048 2304 2560 2816 3072 3328 3584 3840 4096 4608 5120 5632 6144 6656 7168 7680 8192 8704 9216 9728 10240
        #for reqsize in `seq 128 128 1023` `seq 1024 256 4095` `seq 4096 512 10240`

        # 16 entries: 256 512 768 1024 1536 2048 2560 3072 3584 4096 5120 6144 7168 8192 9216 10240
        for reqsize in `seq 256 256 1023` `seq 1024 512 4095` `seq 4096 1024 10240`
        do
            for i in 1 2 3
            do
                echo "=== ${clientMachines}×${cliCount} client processes, each sends $reqPerClient parallel requests, request size is ${reqsize}B ==="

                # create scrnario from template and parameters
                sed "
                    s/_ReqPerClient_/$reqPerClient/;
                    s/_ReqSize_/$reqsize/;
                    s/_ClientCount_/$cliCount/;
                    " "$SCRIPTDIR"/scenario_template > "$SCRIPTDIR"/scenario

                # run the test
                timeout 1m ./runTest.sh "$SCRIPTDIR"

                # select results and put them to output table
                echo -ne "$reqsize $(($reqPerClient*$cliCount*$clientMachines)) " >> $OUT
                "$SCRIPTDIR"/sqlg.sh | sqlite3 jpdb.sqlite3 | tr -s '|\n' ' ' | sed 's/ $/\n/' >> $OUT

                # make sure that all dies
                for dest in pmem-{2..6}
                do
                    ssh $dest -- "pkill -KILL java; pkill -KILL hashMapClient" &
                done
                pendingJobs=$(jobs | wc -l)
                for (( ; pendingJobs>=0 ; --pendingJobs )); do wait; done
            done
        done
    done
done
