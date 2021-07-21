set output "plot1b.pdf"
set term pdfcairo size 8.8cm,3cm font ",10"


set xrange [15:65]
set yrange [35:50]

MARGIN=0.10
WIDTH =0.45
SPACE =0.02

set tmargin 0.1

set key samplen -1

set ytics 4
#set mytics 5

set xtics 10
set mxtics 1

#set grid xtics ytics mxtics mytics
set grid xtics ytics

set linetype 1 linewidth 2 linecolor black

set arrow 1 from 28.46, graph 0 to 28.46, graph 1 nohead dashtype "--"
set arrow 2 from 49.99, graph 0 to 49.99, graph 1 nohead dashtype "--"

set multiplot layout 1,2

set xtics offset 0, 0.25
set ylabel "Requests/s [kRPS]" offset 1
set xlabel ' ' offset 0, 0.25
set lmargin at screen MARGIN
set rmargin at screen MARGIN+WIDTH
plot 'results/lc_rps1_0.05' using 1:($2/1e3) title '3 replicas' with lines

set key bottom samplen 0
unset ylabel
set lmargin at screen MARGIN+WIDTH+SPACE
set rmargin at screen MARGIN+WIDTH+SPACE+WIDTH
set ytics format ""
plot '../recoveryTest5/results/lc_rps3_0.05' using 1:($2/1e3) title '5 replicas' with lines


unset key
unset arrow 1
unset arrow 2
set lmargin at screen MARGIN
set rmargin at screen MARGIN+WIDTH+SPACE+WIDTH
set border 0
unset tics
set xlabel 'Time [s]' offset 0, 0.25
plot [][0:1] -1

unset multiplot



