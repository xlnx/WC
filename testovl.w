fn (int a, int b) -> int sum
{
	return a + b;
}

fn (int a, int b, int c) -> int sum
{
	return a + b + c;
}

fn () -> int shit
{
	return sum(1, 2, 3);
}
