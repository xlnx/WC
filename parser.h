#ifndef __LL_PARSER__HEADER_FILE
#define __LL_PARSER__HEADER_FILE
#include <set>
#include <map>
#include <cstdio>
#include <stack>
#include <queue>
#include "utility.h"
#include "lexer.h"
// TODO: modify this

namespace lr_parser
{

using asl_option = enum { left_asl, right_asl };

struct oper_node;

class parser;
class parser
{
	friend class lexer;
	
	using sign = std::string;
	using action = int;		// > 0: use rule_id; 0: error; -1: move in; -2: accept
	using rule_id = int;	// rule_id > 0 : S -> aBX.
	using state = int;		// state > 0
	using item = std::pair<rule_id, int>;	// rule_id, dot_pos
	using closure = std::set<item>;
	
	static const action a_move_in;
	static const action a_accept;
	static const action a_error;	// S -> aB.X
	
	static const sign stack_bottom;
public:
	static const sign empty_sign;
	using handler = std::function<AST_result(gen_node&, AST_context*)>;
	using matching_callback = std::function<void(parser*, AST&)>;
	using symbol_type = int;
	struct rule
	{
		sign src;
		std::vector<sign> signs;
		int sub_count;
		handler func;
		std::vector<std::pair<int, matching_callback>> on_match;
	};
	struct init_rule_item
	{
		std::string first;
		handler second;
		std::vector<std::pair<int, matching_callback>> on_match;
	};
	struct reinterpret_item
	{
		symbol_type stype;
		std::string target;
	};
	// use a rules list to initialize the parser
	using init_rules = std::vector<std::pair<std::string, std::vector<init_rule_item>>>;
	using expr_init_rules = std::vector<std::vector<oper_node>>;
	using reinterpret_list = std::vector<std::pair<std::string, std::vector<reinterpret_item>>>;
public:
	// no default ctor allowed
	// init with rules and a start node (default "S")
	parser(lexer::init_rules&, init_rules&, expr_init_rules&, const reinterpret_list& = {}, std::string s = "S");
	// derive reserved
	virtual ~parser() = default;
public:
	virtual void parse(pchar buffer);
private:
	lexer::init_rules expr_gen(lexer::init_rules&, init_rules&, expr_init_rules&);
protected:
	AST_global_context context;
	std::set<sign> signs, terms, gens;
	// a map from token name to gen rules
	std::vector<rule> rules;
	std::vector<std::map<sign, action>> ACTION;		// [state][sign]->action->rule_id
	std::vector<std::map<sign, state>> GOTO;		// [state][sign]->state
	std::map<state, matching_callback> matching_callback_map;
	// lexer
	lexer lex;
private:
	std::stack<std::map<std::string, symbol_type>> symbol_lookup;
	std::map<std::string, std::map<symbol_type, std::string>> reinterpret_map;
public:
	static const handler forward;
	static const handler empty;
	static const handler expand;

	static const symbol_type is_type_symbol;
	static const symbol_type is_template_func_symbol;
	static const symbol_type is_template_class_symbol;

	static void enter_block(parser* this_parser, AST& node)
		{ this_parser->symbol_lookup.push(this_parser->symbol_lookup.top()); }
	static void leave_block(parser* this_parser, AST& node)
		{ this_parser->symbol_lookup.pop(); 
			if (this_parser->symbol_lookup.empty()) throw err("error when parsing enclosed scope"); }
	static void register_type(parser* this_parser, AST& node)
		{ this_parser->symbol_lookup.top()[static_cast<term_node&>(node).data.attr->value] = is_type_symbol; }
	static void register_template_func(parser* this_parser, AST& node)
		{
			auto map = std::move(this_parser->symbol_lookup.top());
			this_parser->symbol_lookup.pop();
			this_parser->symbol_lookup.top()[static_cast<term_node&>(node).data.attr->value] = is_template_func_symbol;
			this_parser->symbol_lookup.push(std::move(map));
			this_parser->symbol_lookup.top()[static_cast<term_node&>(node).data.attr->value] = is_template_func_symbol;
		}
	static void register_template_class(parser* this_parser, AST& node)
		{
			auto map = std::move(this_parser->symbol_lookup.top());
			this_parser->symbol_lookup.pop();
			this_parser->symbol_lookup.top()[static_cast<term_node&>(node).data.attr->value] = is_template_class_symbol;
			this_parser->symbol_lookup.push(std::move(map));
			this_parser->symbol_lookup.top()[static_cast<term_node&>(node).data.attr->value] = is_template_class_symbol;
		}
		
	template <unsigned attr>
	static AST_result attribute(gen_node&, AST_context*)
		{ return AST_result(attr); }
};

struct oper_node
{
	std::string mode_str;
	asl_option asl;
	parser::handler func;
};

const int parser::a_move_in = -1;
const int parser::a_accept = -2;
const int parser::a_error = 0;
const std::string parser::stack_bottom = "";
const std::string parser::empty_sign = " ";

struct gen_node: AST
{
	gen_node(token& T, const parser::rule& r): AST(T), func(r.func) {}
	parser::handler func;	// virtual handler pointer
	virtual AST_result code_gen(AST_context* context) override
	{	// handle a gen		
		cur_node = this;
		return func(*this, context);
	}
};

// AST handler
const parser::handler parser::forward = [](gen_node& T, AST_context* context)->AST_result
	{ return T.sub.empty() ? AST_result() : T[0].code_gen(context); };
const parser::handler parser::empty = [](gen_node&, AST_context*)->AST_result
	{ return AST_result(); };
const parser::handler parser::expand = [](gen_node& T, AST_context* context)->AST_result
	{ for (auto p: T.sub) p->code_gen(context); return AST_result(); };

// parser callback
const parser::symbol_type parser::is_type_symbol = 1;
const parser::symbol_type parser::is_template_func_symbol = 2;
const parser::symbol_type parser::is_template_class_symbol = 3;
	
struct expr_gen_err: err
{
	expr_gen_err(const std::string& s): err(s) {}
};

struct parser_err: err
{
	parser_err(const std::string& s, unsigned LN = 0, unsigned COL = 0, pchar PTR = nullptr): err(s, LN, COL, PTR)
	{}
	parser_err(token& t): err("unexpected token: " + (t.attr ? t.attr->value : t.name), t)
	{}
	/*void alert() const override
	{
		err::alert();
	}*/
};

}

// interface
#include "parser.cpp"

#endif
