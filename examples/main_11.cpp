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


safe_ptr<std::map<std::string, std::pair<std::string, int> >> safe_map_strings_global;


void func(decltype(safe_map_strings_global) safe_map_strings) 
{
    std::lock_guard<decltype(safe_map_strings)> lock(safe_map_strings);

    (*safe_map_strings)["apple"].first = "fruit";
    (*safe_map_strings)["potato"].first = "vegetable";

    for (size_t i = 0; i < 10000; ++i) {
        safe_map_strings->at("apple").second++;
        safe_map_strings->find("potato")->second.second++;
    }

    auto const readonly_safe_map_string = safe_map_strings;

    std::cout << "potato is " << readonly_safe_map_string->at("potato").first <<
        " " << readonly_safe_map_string->at("potato").second <<
        ", apple is " << readonly_safe_map_string->at("apple").first <<
        " " << readonly_safe_map_string->at("apple").second << std::endl;
}


int main() {

    std::vector<std::thread> vec_thread(10);
    for (auto &i : vec_thread) i = std::move(std::thread(func, safe_map_strings_global)); 
    for (auto &i : vec_thread) i.join();

    std::cout << "end";
    int b; std::cin >> b;

    return 0;
}