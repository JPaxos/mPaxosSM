set output "plot3.pdf"
set term pdfcairo size 8.8cm,7cm font ",10"

set xrange [15:118]
# set yrange [0:]

TOP   = 0.99
HEI   = 0.20
SPACE = 0.03

set rmargin 0.5

set key samplen -1

set xtics 10
#set mxtics 5

#set grid xtics ytics mxtics mytics
set grid xtics ytics

set linetype 1 linewidth 2 linecolor black

set arrow 1 from 28.46, graph 0 to 28.46, graph 1 nohead dashtype "--"
set arrow 2 from 49.99, graph 0 to 49.99, graph 1 nohead dashtype "--"

set multiplot layout 4,1

set yrange [0:7]
set ytics 3
set mytics 2
#set ytics format ""
#do for [i=0:5:2] {
#    set ytics add (sprintf("%d",i) i)
#}

set key bottom
set lmargin 5
set xtics format ""
set tmargin at screen TOP
set bmargin at screen TOP-HEI
plot 'results/fc_ifUp0_0.05' using 1:($2/1e9) title 'Leader #0' with lines


set key top
set yrange [0:1.5]
set ytics 1
# set ytics add ("½" 0.5)
set mytics 4
#set ytics format ""
#do for [i=0:5:2] {
#    set ytics add (sprintf("%d",i) i)
#}


set xtics format ""
set tmargin at screen TOP-1*(HEI+SPACE)
set bmargin at screen TOP-1*(HEI+SPACE)-HEI
plot 'results/fc_ifUp1_0.05' using 1:($2/1e9) title 'Follower #1 (queried for state)' with lines

set xtics format ""
set tmargin at screen TOP-2*(HEI+SPACE)
set bmargin at screen TOP-2*(HEI+SPACE)-HEI
plot 'results/fc_ifUp2_0.05' using 1:($2/1e9) title 'Follower #2 and #3' with lines


set xlabel 'Time [s]' offset 0, 0.75
set xtics format "%h"
set xtics offset 0, 0.25
set tmargin at screen TOP-3*(HEI+SPACE)
set bmargin at screen TOP-3*(HEI+SPACE)-HEI
plot 'results/fc_ifUp4_0.05' using 1:($2/1e9) title 'Crashing follower #4' with lines


unset key
set lmargin 4
unset arrow 1
unset arrow 2
set tmargin at screen TOP
set bmargin at screen TOP-3*(HEI+SPACE)-HEI
set border 0
unset tics
set ylabel "Uplink usage [Gbit/s]"
unset xlabel 
plot [][0:1] -1

unset multiplot



