namespace lr_parser
{
// ctor
parser::parser(lexer::init_rules& lR, init_rules& iR, expr_init_rules& eiR, std::string s):
	lex(expr_gen(lR, iR, eiR)), rules({{s + "__", {s}}})
{	// use a lexer to parse initializer rules
	lexer_base::init_rules m_lR;
	for (auto& r: lR.first)
	{
		if (r.opts.count(no_attr))
		{
			m_lR.push_back({ r.token_name, r.mode, r.opts });
		}
		else if (!signs.count(r.token_name))
		{
			m_lR.push_back({ r.token_name, r.token_name, {word, no_attr} });
			terms.insert(r.token_name);
		}
		signs.insert(r.token_name);
	}
	for (auto& r: iR)
	{
		if (!signs.count(r.first))
		{	// no such element
			m_lR.push_back(
			{ r.first, r.first, {word, no_attr} });
			signs.insert(r.first);
			gens.insert(r.first);
		}
		else
		{
			throw err("Generate rule redeclared: " + r.first);
		}
	}
	signs.insert(stack_bottom);	// stack empty
	//signs.insert(empty_sign);
	lexer_base m_lexer(m_lR);
	std::map<sign, std::set<rule_id>> rules_map;
	for (auto& p: iR)
	{
		for (auto& g: p.second)
		{
			rule r = { p.first, {}, 0, g.second};	// if second == {} then it is empty
			m_lexer.input(g.first.c_str());
			token T;
			while (T = m_lexer.next_token())
			{	// record all sub rules
				r.signs.push_back(T.name);
				if (terms.count(T.name) || gens.count(T.name)) ++r.sub_count;
			}
			if (r.signs.empty())
			{
				r.signs.push_back(empty_sign);
			}
			rules_map[p.first].insert(rules.size());
			rules.push_back(r);
		}
	}
	
	std::map<sign, std::set<sign>> FIRST, FOLLOW;
	std::set<sign> been, settled;
	std::function<void(const sign&)> gen_first = [&](const sign& s)->void
	{
		bool B = been.count(s), S = settled.count(s);
		if (!B || S)
		{	// it is a gen
			been.insert(s);
			for (auto r_id: rules_map[s])
			{	// for all the gen rules
				for (auto& t: rules[r_id].signs)
				{
					if (s == t) break;
					if (!rules_map[t].empty())
					{
						if (!S) gen_first(t);
						for (auto& g: FIRST[t]) // settle that gen
						{
							FIRST[s].insert(g);
						}	// TODO: repair this
						if (!FIRST[t].count(empty_sign)) break;
						break;
					}
					else
					{	// term sign
						FIRST[s].insert(t);
						break;
					}
				}
			}
			settled.insert(s);
		}
	};
	for (auto& sgn: signs)
	{
		if (!rules_map[sgn].empty())
		{
			gen_first(sgn);
		}
	}
	bool add_sub = false;
	do {
		add_sub = false;
		for (auto& r: rules)
		{	// for all the gen rules
			for (int i = r.signs.size() - 1; i > 0; --i)
			{	// A -> aBb
				if (!rules_map[r.signs[i - 1]].empty())
				{	// B
					if (!rules_map[r.signs[i]].empty())
					{	// b
						for (auto& sgn: FIRST[r.signs[i]])
						{
							if (sgn != empty_sign && !FOLLOW[r.signs[i - 1]].count(sgn))
							{
								FOLLOW[r.signs[i - 1]].insert(sgn);
								add_sub = true;
							}
						}
						if (FIRST[r.signs[i]].count(empty_sign))
						{
							for (auto& sgn: FOLLOW[r.src])
							{
								if (!FOLLOW[r.signs[i - 1]].count(sgn))
								{
									FOLLOW[r.signs[i - 1]].insert(sgn);
									add_sub = true;
								}
							}
						}
					}
					else
					{
						if (!FOLLOW[r.signs[i - 1]].count(r.signs[i]))
						{
							FOLLOW[r.signs[i - 1]].insert(r.signs[i]);
							add_sub = true;
						}
					}
				}
			}
			if (!rules_map[r.signs.back()].empty())
			{
				for (auto& sgn: FOLLOW[r.src])
				{
					if (!FOLLOW[r.signs.back()].count(sgn))
					{
						FOLLOW[r.signs.back()].insert(sgn);
						add_sub = true;
					}
				}
			}
		}
	} while (add_sub);
	for (auto& sgn: signs)
	{
		if (!rules_map[sgn].empty())
		{
			FOLLOW[sgn].insert(stack_bottom);
		}
	}
	FOLLOW[s + "__"].insert(stack_bottom);
	std::vector<closure> closures;	// S__ -> S
	auto GEN = [&](closure I)
	{
		bool gen_sub = false;
		do {
			gen_sub = false;
			for (auto& T: I)
			{
				if (rules[T.first].signs.size() != T.second)
				{
					for (auto r_id: rules_map[rules[T.first].signs[T.second]])
					{	// all rules from this sign
						if (!I.count({r_id, 0}))
						{
							I.insert({r_id, 0});
							gen_sub = true;
						}
					}
				}// else this is mergeable 
			}
		} while (gen_sub);
		closures.push_back(std::move(I));
		GOTO.push_back(std::map<sign, state>());
		ACTION.push_back(std::map<sign, action>());
	};
	GEN({{0, 0}});
	auto show = [&](int i)
	{
		std ::cout << ">>> " << i << std::endl;
		for (auto& T: closures[i])
		{
			std::cout << rules[T.first].src<< " -> ";
			int i = 0;
			for (auto& v: rules[T.first].signs)
			{
				if (i == T.second) std::cout << ".";
				std::cout << v + " ";
				++i;
			} 
			if (i == T.second) std::cout << ".";
			std::cout << std::endl;
		}
	};
	auto alert = [&]()
	{
		for (int i = 0; i < closures.size(); ++i)
		{
			show(i);
			/*for (auto& sgn: signs)
			{
				std::cout << "ACTION[" << i << "][" << sgn << "]" << ACTION[i][sgn] << std::endl;
				std::cout << "GOTO[" << i << "][" << sgn << "]" << GOTO[i][sgn] << std::endl;
			}*/
		}
	};
	closure NEW;
	do {
		for (int i = 0; i < closures.size(); ++i)
		{	// i stands for state
			int has_empty_sign = 0;
			for (auto& T: closures[i])
			{
				if (rules[T.first].signs.size() == 1 && rules[T.first].signs[0] == empty_sign)
				{
					has_empty_sign = T.first;
				}
				else if (rules[T.first].signs.size() == T.second)
				{	// this item ends
					for (auto& sgn: FOLLOW[rules[T.first].src])
					{
						ACTION[i][sgn] = T.first == 0 ? a_accept: T.first;	// S__ -> S
					}
				}
			}
			for (auto& sgn: signs)
			{
				NEW = {};
				if (!GOTO[i][sgn])
				{
					for (auto& T: closures[i])
					{	// for each term in this closure T = {rule_id, pos}
						if (T.second != rules[T.first].signs.size() &&
							rules[T.first].signs[T.second] == sgn)
						{	// check if this sgn matches this rule    pass or 
							NEW.insert({T.first, T.second + 1});
						}
					}
					if (!NEW.empty())
					{
						bool is_sub;
						int j = 0;
						for (; j < closures.size(); ++j)
						{	// TODO: is this right?
							is_sub = true;
							for (auto& T: NEW)
							{
								if (!closures[j].count(T))
								{
									is_sub = false;
									break;
								}
							}
							if (is_sub) break;
						}
						if (is_sub)
						{
							GOTO[i][sgn] = j;
							ACTION[i][sgn] = a_move_in;
						}
						else 
						{
							GOTO[i][sgn] = closures.size();
							//std::cout << sgn << std::endl;
							GEN(NEW); // generate this closure
							if (ACTION[i][sgn])
							{//alert();
								throw parser_err("Not SLR at " + sgn);
							}
							ACTION[i][sgn] = a_move_in;
						}
					}
				}
			}
			if (has_empty_sign)
			{
				for (auto& sgn: signs)
				{
					if (!ACTION[i][sgn]) ACTION[i][sgn] = has_empty_sign;	// S__ -> S
				}
			}
		}
	} while (!NEW.empty());
	//alert();
}

lexer::init_rules parser::expr_gen(lexer::init_rules& lR, init_rules& iR, expr_init_rules& eiR)
{
	auto int2str = [](int n)
	{
		char str[20];
		sprintf(str, "%u", n);
		return std::string(str);
	};
	/*auto alert = [](const std::string& a, const std::string& b)
	{
		std::cout << a << " -> " << b << std::endl;
	};*/
	using value_pack = std::vector<llvm::Value*>;
	auto gen_pack = [&](const std::string& s, const std::string& del = ",")
	{	// TODO: repair this
		iR.push_back(
		{
			s + "pk",
			{
				{ s + "pkhp", forward },
				{ "", [](gen_node&, AST_context* context){ return AST_result(new value_pack); } }
			}
		});
		//alert(s + "pk", s + " " + s + "pkhp");
		iR.push_back(
		{
			s + "pkhp",
			{
				{
					s + "pkhp" + " " + del + " " + s, [](gen_node& G, AST_context* context)
					{
						auto p = (value_pack*)(G[0].code_gen(context).custom_data);
						p->push_back(G[1].code_gen(context).value);
						return AST_result(p);
					}
				},
				{
					s, [](gen_node& G, AST_context* context)
					{
						auto p = new value_pack;
						p->push_back(G[0].code_gen(context).value);
						return AST_result(p);
					}
				}
			}
		});
		//alert(s + "pkhp", del + " " + s + " " + s + "pkhp");
	};
	lexer_base lex(
	{	// type
		{ "pack", "%pk", {no_attr} },
		//{ "ref", "%(?:0|[1-9]\\d*)", {} }
		{ "param", "%", {no_attr} },
		{ "piece", "(?:\\\\%|[^\\s%]*)+", {} },
	});
	gen_pack("expr");
	const std::string tag = "expr";
	std::string r_this = "expr";
	std::vector<std::string> lst;
	for (int i = 0; i < eiR.size(); ++i)
	{
		const std::string r_next = tag + int2str(i);
		iR.push_back( { r_this, {} } );
		//alert(r_this, "follow");
		for (auto& v: eiR[i])
		{
			lex.input(v.mode_str.c_str());
			std::vector<token> arr;
			token T;
			while (T = lex.next_token())
			{
				arr.push_back(T);
			}
			std::string str = "";
			int sz = arr.size();
			int sub_pos = v.asl == right_asl ? 0 : sz - 1;
			for (int i = 0; i < sz; ++i)
			{
				if (arr[i].name == "param")
				{
					str += i == sub_pos ? r_next : r_this + " ";
				}
				else if (arr[i].name == "piece")
				{
					std::string& s = arr[i].attr->value;
					char* ptr = const_cast<char*>(s.c_str());
					char* pst = ptr;
					while (ptr = strstr(ptr, "\\%"))
					{
						s.erase(ptr - pst, 1);
						*ptr = '%';
					}
					lst.push_back(s);
					str += s + " ";
				}
				else if (arr[i].name == "pack")
				{	// TODO: modify
					str += "exprpk ";
				}
			}
			iR.back().second.push_back({ str, v.func });
			//alert("", str);
		}
		iR.back().second.push_back({ r_next, forward });
		//alert("", r_next);
		r_this = "expr" + int2str(i);
	}
	iR.push_back( { r_this, { { "exprelem", forward } } } );
	//alert(r_this, "base");
	std::sort(lst.begin(), lst.end(),
		[](const std::string &a, const std::string &b){ return a > b; });
	for (auto itr = lst.begin(); itr < lst.end() - 1; ++itr)
	{
		if (*itr == *(itr + 1)) lst.erase(itr);
	}
	const auto escape_lst = "*.?+-$^[](){}|\\/";
	auto gen_reg_expr = [&escape_lst](const std::string& s)
	{
		std::string res;
		for (auto c: s)
		{
			if (strchr(escape_lst, c))
				res += "\\";
			res += c;
		}
		return res;
	};
	for (auto& v: lst)
	{
		lR.first.push_back({ v, gen_reg_expr(v), { no_attr }});
	}
	return lR;
}

void parser::parse(pchar buffer)
{
	std::queue<token> tokens;
	lex.input(buffer);
	do {
		tokens.push(lex.next_token());
	} while (tokens.back());	// $ token eostack bool false
	// if cannot match then alert an error
	std::stack<state> states;	// $ state bottom
	states.push(0);
	std::stack<AST*> signs;
	auto merge = [&](rule_id i)
	{
		auto* p = new gen_node(tokens.front(), rules[i]);
		if (rules[i].signs.size() > 1 || rules[i].signs[0] != empty_sign)
		{
			p->sub.resize(rules[i].sub_count);
			for (auto itr = p->sub.rbegin(); itr != p->sub.rend(); ++itr)
			{
				*itr = signs.top(); signs.pop();
			}
			for (auto& dummy: rules[i].signs) states.pop();
		}
		signs.push(p);
		states.push(GOTO[states.top()][rules[i].src]);
	};
	do {
		auto& sgn = tokens.front().name;
		//std::cout << states.top() << " " << sgn << " " << ACTION[states.top()][sgn] <<std::endl;
		switch (ACTION[states.top()][sgn])
		{
		case a_move_in:
			states.push(GOTO[states.top()][sgn]);	// move into a new state
			if (terms.count(sgn)) signs.push(new term_node(tokens.front()));
			tokens.pop(); break;
		case a_accept:
			if (signs.size() == 1 && !tokens.front()) goto SUCCESS;		// accepted
			else
			{
				while (!signs.empty()) { signs.top()->destroy(); signs.pop(); }
				throw parser_err(tokens.front()); break;
			} 
		case a_error:
			while (!signs.empty()) { signs.top()->destroy(); signs.pop(); }
			throw parser_err(tokens.front()); break;
		default:	// merge rule_id
			/*std::cout << rules[ACTION[states.top()][tokens.front().name]].src << " -> ";
			for (auto& v: rules[ACTION[states.top()][tokens.front().name]].signs)
			{
				std::cout << v + " ";
			} 
			std::cout << std::endl;*/
			merge(ACTION[states.top()][sgn]);
		}
	} while (!tokens.empty());
	while (!signs.empty()) { signs.top()->destroy(); signs.pop(); } return;
	SUCCESS:
	signs.top()->code_gen(&context);
	signs.top()->destroy();
}

}
