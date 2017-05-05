#include <iostream>
#include <atomic>
#include <thread>

class spinlock_t {
    std::atomic_flag lock_flag;
public:
    spinlock_t() { lock_flag.clear(); }

    bool try_lock() { return !lock_flag.test_and_set(std::memory_order_acquire); }
    void lock() { for (size_t i = 0; !try_lock(); ++i) if (i % 100 == 0) std::this_thread::yield(); }
    void unlock() { lock_flag.clear(std::memory_order_release); }
};

int shared_value = 0;
spinlock_t spinlock;

void add_to_shared() { 
  spinlock.lock();      // 1 lock - OK
  spinlock.lock();      // 2 lock - error!!! (already locked spinlock)
  shared_value += 25;
  spinlock.unlock();
  spinlock.unlock();
}

int main() {

    std::thread t1([&]() { add_to_shared(); });
    std::thread t2([&]() { add_to_shared(); });
    t1.join(); t2.join();	
	std::cout << "shared_value = " << shared_value 
		<< std::endl;
	return 0;
}