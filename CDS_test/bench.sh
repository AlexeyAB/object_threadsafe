#sudo apt-get install numactl
echo ------------------------------- >> bench_cds_log.txt
##date +%F >> bench_cds_log.txt
date +%T >> bench_cds_log.txt

numactl --localalloc --cpunodebind=0 ./benchmark 18 >> bench_cds_log.txt

# ./benchmark >> bench_cds_log.txt
