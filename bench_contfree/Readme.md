## Benchmark contention free shared mutex

Compares: `std::mutex`, `std::shared_mutex`, `contention_free_shared_mutex<>`


To build and test do:

```
make
./bench.sh
```

----

### Results


The performance of various locks for different ratios of lock types - shared-lock and exclusive-lock:

* % exclusive locks = (% of write operations)
* % shared locks = 100 - (% of write operations)

    (In the case of std::mutex - always used exclusive-lock)

Benchmark on Linux (GCC 6.3.0) 16 threads - 1 x CPU Intel Xeon E-2660v3 2.6 GHz (Haswell) 10 Cores (20 HT)

Command line for starting: `numactl --localalloc --cpunodebind=0 ./benchmark 16`


1. **Performance** (the bigger – the better), MOps - millions operations per second


 ![Performance contention free shared mutex](https://hsto.org/files/3d8/1c1/bbd/3d81c1bbd945413d85eb65be18aa2b0f.png)


2. **Median-latency** (the lower – the better), microseconds

 ![Latency contention free shared mutex](https://hsto.org/files/3ae/e18/850/3aee188503b14363abb9fe0a069ecfa0.png)
