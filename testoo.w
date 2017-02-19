struct A
{
	int l;
	int r;
	fn ()->int mid
	{
		return (l + r) / 2;
	}
};

fn (int x, int y)->int figure
{
	A a_inst;
	a_inst.l = x;
	a_inst.r = y;
	return a_inst.mid();
}
