#include <iostream>
#include <atomic>
#include <thread>
 
void thread_func(std::atomic<int> &shared_val) 
{
    // but both are not thread-safe: error
	// shared_val /= 2; 	// thread-safe: OK
	// here shared_val can be changed by another thread
	// shared_val += 10; 	// thread-safe: OK
 
	// thread-safe - Sequential consistency: OK
	int old_local_val, new_local_val;
	do {
		old_local_val = shared_val;
		new_local_val = old_local_val/2 + 10;
	} while(!shared_val.compare_exchange_weak(old_local_val, new_local_val));
 
}
 
int main() {
	std::atomic<int> shared_val;
	shared_val = 10;
 
	// 15 = 10/2 + 10
	// 17 = 15/2 + 10
	std::thread t1( thread_func, std::ref(shared_val) ); 
	std::thread t2( thread_func, std::ref(shared_val) ); 
	t1.join(); 
	t2.join();
	std::cout << "shared_val = " << shared_val;
 
	return 0;
}