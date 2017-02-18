int sum(int a, int b)
{
	int result = 0;
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

int swh(int a, int b)
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

int fib(int x)
{
	if (x < 3) return 1;
	else return fib(x - 1) + fib(x - 2);
}
