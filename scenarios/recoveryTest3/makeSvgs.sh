#!/bin/bash

if [[ -z "$TITLE" || -z "$RECOVERY_TIME" || -z "$CRASH_TIME" || -z "$XMAX" || -z "$PREFIX" || -z "$TARGET" || -z "$R0" || -z "$R1" || -z "$R2" ]]
then
    echo 'Some env var missing';
    exit 1;
fi

cat << EOF | gnuplot -

set terminal svg size 900,600;
set xrange [0:$XMAX]
set tmargin 1

set style line 1 lc 'gray' dt solid lt 1 lw 1
set style line 2 lc 'gray' dt '.'   lt 1 lw 1
set grid xtics ytics mxtics mytics ls 1, ls 2
set xtics 10
set mxtics 5

set arrow 1 from    $CRASH_TIME, graph 0 to    $CRASH_TIME, graph 1 nohead dashtype "--";
set arrow 2 from $RECOVERY_TIME, graph 0 to $RECOVERY_TIME, graph 1 nohead dashtype "--";


set yrange [0:1e5]
set ytics 1e4
set mytics 5

set output "${TARGET}rps0.svg";
set title "${TITLE}, ${R0}, smoothed 0.05s, RPS" offset 0,-1 noenhanced
plot '${PREFIX}rps0_0.05' with filledcurves x1 fs solid 0.1 notitle , '' with lines lt 1 lw 2 notitle;

set output "${TARGET}rps1.svg";
set title "${TITLE}, ${R1}, smoothed 0.05s, RPS" offset 0,-1 noenhanced
plot '${PREFIX}rps1_0.05' with filledcurves x1 fs solid 0.1 notitle , '' with lines lt 1 lw 2 notitle;

set output "${TARGET}rps2.svg";
set title "${TITLE}, ${R2}, smoothed 0.05s, RPS" offset 0,-1 noenhanced
plot '${PREFIX}rps2_0.05' with filledcurves x1 fs solid 0.1 notitle , '' with lines lt 1 lw 2 notitle;


set yrange [0:3e3]
set ytics 5e2
set mytics 5

set output "${TARGET}dps0.svg";
set title "${TITLE}, ${R0}, smoothed 0.05s, DPS" offset 0,-1 noenhanced
plot '${PREFIX}dps0_0.05' with filledcurves x1 fs solid 0.1 notitle , '' with lines lt 1 lw 2 notitle;

set output "${TARGET}dps1.svg";
set title "${TITLE}, ${R1}, smoothed 0.05s, DPS" offset 0,-1 noenhanced
plot '${PREFIX}dps1_0.05' with filledcurves x1 fs solid 0.1 notitle , '' with lines lt 1 lw 2 notitle;

set output "${TARGET}dps2.svg";
set title "${TITLE}, ${R2}, smoothed 0.05s, DPS" offset 0,-1 noenhanced
plot '${PREFIX}dps2_0.05' with filledcurves x1 fs solid 0.1 notitle , '' with lines lt 1 lw 2 notitle;


set yrange [0:100]
set ytics 10
set mytics 5

set output "${TARGET}maxCpu0.svg";
set title "${TITLE}, ${R0}, smoothed 0.05s, max CPU [%]" offset 0,-1 noenhanced
plot '${PREFIX}maxCpu0_0.05' with filledcurves x1 fs solid 0.1 notitle , '' with lines lt 1 lw 2 notitle;

set output "${TARGET}maxCpu1.svg";
set title "${TITLE}, ${R1}, smoothed 0.05s, max CPU [%]" offset 0,-1 noenhanced
plot '${PREFIX}maxCpu1_0.05' with filledcurves x1 fs solid 0.1 notitle , '' with lines lt 1 lw 2 notitle;

set output "${TARGET}maxCpu2.svg";
set title "${TITLE}, ${R2}, smoothed 0.05s, max CPU [%]" offset 0,-1 noenhanced
plot '${PREFIX}maxCpu2_0.05' with filledcurves x1 fs solid 0.1 notitle , '' with lines lt 1 lw 2 notitle;


set yrange [0:1e10]
set ytics 1e9
set mytics 5

set output "${TARGET}ifUp0.svg";
set title "${TITLE}, ${R0}, smoothed 0.05s, ifUp [b/s]" offset 0,-1 noenhanced
plot '${PREFIX}ifUp0_0.05' with filledcurves x1 fs solid 0.1 notitle , '' with lines lt 1 lw 2 notitle;

set output "${TARGET}ifUp1.svg";
set title "${TITLE}, ${R1}, smoothed 0.05s, ifUp [b/s]" offset 0,-1 noenhanced
plot '${PREFIX}ifUp1_0.05' with filledcurves x1 fs solid 0.1 notitle , '' with lines lt 1 lw 2 notitle;

set output "${TARGET}ifUp2.svg";
set title "${TITLE}, ${R2}, smoothed 0.05s, ifUp [b/s]" offset 0,-1 noenhanced
plot '${PREFIX}ifUp2_0.05' with filledcurves x1 fs solid 0.1 notitle , '' with lines lt 1 lw 2 notitle;


EOF
