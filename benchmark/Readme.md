## Benchmark contention free shared mutex - extended

Benchmark on all CPU cores for 0 - 90% of writes (100 - 10% of reads)

Compares: 

* `std::map` - 1 thread
* `std::map` & `std::mutex`
* `safe_ptr<std::map, std::mutex>`
* `safe_ptr<std::map, std::shared_mutex`
* `contfree_safe_ptr<std::map>`
* `contfree_safe_ptr<std::map>` & rowlock
* `safe_map_partitioned_t<>`
* `safe_map_partitioned_t<,, contfree_safe_ptr>`



To build and test do:

```
make
./bench.sh
```


----

### Results


Here are the performance graphs - the number of millions of operations per second (MOps), with a different percentage of modification operations 0 - 90%. 

For example, with 15% of modifications, the following operations are involved: 5% insert, 5% delete, 5% update, 85% read. Compiler: g ++ 4.9.2 (Linux Ubuntu) x86_64

First of all, we are interested in the orange line: `contfree_safe_ptr<map> & rowlock`

Benchmark on Linux (GCC 4.9.2) - **1 x CPU** Intel Xeon E5-2660v3 2.6 GHz (Haswell) 10 Cores (20 HT) - total: 20 Threads

Command line for starting: `numactl --localalloc --cpunodebind=0 ./benchmark 16`


1. **Performance** (the bigger – the better), MOps - millions operations per second


 ![Performance contention free shared mutex](https://hsto.org/files/128/eb1/699/128eb1699b5a4c51bb12fa9763d4faba.png)



2. **Median-latency** (the lower – the better), microseconds

To measure median latency – in the test code main.cpp, you should to set: `const bool measure_latency = true;`

 ![Latency contention free shared mutex](https://hsto.org/files/cdd/0ab/652/cdd0ab652e6e4e58ba5ebbf1ed93b5de.png)


