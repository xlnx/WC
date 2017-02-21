int add()
{
	int (*pf)(void) = add;
	return pf();
}
