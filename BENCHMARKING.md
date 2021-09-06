To benchmark:

1. Install git, GNU make, a C compiler, a C++ comiler, gnuplot, and python 3. Note that BSD make will not work!
2. `make`
3. `make -k cpp-world`
4. Ignore errors
5. `cpp/extras/benchmarks/bench.exe --ndv 1000000 --reps 1 --bytes 1 --fpp 0.004 | tee doc/taffy/all-bench-100000000-016.txt` or `cpp/extras/benchmarks/bench.exe --ndv 1000000 --reps 1 --bytes 1 --fpp 0.004 | tee doc/taffy/m6g.medium.txt`
6. cd doc/taffy
7. gnuplot plot.gnuplot

These instructions have been tested in Ubuntu 20.04, RHEL 8, Amazon Linux 2, and FreeBSD 13.0.
