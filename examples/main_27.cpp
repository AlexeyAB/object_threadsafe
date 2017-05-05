#include <iostream>
#include <atomic>
#include <thread>
#include <cassert>

class recursive_spinlock_t {
	std::atomic_flag lock_flag;
	int64_t recursive_counter;

	typedef std::thread::id thread_id_t;
	std::atomic<std::thread::id> owner_thread_id;
	std::thread::id get_fast_this_thread_id() { return std::this_thread::get_id(); }
public:
	recursive_spinlock_t() : recursive_counter(0), owner_thread_id(thread_id_t()) { lock_flag.clear(); }

	bool try_lock() {
		if (!lock_flag.test_and_set(std::memory_order_acquire)) {
			owner_thread_id.store(get_fast_this_thread_id(), std::memory_order_release);
		}
		else {
			if (owner_thread_id.load(std::memory_order_acquire) != get_fast_this_thread_id())
				return false;
		}
		++recursive_counter;
		return true;
	}

	void lock() {
		for (volatile size_t i = 0; !try_lock(); ++i)
			if (i % 100000 == 0) std::this_thread::yield();
	}

	void unlock() {
		assert(owner_thread_id.load(std::memory_order_acquire) == get_fast_this_thread_id());
		assert(recursive_counter > 0);

		if (--recursive_counter == 0) {
			owner_thread_id.store(thread_id_t(), std::memory_order_release);
			lock_flag.clear(std::memory_order_release);
		}
	}
};
// ---------------------------------------------------------------

int shared_value = 0;
recursive_spinlock_t recursive_spinlock;

void add_to_shared() { 
  recursive_spinlock.lock();    // 1 lock - OK
  recursive_spinlock.lock();    // 2 lock - OK (already locked recursive_spinlock)
  shared_value += 25;
  recursive_spinlock.unlock();
  recursive_spinlock.unlock();
}

int main() {

    std::thread t1([&]() { add_to_shared(); });
    std::thread t2([&]() { add_to_shared(); });
    t1.join(); t2.join();	
	std::cout << "shared_value = " << shared_value 
		<< std::endl;
	return 0;
}