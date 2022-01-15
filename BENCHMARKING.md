To benchmark:

1. Install git, GNU make, a C compiler, a C++ comiler, gnuplot, and python 3. Note that BSD make will not work!
2. `make`
3. `make cpp-world`
4. `cpp/extras/benchmarks/bench.exe --ndv 1000000 --reps 1 --bytes 1 --taffy_fpp 0.01 --block_fpp 0.004 | tee $(mktemp)`
5. cd doc/taffy
6. Adjust plot.gnuplot to point at the output location of bench.exe
7. gnuplot plot.gnuplot
