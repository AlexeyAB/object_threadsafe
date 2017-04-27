## Benchmark lock-free lib-CDS and `std::map` guarded by contention free shared mutex

Compares: 

* `std::mutex + std::map`
* `SkipListMap`
* `BronsonAVLTreeMap`
* `contention_free_shared_mutex<> + std::map`


To build and test do:

```
cd libcds
make
cd ..
make

./bench.sh
```


----

### Results


Performance comparison of different multithread associative arrays on one server-CPU by using different number of threads.

In this benchmarks used this commit of libCDS: https://github.com/khizmax/libcds/tree/5e2a9854b0a8625183818eb8ad91e5511fed5898

Benchmark on Linux (GCC 4.9.2) - **1 x CPU** Intel Xeon E5-2660v3 2.6 GHz (Haswell) 10 Cores (20 HT) - total: 20 Threads

Command line for starting: `numactl --localalloc --cpunodebind=0 ./benchmark 16`


1. **Performance** (the bigger – the better), MOps - millions operations per second


 ![Performance contention free shared mutex](https://hsto.org/files/2c5/d6c/93b/2c5d6c93b48c464f8c7d2eb2f2254270.png)



2. **Median-latency** (the lower – the better), microseconds

To measure median latency – in the test code main.cpp, you should to set: `const bool measure_latency = true;`

 ![Latency contention free shared mutex](https://hsto.org/files/299/3f3/904/2993f39048694f9799a520c87dfbd5ac.png)


