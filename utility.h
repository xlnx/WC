#ifndef __LL_UTILS__HEADER_FILE
#define __LL_UTILS__HEADER_FILE
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>
#include <initializer_list>
#include <map>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

namespace lr_parser
{

using pchar = const char*;

struct attr_type;
//std::vector<attr_type> attr_list;

struct token
{
private:
	using pointer = std::shared_ptr<attr_type>;
public:
	token() = default;
	token(const std::string& n, unsigned _ln, unsigned _col, pchar _ptr, const pointer& p = nullptr):
		name(n), ln(_ln), col(_col), ptr(_ptr), attr(p) {}
	explicit operator bool() { return name[0]; }
public:
	std::string name;
	unsigned ln;
	unsigned col;
	pchar ptr;
	pointer attr;
};

struct AST;
struct err;
struct gen_node;
struct term_node;
struct AST_context;
using sub_nodes = std::vector<AST*>;
// AST node base

struct AST_result
{
	using result_type = enum { is_none = 0, is_type, is_lvalue, is_rvalue, is_function, is_custom };
	union
	{
		llvm::Type* type;
		llvm::Value* value;
		llvm::Function* function;
		void* custom_data;
	};
	result_type flag;
	AST_result(): value(nullptr), flag(is_none) {}
	AST_result(llvm::Type* p): type(p), flag(is_type) {}
	AST_result(llvm::Function* p): function(p), flag(is_function) {}
	AST_result(llvm::Value* p, bool islvalue): value(p), flag(islvalue?is_lvalue:is_rvalue) {}
	explicit AST_result(void* p): custom_data(p), flag(is_custom) {}
};

struct AST
{
	AST(token& T): ln(T.ln), col(T.col), ptr(T.ptr) {} 
	friend struct err;
	sub_nodes sub;
	virtual AST_result code_gen(AST_context* context) = 0;
	void append_child(AST* p) { sub.push_back(p); }
	AST& operator[](int i) { return *sub[i]; }
	term_node& operator()(int i) { return *(term_node*)sub[i]; }
	void destroy() { for (auto& n: sub) n->destroy(); delete this; }
private:
	unsigned ln;
	unsigned col;
	pchar ptr;
};
AST* cur_node = nullptr;

struct err:std::logic_error
{	
	err(const std::string& s, AST& T): std::logic_error(s), ln(T.ln), col(T.col), ptr(T.ptr)
	{}
	err(const std::string& s, token& T): std::logic_error(s), ln(T.ln), col(T.col), ptr(T.ptr)
	{}
	err(const std::string& s): std::logic_error(s), ln(cur_node?cur_node->ln:0), col(cur_node?cur_node->col:0),
		ptr(cur_node?cur_node->ptr:nullptr)
	{}
	err(const std::string& s, unsigned LN, unsigned COL, pchar PTR): std::logic_error(s), ln(LN), col(COL), ptr(PTR)
	{}
	virtual void alert() const
	{
		std::cerr << "[" << ln << ", " << col << "]: " << what() << std::endl;
		if (ptr)
		{
			pchar p = ptr;
			while (*p && *p != '\n') std::cerr << *p++;
			std::cerr << std::endl;
			for (p = ptr; p < ptr + col; ++p) std::cerr << (*p != '\t' ? ' ' : '\t');
			std::cerr << '^' << std::endl;
		}
	}
protected:
	unsigned ln;
	unsigned col;
	pchar ptr;
};

static llvm::Module *lModule = new llvm::Module("LRparser", llvm::getGlobalContext());
static llvm::IRBuilder<> lBuilder(llvm::getGlobalContext());

class AST_context
{
	AST_context* parent;
	std::map<std::string, llvm::Type*> types;
	std::map<std::string, llvm::Value*> vars;
	std::map<std::string, llvm::Function*> funcs;
public:
	llvm::BasicBlock* block;
	llvm::Function* function;
	AST_context(AST_context* p, llvm::Function* F): parent(p),
		block(llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry", F)), function(F)
	{}
	AST_context(AST_context* p, const std::string& block_name): parent(p),
		block(llvm::BasicBlock::Create(llvm::getGlobalContext(), block_name, p->function)), function(p->function)
	{}
	AST_context(): parent(nullptr), block(nullptr), function(nullptr)
	{}
	~AST_context()
	{}
	llvm::BasicBlock* new_block(const std::string& block_name, bool append_now = true)
	{
		auto b = llvm::BasicBlock::Create(llvm::getGlobalContext(), block_name);
		if (append_now) append_block(b);
		return b;
	}
	void append_block(llvm::BasicBlock* b)
	{
		function->getBasicBlockList().push_back(block = b);
	}
	AST_result get_type(const std::string& name)
	{
		if (types[name]) return AST_result(types[name]);
		if (parent) return parent->get_type(name);
		throw err("undefined type: " + name);
	}
	AST_result get_var(const std::string& name)
	{
		if (vars[name]) return AST_result(vars[name], true);
		if (parent) return parent->get_var(name);
		throw err("undefined variable: " + name);
	}
	AST_result get_func(const std::string& name)
	{
		if (funcs[name]) return AST_result(funcs[name]);
		if (parent) return parent->get_func(name);
		throw err("undefined function: " + name);
	}
	void check_conflict(const std::string& name)
	{
		if (types[name] || vars[name] || funcs[name]) throw err("name conflicted: " + name);
	}
	void add_type(const std::string& name, llvm::Type* type)
	{
		if (types[name]) throw err("redefined type: " + name);
		check_conflict(name);
		types[name] = type;
	}
	void add_var(const std::string& name, llvm::Value* value)
	{
		if (vars[name]) throw err("redefined variable: " + name);
		check_conflict(name);
		vars[name] = value;
	}
	void add_func(const std::string& name, llvm::Function* func)
	{
		if (funcs[name]) throw err("redefined function: " + name);
		check_conflict(name);
		funcs[name] = func;
	}
	AST_result get_id(const std::string& name)
	{
		if (types[name]) return AST_result(types[name]);
		if (vars[name]) return AST_result(vars[name], true);
		if (funcs[name]) return AST_result(funcs[name]);
		if (parent) return parent->get_id(name);
		throw err("undefined identifier: " + name);
	}
};

}

#endif
