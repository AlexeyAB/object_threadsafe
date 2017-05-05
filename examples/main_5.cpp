#include <iostream>
#include <atomic>
#include <thread>

struct T {
    int price, count, total;    
};
 
void thread_func(std::atomic<T> &shared_val) 
{

	// thread-safe - Sequential consistency: OK
	T old_local_val, new_local_val;
	do {
		old_local_val = shared_val; // lock-based operator= inside std::atomic<T>
		// all required calculations
        new_local_val = old_local_val;
		new_local_val.count = new_local_val.count + 1;
		new_local_val.total = new_local_val.price * new_local_val.count;
        
    // lock-based function compare_exchange_weak() inside std::atomic<T>
	} while(!shared_val.compare_exchange_weak(old_local_val, new_local_val));
 
}
 
int main() {
	std::atomic<T> shared_val;
	shared_val = {10,5,50};
    std::cout << std::boolalpha << "shared_val.is_lock_free() = " << shared_val.is_lock_free() << std::endl;
 
	std::thread t1( thread_func, std::ref(shared_val) ); 
	std::thread t2( thread_func, std::ref(shared_val) ); 
	t1.join(); 
	t2.join();
	
	T local_result = shared_val;
	std::cout << local_result.price << ", " << local_result.count << ", "
		<< local_result.total;
 
	return 0;
}