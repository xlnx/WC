int sum(int a, int b)
{
	int result = 0;
	while (a <= b)
	{
		result += a;
		a += 1;
	}
	return result;
}

int fib(int x)
{
	if (x < 3) return 1;
	return fib(x-1) + fib(x-2);
}
