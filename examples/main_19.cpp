#include <iostream>
#include <atomic>
#include <thread>

int shared_value = 0;
std::atomic_flag lock_flag;

void add_to_shared() { 
  while(lock_flag.test_and_set(std::memory_order_acquire));
  shared_value += 25;
  lock_flag.clear(std::memory_order_release);
  int new_shared_value = shared_value;     // can be reordered
}

int main() {
    lock_flag.clear();
	std::thread t1([&]() { add_to_shared(); });
	std::thread t2([&]() { add_to_shared(); });
	t1.join(); t2.join();	
	std::cout << "shared_value = " << shared_value 
		<< std::endl;
	return 0;
}