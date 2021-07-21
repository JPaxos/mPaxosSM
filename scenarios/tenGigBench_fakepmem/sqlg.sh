#!/bin/bash

# ~/TEMP/sqlg.sh | sqlite3 jpdb.sqlite3 | tr -s '|\n' ' ' | sed 's/ $/\n/'
# HEADER=1 ~/TEMP/sqlg.sh | tr -s '|\n' ' ' | sed 's/ $/\n/'

if [ $HEADER ]
then
for id in {0..2}
do
echo \
"${id}_dps|${id}_devDps
${id}_rps|${id}_devRps
${id}_maxCpu|${id}_devMaxCpu
${id}_avgCpu|${id}_devAvgCpu
${id}_ifDown|${id}_ifDevDown
${id}_ifUp|${id}_ifDevUp"
done
for id in a b
do
echo \
"${id}_maxCpu|${id}_devMaxCpu
${id}_avgCpu|${id}_devAvgCpu
${id}_ifDown|${id}_ifDevDown
${id}_ifUp|${id}_ifDevUp"
done

exit
fi


echo ".output /dev/null
SELECT load_extension('./tools/libsqlitefunctions.so');
.output stdout
"

for id in {0..2}
do
wi="time+0 >= 12 and time+0 <= 22 and id=$id"
echo \
"select ifnull(avg(dps),0.0), stdev(dps) from dps where $wi;
select ifnull(avg(rps),0.0), stdev(rps) from rps where $wi;
select avg(max), stdev(max) from cpu where $wi;
select avg(avg), stdev(avg) from cpu where $wi;
select avg(down*8/1000/1000), stdev(down*8/1000/1000) from net where $wi;
select avg(  up*8/1000/1000), stdev(  up*8/1000/1000) from net where $wi;"
done

for id in a b
do
wi="time+0 >= 12 and time+0 <= 22 and id=\"$id\""
echo \
"select avg(max), stdev(max) from cliCpu where $wi;
select avg(avg), stdev(avg) from cliCpu where $wi;
select avg(down*8/1000/1000), stdev(down*8/1000/1000) from cliNet where $wi;
select avg(  up*8/1000/1000), stdev(  up*8/1000/1000) from cliNet where $wi;"
done
