fn ()->int add
{
	let a = 0, b = 1.5;
	let add = lambda(int x, int y)->int{
		return x + y;
	};
	return add(a, b);
}

class A
{
	int a;
	float b;
	int c;
};

fn () test
{
	let a = [[1, 2, 3], [1, 3, 4]];
	A c = [1, 2, 5.5];
	a[0][0] += 100;
}
