any_object_threadsafe : main.o
	g++ -lm -pthread -O3 -o any_object_threadsafe main.o
		 

gcc = g++ -std=c++14 -pthread -O3 -c

main.o : main.cpp
	$(gcc) main.cpp


clean : 
	rm main.o any_object_threadsafe
