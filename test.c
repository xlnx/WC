int fib(int x)
{
	if (x < 3) return 1;
	return fib(x+1) + fib(x+2);
}

int __main()
{
	int a = fib(100);
	return 0;
}
