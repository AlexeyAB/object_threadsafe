#include <iostream>
#include <thread>
#include <mutex>
#include <memory>
#include <vector>
#include <numeric>
#include <algorithm>
 
template<typename T, typename mutex_type = std::recursive_mutex>
class execute_around {
  std::shared_ptr<mutex_type> mtx;
  std::shared_ptr<T> p;
 
  void lock() const { mtx->lock(); }
  void unlock() const { mtx->unlock(); }
  public:
    class proxy {
      std::unique_lock<mutex_type> lock;
      T *const p;
      public:
        proxy (T * const _p, mutex_type& _mtx) : lock(_mtx), p(_p)  { std::cout << "locked \n";} 
        proxy(proxy &&px) : lock(std::move(px.lock)), p(px.p)  {}
        ~proxy () { std::cout << "unlocked \n"; }
        T* operator -> () {return p;}
        const T* operator -> () const {return p;}
    };
 
    template<typename ...Args>
    execute_around (Args ... args) : 
        mtx(std::make_shared<mutex_type>()), p(std::make_shared<T>(args...))  {}  
 
    proxy operator -> () { return proxy(p.get(), *mtx); }
    const proxy operator -> () const { return proxy(p.get(), *mtx); }
    template<class Args> friend class std::lock_guard;
};
 
template<typename T, typename T2>
int my_accumulate(T b, T e, T2 v) {
    std::cout << "accumulate() - start \n";
	int result = std::accumulate(b, e, v);
	std::cout << "accumulate() -  finish \n";
	return result;
}
 
int main()
{
  execute_around<std::vector<int>> vecc(10, 10);
 
  int res = my_accumulate(vecc->begin(), vecc->end(), 0); // thread-safe
 
  std::cout << std::string("res = " + std::to_string(res) + "\n");
 
  return 0;
}