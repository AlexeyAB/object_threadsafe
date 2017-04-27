#sudo apt-get install numactl


numactl --localalloc --cpunodebind=0 ./benchmark 18

# ./benchmark