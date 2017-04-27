#include <iostream>
#include <thread>
#include <vector>

#include "safe_ptr.h"

template <typename T>
void func(T &s_m, int &a, int &b)
{
	for (size_t i = 0; i < 100000; ++i)
	{
		// x-lock for modification
		{
			s_m.lock();
			a++; 
			b++;
			s_m.unlock();
		}

		// s-lock for reading
		{
			s_m.lock_shared();
			assert(a == b);		// will never happen
			s_m.unlock_shared();
		}
	}
}

int main() {

	int a = 0;
	int b = 0;
	sf::contention_free_shared_mutex<> s_m;
	
	// 19 threads
	std::vector<std::thread> vec_thread(20);
	for (auto &i : vec_thread) i = std::move(std::thread([&]() { func(s_m, a, b); }));
	for (auto &i : vec_thread) i.join();
	
	std::cout << "a = " << a << ", b = " << b << std::endl;
	getchar();

	return 0;
}