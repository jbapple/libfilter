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
set xlabel "Keys inserted";
set ylabel "Bits per key"
set datafile separator ",";
unset format
set format x "10^{%.1T}";
set yrange [0:50]
set key inside;
set key bottom left;
#set title "Bits per key"
plot '< grep fpp all-bench-100000000-017.txt | grep MinTaffy    | sort -n -t , -k 3' using 3:(8*$4/$3) with lines lw 9 title "MTCF", \
     "< grep fpp all-bench-100000000-017.txt | grep TaffyCuckoo | sort -n -t , -k 3" using 3:(8*$4/$3) with linespoints title "TCF", \
     "< grep fpp all-bench-100000000-017.txt | grep TaffyBlock  | sort -n -t , -k 3" using 3:(8*$4/$3) with lines title "TBF", \
     "< grep fpp all-bench-100000000-017.txt | grep \\\"Cuckoo\\\"  | sort -n -t , -k 3" using 3:(8*$4/$3) with linespoints  lw 1  title "CF", \
     "< grep fpp all-bench-100000000-017.txt | grep Simd        | sort -n -t , -k 3" using 3:(8*$4/$3) with lines lw 5  title "SBBF"

set terminal postscript eps enhanced color size 3.333in,7cm fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman19,19";
set output 'ideal-bits-per-item.eps';
set grid noxtics ytics;
unset grid
set logscale y 10;
set logscale x;
set xlabel "Keys inserted";
set ylabel "False positive probability"
set datafile separator ",";
unset format;
set format y '%g%%';
set format x "10^{%.1T}"
set yrange [0.01:3];
set key inside;
set key top left;
#set title "Empirical false positive probabilty after adding N keys"
plot '< grep fpp all-bench-100000000-017.txt | grep MinTaffy    | sort -n -t , -k 3' using 3:(100*$6) with lines lw 9 title "MTCF", \
     "< grep fpp all-bench-100000000-017.txt | grep TaffyCuckoo | sort -n -t , -k 3" using 3:(100*$6) with linespoints title "TCF", \
     "< grep fpp all-bench-100000000-017.txt | grep TaffyBlock  | sort -n -t , -k 3" using 3:(100*$6) with lines title "TBF", \
     "< grep fpp all-bench-100000000-017.txt | grep \\\"Cuckoo\\\"  | sort -n -t , -k 3" using 3:(100*$6) with linespoints  lw 1  title "CF", \
     "< grep fpp all-bench-100000000-017.txt | grep Simd        | sort -n -t , -k 3" using 3:(100*$6) with lines  lw 5  title "SBBF"

# space usage; lower is better
set terminal postscript eps enhanced color size 3.333in,7cm  fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman19,19"; # fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
set output 'space.eps'
unset grid;
unset logscale y;
unset logscale x;
set datafile separator ",";
set key inside;
set key top left;
set yrange[*:*]
set xrange[1:100000000];
set xlabel "Keys inserted (millions)";
set ylabel "Bytes occupied (millions)";
unset format;
set format x "%.0s";
set format y "%.0s";
#set title "Space used"
plot "< grep fpp all-bench-100000000-017.txt | grep MinTaffy    | sort -n -t , -k 3" using 3:4 with lines lw 9 title "MTCF", \
     "< grep fpp all-bench-100000000-017.txt | grep TaffyCuckoo | sort -n -t , -k 3" using 3:4 with linespoints title "TCF", \
     "< grep fpp all-bench-100000000-017.txt | grep TaffyBlock  | sort -n -t , -k 3" using 3:4 with lines title "TBF", \
     "< grep fpp all-bench-100000000-017.txt | grep \\\"Cuckoo\\\"  | sort -n -t , -k 3" using 3:4 with linespoints  lw 1 title "CF", \
     "< grep fpp all-bench-100000000-017.txt | grep Simd        | sort -n -t , -k 3" using 3:4 with lines lw 5 title "SBBF"

set terminal postscript eps enhanced color size 3.333in,7cm  fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman19,19";
# fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman17,17";
set output 'lookup-both.eps';
unset grid;
set logscale y;
set logscale x;
set key inside;
set key top left;
set datafile separator ",";
set xlabel "Keys inserted";
set ylabel "Nanoseconds";
set yrange [5:*]
#set title "Lookup performance (present)"
unset format;
set format x "10^{%.1T}";
plot "< grep find_missing taffy-mins.csv | grep MinTaffy        | sort -n -t , -k 2,3" using 2:4 with lines lw 9 title "MTCF", \
     "< grep find_missing taffy-mins.csv | grep TaffyCuckoo     | sort -n -t , -k 2,3" using 2:4 with linespoints  title "TCF", \
     "< grep find_missing taffy-mins.csv | grep TaffyBlock      | sort -n -t , -k 2,3" using 2:4 with lines  title "TBF", \
     "< grep find_missing taffy-mins.csv | grep \\\"Cuckoo\\\"  | sort -n -t , -k 2,3" using 2:4 with linespoints lw 1  title "CF", \
     "< grep find_missing taffy-mins.csv | grep Simd            | sort -n -t , -k 2,3" using 2:4 with lines lw 5  title "SBBF"

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
set key inside;
set key top left;
set datafile separator ",";
set xlabel "Keys inserted";
set ylabel "Nanoseconds";
set yrange [5:*]
#set title "Lookup performance (present)"
unset format;
set format x "10^{%.1T}";
plot "< grep find_missing m6g.medium.again-mins.csv | grep MinTaffy    | sort -n -t , -k 2,3" using 2:4 with lines lw 9 title "MTCF", \
     "< grep find_missing m6g.medium.again-mins.csv | grep TaffyCuckoo | sort -n -t , -k 2,3" using 2:4 with linespoints title "TCF", \
     "< grep find_missing m6g.medium.again-mins.csv | grep TaffyBlock  | sort -n -t , -k 2,3" using 2:4 with lines title "TBF", \
     "< grep find_missing m6g.medium.again-mins.csv | grep \\\"Cuckoo\\\"  | sort -n -t , -k 2,3" using 2:4 with linespoints lw 1  title "CF", \
     "< grep find_missing m6g.medium.again-mins.csv | grep Simd        | sort -n -t , -k 2,3" using 2:4 with lines lw 5  title "SBBF"

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
set xlabel "Keys inserted";
set ylabel "Average nanoseconds per key";
#set title "Insert performance"
unset format;
set format x "10^{%.1T}";
plot "< python3 running-sum.py taffy-mins-with-ndv-finish.csv | grep MinTaffy    | sort -n -t , -k 3" using 2:($3/$2) with lines lw 9 title "MTCF", \
     "< python3 running-sum.py taffy-mins-with-ndv-finish.csv | grep TaffyCuckoo | sort -n -t , -k 3" using 2:($3/$2) with linespoints title "TCF", \
     "< python3 running-sum.py taffy-mins-with-ndv-finish.csv | grep TaffyBlock  | sort -n -t , -k 3" using 2:($3/$2) with lines title "TBF", \
     "< python3 running-sum.py taffy-mins-with-ndv-finish.csv | grep '^Cuckoo,'  | sort -n -t , -k 3" using 2:($3/$2) with linespoints lw 1 title "CF", \
     "< python3 running-sum.py taffy-mins-with-ndv-finish.csv | grep Simd        | sort -n -t , -k 3" using 2:($3/$2) with lines lw 5 title "SBBF"


set terminal postscript eps enhanced color size 3.333in,7cm fontfile "/usr/share/texmf/fonts/type1/public/lm/lmr17.pfb" "LMRoman19,19";
set output 'arm-insert-cumulative.eps'
unset grid;
set logscale y 10;
set logscale x;
set key inside
set key top left;
set datafile separator ",";
set xlabel "Keys inserted";
set ylabel "Average nanoseconds per key";
#set title "Insert performance"
unset format;
set format x "10^{%.1T}";
plot "< python3 running-sum.py m6g.medium.again-mins-wth-ndv-finish.csv | grep MinTaffy    | sort -n -t , -k 3" using 2:($3/$2) with lines lw 9 title "MTCF", \
     "< python3 running-sum.py m6g.medium.again-mins-wth-ndv-finish.csv | grep TaffyCuckoo | sort -n -t , -k 3" using 2:($3/$2) with linespoints title "TCF", \
     "< python3 running-sum.py m6g.medium.again-mins-wth-ndv-finish.csv | grep TaffyBlock  | sort -n -t , -k 3" using 2:($3/$2) with lines title "TBF", \
     "< python3 running-sum.py m6g.medium.again-mins-wth-ndv-finish.csv | grep '^Cuckoo,'  | sort -n -t , -k 3" using 2:($3/$2) with linespoints lw 1 title "CF", \
     "< python3 running-sum.py m6g.medium.again-mins-wth-ndv-finish.csv | grep Simd        | sort -n -t , -k 3" using 2:($3/$2) with lines lw 5 title "SBBF"
