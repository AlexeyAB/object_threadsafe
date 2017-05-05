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

//#define SHARED_MTX   // C++14

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
    template<class mutex_type> friend class std::lock_guard;  // MSVS2013 or Clang 4.0
    //template<class... mutex_types> friend class std::lock_guard;  // C++17 or MSVS2015
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

template<typename T, typename mutex_t = std::recursive_mutex, typename x_lock_t = std::unique_lock<mutex_t>,
    typename s_lock_t = std::unique_lock<mutex_t >>
class safe_obj {
protected:
    T obj;
    mutable mutex_t mtx;

    T * get_obj_ptr() const { return const_cast<T*>(&obj); }
    mutex_t * get_mtx_ptr() const { return &mtx; }

    template<typename req_lock> using auto_lock_t = typename safe_ptr<T, mutex_t, x_lock_t, s_lock_t>::template auto_lock_t<req_lock>;
    template<typename req_lock> using auto_lock_obj_t = typename safe_ptr<T, mutex_t, x_lock_t, s_lock_t>::template auto_lock_obj_t<req_lock>;
    using auto_nolock_t = typename safe_ptr<T, mutex_t, x_lock_t, s_lock_t>::auto_nolock_t;
    template<typename some_type> friend struct xlocked_safe_ptr;
    template<typename some_type> friend struct slocked_safe_ptr;
public:
    template<typename... Args>
    safe_obj(Args... args) : obj(args...) {}
    safe_obj(safe_obj const& safe_obj) { std::lock_guard<mutex_t> lock(safe_obj.mtx); obj = safe_obj.obj; }
    explicit operator T() const { s_lock_t lock(mtx); T obj_tmp = obj; return obj_tmp; };

    auto_lock_t<x_lock_t> operator -> () { return auto_lock_t<x_lock_t>(get_obj_ptr(), *get_mtx_ptr()); }
    auto_lock_obj_t<x_lock_t> operator * () { return auto_lock_obj_t<x_lock_t>(get_obj_ptr(), *get_mtx_ptr()); }
    const auto_lock_t<s_lock_t> operator -> () const { return auto_lock_t<s_lock_t>(get_obj_ptr(), *get_mtx_ptr()); }
    const auto_lock_obj_t<s_lock_t> operator * () const { return auto_lock_obj_t<s_lock_t>(get_obj_ptr(), *get_mtx_ptr()); }

    typedef mutex_t mtx_t;
    typedef T obj_t;
    typedef x_lock_t xlock_t;
    typedef s_lock_t slock_t;
};
// ---------------------------------------------------------------

template<typename T, typename mutex_t = std::recursive_mutex, typename x_lock_t = std::unique_lock<mutex_t>,
    typename s_lock_t = std::unique_lock<mutex_t >>
struct safe_hide_ptr : protected safe_ptr<T, mutex_t, x_lock_t, s_lock_t> {
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

template<typename T, typename mutex_t = std::recursive_mutex, typename x_lock_t = std::unique_lock<mutex_t>,
    typename s_lock_t = std::unique_lock<mutex_t >>
struct safe_hide_obj : protected safe_obj<T, mutex_t, x_lock_t, s_lock_t> {
    template<typename... Args> safe_hide_obj(Args... args) : safe_obj<T, mutex_t, x_lock_t, s_lock_t>(args...) {}
    explicit operator T() const { return static_cast< safe_obj<T, mutex_t, x_lock_t, s_lock_t> >( *this ); };
  
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

class spinlock_t {
    std::atomic_flag lock_flag;
public:
    spinlock_t() { lock_flag.clear(); }

    bool try_lock() { return !lock_flag.test_and_set(std::memory_order_acquire); }
    void lock() { for (volatile size_t i = 0; !try_lock(); ++i) if (i % 10000 == 0) std::this_thread::yield(); }
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
				owner_thread_id.store(decltype(owner_thread_id)(), std::memory_order_release);
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

using default_contention_free_shared_mutex = contention_free_shared_mutex<>;

template<typename T> using contfree_safe_ptr = safe_ptr<T, contention_free_shared_mutex<>,
    std::unique_lock<contention_free_shared_mutex<>>, shared_lock_guard<contention_free_shared_mutex<>> >;
    
template<typename T> using contfree_safe_hide_ptr = safe_hide_ptr<T, contention_free_shared_mutex<>,
    std::unique_lock<contention_free_shared_mutex<>>, shared_lock_guard<contention_free_shared_mutex<>> >;    
// ---------------------------------------------------------------



struct field_t { int money, time; field_t(int m, int t) : money(m), time(t) {} field_t() : money(0), time(0) {} };
typedef safe_hide_obj<field_t, spinlock_t> safe_obj_field_t;


//contfree_safe_ptr< std::map<int, safe_obj_field_t> > safe_map_contfree_rowlock_global;
contfree_safe_hide_ptr< std::map<int, safe_obj_field_t> > safe_map_contfree_rowlock_global;



template<typename T>
void benchmark_safe_ptr_rowlock(T safe_map, size_t const iterations_count, size_t const container_size)
{
    const unsigned int seed = (unsigned)std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<size_t> index_distribution(0, container_size);
                
    enum { insert_op, delete_op, update_op, read_op };
    std::uniform_int_distribution<size_t> operation_distribution(insert_op, read_op);    // 0 - 4

    for (size_t i = 0; i < iterations_count; ++i) 
    {
        int const rnd_index = index_distribution(generator);    // 0 - container_size
        int const num_op = operation_distribution(generator);   // insert_op, update_op, delete_op, read_op

        switch (num_op) {
        case insert_op: {
            xlock_safe_ptr(safe_map)->emplace(rnd_index, (field_t(rnd_index, rnd_index)));  // insert with X-lock on Table
            break;
        }
        case delete_op: {
            size_t erased_elements = xlock_safe_ptr(safe_map)->erase(rnd_index);    // erase with X-lock on Table
        }
            break;
        case update_op: {
            auto s_safe_map = slock_safe_ptr(safe_map); // S-lock on Table
            auto it = s_safe_map->find(rnd_index);
            if (it != s_safe_map->cend()) {
                auto x_field = xlock_safe_ptr(it->second);
                x_field->money += rnd_index;   // X-lock on field, still S-lock on Table (must necessarily be)
            }
        }
            break;
        case read_op: {
            auto s_safe_map = slock_safe_ptr(safe_map); // S-lock on Table
            auto it = s_safe_map->find(rnd_index);
            if (it != s_safe_map->cend()) {
                auto s_field = slock_safe_ptr(it->second);
                volatile int money = s_field->money;   // S-lock on field, still S-lock on Table (must necessarily be)
                // volatile here only to avoid optimization for unused money-variable
            }
        }
            break;
        default: std::cout << "\n wrong way! \n";  break;
        }
    }
}


int main() {

    const size_t iterations_count = 100000;     // operation of data exchange between threads
    const size_t container_size = 100000;       // elements in container
    std::vector<std::thread> vec_thread(8);    // threads number
    std::cout << "Threads: " << vec_thread.size() << std::endl;


    std::cout << "safe<map,contf>rowlock: \n"; 
    for (auto &i : vec_thread) i = std::move(std::thread([&]() {
        benchmark_safe_ptr_rowlock(safe_map_contfree_rowlock_global, iterations_count, container_size);
    }));
    for (auto &i : vec_thread) i.join();

    // safe_hide_ptr & safe_hide_obj
    {
        safe_hide_obj<field_t, spinlock_t> field_hide_tmp;
        //safe_obj<field_t, spinlock_t> &field_tmp = field_hide_tmp;    // conversion denied - compile-time error     
        
        //field_hide_tmp->money = 10;    // access denied - compile-time error
        auto x_field = xlock_safe_ptr(field_hide_tmp);  // locked until x_field is alive
        x_field->money = 10;            // access granted
    }

    std::cout << "end";

    return 0;
}