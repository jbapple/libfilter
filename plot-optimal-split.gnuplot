set terminal pdf size 8.1,5.4
set output "plot-optimal-split.pdf"
set datafile separator ","

set grid
set yrange[*:140]

set logscale x
set xlabel "false positive percent"
set ylabel "wasted space percent"

plot for [i=2:14] 'plot-optimal-split.csv' using 1:i title columnheader(i) with linespoints lw 3

