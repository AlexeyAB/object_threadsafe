#include <iostream>
#include <atomic>
#include <thread>
 
void thread_func(std::atomic<int> &shared_val) 
{
    shared_val += 10; 	// thread-safe: OK
}
 
int main() {
	std::atomic<int> shared_val;
	shared_val = 0;
 
	std::thread t1( thread_func, std::ref(shared_val) );
	std::thread t2( thread_func, std::ref(shared_val) );
	t1.join(); 
	t2.join();
	std::cout << "shared_val = " << shared_val;
 
	return 0;
}