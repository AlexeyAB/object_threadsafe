#include <iostream>
#include <mutex>
#include <thread>
#include <map>

struct T { int price, count, total; };
 
void thread_func(std::map<int, T> &shared_map, std::mutex &mtx) 
{
    std::lock_guard<std::mutex> lock(mtx); // RAII (destructor will unlock mutex)
    
    auto it = shared_map.find(1);
    if(it != shared_map.end()) 
    {
        T &ref_val = it->second;
        ref_val.count = ref_val.count + 1;
        ref_val.total = ref_val.price * ref_val.count;
    }
    else {
        shared_map.insert( std::make_pair(1, T({10,5,50})) );
    }
} // mutex will be unlocked automatically
 
int main() {
    std::mutex mtx;
	std::map<int, T> shared_map;
    
	std::thread t1( thread_func, std::ref(shared_map), std::ref(mtx) ); 
	std::thread t2( thread_func, std::ref(shared_map), std::ref(mtx) ); 
    std::thread t3( thread_func, std::ref(shared_map), std::ref(mtx) ); 
	t1.join(); 
	t2.join();
    t3.join();
	
	T local_result = shared_map[1];
	std::cout << local_result.price << ", " << local_result.count << ", "
		<< local_result.total;
 
	return 0;
}