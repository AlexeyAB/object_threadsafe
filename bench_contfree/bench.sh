echo ------------------------------- >> bench_log.txt
##date +%F >> bench_log.txt
date +%T >> bench_log.txt

numactl --localalloc --cpunodebind=0 ./benchmark 16 >> bench_log.txt

# ./benchmark >> bench_log.txt
