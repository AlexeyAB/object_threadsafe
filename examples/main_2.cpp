#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <map>

template<typename T, typename mutex_t = std::recursive_mutex, typename x_lock_t = std::unique_lock<mutex_t>,
    typename s_lock_t = std::unique_lock<mutex_t >>
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
    template<size_t, typename, size_t, size_t> friend class lock_timed_any;
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

enum lock_count_t { lock_once, lock_infinity };

template<size_t lock_count, typename duration = std::chrono::nanoseconds,
    size_t deadlock_timeout = 100000, size_t spin_iterations = 100>
class lock_timed_any {
    std::vector<std::shared_ptr<void>> locks_ptr_vec;
    bool success;
    
    template<typename mtx_t>
    std::unique_lock<mtx_t> try_lock_one(mtx_t &mtx) const {
        std::unique_lock<mtx_t> lock(mtx, std::defer_lock_t());
        for (size_t i = 0; i < spin_iterations; ++i) if (lock.try_lock()) return lock;
        const std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
        //while (!lock.try_lock_for(duration(deadlock_timeout)))    // only for timed mutexes
        while (!lock.try_lock()) {
            auto const time_remained = duration(deadlock_timeout) - std::chrono::duration_cast<duration>(std::chrono::steady_clock::now() - start_time);
            if (time_remained <= duration(0))
                break;
            else 
                std::this_thread::sleep_for(time_remained);
        }
        return lock;
    }

    template<typename mtx_t>
    std::shared_ptr<std::unique_lock<mtx_t>> try_lock_ptr_one(mtx_t &mtx) const {
        return std::make_shared<std::unique_lock<mtx_t>>(try_lock_one(mtx));
    }

public:
    template<typename... Args>
    lock_timed_any(Args& ...args) {
        do {
            success = true;
            for (auto &lock_ptr : { try_lock_ptr_one(*args.mtx_ptr.get()) ... }) {
                locks_ptr_vec.emplace_back(lock_ptr);
                if (!lock_ptr->owns_lock()) {
                    success = false;
                    locks_ptr_vec.clear();
                    std::this_thread::sleep_for(duration(deadlock_timeout));
                    break;
                }
            }
        } while (!success && lock_count == lock_count_t::lock_infinity);
    }

    explicit operator bool() const throw() { return success; }
    lock_timed_any(lock_timed_any&& other) throw() : locks_ptr_vec(other.locks_ptr_vec) { }
    lock_timed_any(const lock_timed_any&) = delete;
    lock_timed_any& operator=(const lock_timed_any&) = delete;
};

using lock_timed_any_once = lock_timed_any<lock_count_t::lock_once>;
using lock_timed_any_infinity = lock_timed_any<lock_count_t::lock_infinity>;
// ---------------------------------------------------------------


template<typename T>
void func(safe_ptr<T> safe_map_strings_1, safe_ptr<T> safe_map_strings_2) {
    for (size_t i = 0; i < 10000; ++i) {
        // only for example
        if(i % 2 == 0) {
            lock_timed_any_infinity lock_all(safe_map_strings_1, safe_map_strings_2);
            safe_map_strings_1->at("apple")++;
            safe_map_strings_2->at("potato")++;            
        } else {
            lock_timed_any_infinity lock_all(safe_map_strings_2, safe_map_strings_1);
            safe_map_strings_1->at("apple")++;
            safe_map_strings_2->at("potato")++;
        }
    }

    std::cout << "apple is " << safe_map_strings_1->at("apple") <<
        ", potato is " << safe_map_strings_2->at("potato") << std::endl;
}

int main() {    
    safe_ptr< std::map<std::string, int> > safe_map_strings_1, safe_map_strings_2;    
    (*safe_map_strings_1)["apple"] = 0;
    (*safe_map_strings_2)["potato"] = 0;

    std::thread t1([&]() { func(safe_map_strings_1, safe_map_strings_2);} ); 
    std::thread t2([&]() { func(safe_map_strings_1, safe_map_strings_2);} ); 
    t1.join(); t2.join();
    return 0;
}