#include <iostream>
#include <atomic>
#include <thread>

int shared_value = 0;
std::atomic<int> lock_flag;

void add_to_shared() { 
  int expected = 0;
  while(!lock_flag.compare_exchange_weak(expected, 1, std::memory_order_acquire))
      expected = 0;
  shared_value += 25;
  lock_flag.store(0, std::memory_order_release);
  int new_shared_value = shared_value;     // can be reordered
}

int main() {
    lock_flag = 0;
	std::thread t1([&]() { add_to_shared(); });
	std::thread t2([&]() { add_to_shared(); });
	t1.join(); t2.join();	
	std::cout << "shared_value = " << shared_value 
		<< std::endl;
	return 0;
}