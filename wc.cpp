#include <llvm/Support/raw_ostream.h>
#include "wc.h"
#include <fstream>
int main(int argc, char *argv[])
{
	string inf, ouf;
	char* p;
	switch (argc)
	{	
	case 2: inf = argv[1];
		p = argv[1] + strlen(argv[1]);
		while (p != argv[1] && *p != '.')
			--p;
		if (*p == '.')
			*p = 0;
		ouf = string(argv[1]) + ".ll"; break;
	case 3: inf = argv[1], ouf = argv[2]; break;
	case 1: cerr << "wc: no input file" << endl; return 1;
	default: cerr << "too many params provided" << endl; return 1;
	}
	try
	{
		parser mparser(mlex_rules, mparse_rules, mexpr_rules);
		ifstream is(inf);
		ofstream os(ouf);
		string src, dest;
		getline(is, src, static_cast<char>(EOF));
		try
		{
			mparser.parse(src.c_str());
		}
		catch (const err& e)
		{		// poly
			e.alert();
		}
		is.close();
		raw_string_ostream los(dest);
		lModule->print(los, nullptr);
		os << dest;
		os.close();
	}
	catch (const err& e)		// poly
	{
		e.alert();
	}
}
