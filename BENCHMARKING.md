To benchmark:

1. Install git, GNU make, a C compiler, a C++ comiler, gnuplot, and python 3
2. `make`
3. `make -k cpp-world`
4. Ignore errors
5. `cpp/extras/benchmarks/bench.exe --ndv 1000000 --reps 1 --bytes 1 --fpp 0.004 | tee doc/taffy/all-bench-100000000-016.txt` or `cpp/extras/benchmarks/bench.exe --ndv 1000000 --reps 1 --bytes 1 --fpp 0.004 | tee doc/taffy/m6g.medium.txt`
6. cd doc/taffy
7. gnuplot plot.gnuplot
