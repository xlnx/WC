class A
{
	int a;
	float b;
	fn () exec virtual
	{}
};

fn (int a, int b) -> int sum
{
	int result = 0;
	A a_inst;
	ptr A pa;
	a_inst.a = 1.1;
	pa->b = 'a';
	while (a <= b)
	{	
		result += a;
		a += 1;
		if (a == 10) break;
	}
	finally
	{	
		result |= 1;
	}
	return result;
}

fn (int a, int b, int c) -> int sum
{
	return a + b + c; 
}

fn (int a, int b) -> int swh
{
	switch (a)
	{
	case 0: case 2:
		{
			a += 1;
		}
	case 1:
		{
			b += 1;
			break;
		}
	case 5:
	}
	return -a | b;
}

fn (int x) -> int fib
{
	if (x < 3) return 1;
	else return fib(x - 1) + fib(x - 2);
}

fn (ptr fn(int, int)->int f, int L, int R) -> int calc
{
	
}
