## Benchmark lock-free lib-CDS and `std::map` guarded by contention free shared mutex

Simulates real application - added 20 usec delay between each inter-thread data exchange

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


Performance comparison of different multithread associative arrays on one server by using different number of threads.

In this benchmarks used this commit of libCDS: https://github.com/khizmax/libcds/tree/5e2a9854b0a8625183818eb8ad91e5511fed5898

Benchmark on Linux (GCC 4.9.2) - **2 x CPU** Intel Xeon E5-2686v4 2.3 GHz (Broadwell) 18 Cores (36 HT) - total: 72 Threads

Command line for starting: `./benchmark`


1. **Performance** (the bigger – the better), MOps - millions operations per second


 ![Performance contention free shared mutex](https://hsto.org/files/4bd/437/949/4bd437949f934c51aa1a8c65e4c664d9.png)


2. **Median-latency** (the lower – the better), microseconds

To measure median latency – in the test code main.cpp, you should to set: `const bool measure_latency = true;`

 ![Latency contention free shared mutex](https://hsto.org/files/8e4/613/c15/8e4613c15d194c95915939cbec3597cb.png)

