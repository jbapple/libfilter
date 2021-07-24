# incremental-0.001-stashes-8-007.txt
# incremental-0.001-stashes-8-if-80p-002.txt
# incremental-0.001-8-slices-stashes-8-if-90p-001.txt


set terminal postscript eps enhanced color size 12cm,6.4cm; # fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
set output 'bits-per-item.eps';
unset grid;
unset logscale y;
set logscale x;
set xlabel "keys inserted";
set ylabel "bits per key"
set datafile separator ",";
unset format y
set yrange [0:64]
set title "Bits per key"
plot '< grep fpp incremental-0.001-stashes-8-007.txt | grep Min   | sort -n -t , -k 3' using 3:(8*$4/$3) with lines lw 9 title "MPCF", \
     "< grep fpp incremental-0.001-stashes-8-007.txt | grep \\\"E | sort -n -t , -k 3" using 3:(8*$4/$3) with linespoints title "PCF", \
     "< grep fpp incremental-0.001-stashes-8-007.txt | grep \\\"B | sort -n -t , -k 3" using 3:(8*$4/$3) with lines title "PBF"

set terminal postscript eps enhanced color size 12cm,6.4cm; # fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
set output 'ideal-bits-per-item.eps';
unset grid;
unset logscale y;
set logscale x;
set xlabel "keys inserted";
set ylabel "bits per key"
set datafile separator ",";
unset format y
unset yrange
set title "Bits per key"
plot '< grep fpp incremental-0.001-stashes-8-007.txt | grep Min   | sort -n -t , -k 3' using 3:(-log($6)/log(2)) with lines lw 9 title "MPCF", \
     "< grep fpp incremental-0.001-stashes-8-007.txt | grep \\\"E | sort -n -t , -k 3" using 3:(-log($6)/log(2)) with linespoints title "PCF", \
     "< grep fpp incremental-0.001-stashes-8-007.txt | grep \\\"B | sort -n -t , -k 3" using 3:(-log($6)/log(2)) with lines title "PBF"

# efficiency: number of bytes per item compared to the minimum needed
# lower is better
set terminal postscript eps enhanced color size 12cm,6.4cm; # fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
set output 'overage.eps';
unset grid;
set logscale y;
set logscale x;
set xlabel "keys inserted";
set ylabel "space overage"
set datafile separator ",";
unset format y;
set title "Space over information-theoretic minimum";
plot '< grep fpp incremental-0.001-stashes-8-007.txt | grep   Min | sort -n -t , -k 3' using 3:(8*($4/$3)/-(log($6)/log(2))) with lines lw 9 title "MPCF", \
     "< grep fpp incremental-0.001-stashes-8-007.txt | grep \\\"E | sort -n -t , -k 3" using 3:(8*($4/$3)/-(log($6)/log(2))) with linespoints title "PCF", \
     "< grep fpp incremental-0.001-stashes-8-007.txt | grep \\\"B | sort -n -t , -k 3" using 3:(8*($4/$3)/-(log($6)/log(2))) with lines title "PBF


# efficiency: number of bytes per item compared to the minimum needed
# lower is better
set terminal postscript eps enhanced color size 12cm,6.4cm; # fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
set output 'deficiency.eps';
unset grid;
unset logscale y;
set logscale x;
set xlabel "keys inserted";
set ylabel "space overage"
set datafile separator ",";
set format y '%.0f%%'
set title "Space over information-theoretic minimum"
plot '< grep fpp incremental-0.001-stashes-8-007.txt | grep   Min | sort -n -t , -k 3' using 3:(100 - 100/(8*($4/$3)/-(log($6)/log(2)))) with lines lw 9 title "MPCF", \
     "< grep fpp incremental-0.001-stashes-8-007.txt | grep \\\"E | sort -n -t , -k 3" using 3:(100 - 100/(8*($4/$3)/-(log($6)/log(2)))) with linespoints title "PCF", \
     "< grep fpp incremental-0.001-stashes-8-007.txt | grep \\\"B | sort -n -t , -k 3" using 3:(100 - 100/(8*($4/$3)/-(log($6)/log(2)))) with lines title "PBF

# insert time; lower is better
set terminal postscript eps enhanced color size 12cm,6.4cm; # fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
set output 'insert.eps'
unset grid;
set logscale y 2;
set logscale x;
set key bottom left;
set datafile separator ",";
set xlabel "keys inserted";
set ylabel "nanoseconds";
set title "Insert performance"
unset format y;
plot "< grep insert incremental-0.001-stashes-8-007.txt | grep   Min | sort -n -t , -k 3" using 3:6 with linespoints title "MPCF", \
     "< grep insert incremental-0.001-stashes-8-007.txt | grep \\\"E | sort -n -t , -k 3" using 3:6 with lines lw 9 title "PCF", \
     "< grep insert incremental-0.001-stashes-8-007.txt | grep \\\"B | sort -n -t , -k 3" using 3:6 with lines title "PBF"

# lookup time; lower is better
set terminal postscript eps enhanced color size 12cm,6.4cm; # fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
set output 'lookup-absent.eps'
unset grid;
set logscale y 2;
set logscale x;
set key bottom right;
set datafile separator ",";
set xlabel "keys inserted";
set ylabel "nanoseconds";
set title "Lookup performance (absent)"
unset format y;
plot "< grep find_missing incremental-0.001-stashes-8-007.txt | grep   Min | sort -n -t , -k 3" using 3:6 with lines lw 9 title "MPCF", \
     "< grep find_missing incremental-0.001-stashes-8-007.txt | grep \\\"E | sort -n -t , -k 3" using 3:6 with linespoints title "PCF", \
     "< grep find_missing incremental-0.001-stashes-8-007.txt | grep \\\"B | sort -n -t , -k 3" using 3:6 with lines title "PBF"

# lookup time; lower is better
set terminal postscript eps enhanced color size 12cm,6.4cm; # fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
set output 'lookup-present.eps'
unset grid;
set logscale y 2;
set logscale x;
set key bottom right;
set datafile separator ",";
set xlabel "keys inserted";
set ylabel "nanoseconds";
set title "Lookup performance (present)"
unset format y;
plot "< grep find_present incremental-0.001-stashes-8-007.txt | grep   Min | sort -n -t , -k 3" using 3:6 with lines lw 9 title "MPCF", \
     "< grep find_present incremental-0.001-stashes-8-007.txt | grep \\\"E | sort -n -t , -k 3" using 3:6 with linespoints title "PCF", \
     "< grep find_present incremental-0.001-stashes-8-007.txt | grep \\\"B | sort -n -t , -k 3" using 3:6 with lines title "PBF"

# space usage; lower is better
set terminal postscript eps enhanced color size 12cm,6.4cm; # fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
set output 'space.eps'
unset grid;
unset logscale y;
unset logscale x;
set datafile separator ",";
set key top left;
set xrange[1:100000000];
set xlabel "keys inserted";
set ylabel "bytes occupied";
unset format y;
set title "Space used"
plot "< grep fpp incremental-0.001-stashes-8-007.txt | grep   Min | sort -n -t , -k 3" using 3:4  lw 9 title "MPCF", \
     "< grep fpp incremental-0.001-stashes-8-007.txt | grep \\\"E | sort -n -t , -k 3" using 3:4 with linespoints title "PCF", \
     "< grep fpp incremental-0.001-stashes-8-007.txt | grep \\\"B | sort -n -t , -k 3" using 3:4 with lines title "PBF"

# x axis is find time, y axis is fpp efficiency. lower left is better
# set terminal postscript eps enhanced color size 12cm,6.4cm; # fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
# set output 'insert-deficiency.eps'
# unset grid;
# unset logscale y;
# set logscale x;
# set key top right;
# unset xrange
# set datafile separator ",";
# set xlabel "lookup (nanoseconds)";
# set ylabel "wasted space";
# unset format y;
# unset title
# plot "< csvsql -H incremental-0.001-stashes-8-003.txt incremental-0.001-stashes-8-003.txt --query \"select x.b as ndv, x.d as size, x.f as find_missing_nanos, y.f as fpp from x, y where x.a = y.a AND x.b = y.b AND x.e = 'find_missing_nanos' AND y.e = 'fpp' AND x.a = 'MinPlastic'\" --tables x,y" using 3:(100 - 100/(8*($2/$1)/-(log($4)/log(2)))) with lines title "MPCF", \
#       "< csvsql -H incremental-0.001-stashes-8-003.txt incremental-0.001-stashes-8-003.txt --query \"select x.b as ndv, x.d as size, x.f as find_missing_nanos, y.f as fpp from x, y where x.a = y.a AND x.b = y.b AND x.e = 'find_missing_nanos' AND y.e = 'fpp' AND x.a = 'Elastic'\" --tables x,y" using 3:(100 - 100/(8*($2/$1)/-(log($4)/log(2)))) with lines lw 9 title "PCF", \
#            "< csvsql -H incremental-0.001-stashes-8-003.txt incremental-0.001-stashes-8-003.txt --query \"select x.b as ndv, x.d as size, x.f as find_missing_nanos, y.f as fpp from x, y where x.a = y.a AND x.b = y.b AND x.e = 'find_missing_nanos' AND y.e = 'fpp' AND x.a = 'BlockElastic'\" --tables x,y" using 3:(100 - 100/(8*($2/$1)/-(log($4)/log(2)))) with linespoints title "PBF"