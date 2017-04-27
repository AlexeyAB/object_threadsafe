#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <map>
#include <unordered_map>
#include <array>
#include <sstream>
#include <cassert>
#include <random>
#include <iomanip>
#include <algorithm>

#define SHARED_MTX   // C++14

#ifdef SHARED_MTX
#include <shared_mutex>   // C++14
#endif


template<typename T, typename mutex_t = std::recursive_mutex, typename x_lock_t = std::unique_lock<mutex_t>,
    typename s_lock_t = std::unique_lock<mutex_t >>
    // std::shared_lock<std::shared_timed_mutex>, when mutex_t = std::shared_timed_mutex
class safe_ptr {
protected:
    const std::shared_ptr<T> ptr;   // std::experimental::propagate_const<std::shared_ptr<T>> ptr;  // C++17
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
        auto operator [] (arg_t &&arg) -> decltype((*ptr)[arg]) { return (*ptr)[arg]; }
    };

    struct no_lock_t { no_lock_t(no_lock_t &&) {} template<typename sometype> no_lock_t(sometype&) {} };
    using auto_nolock_t = auto_lock_obj_t<no_lock_t>;

    T * get_obj_ptr() const { return ptr.get(); }
    mutex_t * get_mtx_ptr() const { return mtx_ptr.get(); }

    template<typename... Args> void lock_shared() const { get_mtx_ptr()->lock_shared(); }
    template<typename... Args> void unlock_shared() const { get_mtx_ptr()->unlock_shared(); }
    void lock() const { get_mtx_ptr()->lock(); }
    void unlock() const { get_mtx_ptr()->unlock(); }
    friend struct link_safe_ptrs;
    template<typename, typename, typename, typename> friend class safe_obj;
    template<typename some_type> friend struct xlocked_safe_ptr;
    template<typename some_type> friend struct slocked_safe_ptr;
    template<typename, typename, size_t, size_t> friend class lock_timed_transaction;
#if (_WIN32 && _MSC_VER < 1900)
    template<class mutex_type> friend class std::lock_guard;  // MSVS2013 or Clang 4.0
#else
    template<class... mutex_types> friend class std::lock_guard;  // C++17 or MSVS2015
#endif
#ifdef SHARED_MTX    
    template<typename mutex_type> friend class std::shared_lock;  // C++14
#endif

public:
    template<typename... Args>
    safe_ptr(Args... args) : ptr(std::make_shared<T>(args...)), mtx_ptr(std::make_shared<mutex_t>()) {}

    auto_lock_t<x_lock_t> operator -> () { return auto_lock_t<x_lock_t>(get_obj_ptr(), *get_mtx_ptr()); }
    auto_lock_obj_t<x_lock_t> operator * () { return auto_lock_obj_t<x_lock_t>(get_obj_ptr(), *get_mtx_ptr()); }
    const auto_lock_t<s_lock_t> operator -> () const { return auto_lock_t<s_lock_t>(get_obj_ptr(), *get_mtx_ptr()); }
    const auto_lock_obj_t<s_lock_t> operator * () const { return auto_lock_obj_t<s_lock_t>(get_obj_ptr(), *get_mtx_ptr()); }

    typedef mutex_t mtx_t;
    typedef T obj_t;
    typedef x_lock_t xlock_t;
    typedef s_lock_t slock_t;
};

template<typename T> using default_safe_ptr = safe_ptr<T, std::recursive_mutex, std::unique_lock<std::recursive_mutex>, std::unique_lock<std::recursive_mutex>>;

#ifdef SHARED_MTX // C++14
template<typename T> using shared_mutex_safe_ptr = 
safe_ptr< T, std::shared_timed_mutex, std::unique_lock<std::shared_timed_mutex>, std::shared_lock<std::shared_timed_mutex> >;
#endif
// ---------------------------------------------------------------


// hide ptr
template<typename T, typename mutex_t = std::recursive_mutex, typename x_lock_t = std::unique_lock<mutex_t>,
    typename s_lock_t = std::unique_lock<mutex_t >>
class safe_hide_ptr : protected safe_ptr<T, mutex_t, x_lock_t, s_lock_t> {
public:
    template<typename... Args> safe_hide_ptr(Args... args) : safe_ptr<T, mutex_t, x_lock_t, s_lock_t>(args...) {}

    friend struct link_safe_ptrs;
    template<typename, typename, size_t, size_t> friend class lock_timed_transaction;
    template<typename some_type> friend struct xlocked_safe_ptr;
    template<typename some_type> friend struct slocked_safe_ptr;

    template<typename req_lock> using auto_lock_t = typename safe_ptr<T, mutex_t, x_lock_t, s_lock_t>::template auto_lock_t<req_lock>;
    template<typename req_lock> using auto_lock_obj_t = typename safe_ptr<T, mutex_t, x_lock_t, s_lock_t>::template auto_lock_obj_t<req_lock>;
    using auto_nolock_t = typename safe_ptr<T, mutex_t, x_lock_t, s_lock_t>::auto_nolock_t;
    typedef mutex_t mtx_t;
    typedef T obj_t;
    typedef x_lock_t xlock_t;
    typedef s_lock_t slock_t;
};

// hide obj
template<typename T, typename mutex_t = std::recursive_mutex, typename x_lock_t = std::unique_lock<mutex_t>,
    typename s_lock_t = std::unique_lock<mutex_t >>
class safe_hide_obj : protected safe_obj<T, mutex_t, x_lock_t, s_lock_t> {
public:
    template<typename... Args> safe_hide_obj(Args... args) : safe_obj<T, mutex_t, x_lock_t, s_lock_t>(args...) {}
    explicit operator T() const { return static_cast< safe_obj<T, mutex_t, x_lock_t, s_lock_t> >(*this); };

    friend struct link_safe_ptrs;
    template<typename, typename, size_t, size_t> friend class lock_timed_transaction;
    template<typename some_type> friend struct xlocked_safe_ptr;
    template<typename some_type> friend struct slocked_safe_ptr;

    template<typename req_lock> using auto_lock_t = typename safe_obj<T, mutex_t, x_lock_t, s_lock_t>::template auto_lock_t<req_lock>;
    template<typename req_lock> using auto_lock_obj_t = typename safe_obj<T, mutex_t, x_lock_t, s_lock_t>::template auto_lock_obj_t<req_lock>;
    using auto_nolock_t = typename safe_obj<T, mutex_t, x_lock_t, s_lock_t>::auto_nolock_t;
    typedef mutex_t mtx_t;
    typedef T obj_t;
    typedef x_lock_t xlock_t;
    typedef s_lock_t slock_t;
};
// ---------------------------------------------------------------

template<typename T>
struct xlocked_safe_ptr {
    T &ref_safe;
    typename T::xlock_t xlock;
    xlocked_safe_ptr(T const& p) : ref_safe(*const_cast<T*>(&p)), xlock(*(ref_safe.get_mtx_ptr())) {}// ++xp;}
    typename T::obj_t* operator -> () { return ref_safe.get_obj_ptr(); }
    typename T::auto_nolock_t operator * () { return typename T::auto_nolock_t(ref_safe.get_obj_ptr(), *ref_safe.get_mtx_ptr()); }
    operator typename T::obj_t() { return ref_safe.obj; } // only for safe_obj
};

template<typename T>
xlocked_safe_ptr<T> xlock_safe_ptr(T const& arg) { return xlocked_safe_ptr<T>(arg); }

template<typename T>
struct slocked_safe_ptr {
    T &ref_safe;
    typename T::slock_t slock;
    slocked_safe_ptr(T const& p) : ref_safe(*const_cast<T*>(&p)), slock(*(ref_safe.get_mtx_ptr())) { }//++sp;}
    typename T::obj_t const* operator -> () const { return ref_safe.get_obj_ptr(); }
    const typename T::auto_nolock_t operator * () const { return typename T::auto_nolock_t(ref_safe.get_obj_ptr(), *ref_safe.get_mtx_ptr()); }
    operator typename T::obj_t() const { return ref_safe.obj; } // only for safe_obj
};

template<typename T>
slocked_safe_ptr<T> slock_safe_ptr(T const& arg) { return slocked_safe_ptr<T>(arg); }
// ---------------------------------------------------------------

// contention free shared mutex (same-lock-type is recursive for X->X, X->S or S->S locks), but (S->X - is UB)
template<unsigned contention_free_count = 36, bool shared_flag = false>
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

#if (_WIN32 && _MSC_VER < 1900) // only for MSVS 2013
	typedef int64_t thread_id_t;
	std::atomic<thread_id_t> owner_thread_id;
	std::array<int64_t, contention_free_count> register_thread_array;
	int64_t get_fast_this_thread_id() {
		static __declspec(thread) int64_t fast_this_thread_id = 0;  // MSVS 2013 thread_local partially supported - only POD
		if (fast_this_thread_id == 0) {
			std::stringstream ss;
			ss << std::this_thread::get_id();   // https://connect.microsoft.com/VisualStudio/feedback/details/1558211
			fast_this_thread_id = std::stoll(ss.str());
		}
		return fast_this_thread_id;
	}

	int get_or_set_index(index_op_t index_op = get_index_op, int set_index = -1) {
		if (index_op == get_index_op) {  // get index
			auto const thread_id = get_fast_this_thread_id();

			for (size_t i = 0; i < register_thread_array.size(); ++i) {
				if (register_thread_array[i] == thread_id) {
					set_index = i;   // thread already registred                
					break;
				}
			}
		}
		else if (index_op == register_thread_op) {  // register thread
			register_thread_array[set_index] = get_fast_this_thread_id();
		}
		return set_index;
	}

#else
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

#endif

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
				//std::cout << "\n thread_id = " << std::this_thread::get_id() << ", register_thread_index = " << cur_index <<
				//    ", shared_locks_array[cur_index].value = " << shared_locks_array[cur_index].value << std::endl;
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
				owner_thread_id.store(thread_id_t(), std::memory_order_release);
				want_x_lock.store(false, std::memory_order_release);
			}
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
			owner_thread_id.store(thread_id_t(), std::memory_order_release);
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

using default_contention_free_shared_mutex = contention_free_shared_mutex<>;

template<typename T> using contfree_safe_ptr = safe_ptr<T, contention_free_shared_mutex<>,
    std::unique_lock<contention_free_shared_mutex<>>, shared_lock_guard<contention_free_shared_mutex<>> >;
// ---------------------------------------------------------------

struct field_t { int money, time; field_t(int m, int t) : money(m), time(t) {} field_t() : money(0), time(0) {} };


// container-1 (sequential 1-thread & in parallel multi-thread)
std::map<int, field_t> map_global;
std::mutex mtx_map;


// container-2
safe_ptr< std::map<int, field_t> > safe_map_mutex_global;


// container-3
#ifdef SHARED_MTX
shared_mutex_safe_ptr< std::map<int, field_t> > safe_map_shared_mutex_global;
#endif

// container-4
contfree_safe_ptr< std::map<int, field_t> > safe_map_contfree_global;


enum { insert_op, delete_op, update_op, read_op };
std::uniform_int_distribution<size_t> percent_distribution(1, 100);    // 1 - 100 %


safe_ptr<std::vector<double>> safe_vec_max_latency;
static const size_t median_array_size = 1000000;
safe_ptr<std::vector<double>> safe_vec_median_latency;

// for container-1
template<typename T>
void benchmark_std_map(T &test_map, size_t const iterations_count,
    size_t const percent_write, std::function<void(void)> burn_cpu, const bool measure_latency = false)
{
    const unsigned int seed = (unsigned)std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<size_t> index_distribution(0, test_map.size() - 1);
    std::chrono::high_resolution_clock::time_point hrc_end, hrc_start = std::chrono::high_resolution_clock::now();
    double max_time = 0;
    std::vector<double> median_arr;
    
    for (size_t i = 0; i < iterations_count; ++i) {
        int const rnd_index = index_distribution(generator);
        bool const write_flag = (percent_distribution(generator) < percent_write);
        int const num_op = (write_flag) ? i % 3 : read_op;   // (insert_op, update_op, delete_op), read_op
        
        if (measure_latency) {
            hrc_end = std::chrono::high_resolution_clock::now();
            const double cur_time = std::chrono::duration<double>(hrc_end - hrc_start).count();
            max_time = std::max(max_time, cur_time);
            if (median_arr.size() == 0) median_arr.resize(std::min(median_array_size, iterations_count));
            if (i < median_arr.size()) median_arr[i] = cur_time;            
            hrc_start = std::chrono::high_resolution_clock::now();
        }

        std::lock_guard<std::mutex> lock(mtx_map);

        switch (num_op) {
        case insert_op:
            burn_cpu(); // do some work with the data exchange
            test_map.emplace(rnd_index, field_t(rnd_index, rnd_index));
            break;
        case delete_op: {
            burn_cpu(); // do some work with the data exchange
            size_t erased_elements = test_map.erase(rnd_index);
        }
            break;
        case update_op:  {
            auto it = test_map.find(rnd_index);
            if (it != test_map.cend()) {
                it->second.money += rnd_index;
                burn_cpu(); // do some work with the data exchange
            }
        }
            break;
        case read_op: {
            // don't use at() with exception - very slow: try { auto field = test_map.at(rnd_index); } catch (...) {}
            auto it = test_map.find(rnd_index);
            if (it != test_map.cend()) {
                volatile int money = it->second.money;
                burn_cpu(); // do some work with the data exchange
            }
        }
            break;
        default: std::cout << "\n wrong way! \n";  break;
        }        
    }
    safe_vec_max_latency->push_back(max_time);
    safe_vec_median_latency->insert(safe_vec_median_latency->end(), median_arr.begin(), median_arr.end());
}


// for containers: 2, 3, 4
template<typename T>
void benchmark_safe_ptr(T safe_map, size_t const iterations_count,
    size_t const percent_write, std::function<void(void)> burn_cpu, const bool measure_latency = false)
{
    const unsigned int seed = (unsigned)std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<size_t> index_distribution(0, safe_map->size() - 1);
    std::chrono::high_resolution_clock::time_point hrc_end, hrc_start = std::chrono::high_resolution_clock::now();
    double max_time = 0;
    std::vector<double> median_arr;

    for (size_t i = 0; i < iterations_count; ++i) {
        int const rnd_index = index_distribution(generator);
        bool const write_flag = (percent_distribution(generator) < percent_write);
        int const num_op = (write_flag) ? i % 3 : read_op;   // (insert_op, update_op, delete_op), read_op

        if (measure_latency) {
            hrc_end = std::chrono::high_resolution_clock::now();
            const double cur_time = std::chrono::duration<double>(hrc_end - hrc_start).count();
            max_time = std::max(max_time, cur_time);
            if (median_arr.size() == 0) median_arr.resize(std::min(median_array_size, iterations_count));
            if (i < median_arr.size()) median_arr[i] = cur_time;
            hrc_start = std::chrono::high_resolution_clock::now();
        }

        switch (num_op) {
        case insert_op:
            safe_map->emplace(rnd_index, (field_t(rnd_index, rnd_index)));
            burn_cpu(); // do some work with the data exchange
            break;
        case delete_op: {
            size_t erased_elements = safe_map->erase(rnd_index);
            burn_cpu(); // do some work with the data exchange
        }
            break;
        case update_op: {
            auto x_safe_map = xlock_safe_ptr(safe_map);
            auto it = x_safe_map->find(rnd_index);
            if (it != x_safe_map->cend()) {
                it->second.money += rnd_index;   // X-lock on Table (must necessarily be)
                burn_cpu(); // do some work with the data exchange
            }
        }
            break;
        case read_op: {
            auto s_safe_map = slock_safe_ptr(safe_map);
            auto it = s_safe_map->find(rnd_index);
            if (it != s_safe_map->cend()) {
                volatile int money = it->second.money;   // S-lock on Table (must necessarily be)
                burn_cpu(); // do some work with the data exchange
            }
        }
            break;
        default: std::cout << "\n wrong way! \n";  break;
        }
    }
    safe_vec_max_latency->push_back(max_time);
    safe_vec_median_latency->insert(safe_vec_median_latency->end(), median_arr.begin(), median_arr.end());
}



int main(int argc, char** argv) {

    const size_t iterations_count = 2000000;    // operation of data exchange between threads
    const size_t container_size = 100000;       // elements in container
    std::vector<std::thread> vec_thread(std::thread::hardware_concurrency());    // threads number
    const bool measure_latency = false;         // measure latency time for each operation (Max, Median)

    std::function<void(void)> burn_cpu = []() {};// for (volatile int i = 0; i < 0; ++i);};
    
	if (argc >= 2) {
		vec_thread.resize(std::stoi(std::string(argv[1])));		// max threads
	}

    std::cout << "CPU Cores: " << std::thread::hardware_concurrency() << std::endl;
    std::cout << "Benchmark thread-safe associative containers with size = " << container_size << std::endl;
    std::cout << "Threads = " << vec_thread.size() << ", iterations per thread = " << iterations_count << std::endl;
    std::cout << "Time & MOps - steady_clock is_steady = " << std::chrono::steady_clock::is_steady << ", num/den = " << 
        (double)std::chrono::steady_clock::period::num << " / " << std::chrono::steady_clock::period::den << std::endl;
    std::cout << "Latency     - high_resolution_clock is_steady = " << std::chrono::high_resolution_clock::is_steady << ", num/den = " <<
        (double)std::chrono::high_resolution_clock::period::num << " / " << std::chrono::high_resolution_clock::period::den << std::endl;

    std::chrono::steady_clock::time_point steady_start, steady_end;
    double took_time = 0;

    steady_start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < 1000000; ++i)
        burn_cpu();

    steady_end = std::chrono::steady_clock::now();
    std::cout << "burn_cpu() for each multithread operations took: " << 
        std::chrono::duration<double>(steady_end - steady_start).count()*1000 << " nano-sec \n";
    std::cout << std::endl;


    std::cout << "Filling of containers... ";
    try {
        for (size_t i = 0; i < container_size; ++i)
        {
            map_global.emplace(i, field_t(i, i));
            safe_map_mutex_global->emplace(i, field_t(i, i));
            safe_map_contfree_global->emplace(i, field_t(i, i));
#ifdef SHARED_MTX
            safe_map_shared_mutex_global->emplace(i, field_t(i, i));
#endif
        }
    }
    catch (std::runtime_error &e) { std::cerr << "\n exception - std::runtime_error = " << e.what() << std::endl; }
    catch (...) { std::cerr << "\n unknown exception \n"; }
    std::cout << "filled containers." << std::endl;

    //for (size_t thread_num = 1; thread_num <= std::thread::hardware_concurrency(); thread_num *= 2)
    //{
        //vec_thread.resize(thread_num);

    // % of write operations(insert, delete, update)
	for (size_t percent_write = 0; percent_write <= 90; percent_write += 15)
	{
		std::cout << std::endl << percent_write << "\t % of write operations (1/3 insert, 1/3 delete, 1/3 update) " << std::endl;
		std::cout << "                                        (1 Operation latency, usec)" << std::endl;;
		std::cout << "               \t     time, sec \t MOps \t Median\t Min \t Max " << std::endl;
		std::cout << std::setprecision(3);


		std::cout << "std::map & std::mutex:";
		steady_start = std::chrono::steady_clock::now();
		for (auto &i : vec_thread) i = std::move(std::thread([&]() {
			benchmark_std_map(map_global, iterations_count, percent_write, burn_cpu, measure_latency);
		}));
		for (auto &i : vec_thread) i.join();
		steady_end = std::chrono::steady_clock::now();
		took_time = std::chrono::duration<double>(steady_end - steady_start).count();
		std::cout << "\t" << took_time << " \t" << (vec_thread.size() * iterations_count / (took_time * 1000000));
		if (measure_latency) {
			std::sort(safe_vec_median_latency->begin(), safe_vec_median_latency->end());
			std::cout << " \t " << (safe_vec_median_latency->at(safe_vec_median_latency->size() / 2) * 1000000) <<
				" \t " << (safe_vec_median_latency->at(5) * 1000000) <<
				" \t " << *std::max_element(safe_vec_max_latency->begin(), safe_vec_max_latency->end()) * 1000000;
		}
		std::cout << std::endl;
		safe_vec_max_latency->clear();
		safe_vec_median_latency->clear();




		std::cout << "safe_ptr<map,mutex>:";
		steady_start = std::chrono::steady_clock::now();
		for (auto &i : vec_thread) i = std::move(std::thread([&]() {
			benchmark_safe_ptr(safe_map_mutex_global, iterations_count, percent_write, burn_cpu, measure_latency);
		}));
		for (auto &i : vec_thread) i.join();
		steady_end = std::chrono::steady_clock::now();
		took_time = std::chrono::duration<double>(steady_end - steady_start).count();
		std::cout << "\t" << took_time << " \t" << (vec_thread.size() * iterations_count / (took_time * 1000000));
		if (measure_latency) {
			std::sort(safe_vec_median_latency->begin(), safe_vec_median_latency->end());
			std::cout << " \t " << (safe_vec_median_latency->at(safe_vec_median_latency->size() / 2) * 1000000) <<
				" \t " << (safe_vec_median_latency->at(5) * 1000000) <<
				" \t " << *std::max_element(safe_vec_max_latency->begin(), safe_vec_max_latency->end()) * 1000000;
		}
		std::cout << std::endl;
		safe_vec_max_latency->clear();
		safe_vec_median_latency->clear();


#ifdef SHARED_MTX
		std::cout << "safe_ptr<map,shared>:";
		steady_start = std::chrono::steady_clock::now();
		for (auto &i : vec_thread) i = std::move(std::thread([&]() {
			benchmark_safe_ptr(safe_map_shared_mutex_global, iterations_count, percent_write, burn_cpu, measure_latency);
		}));
		for (auto &i : vec_thread) i.join();
		steady_end = std::chrono::steady_clock::now();
		took_time = std::chrono::duration<double>(steady_end - steady_start).count();
		std::cout << "\t" << took_time << " \t" << (vec_thread.size() * iterations_count / (took_time * 1000000));
		if (measure_latency) {
			std::sort(safe_vec_median_latency->begin(), safe_vec_median_latency->end());
			std::cout << " \t " << (safe_vec_median_latency->at(safe_vec_median_latency->size() / 2) * 1000000) <<
				" \t " << (safe_vec_median_latency->at(5) * 1000000) <<
				" \t " << *std::max_element(safe_vec_max_latency->begin(), safe_vec_max_latency->end()) * 1000000;
		}
		std::cout << std::endl;
		safe_vec_max_latency->clear();
		safe_vec_median_latency->clear();
#endif

		std::cout << "safe_ptr<map,contfree>:";
		steady_start = std::chrono::steady_clock::now();
		for (auto &i : vec_thread) i = std::move(std::thread([&]() {
			benchmark_safe_ptr(safe_map_contfree_global, iterations_count, percent_write, burn_cpu, measure_latency);
		}));
		for (auto &i : vec_thread) i.join();
		steady_end = std::chrono::steady_clock::now();
		took_time = std::chrono::duration<double>(steady_end - steady_start).count();
		std::cout << "\t" << took_time << " \t" << (vec_thread.size() * iterations_count / (took_time * 1000000));
		if (measure_latency) {
			std::sort(safe_vec_median_latency->begin(), safe_vec_median_latency->end());
			std::cout << " \t " << (safe_vec_median_latency->at(safe_vec_median_latency->size() / 2) * 1000000) <<
				" \t " << (safe_vec_median_latency->at(5) * 1000000) <<
				" \t " << *std::max_element(safe_vec_max_latency->begin(), safe_vec_max_latency->end()) * 1000000;
		}
		std::cout << std::endl;
		safe_vec_max_latency->clear();
		safe_vec_median_latency->clear();

	}
	    
    std::cout << "end"; 
    int b; std::cin >> b;

    return 0;
}