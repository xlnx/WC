#include <iostream>

extern "C" int fib(int x);

int main()
{
	int a = fib(40);
	return a;
}
