#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <list>
#include <memory>
#include <sstream>
#include <mutex>
#include <thread>
#include <map>
#include <unordered_map>
#include <array>
#include <atomic>
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
        auto_lock_t(T * const _ptr, mutex_t& _mtx) : ptr(_ptr), lock(_mtx) {}
        T* operator -> () { return ptr; }
        const T* operator -> () const { return ptr; }
    };

    template<typename req_lock>
    class auto_lock_obj_t {
        T * const ptr;
        req_lock lock;
    public:
        auto_lock_obj_t(auto_lock_obj_t&& o) : ptr(std::move(o.ptr)), lock(std::move(o.lock)) { }
        auto_lock_obj_t(T * const _ptr, mutex_t& _mtx) : ptr(_ptr), lock(_mtx) {}
        template<typename arg_t>
        auto operator [] (arg_t arg) -> decltype((*ptr)[arg]) { return (*ptr)[arg]; }
    };

    void lock() { mtx_ptr->lock(); }
    void unlock() { mtx_ptr->unlock(); }
    friend struct link_safe_ptrs;
    template<typename, typename, size_t, size_t> friend class lock_timed_transaction;
    //template<typename mutex_type> friend class std::lock_guard;   // MSVC 2013
    template<class... mutex_types> friend class std::lock_guard;    // C++17, MSVC 2015
    public:
        template<typename... Args>
        safe_ptr(Args... args) : ptr(std::make_shared<T>(args...)), mtx_ptr(std::make_shared<mutex_t>()) {}

        auto_lock_t<x_lock_t> operator -> () { return auto_lock_t<x_lock_t>(ptr.get(), *mtx_ptr); }
        auto_lock_obj_t<x_lock_t> operator * () { return auto_lock_obj_t<x_lock_t>(ptr.get(), *mtx_ptr); }
        const auto_lock_t<s_lock_t> operator -> () const { return auto_lock_t<s_lock_t>(ptr.get(), *mtx_ptr); }
        const auto_lock_obj_t<s_lock_t> operator * () const { return auto_lock_obj_t<s_lock_t>(ptr.get(), *mtx_ptr); }
};
// ---------------------------------------------------------------

// contention free shared mutex (same-lock-type is recursive for X->X, X->S or S->S locks), but (S->X - is UB)
template<unsigned contention_free_count = 20, bool shared_flag = false>
class contention_free_shared_mutex {
	std::atomic<bool> want_x_lock;
	//struct cont_free_flag_t { alignas(std::hardware_destructive_interference_size) std::atomic<int> value; cont_free_flag_t() { value = 0; } }; // C++17
	struct cont_free_flag_t { char tmp[60]; std::atomic<int> value; cont_free_flag_t() { value = 0; } };   // tmp[] to avoid false sharing
	typedef std::array<cont_free_flag_t, contention_free_count> array_slock_t;

	const std::shared_ptr<array_slock_t> shared_locks_array_ptr;  // 0 - unregistred, 1 registred & free, 2... - busy
	char avoid_falsesharing_1[64];

	array_slock_t &shared_locks_array;
	char avoid_falsesharing_2[64];

	int recursive_xlock_count;


	enum index_op_t { unregister_thread_op, get_index_op, register_thread_op };

	typedef std::thread::id thread_id_t;
	std::atomic<std::thread::id> owner_thread_id;
	std::thread::id get_fast_this_thread_id() { return std::this_thread::get_id(); }

	struct unregister_t {
		int thread_index;
		std::shared_ptr<array_slock_t> array_slock_ptr;
		unregister_t(int index, std::shared_ptr<array_slock_t> const& ptr) : thread_index(index), array_slock_ptr(ptr) {}
		unregister_t(unregister_t &&src) : thread_index(src.thread_index), array_slock_ptr(std::move(src.array_slock_ptr)) {}
		~unregister_t() { if (array_slock_ptr.use_count() > 0) (*array_slock_ptr)[thread_index].value--; }
	};

	int get_or_set_index(index_op_t index_op = get_index_op, int set_index = -1) {
		thread_local static std::unordered_map<void *, unregister_t> thread_local_index_hashmap;
		// get thread index - in any cases
		auto it = thread_local_index_hashmap.find(this);
		if (it != thread_local_index_hashmap.cend())
			set_index = it->second.thread_index;

		if (index_op == unregister_thread_op) {  // unregister thread
			if (shared_locks_array[set_index].value == 1) // if isn't shared_lock now
				thread_local_index_hashmap.erase(this);
			else
				return -1;
		}
		else if (index_op == register_thread_op) {  // register thread
			thread_local_index_hashmap.emplace(this, unregister_t(set_index, shared_locks_array_ptr));

			// remove info about deleted contfree-mutexes
			for (auto it = thread_local_index_hashmap.begin(), ite = thread_local_index_hashmap.end(); it != ite;) {
				if (it->second.array_slock_ptr->at(it->second.thread_index).value < 0)    // if contfree-mtx was deleted
					it = thread_local_index_hashmap.erase(it);
				else
					++it;
			}
		}
		return set_index;
	}

public:
	contention_free_shared_mutex() :
		shared_locks_array_ptr(std::make_shared<array_slock_t>()), shared_locks_array(*shared_locks_array_ptr), want_x_lock(false), recursive_xlock_count(0),
		owner_thread_id(thread_id_t()) {}

	~contention_free_shared_mutex() {
		for (auto &i : shared_locks_array) i.value = -1;
	}


	bool unregister_thread() { return get_or_set_index(unregister_thread_op) >= 0; }

	int register_thread() {
		int cur_index = get_or_set_index();

		if (cur_index == -1) {
			if (shared_locks_array_ptr.use_count() <= (int)shared_locks_array.size())  // try once to register thread
			{
				for (size_t i = 0; i < shared_locks_array.size(); ++i) {
					int unregistred_value = 0;
					if (shared_locks_array[i].value == 0)
						if (shared_locks_array[i].value.compare_exchange_strong(unregistred_value, 1)) {
							cur_index = i;
							get_or_set_index(register_thread_op, cur_index);   // thread registred success
							break;
						}
				}
				std::stringstream ss;
				ss << "\n thread_id = " << std::this_thread::get_id() << ", register_thread_index = " << cur_index <<
				    ", shared_locks_array[cur_index].value = " << shared_locks_array[cur_index].value << std::endl;
				std::cout << ss.str();
			}
		}
		return cur_index;
	}

	void lock_shared() {
		int const register_index = register_thread();

		if (register_index >= 0) {
			int recursion_depth = shared_locks_array[register_index].value.load(std::memory_order_acquire);
			assert(recursion_depth >= 1);

			if (recursion_depth > 1)
				shared_locks_array[register_index].value.store(recursion_depth + 1, std::memory_order_release); // if recursive -> release
			else {
				shared_locks_array[register_index].value.store(recursion_depth + 1, std::memory_order_seq_cst); // if first -> sequential
				while (want_x_lock.load(std::memory_order_seq_cst)) {
					shared_locks_array[register_index].value.store(recursion_depth, std::memory_order_seq_cst);
					for (volatile size_t i = 0; want_x_lock.load(std::memory_order_seq_cst); ++i) if (i % 100000 == 0) std::this_thread::yield();
					shared_locks_array[register_index].value.store(recursion_depth + 1, std::memory_order_seq_cst);
				}
			}
			// (shared_locks_array[register_index] == 2 && want_x_lock == false) ||     // first shared lock
			// (shared_locks_array[register_index] > 2)                                 // recursive shared lock
		}
		else {
			if (owner_thread_id.load(std::memory_order_acquire) != get_fast_this_thread_id()) {
				size_t i = 0;
				for (bool flag = false; !want_x_lock.compare_exchange_weak(flag, true, std::memory_order_seq_cst); flag = false)
					if (++i % 100000 == 0) std::this_thread::yield();
				owner_thread_id.store(get_fast_this_thread_id(), std::memory_order_release);
			}
			++recursive_xlock_count;
			std::cout << "lock_shared as lock exclusive \n";
		}
	}

	void unlock_shared() {
		int const register_index = get_or_set_index();

		if (register_index >= 0) {
			int const recursion_depth = shared_locks_array[register_index].value.load(std::memory_order_acquire);
			assert(recursion_depth > 1);

			shared_locks_array[register_index].value.store(recursion_depth - 1, std::memory_order_release);
		}
		else {
			if (--recursive_xlock_count == 0) {
				owner_thread_id.store(decltype(owner_thread_id)(), std::memory_order_release);
				want_x_lock.store(false, std::memory_order_release);
			}
			std::cout << "unlock_shared as unlock exclusive \n";
		}
	}

	void lock() {
		// forbidden upgrade S-lock to X-lock - this is an excellent opportunity to get deadlock
		int const register_index = get_or_set_index();
		if (register_index >= 0)
			assert(shared_locks_array[register_index].value.load(std::memory_order_acquire) == 1);

		if (owner_thread_id.load(std::memory_order_acquire) != get_fast_this_thread_id()) {
			size_t i = 0;
			for (bool flag = false; !want_x_lock.compare_exchange_weak(flag, true, std::memory_order_seq_cst); flag = false)
				if (++i % 1000000 == 0) std::this_thread::yield();

			owner_thread_id.store(get_fast_this_thread_id(), std::memory_order_release);

			for (auto &i : shared_locks_array)
				while (i.value.load(std::memory_order_seq_cst) > 1);
		}

		++recursive_xlock_count;
	}

	void unlock() {
		assert(recursive_xlock_count > 0);
		if (--recursive_xlock_count == 0) {
			owner_thread_id.store(decltype(owner_thread_id)(), std::memory_order_release);
			want_x_lock.store(false, std::memory_order_release);
		}
	}
};

template<typename mutex_t>
struct shared_lock_guard {
    mutex_t &ref_mtx;
    shared_lock_guard(mutex_t &mtx) : ref_mtx(mtx) { ref_mtx.lock_shared(); }
    ~shared_lock_guard() { ref_mtx.unlock_shared(); }
};

template<typename T> using contfree_safe_ptr = safe_ptr<T, contention_free_shared_mutex<>,
    std::unique_lock<contention_free_shared_mutex<>>, shared_lock_guard<contention_free_shared_mutex<>> >;
// ---------------------------------------------------------------


contfree_safe_ptr< std::map<std::string, int> > safe_map_strings_global;   // cont-free shared-mutex


//safe_ptr<std::map<std::string, std::pair<std::string, int> >> safe_map_strings_global;    // std::mutex


template<typename T>
void func(contfree_safe_ptr<T> safe_map_strings)
{
    contfree_safe_ptr<T> const &readonly_safe_map_string = safe_map_strings;    // read-only (shared lock during access)
    
    for (size_t i = 0; i < 1000000; ++i)  
    {
        assert(readonly_safe_map_string->at("apple") == readonly_safe_map_string->at("potato"));    // two Shared locks (recursive)

        //std::lock_guard<decltype(safe_map_strings)> lock(safe_map_strings); // eXclusive lock
        //safe_map_strings->at("apple") += 1;
        //safe_map_strings->find("potato")->second += 1;
    }
}

int main() {
    // 1 thread - main()    

    (*safe_map_strings_global)["apple"] = 0;
    (*safe_map_strings_global)["potato"] = 0;

    // 19 threads
    std::vector<std::thread> vec_thread(19);
    for (auto &i : vec_thread) i = std::move(std::thread([&]() { func(safe_map_strings_global); }));
    for (auto &i : vec_thread) i.join();

    // 19 threads
    for (auto &i : vec_thread) i = std::move(std::thread([&]() { func(safe_map_strings_global); }));
    for (auto &i : vec_thread) i.join(); 


    return 0; 
}