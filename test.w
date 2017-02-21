class A
{
	int a;
	fn ()  -> int exec virtual
	{
		return a - 1;
	}
	fn () init
	{
		a = 10;
	}
};

class B: A
{
	fn () -> int exec override
	{
		return a + 1;
	}
};

fn () -> int sub
{
	A a;
	a.init();
	return a.exec();
}

fn () -> int add
{
	B b;
	ptr B p = &b;
	b.init();
	return p->exec();
}
