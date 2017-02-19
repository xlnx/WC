#include <iostream>

using namespace std;

extern "C" int sum(int a, int b);
extern "C" int swh(int a, int b);
extern "C" int fib(int x);

int main()
{
	cout << sum(5, 9) << endl;
	cout << swh(2, 8) << endl;
	cout << fib(40) << endl;
}
