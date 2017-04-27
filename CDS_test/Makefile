benchmark : main.o
	g++ -lm -pthread -O3 -o benchmark main.o ./libcds/bin/libcds-s.a
		 

gcc = g++ -std=c++14 -pthread -I./libcds -O3 -c

main.o : main.cpp
	$(gcc) main.cpp


clean : 
	rm main.o benchmark
