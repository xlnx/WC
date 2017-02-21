#include <iostream>

using namespace std;

extern "C" int add();
extern "C" int sub();

int main()
{
	cout << add() << endl;
	cout << sub() << endl;
}
