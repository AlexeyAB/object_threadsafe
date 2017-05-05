#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <map>
#include <numeric>
#include <sstream>
#include <cassert>
#include <new>
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

struct link_safe_ptrs {
    template<typename T1, typename... Args>
    link_safe_ptrs(T1 &first_ptr, Args&... args) {
        std::lock_guard<T1> lock(first_ptr);
        typedef typename T1::mtx_t mutex_t;
        std::shared_ptr<mutex_t> old_mtxs[] = { args.mtx_ptr ... }; // to unlock before mutexes destroyed
        std::shared_ptr<std::lock_guard<mutex_t>> locks[] = { std::make_shared<std::lock_guard<mutex_t>>(*args.mtx_ptr) ... };
        std::shared_ptr<mutex_t> mtxs[] = { args.mtx_ptr = first_ptr.mtx_ptr ... };
    }
};
// ---------------------------------------------------------------

struct user_accounts_t { 
    std::string user_name; int64_t money; 
    user_accounts_t(std::string u, int64_t m) : user_name(u), money(m) {}
};

safe_ptr<std::map<uint64_t, user_accounts_t >> safe_user_accounts(
    std::map<uint64_t, user_accounts_t >({
    std::make_pair(1, user_accounts_t("John Smith", 100)),
    std::make_pair(2, user_accounts_t("John Rambo", 150))
}));

struct cash_flows_t { uint64_t unique_id, src_id, dst_id, time; int64_t money; };

std::atomic<uint64_t> global_unique_id;
safe_ptr<std::multimap<uint64_t, std::shared_ptr<cash_flows_t>>> safe_cash_flows_src_id;
safe_ptr<std::multimap<uint64_t, std::shared_ptr<cash_flows_t>>> safe_cash_flows_dst_id;

// too much granularity (very slow) 
//static link_safe_ptrs tmp_link(safe_user_accounts, safe_cash_flows_table, safe_cash_flows_src_id, safe_cash_flows_dst_id); 


void move_money(uint64_t src_id, uint64_t dst_id, uint64_t time, int64_t money)
{
    auto cash_flow_row_ptr = std::make_shared<cash_flows_t>();
    cash_flow_row_ptr->unique_id = ++global_unique_id;
    cash_flow_row_ptr->src_id = src_id;
    cash_flow_row_ptr->dst_id = dst_id;
    cash_flow_row_ptr->time = time;
    cash_flow_row_ptr->money = money;

    std::cout << " - start transaction... move_money() \n";
    std::lock_guard<decltype(safe_cash_flows_src_id)> lock2(safe_cash_flows_src_id);    // 2
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // for example - OS decides to interrupt this thread
    std::lock_guard<decltype(safe_cash_flows_dst_id)> lock3(safe_cash_flows_dst_id);    // 3
        std::lock_guard<decltype(safe_user_accounts)> lock1(safe_user_accounts);        // 1
        
    // update table-1
    safe_user_accounts->at(src_id).money -= money;            
    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // for example - OS decides to interrupt this thread
    safe_user_accounts->at(dst_id).money += money;

    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // for example - OS decides to interrupt this thread

    // inset to indexes in table safe_cash_flows: src & dst
    safe_cash_flows_src_id->emplace(src_id, cash_flow_row_ptr);
    safe_cash_flows_dst_id->emplace(dst_id, cash_flow_row_ptr);
 
    std::cout << " - end transaction: move_money() \n";
}

void show_total_amount() 
{
        int64_t total_amount = 0;

        std::cout << " - start transaction... show_total_amount() \n";
        std::lock_guard<decltype(safe_user_accounts)> lock1(safe_user_accounts);  
            
        std::cout << std::endl;
        for (auto it = safe_user_accounts->begin(); it != safe_user_accounts->end(); ++it) {
            total_amount += it->second.money;
            std::cout << it->first << " => " << it->second.user_name << ", " <<
                it->second.money << std::endl;
        }
        std::cout << "Result: all accounts total_amount = " << total_amount << " \t <<< \n\n";

        std::cout << " - end transaction: show_total_amount() \n";
}


void show_user_money_on_time(uint64_t user_id, uint64_t time)
{
        int64_t incoming = 0;
        int64_t outcoming = 0;
        int64_t user_money = 0;
        std::string user_name;

        std::cout << " - start transaction... show_user_money_on_time() \n";
        std::lock_guard<decltype(safe_cash_flows_dst_id)> lock3(safe_cash_flows_dst_id);    // 3
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // for example - OS decides to interrupt this thread         
        std::lock_guard<decltype(safe_cash_flows_src_id)> lock2(safe_cash_flows_src_id);    // 2
        std::lock_guard<decltype(safe_user_accounts)> lock1(safe_user_accounts);            // 1

        std::cout << std::endl;
        auto in_range = safe_cash_flows_dst_id->equal_range(user_id);
        for (auto it = in_range.first; it != in_range.second; ++it)
            if (it->second->time > time)
                incoming += it->second->money;

        auto out_range = safe_cash_flows_src_id->equal_range(user_id);
        for (auto it = out_range.first; it != out_range.second; ++it)
            if (it->second->time > time)
                outcoming += it->second->money;

        user_money = safe_user_accounts->at(user_id).money;
        user_name = safe_user_accounts->at(user_id).user_name;

        std::cout << std::endl << "incoming = " << incoming << ", outcoming = " << outcoming <<
            ", current user_money = " << user_money << std::endl;

        user_money = user_money - incoming + outcoming; // take into account cash flow

        std::cout << user_id << " => " << user_name << ", " << user_money <<
            ", at time = " << time << " \t <<< \n\n";

        std::cout << " - end transaction: show_user_money_on_time() \n";
}

int main() {

    std::cout << "Init table safe_user_accounts: " << std::endl;
    std::cout << "at time = 0  \t\t <<< " << std::endl;

    for (auto it = safe_user_accounts->begin(); it != safe_user_accounts->end(); ++it)
        std::cout << it->first << " => " << it->second.user_name << ", " <<
        it->second.money << std::endl;
    std::cout << std::endl;


    std::thread t1([&]() { move_money(2, 1, 12, 50); });    // src_id, dst_id, time, money

    std::this_thread::sleep_for(std::chrono::milliseconds(5)); // OS decides that another thread runs at this time   

    std::thread t2([&]() { show_total_amount(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(5)); // OS decides that another thread runs at this time   

    std::thread t3([&]() { show_user_money_on_time(1, 0); });   // user_id, time

    t1.join();
    t2.join();
    t3.join();

    std::cout << "end";
    int b; std::cin >> b;

    return 0;
}