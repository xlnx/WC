#include "wc.h"

const int buff_len = 2048;
char buff[buff_len];

int main()
{
	try
	{
		parser mparser(mlex_rules, mparse_rules, mexpr_rules);
		while (1)
		{
			printf(">>> ");
			auto* ptr = buff;
			while (scanf("%[^\n]s", ptr) == 1)
			{
				fflush(stdin);
				ptr += strlen(ptr);
				*ptr++ = '\n';
				*ptr = 0;
			}
			if (!*buff) break;
			try
			{
				mparser.parse(buff);
			}
			catch (const err& e)
			{		// poly
				e.alert();
			}
			fflush(stdin);
			*buff = 0;
		}
		lModule->dump();
		system("pause");
	}
	catch (const err& e)		// poly
	{
		e.alert();
	}
}
