set output "plot1.pdf"
set term pdfcairo size 8.8cm,4.4cm font ",10"


set xrange [15:98]
# set yrange [0:]
set yrange [35:50]

TOP   = 0.99
HEI   = 0.40
SPACE = 0.02

set rmargin 0.5

set key bottom samplen -1

set ytics 4
#set mytics 5

set xtics 10
#set mxtics 5

#set grid xtics ytics mxtics mytics
set grid xtics ytics

set linetype 1 linewidth 2 linecolor black

set arrow 1 from 28.46, graph 0 to 28.46, graph 1 nohead dashtype "--"
set arrow 2 from 49.99, graph 0 to 49.99, graph 1 nohead dashtype "--"

set multiplot layout 2,1


set lmargin 6
set xtics format ""
set tmargin at screen TOP
set bmargin at screen TOP-HEI
plot 'results/fc_rps0_0.05' using 1:($2/1e3) title 'Follower crash' with lines

set xlabel 'Time [s]' offset 0, 0.25
set xtics format "%h"
set xtics offset 0, 0.25
set tmargin at screen TOP-HEI-SPACE
set bmargin at screen TOP-HEI-SPACE-HEI
plot 'results/lc_rps1_0.05' using 1:($2/1e3) title 'Leader crash' with lines


unset key
set lmargin 4
unset arrow 1
unset arrow 2
set tmargin at screen TOP
set bmargin at screen TOP-HEI-SPACE-HEI
set border 0
unset tics
set ylabel "Requests per second [kRPS]"
unset xlabel 
plot [][0:1] -1

unset multiplot



