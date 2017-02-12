namespace lr_parser
{

lexer_base::lexer_base(const init_rules& iR)
{	
	for (auto& ir: iR)
	{	// ^maches the beginning and white spaces are allowed
		// catch the matching token and store it in the table
		rule r;
		r.token_name = ir.token_name;
		std::string suffix = ir.opts.count(word) ? "\\b" : "";
		//std::cout << prefix + ir.mode + suffix << std::endl;
		if (ir.opts.count(ignore_case))
			r.mode = std::regex(ir.mode + suffix, std::regex::icase |
				std::regex::nosubs | std::regex::optimize);
		else
			r.mode = std::regex(ir.mode + suffix, std::regex::nosubs |
				std::regex::optimize);
		r.no_attr = ir.opts.count(no_attr);
		rules_list.push_back(std::move(r));
	}
}

void lexer_base::input(pchar p)
{
	ln = col = 0;
	buffer = p;
	cur_ptr = buffer.c_str();
	ln_lookup[0] = cur_ptr;
	while (spaces.count(*cur_ptr))
	{
		if (*cur_ptr == '\n')
		{
			ln_lookup[++ln] = cur_ptr + 1;
			col = 0;
		}
		else ++col;
		++cur_ptr;
	}
}

token lexer_base::next_token()
{
	if (cur_ptr && *cur_ptr)
	{
		std::cmatch m;
		for (auto& p: rules_list)
		{
			if (std::regex_search(cur_ptr, m, p.mode, std::regex_constants::match_continuous))
			{
				unsigned LN = ln, COL = col;
				while (cur_ptr != m.suffix().first)
				{
					if (*cur_ptr == '\n')
					{
						ln_lookup[++ln] = cur_ptr + 1;
						col = 0;
					}
					else ++col;
					++cur_ptr;
				}
				while (spaces.count(*cur_ptr))
				{
					if (*cur_ptr == '\n')
					{
						ln_lookup[++ln] = cur_ptr + 1;
						col = 0;
					}
					else ++col;
					++cur_ptr;
				}
				if (!p.no_attr)
				{	// goto next token
					return token(p.token_name, LN, COL, ln_lookup[LN], std::make_shared<attr_type>(m[0]));
				}
				else return token(p.token_name, LN, COL, ln_lookup[LN]);
			}
		}
		std::cerr << cur_ptr << std::endl;
		cur_ptr = nullptr;
		// input not valid
		throw lex_err(ln, col, ln_lookup[ln]);
	}
	return token();
}

lexer::lexer(const init_rules& lex_R): lexer_base(lex_R.first)
{	
	for (auto& v: lex_R.second)
	{
		handler_lookup[v.first] = v.second;
	}
	for (auto& v: lex_R.first)
	{
		if (!v.opts.count(no_attr) && !handler_lookup[v.token_name])
		{
			/*std::cerr << v.token_name + " declared attr but dont have a callback function" << std::endl;
			std::cerr << "Using empty callback instead" << std::endl;*/
			handler_lookup[v.token_name] = [](term_node&, AST_context*)->AST_result{ return AST_result(); };
		}
	}
}

token lexer::next_token()
{
	if (cur_ptr && *cur_ptr)
	{
		std::cmatch m;
		int sz = rules_list.size();
		for (int i = 0; i < sz; ++i)
		{
			auto& p = rules_list[i];
			if (std::regex_search(cur_ptr, m, p.mode, std::regex_constants::match_continuous))
			{	// this token has attr
				unsigned LN = ln, COL = col;
				while (cur_ptr != m.suffix().first)
				{
					if (*cur_ptr == '\n')
					{
						ln_lookup[++ln] = cur_ptr + 1;
						col = 0;
					}
					else ++col;
					++cur_ptr;
				}
				while (spaces.count(*cur_ptr))
				{
					if (*cur_ptr == '\n')
					{
						ln_lookup[++ln] = cur_ptr + 1;
						col = 0;
					}
					else ++col;
					++cur_ptr;
				}
				if (!p.no_attr)
				{	// for the non-reserved tokens which need attrs
					// goto next token
					return token(p.token_name, LN, COL, ln_lookup[LN], std::make_shared<attr_type>(
						m[0], handler_lookup[p.token_name]
					));
				}
				else return token(p.token_name, LN, COL, ln_lookup[LN]);
			}
		}
		std::cerr << cur_ptr << std::endl;
		cur_ptr = nullptr;
		// input not valid
		throw lex_err(ln, col, ln_lookup[ln]);
	}
	return token();
}

}
