#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <map>
#include <sstream>
#include <cassert>
//#include <shared_mutex>

template<typename T, typename mutex_t = std::recursive_mutex, typename x_lock_t = std::unique_lock<mutex_t>,
    typename s_lock_t = std::unique_lock<mutex_t >>
    // std::shared_lock<std::shared_timed_mutex>, when mutex_t = std::shared_timed_mutex
class safe_ptr {
    typedef mutex_t mtx_t;
    const std::shared_ptr<T> ptr;
    std::shared_ptr<mutex_t> mtx_ptr;

    template<typename req_lock>
    class auto_lock_t {
        T * const ptr;
        req_lock lock;
    public:
        auto_lock_t(auto_lock_t&& o) : ptr(std::move(o.ptr)), lock(std::move(o.lock)) { }
        auto_lock_t(T * const _ptr, mutex_t& _mtx) : ptr(_ptr), lock(_mtx){}
        T* operator -> () { return ptr; }
        const T* operator -> () const { return ptr; }
    };

    template<typename req_lock>
    class auto_lock_obj_t {
        T * const ptr;
        req_lock lock;
    public:
        auto_lock_obj_t(auto_lock_obj_t&& o) : ptr(std::move(o.ptr)), lock(std::move(o.lock)) { }
        auto_lock_obj_t(T * const _ptr, mutex_t& _mtx) : ptr(_ptr), lock(_mtx){}
        template<typename arg_t>
        auto operator [] (arg_t arg) -> decltype((*ptr)[arg]) { return (*ptr)[arg]; }
    };

    void lock() { mtx_ptr->lock(); }
    void unlock() { mtx_ptr->unlock(); }
    friend struct link_safe_ptrs;
    template<typename mutex_type> friend class std::lock_guard;
    //template<class... mutex_types> friend class std::lock_guard;    // C++17
public:
    template<typename... Args>
    safe_ptr(Args... args) : ptr(std::make_shared<T>(args...)), mtx_ptr(std::make_shared<mutex_t>()) {}

    auto_lock_t<x_lock_t> operator -> () { return auto_lock_t<x_lock_t>(ptr.get(), *mtx_ptr); }
    auto_lock_obj_t<x_lock_t> operator * () { return auto_lock_obj_t<x_lock_t>(ptr.get(), *mtx_ptr); }
    const auto_lock_t<s_lock_t> operator -> () const { return auto_lock_t<s_lock_t>(ptr.get(), *mtx_ptr); }
    const auto_lock_obj_t<s_lock_t> operator * () const { return auto_lock_obj_t<s_lock_t>(ptr.get(), *mtx_ptr); }
};
// ---------------------------------------------------------------

class spinlock_t {
    std::atomic_flag lock_flag;
public:
    spinlock_t() { lock_flag.clear(); }

    bool try_lock() { return !lock_flag.test_and_set(std::memory_order_acquire); }
    void lock() { for (size_t i = 0; !try_lock(); ++i) if (i % 100 == 0) std::this_thread::yield(); }
    void unlock() { lock_flag.clear(std::memory_order_release); }
};
// ---------------------------------------------------------------

class recursive_spinlock_t {
	std::atomic_flag lock_flag;
	int64_t recursive_counter;
#if (_WIN32 && _MSC_VER < 1900)
	typedef int64_t thread_id_t;
	std::atomic<thread_id_t> owner_thread_id;
	int64_t get_fast_this_thread_id() {
		static __declspec(thread) int64_t fast_this_thread_id = 0;  // MSVS 2013 thread_local partially supported - only POD
		if (fast_this_thread_id == 0) {
			std::stringstream ss;
			ss << std::this_thread::get_id();   // https://connect.microsoft.com/VisualStudio/feedback/details/1558211
			fast_this_thread_id = std::stoll(ss.str());
		}
		return fast_this_thread_id;
	}
#else
	typedef std::thread::id thread_id_t;
	std::atomic<std::thread::id> owner_thread_id;
	std::thread::id get_fast_this_thread_id() { return std::this_thread::get_id(); }
#endif

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


safe_ptr<std::pair<std::string, int>, spinlock_t> safe_int_spin(std::make_pair("apple", 0));
safe_ptr<std::pair<std::string, int>, recursive_spinlock_t> safe_int_spin_recursive(std::make_pair("apple", 0));

void benchmark_spinlock() {
    for (size_t i = 0; i < 10000000; ++i) {
        safe_int_spin->second++;
        //safe_int_spin->second = safe_int_spin->second + 1; // not supported
    }
}

void benchmark_recursive_spinlock() {
    for (size_t i = 0; i < 10000000; ++i) {
        safe_int_spin_recursive->second++;
        //safe_int_spin_recursive->second = safe_int_spin->second + 1;    // supported
    }
}


int main() {

    std::vector<std::thread> vec_thread(10);
    
    std::chrono::time_point<std::chrono::high_resolution_clock> start, end;

    start = std::chrono::high_resolution_clock::now();
    for (auto &i : vec_thread) i = std::move(std::thread(benchmark_spinlock));
    for (auto &i : vec_thread) i.join();
    end = std::chrono::high_resolution_clock::now();
    std::cout << "spinlock: " << std::chrono::duration<double>(end - start).count() << " s \n";

    start = std::chrono::high_resolution_clock::now();
    for (auto &i : vec_thread) i = std::move(std::thread(benchmark_recursive_spinlock));
    for (auto &i : vec_thread) i.join();
    end = std::chrono::high_resolution_clock::now();
    std::cout << "recursive_spinlock: " << std::chrono::duration<double>(end - start).count() << " s \n";

    std::cout << "Result: \n";
    std::cout << "safe_int_spin = " << safe_int_spin->second << std::endl;
    std::cout << "recursive_spinlock = " << safe_int_spin->second << std::endl;

    std::cout << "end";
    int b; std::cin >> b;

    return 0;
}