## We make a std::shared_mutex 10 times faster

This is an Windows/Linux example of how to use `contention_free_shared_mutex<>` in C++ as highly optimized shared_mutex.

This code is in the online compiler: http://coliru.stacked-crooked.com/a/11c191b06aeb5fb6

To build and test do:

```
make
./contfree_shared_mutex
```
