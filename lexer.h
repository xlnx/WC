#ifndef __LL_LEXER__HEADER_FILE
#define __LL_LEXER__HEADER_FILE
#include <regex>
#include <set>
#include <stack>
#include <functional>
#include "utility.h"

namespace lr_parser
{
	// word : this token must be whole word
	// no_attr : this token needs no attr so the lexer dont need to record default attrs.
using reg_option = enum { word, no_attr, ignore_case };

class lexer_base
{
	struct rule
	{
		std::string token_name;
		std::regex mode;
		bool no_attr;
	};
public:
	struct init_rule
	{
		std::string token_name, mode;
		std::set<reg_option> opts;
	};
	using init_rules = std::vector<init_rule>;
public:
	lexer_base(const init_rules& iR);
	virtual ~lexer_base() = default;
	void input(pchar p);
	virtual token next_token();
	std::map<unsigned, pchar> ln_lookup;
protected:
	std::string buffer;
	pchar cur_ptr = nullptr;
	std::vector<rule> rules_list;
	unsigned ln, col;
};

const std::set<char> spaces = { ' ', '\n', '\r', '\t' };

class lexer: public lexer_base
{
public:
	// record a rule
	using handler = std::function<AST_result(term_node&, AST_context*)>;
	using helper_init_rule = std::pair<std::string, handler>;
	using helper_init_rules = std::vector<helper_init_rule>;
	using init_rules = std::pair<lexer_base::init_rules, helper_init_rules>;
public:
	lexer(const init_rules& h);
	virtual ~lexer() = default;
	virtual token next_token() override;
protected:
	// no sub rules allowed
	std::map<std::string, handler> handler_lookup;
};

struct attr_type
{	
	std::string value;
	lexer::handler func;
	attr_type(std::string&& s = "", lexer::handler f = [](term_node&, AST_context*)->AST_result{}):
		value(std::forward<std::string>(s)), func(f) {}
};

struct term_node: AST
{
	term_node(token& T): AST(T), data(T) {}
	token data;
	virtual AST_result code_gen(AST_context* context) override
	{	// handle a term
		cur_node = this;
		return data.attr->func(*this, context);
	}
};

struct lex_err: err
{
	lex_err(unsigned p, unsigned t, pchar ptr): err("invalid token:", p, t, ptr)
	{}
	//void alert() const override
};

}

#include "lexer.cpp"

#endif
