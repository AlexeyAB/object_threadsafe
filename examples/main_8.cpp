#include <iostream>
#include <vector>

struct T4 { int a = 1, b = 2, c = 3; };
T4 t4;

struct T3 { T4* operator -> () { return &t4; } };
struct T2 { T3 operator -> () { return T3(); } };
struct T1 { T2 operator -> () { return T2(); } };

int main()
{
    T1 t1;
	std::cout << t1->c << std::endl; 
	std::cout << (t1.operator->())->c << std::endl; 
	std::cout << (t1.operator->().operator->())->c << std::endl; 
	
	return 0;
}