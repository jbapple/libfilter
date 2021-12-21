# incremental-0.001-stashes-8-007.txt
# incremental-0.001-stashes-8-if-80p-002.txt
# incremental-0.001-8-slices-stashes-8-if-90p-001.txt
# incremental-0.001-stashes-8-true-found-001.txt
# 004-levels-75p-001.txt
# all-bench-100000000.txt
# cuckoo-12-ndv-100000000-bench.txt
# 12cm,6.4cm


set terminal postscript eps enhanced color size 3.333in,7cm fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman19,19";
#font "libertine"
#fontfile "/usr/share/fonts/opentype/linux-libertine/LinBiolinum_K.otf"
#fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
set output 'bits-per-item.eps';
unset grid;
unset logscale y;
set logscale x;
set xlabel "keys inserted (power of 10)";
set ylabel "bits per key"
set datafile separator ",";
unset format
set format x "10^{%.0T}";
set yrange [10:50]
set key outside;
set key top center;
#set title "Bits per key"
plot '< grep fpp all-bench-100000000-017.txt | grep MinTaffy    | sort -n -t , -k 3' using 3:(8*$4/$3) with lines lw 9 title "MTCF", \
     "< grep fpp all-bench-100000000-017.txt | grep TaffyCuckoo | sort -n -t , -k 3" using 3:(8*$4/$3) with linespoints title "TCF", \
     "< grep fpp all-bench-100000000-017.txt | grep TaffyBlock  | sort -n -t , -k 3" using 3:(8*$4/$3) with lines title "TBF", \
     "< grep fpp all-bench-100000000-017.txt | grep \\\"Cuckoo\\\"  | sort -n -t , -k 3" using 3:(8*$4/$3) with linespoints  lw 1  title "CF", \
     "< grep fpp all-bench-100000000-017.txt | grep Simd        | sort -n -t , -k 3" using 3:(8*$4/$3) with lines lw 5  title "SBBF"

set terminal postscript eps enhanced color size 3.333in,2.5in fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman19,19";
set output 'ideal-bits-per-item.eps';
set grid noxtics ytics;
unset grid
set logscale y 10;
set logscale x;
set xlabel "keys inserted";
set ylabel "false positive probability"
set datafile separator ",";
unset format;
set format y '%g%%';
set format x "10^{%.0T}"
set yrange [0.01:3];
set key inside;
set key top left;
#set title "Empirical false positive probabilty after adding N keys"
plot '< grep fpp all-bench-100000000-017.txt | grep MinTaffy    | sort -n -t , -k 3' using 3:(100*$6) with lines lw 9 title "MTCF", \
     "< grep fpp all-bench-100000000-017.txt | grep TaffyCuckoo | sort -n -t , -k 3" using 3:(100*$6) with linespoints title "TCF", \
     "< grep fpp all-bench-100000000-017.txt | grep TaffyBlock  | sort -n -t , -k 3" using 3:(100*$6) with lines title "TBF", \
     "< grep fpp all-bench-100000000-017.txt | grep \\\"Cuckoo\\\"  | sort -n -t , -k 3" using 3:(100*$6) with linespoints  lw 1  title "CF", \
     "< grep fpp all-bench-100000000-017.txt | grep Simd        | sort -n -t , -k 3" using 3:(100*$6) with lines  lw 5  title "SBBF"

# # efficiency: number of bytes per item compared to the minimum needed
# # lower is better
# set terminal postscript eps enhanced color size 9cm,6cm; # fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
# set output 'overage.eps';
# unset grid;
# set logscale y;
# set logscale x;
# set xlabel "keys inserted";
# set ylabel "space overage"
# set datafile separator ",";
# unset format;
# set title "Space over information-theoretic minimum";
# plot '< grep fpp incremental-0.001-stashes-8-007.txt | grep   Min | sort -n -t , -k 3' using 3:(8*($4/$3)/-(log($6)/log(2))) with lines lw 9 title "MTCF", \
#      "< grep fpp incremental-0.001-stashes-8-007.txt | grep \\\"E | sort -n -t , -k 3" using 3:(8*($4/$3)/-(log($6)/log(2))) with linespoints title "TCF", \
#      "< grep fpp incremental-0.001-stashes-8-007.txt | grep \\\"B | sort -n -t , -k 3" using 3:(8*($4/$3)/-(log($6)/log(2))) with lines title "TBF


# # efficiency: number of bytes per item compared to the minimum needed
# # lower is better
# set terminal postscript eps enhanced color size 9cm,6cm; # fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
# set output 'deficiency.eps';
# unset grid;
# unset logscale y;
# set logscale x;
# set xlabel "keys inserted";
# set ylabel "space overage"
# set datafile separator ",";
# set format y '%.0f%%'
# set title "Space over information-theoretic minimum"
# plot '< grep fpp incremental-0.001-stashes-8-007.txt | grep   Min | sort -n -t , -k 3' using 3:(100 - 100/(8*($4/$3)/-(log($6)/log(2)))) with lines lw 9 title "MTCF", \
#      "< grep fpp incremental-0.001-stashes-8-007.txt | grep \\\"E | sort -n -t , -k 3" using 3:(100 - 100/(8*($4/$3)/-(log($6)/log(2)))) with linespoints title "TCF", \
#      "< grep fpp incremental-0.001-stashes-8-007.txt | grep \\\"B | sort -n -t , -k 3" using 3:(100 - 100/(8*($4/$3)/-(log($6)/log(2)))) with lines title "TBF

# insert time; lower is better
# set terminal postscript eps enhanced color size 9cm,6cm; # fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
# set output 'insert.eps'
# unset grid;
# set logscale y 2;
# set logscale x;
# set key top left;
# set datafile separator ",";
# set xlabel "keys inserted";
# set ylabel "nanoseconds";
# #set title "Insert performance"
# unset format;
# set yrange[*:*];
# plot "< grep insert all-bench-100000000-017.txt | grep MinTaffy    | sort -n -t , -k 3" using 3:6 with lines lw 1 title "MTCF", \
#      "< grep insert all-bench-100000000-017.txt | grep TaffyCuckoo | sort -n -t , -k 3" using 3:6 with lines lw 1 title "TCF", \
#      "< grep insert all-bench-100000000-017.txt | grep TaffyBlock  | sort -n -t , -k 3" using 3:6 with lines lw 1 title "TBF", \
#      "< grep insert all-bench-100000000-017.txt | grep \\\"Cuckoo\\\"  | sort -n -t , -k 3" using 3:6 with lines lw 1 title "CF", \
#      "< grep insert all-bench-100000000-017.txt | grep Simd        | sort -n -t , -k 3" using 3:6 with lines lw 1 title "SBBF"

# lookup time; lower is better
# set terminal postscript eps enhanced color size 9cm,6cm; # fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
# set output 'lookup-absent.eps'
# unset grid;
# set logscale x;
# unset logscale y;
# set xrange [*:100000000];
# set key top left;
# set datafile separator ",";
# set xlabel "keys inserted";
# set ylabel "nanoseconds";
# #set title "Lookup performance (absent)"
# unset format;
# plot "< grep find_missing all-bench-100000000-017.txt | grep MinTaffy    | sort -n -t , -k 3" using 3:6 with lines lw 9 title "MTCF", \
#      "< grep find_missing all-bench-100000000-017.txt | grep TaffyCuckoo | sort -n -t , -k 3" using 3:6 with linespoints title "TCF", \
#      "< grep find_missing all-bench-100000000-017.txt | grep TaffyBlock  | sort -n -t , -k 3" using 3:6 with lines title "TBF", \
#      "< grep find_missing all-bench-100000000-017.txt | grep \\\"Cuckoo\\\"  | sort -n -t , -k 3" using 3:6 with linespoints  lw 1  title "CF", \
#      "< grep find_missing all-bench-100000000-017.txt | grep Simd        | sort -n -t , -k 3" using 3:6 with lines  lw 5  title "SBBF"

# lookup time; lower is better
# set terminal postscript eps enhanced color size 9cm,6cm; # fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
# set output 'lookup-present.eps'
# unset grid;
# unset logscale y;
# set logscale x;
# set key top left;
# set datafile separator ",";
# set xlabel "keys inserted";
# set ylabel "nanoseconds";
# #set title "Lookup performance (present)"
# unset format;
# plot "< grep find_present all-bench-100000000-017.txt | grep MinTaffy    | sort -n -t , -k 3" using 3:6 with lines lw 1 title "MTCF", \
#      "< grep find_present all-bench-100000000-017.txt | grep TaffyCuckoo | sort -n -t , -k 3" using 3:6 with lines lw 1 title "TCF", \
#      "< grep find_present all-bench-100000000-017.txt | grep TaffyBlock  | sort -n -t , -k 3" using 3:6 with lines lw 1 title "TBF", \
#      "< grep find_present all-bench-100000000-017.txt | grep \\\"Cuckoo\\\"  | sort -n -t , -k 3" using 3:6 with lines lw 1  title "CF", \
#      "< grep find_present all-bench-100000000-017.txt | grep Simd        | sort -n -t , -k 3" using 3:6 with lines lw 1  title "SBBF"
     


# space usage; lower is better
set terminal postscript eps enhanced color size 3.333in,5cm  fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman19,19"; # fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
set output 'space.eps'
unset grid;
unset logscale y;
unset logscale x;
set datafile separator ",";
set key inside;
set key top left;
set yrange[*:*]
set xrange[1:100000000];
set xlabel "keys inserted (millions)";
set ylabel "bytes occupied (millions)";
unset format;
set format x "%.0s";
set format y "%.0s";
#set title "Space used"
plot "< grep fpp all-bench-100000000-017.txt | grep MinTaffy    | sort -n -t , -k 3" using 3:4 with lines lw 9 title "MTCF", \
     "< grep fpp all-bench-100000000-017.txt | grep TaffyCuckoo | sort -n -t , -k 3" using 3:4 with linespoints title "TCF", \
     "< grep fpp all-bench-100000000-017.txt | grep TaffyBlock  | sort -n -t , -k 3" using 3:4 with lines title "TBF", \
     "< grep fpp all-bench-100000000-017.txt | grep \\\"Cuckoo\\\"  | sort -n -t , -k 3" using 3:4 with linespoints  lw 1 title "CF", \
     "< grep fpp all-bench-100000000-017.txt | grep Simd        | sort -n -t , -k 3" using 3:4 with lines lw 5 title "SBBF"

# x axis is find time, y axis is fpp efficiency. lower left is better
# set terminal postscript eps enhanced color size 9cm,6cm; # fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
# set output 'insert-deficiency.eps'
# unset grid;
# unset logscale y;
# set logscale x;
# set key top right;
# unset xrange
# set datafile separator ",";
# set xlabel "lookup (nanoseconds)";
# set ylabel "wasted space";
# unset format;
# unset title
# plot "< csvsql -H incremental-0.001-stashes-8-003.txt incremental-0.001-stashes-8-003.txt --query \"select x.b as ndv, x.d as size, x.f as find_missing_nanos, y.f as fpp from x, y where x.a = y.a AND x.b = y.b AND x.e = 'find_missing_nanos' AND y.e = 'fpp' AND x.a = 'MinPlastic'\" --tables x,y" using 3:(100 - 100/(8*($2/$1)/-(log($4)/log(2)))) with lines title "MTCF", \
#       "< csvsql -H incremental-0.001-stashes-8-003.txt incremental-0.001-stashes-8-003.txt --query \"select x.b as ndv, x.d as size, x.f as find_missing_nanos, y.f as fpp from x, y where x.a = y.a AND x.b = y.b AND x.e = 'find_missing_nanos' AND y.e = 'fpp' AND x.a = 'Elastic'\" --tables x,y" using 3:(100 - 100/(8*($2/$1)/-(log($4)/log(2)))) with lines lw 9 title "TCF", \
#            "< csvsql -H incremental-0.001-stashes-8-003.txt incremental-0.001-stashes-8-003.txt --query \"select x.b as ndv, x.d as size, x.f as find_missing_nanos, y.f as fpp from x, y where x.a = y.a AND x.b = y.b AND x.e = 'find_missing_nanos' AND y.e = 'fpp' AND x.a = 'BlockElastic'\" --tables x,y" using 3:(100 - 100/(8*($2/$1)/-(log($4)/log(2)))) with linespoints title "TBF"



# set terminal postscript eps enhanced color size 9cm,6cm; # fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
# set output 'missing-ratio.eps'
# unset grid;
# set logscale y;
# set logscale x;
# set datafile separator ",";
# set key top left;
# set xrange[1:100000000];
# set xlabel "keys inserted";
# set ylabel "ratio";
# unset format;
# #set title "ratio of find_missing to find_missing in CF
# plot "< csvsql -H all-bench-100000000-010.txt all-bench-100000000-010.txt --tables x,y --query \"select x.a, x.c, x.f, y.f from x, y where x.b = y.b and x.e = y.e and y.a = 'Cuckoo' and x.a = 'MinTaffy' and x.e = y.e and x.e like 'find%' order by x.b asc, x.e\"" using 2:($3/$4) with lines lw 9 title "MTCF", \
#      "< csvsql -H all-bench-100000000-010.txt all-bench-100000000-010.txt --tables x,y --query \"select x.a, x.c, x.f, y.f from x, y where x.b = y.b and x.e = y.e and y.a = 'Cuckoo' and x.a = 'TaffyCuckoo' and x.e = y.e and x.e like 'find%' order by x.b asc, x.e\"" using 2:($3/$4) with linespoints title "TCF", \
#      "< csvsql -H all-bench-100000000-010.txt all-bench-100000000-010.txt --tables x,y --query \"select x.a, x.c, x.f, y.f from x, y where x.b = y.b and x.e = y.e and y.a = 'Cuckoo' and x.a = 'TaffyBlock' and x.e = y.e and x.e like 'find%' order by x.b asc, x.e\"" using 2:($3/$4) with lines title "TBF", \
#      "< csvsql -H all-bench-100000000-010.txt all-bench-100000000-010.txt --tables x,y --query \"select x.a, x.c, x.f, y.f from x, y where x.b = y.b and x.e = y.e and y.a = 'Cuckoo' and x.a = 'Cuckoo' and x.e = y.e and x.e like 'find%' order by x.b asc, x.e\"" using 2:($3/$4) with lines lw 1 title "CF", \
#      "< csvsql -H all-bench-100000000-010.txt all-bench-100000000-010.txt --tables x,y --query \"select x.a, x.c, x.f, y.f from x, y where x.b = y.b and x.e = y.e and y.a = 'Cuckoo' and x.a = 'SimdBlockFilter' and x.e = y.e and x.e like 'find%' order by x.b asc, x.e\"" using 2:($3/$4) with lines lw 5 title "SBBF"

set terminal postscript eps enhanced color size 3.333in,5cm  fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman19,19"; # fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
set output 'lookup-both.eps'
unset grid;
set logscale y;
set logscale x;
set key inside;
set key top left;
set datafile separator ",";
set xlabel "keys inserted";
set ylabel "nanoseconds";
set yrange [5:*]
#set title "Lookup performance (present)"
unset format;
set format x "10^{%.0T}";
plot "< grep find_missing all-bench-100000000-017.txt | grep MinTaffy    | sort -n -t , -k 2,3" using 3:6 with lines lw 9 title "MTCF", \
     "< grep find_missing all-bench-100000000-017.txt | grep TaffyCuckoo | sort -n -t , -k 2,3" using 3:6 with linespoints  title "TCF", \
     "< grep find_missing all-bench-100000000-017.txt | grep TaffyBlock  | sort -n -t , -k 2,3" using 3:6 with lines  title "TBF", \
     "< grep find_missing all-bench-100000000-017.txt | grep \\\"Cuckoo\\\"  | sort -n -t , -k 2,3" using 3:6 with linespoints lw 1  title "CF", \
     "< grep find_missing all-bench-100000000-017.txt | grep Simd        | sort -n -t , -k 2,3" using 3:6 with lines lw 5  title "SBBF"

# plot "< grep fpp all-bench-100000000-017.txt | grep MinTaffy    | sort -n -t , -k 3" using 3:4 with lines lw 9 title "MTCF", \
     "< grep fpp all-bench-100000000-017.txt | grep TaffyCuckoo | sort -n -t , -k 3" using 3:4 with linespoints title "TCF", \
     "< grep fpp all-bench-100000000-017.txt | grep TaffyBlock  | sort -n -t , -k 3" using 3:4 with lines title "TBF", \
     "< grep fpp all-bench-100000000-017.txt | grep \\\"Cuckoo\\\"  | sort -n -t , -k 3" using 3:4 with linespoints  lw 1 title "CF", \
     "< grep fpp all-bench-100000000-017.txt | grep Simd        | sort -n -t , -k 3" using 3:4 with lines lw 5 title "SBBF"


set terminal postscript eps enhanced color size 3.333in,7cm  fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman19,19"; # fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
set output 'arm-lookup-both.eps'
unset grid;
set logscale y;
set logscale x;
set key outside;
set key top center;
set datafile separator ",";
set xlabel "keys inserted";
set ylabel "nanoseconds";
set yrange [5:*]
#set title "Lookup performance (present)"
unset format;
set format x "10^{%.0T}";
plot "< grep find_missing m6g.medium.txt | grep MinTaffy    | sort -n -t , -k 2,3" using 3:6 with lines lw 9 title "MTCF", \
     "< grep find_missing m6g.medium.txt | grep TaffyCuckoo | sort -n -t , -k 2,3" using 3:6 with linespoints title "TCF", \
     "< grep find_missing m6g.medium.txt | grep TaffyBlock  | sort -n -t , -k 2,3" using 3:6 with lines title "TBF", \
     "< grep find_missing m6g.medium.txt | grep \\\"Cuckoo\\\"  | sort -n -t , -k 2,3" using 3:6 with linespoints lw 1  title "CF", \
     "< grep find_missing m6g.medium.txt | grep Simd        | sort -n -t , -k 2,3" using 3:6 with lines lw 5  title "SBBF"

# insert time; lower is better
# set terminal postscript eps enhanced color size 9cm,6cm; # fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
# set output 'arm-insert.eps'
# unset grid;
# set logscale y 2;
# set logscale x;
# set key top left;
# set datafile separator ",";
# set xlabel "keys inserted";
# set ylabel "nanoseconds";
# #set title "Insert performance"
# unset format;
# plot "< grep insert m6g.medium.txt | grep MinTaffy    | sort -n -t , -k 3" using 3:6 with lines lw 1 title "MTCF", \
#      "< grep insert m6g.medium.txt | grep TaffyCuckoo | sort -n -t , -k 3" using 3:6 with lines lw 1 title "TCF", \
#      "< grep insert m6g.medium.txt | grep TaffyBlock  | sort -n -t , -k 3" using 3:6 with lines lw 1 title "TBF", \
#      "< grep insert m6g.medium.txt | grep \\\"Cuckoo\\\"  | sort -n -t , -k 3" using 3:6 with lines lw 1 title "CF", \
#      "< grep insert m6g.medium.txt | grep Simd        | sort -n -t , -k 3" using 3:6 with lines lw 1 title "SBBF"

set terminal postscript eps enhanced color size 3.333in,7cm fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman19,19"; # fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
set output 'insert-cumulative.eps'
unset grid;
set logscale y 10;
set logscale x;
set key inside;
set key top left;
set datafile separator ",";
set xlabel "keys inserted";
set ylabel "average nanoseconds per key";
#set title "Insert performance"
unset format;
set format x "10^{%.0T}";
plot "< python3 running-sum.py all-bench-100000000-017.txt | grep MinTaffy    | sort -n -t , -k 3" using 2:($3/$2) with lines lw 9 title "MTCF", \
     "< python3 running-sum.py all-bench-100000000-017.txt | grep TaffyCuckoo | sort -n -t , -k 3" using 2:($3/$2) with linespoints title "TCF", \
     "< python3 running-sum.py all-bench-100000000-017.txt | grep TaffyBlock  | sort -n -t , -k 3" using 2:($3/$2) with lines title "TBF", \
     "< python3 running-sum.py all-bench-100000000-017.txt | grep '^Cuckoo,'   | sort -n -t , -k 3" using 2:($3/$2) with linespoints lw 1 title "CF", \
     "< python3 running-sum.py all-bench-100000000-017.txt | grep Simd        | sort -n -t , -k 3" using 2:($3/$2) with lines lw 5 title "SBBF"

set terminal postscript eps enhanced color size 3.333in,7cm fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman19,19";
set output 'arm-insert-cumulative.eps'
unset grid;
set logscale y 10;
set logscale x;
set key outside
set key top center;
set datafile separator ",";
set xlabel "keys inserted";
set ylabel "average nanoseconds per key";
#set title "Insert performance"
unset format;
set format x "10^{%.0T}";
plot "< python3 running-sum.py m6g.medium.txt | grep MinTaffy    | sort -n -t , -k 3" using 2:($3/$2) with lines lw 9 title "MTCF", \
     "< python3 running-sum.py m6g.medium.txt | grep TaffyCuckoo | sort -n -t , -k 3" using 2:($3/$2) with linespoints title "TCF", \
     "< python3 running-sum.py m6g.medium.txt | grep TaffyBlock  | sort -n -t , -k 3" using 2:($3/$2) with lines title "TBF", \
     "< python3 running-sum.py m6g.medium.txt | grep '^Cuckoo,'   | sort -n -t , -k 3" using 2:($3/$2) with linespoints lw 1 title "CF", \
     "< python3 running-sum.py m6g.medium.txt | grep Simd        | sort -n -t , -k 3" using 2:($3/$2) with lines lw 5 title "SBBF"

 # plot "< grep fpp all-bench-100000000.txt | grep MinTaffy    | sort -n -t , -k 3" using 3:4 with lines lw 9 title "MTCF", \
 #     "< grep fpp all-bench-100000000.txt | grep TaffyCuckoo | sort -n -t , -k 3" using 3:4 with linespoints title "TCF", \
 #     "< grep fpp all-bench-100000000.txt | grep TaffyBlock  | sort -n -t , -k 3" using 3:4 with lines title "TBF", \
 #     "< grep fpp all-bench-100000000.txt | grep \\\"Cuckoo  | sort -n -t , -k 3" using 3:4 with linespoints  lw 1 title "CF", \
 #     "< grep fpp all-bench-100000000.txt | grep Simd        | sort -n -t , -k 3" using 3:4 with lines lw 5 title "SBBF"
