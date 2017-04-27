contfree_shared_mutex : main.o
	g++ -lm -pthread -O3 -o contfree_shared_mutex main.o
		 

gcc = g++ -std=c++14 -pthread -O3 -c

main.o : main.cpp
	$(gcc) main.cpp


clean : 
	rm main.o contfree_shared_mutex
