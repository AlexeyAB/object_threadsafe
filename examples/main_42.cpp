#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <map>
//#include <shared_mutex>

template<typename T, typename mutex_t = std::recursive_mutex, typename x_lock_t = std::unique_lock<mutex_t>,
    typename s_lock_t = std::unique_lock<mutex_t >>
    // std::shared_lock<std::shared_timed_mutex>, when mutex_t = std::shared_timed_mutex
class safe_ptr {
protected:
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
        auto operator [] (arg_t &&arg) -> decltype((*ptr)[arg]) { return (*ptr)[arg]; }
    };

    template<typename... Args> void lock_shared() const { mtx_ptr->lock_shared(); }
    template<typename... Args> void unlock_shared() const { mtx_ptr->unlock_shared(); }
    void lock() { mtx_ptr->lock(); }
    void unlock() { mtx_ptr->unlock(); }
    friend struct link_safe_ptrs;
    template<typename, typename, size_t, size_t> friend class lock_timed_transaction;
    template<typename mutex_type> friend class std::lock_guard;
    //template<typename mutex_type> friend class std::shared_lock;  // C++14
    //template<class... mutex_types> friend class std::lock_guard;  // C++17
public:
    template<typename... Args>
    safe_ptr(Args... args) : ptr(std::make_shared<T>(args...)), mtx_ptr(std::make_shared<mutex_t>()) {}

    auto_lock_t<x_lock_t> operator -> () { return auto_lock_t<x_lock_t>(ptr.get(), *mtx_ptr); }
    auto_lock_obj_t<x_lock_t> operator * () { return auto_lock_obj_t<x_lock_t>(ptr.get(), *mtx_ptr); }
    const auto_lock_t<s_lock_t> operator -> () const { return auto_lock_t<s_lock_t>(ptr.get(), *mtx_ptr); }
    const auto_lock_obj_t<s_lock_t> operator * () const { return auto_lock_obj_t<s_lock_t>(ptr.get(), *mtx_ptr); }

    typedef mutex_t mtx_t;
    typedef T obj_t;
    typedef x_lock_t xlock_t;
    typedef s_lock_t slock_t;
};

template<typename T> using default_safe_ptr = safe_ptr<T, std::recursive_mutex, std::unique_lock<std::recursive_mutex>, std::unique_lock<std::recursive_mutex>>;
// C++14
//template<typename T> using shared_mutex_safe_ptr = 
//safe_ptr< T, std::shared_timed_mutex, std::unique_lock<std::shared_timed_mutex>, std::shared_lock<std::shared_timed_mutex> >;
// ---------------------------------------------------------------

// safe partitioned map
template<typename key_t, typename val_t, template<class> class safe_ptr_t = default_safe_ptr,
    typename container_t = std::map<key_t, val_t>, typename part_t = std::map<key_t, safe_ptr_t<container_t>> >
class safe_map_partitioned_t
{
    using safe_container_t = safe_ptr_t<container_t>;
    typedef typename part_t::iterator part_iterator;
    typedef typename part_t::const_iterator const_part_iterator;
    std::shared_ptr<part_t> partition;

public:
    typedef std::vector<std::pair<key_t, val_t>> result_vector_t;

    safe_map_partitioned_t() : partition(std::make_shared<part_t>()) { partition->emplace(key_t(), container_t()); }

    safe_map_partitioned_t(const key_t start, const key_t end, const key_t step) : partition(std::make_shared<part_t>()) {
        for (key_t i = start; i <= end; i += step) partition->emplace(i, container_t());
    }

    safe_map_partitioned_t(std::initializer_list<key_t> const& il) : partition(std::make_shared<part_t>()) {
        for (auto &i : il) partition->emplace(i, container_t());
    }

    part_iterator part_it(key_t const& k) { auto it = partition->lower_bound(k); if (it == partition->cend()) --it; return it; }
    const_part_iterator part_it(key_t const& k) const { auto it = partition->lower_bound(k); if (it == partition->cend()) --it; return it; }
    safe_container_t& part(key_t const& k) { return part_it(k)->second; }
    const safe_container_t& part(key_t const& k) const { return part_it(k)->second; }
    //slocked_safe_ptr<safe_container_t> read_only_part(key_t const& k) const { return slock_safe_ptr(part(k)); }

    void get_range_equal(const key_t& key, result_vector_t &result_vec) const {
        result_vec.clear();
        auto slock_container = slock_safe_ptr(part(key));
        for (auto it = slock_container->lower_bound(key); it != slock_container->upper_bound(key); ++it)
            result_vec.emplace_back(*it);
    }

    void get_range_lower_upper(const key_t& low, const key_t& up, result_vector_t &result_vec) const {
        result_vec.clear();
        auto const& const_part = *partition;
        auto end_it = (const_part.upper_bound(up) == const_part.cend()) ? const_part.cend() : std::next(const_part.upper_bound(up), 1);
        auto it = const_part.lower_bound(low);
        if (it == const_part.cend()) --it;
        for (; it != end_it; ++it)
            result_vec.insert(result_vec.end(), it->second->lower_bound(low), it->second->upper_bound(up));
    }

    void erase_lower_upper(const key_t& low, const key_t& up) {
        auto end_it = (partition->upper_bound(up) == partition->end()) ? partition->end() : std::next(partition->upper_bound(up), 1);
        for (auto it = part_it(low); it != end_it; ++it)
            it->second->erase(it->second->lower_bound(low), it->second->upper_bound(up));
    }

    template<typename T, typename... Args> void emplace(T const& key, Args const&&...args) {
        part(key)->emplace(key, args...);
    }

    size_t size() const {
        size_t size = 0;
        for (auto it = partition->begin(); it != partition->end(); ++it) size += it->second->size();
        return size;
    }
    size_t erase(key_t const& key) throw() { return part(key)->erase(key); }
    void clear() { for (auto it = partition->begin(); it != partition->end(); ++it) it->second->clear(); }
};
// ---------------------------------------------------------------

safe_map_partitioned_t<std::string, std::pair<std::string, int> > safe_map_strings_global{ "a", "f", "k", "p", "u" };



void func(decltype(safe_map_strings_global) safe_map_strings)
{
    for (size_t i = 0; i < 10000; ++i) {
        (*safe_map_strings.part("apple"))["apple"].first = "fruit";
        (*safe_map_strings.part("apple"))["apple"].second++;
        (*safe_map_strings.part("potato"))["potato"].first = "vegetable";
        (*safe_map_strings.part("potato"))["potato"].second++;
    }
    
    auto const& readonly_safe_map_string = safe_map_strings;
    
    std::cout << "potato is " << readonly_safe_map_string.part("potato")->at("potato").first <<
        " " << readonly_safe_map_string.part("potato")->at("potato").second <<
        ", apple is " << readonly_safe_map_string.part("apple")->at("apple").first <<
        " " << readonly_safe_map_string.part("apple")->at("apple").second << std::endl;
    
}


int main() {
    
    // start 10 concurrent threads
    std::vector<std::thread> vec_thread(10);
    for (auto &i : vec_thread) i = std::thread(func, safe_map_strings_global); 
    for (auto &i : vec_thread) i.join();

    // erase "apple": a -> c
    safe_map_strings_global.erase_lower_upper("a", "c");

    // get all keys-values: a -> z
    decltype(safe_map_strings_global)::result_vector_t all_ragne;
    safe_map_strings_global.get_range_lower_upper("a", "z", all_ragne);
    for (auto &i : all_ragne)
        std::cout << i.first << " => " << i.second.first << ", " << i.second.second << std::endl;

    std::cout << "end";
    int b; std::cin >> b;

    return 0;
}