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
	explicit operator bool() const { return name != ""; }
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
class AST_result;
using sub_nodes = std::vector<AST*>;
// AST node base

struct AST
{
	AST(token& T): ln(T.ln), col(T.col), ptr(T.ptr) {} 
	friend struct err;
	sub_nodes sub;
	virtual AST_result code_gen(AST_context* context) = 0;
	void append_child(AST* p) { sub.push_back(p); }
	AST& operator[](int i) { return *sub[i]; }
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

class AST_result
{
	//union { llvm::Type* type; llvm::Value* value; void* custom_data; };
	void* value;
public:
	using result_type = enum { is_none = 0, is_type, is_lvalue, is_rvalue, is_custom };
	result_type flag;
	AST_result(): value(nullptr), flag(is_none) {}
	explicit AST_result(llvm::Type* p): value(p), flag(is_type) {}
	//AST_result(llvm::Function* p): value(p), flag(is_rvalue) {}
	AST_result(llvm::Value* p, bool islvalue): value(p), flag(islvalue ? is_lvalue : is_rvalue) {}
	explicit AST_result(void* p): value(p), flag(is_custom) {}
	llvm::Type* get_type() const
	{
		if (flag != is_type) throw err("invalid typename"); 
		return reinterpret_cast<llvm::Type*>(value);
	}
	llvm::Value* get_rvalue() const
	{
		switch (flag)
		{	// cast lvalue to rvalue
		case is_lvalue: return lBuilder.CreateLoad(reinterpret_cast<llvm::Value*>(value));
		case is_rvalue: return reinterpret_cast<llvm::Value*>(value);
		default: throw err("invalid value");
		}
	}
	llvm::Value* get_lvalue() const
	{
		if (flag != is_lvalue) throw err("value cannot be assigned to");
		return reinterpret_cast<llvm::Value*>(value);
	}
	llvm::Function* get_function() const
	{
		if (flag != is_rvalue) throw err("invalid function");
		auto func_ptr = reinterpret_cast<llvm::Value*>(value);
		if (!func_ptr->getType()->isFunctionTy()) throw err("invalid function");
		return static_cast<llvm::Function*>(func_ptr);
	}
	llvm::AllocaInst* get_array() const
	{
		if (flag != is_lvalue) throw err("invalid array");
		auto array_ptr = static_cast<llvm::AllocaInst*>(reinterpret_cast<llvm::Value*>(value));
		if (array_ptr->getAllocatedType()->isArrayTy()) return array_ptr;
		throw err("invalid array");
		//if (!array_ptr->getType()->isArrayTy()) throw err("invalid array");
		//return array_ptr;
	}
	template <typename T> T* get_data() const
	{
		if (flag != is_custom) throw err("invalid custom data");
		return reinterpret_cast<T*>(value);
	}
};

auto void_type = lBuilder.getVoidTy();
auto int_type = lBuilder.getInt32Ty();
auto float_type = lBuilder.getDoubleTy();
auto char_type = lBuilder.getInt8Ty();
auto bool_type = lBuilder.getInt1Ty();

using type_name_lookup = std::map<llvm::Type*, std::string>;
using type_item = type_name_lookup::value_type;
type_name_lookup type_names = 
{
	type_item(void_type, "void"),
	type_item(int_type, "int"),
	type_item(float_type, "float"),
	type_item(char_type, "char"),
	type_item(bool_type, "bool")
};

// use this table to create static cast command
using implicit_cast_lookup = std::map<std::pair<llvm::Type*, llvm::Type*>, std::function<llvm::Value*(llvm::Value*)>>;
using cast_item = implicit_cast_lookup::value_type;
implicit_cast_lookup implicit_casts =
{	// cast from int
	cast_item({int_type, float_type}, [](llvm::Value* v){ return lBuilder.CreateSIToFP(v, float_type); } ),
	cast_item({int_type, char_type}, [](llvm::Value* v){ return lBuilder.CreateTrunc(v, char_type); } ),
	cast_item({int_type, bool_type}, [](llvm::Value* v){ return lBuilder.CreateICmpNE(v, 
		llvm::ConstantInt::get(int_type, 0)); } ),
	// cast from float
	cast_item({float_type, int_type}, [](llvm::Value* v){ return lBuilder.CreateFPToSI(v, int_type); } ),
	cast_item({float_type, char_type}, [](llvm::Value* v){ return lBuilder.CreateFPToSI(v, char_type); } ),	// disable
	cast_item({float_type, bool_type}, [](llvm::Value* v){ return lBuilder.CreateFCmpONE(v, 
		llvm::ConstantFP::get(float_type, 0) ); } ),
	// cast from char
	cast_item({char_type, int_type}, [](llvm::Value* v){ return lBuilder.CreateSExt(v, int_type); } ),
	cast_item({char_type, float_type}, [](llvm::Value* v){ return lBuilder.CreateSIToFP(v, float_type); } ),
	cast_item({char_type, bool_type}, [](llvm::Value* v){ return lBuilder.CreateICmpNE(v,
		llvm::ConstantInt::get(char_type, 0)); } ),
	// cast from bool
	cast_item({bool_type, int_type}, [](llvm::Value* v){ return lBuilder.CreateZExt(v, int_type); } ),
	cast_item({bool_type, float_type}, [](llvm::Value* v){ return lBuilder.CreateUIToFP(v, float_type); } ),
	cast_item({bool_type, char_type}, [](llvm::Value* v){ return lBuilder.CreateZExt(v, char_type); } ),
};

using cast_priority_map = std::map<llvm::Type*, unsigned>;
using priority_item = cast_priority_map::value_type;
cast_priority_map cast_priority =
{
	priority_item(bool_type, 0),
	priority_item(char_type, 1),
	priority_item(int_type, 2),
	priority_item(float_type, 3)
};

llvm::Value* create_implicit_cast(llvm::Value* value, llvm::Type* type)
{
	llvm::Type* cur_type = value->getType();
	if (cur_type != type)
	{
		std::pair<llvm::Type*, llvm::Type*> key = {cur_type, type};
		if (implicit_casts[key]) return implicit_casts[key](value);
		throw err("cannot cast " + type_names[cur_type] + " to " + type_names[type] + " automatically");
	}
	return value;
}

llvm::Type* get_binary_sync_type(llvm::Value* LHS, llvm::Value* RHS)
{
	auto ltype = LHS->getType(), rtype = RHS->getType();
	if (cast_priority[ltype] < cast_priority[rtype]) return rtype;
	return ltype;
}

llvm::Type* binary_sync_cast(llvm::Value*& LHS, llvm::Value*& RHS, llvm::Type* type = nullptr)
{
	if (!type)
	{
		auto ltype = LHS->getType(), rtype = RHS->getType();
		if (cast_priority[ltype] < cast_priority[rtype]) LHS = create_implicit_cast(LHS, rtype);
		else if (cast_priority[ltype] > cast_priority[rtype]) RHS = create_implicit_cast(RHS, ltype);
	}
	else
	{
		LHS = create_implicit_cast(LHS, type);
		RHS = create_implicit_cast(RHS, type);
	}
	return LHS->getType();
}

class AST_namespace;
class AST_namespace
{
	enum mapped_value_type { is_none = 0, is_type, is_alloc, is_func};
	std::map<std::string, std::pair<void*, mapped_value_type>> name_map;
protected:
	AST_namespace* parent_namespace = nullptr;
public:
	AST_namespace() {}
	AST_namespace(AST_namespace* p): parent_namespace(p) {}
	/*void check_conflict(const std::string& name)
	{
		if (name_map[name].second) throw err("name conflicted: " + name);
	}*/
	void add_type(llvm::Type* type, const std::string& name)
	{
		if (name == "") throw err("cannot define a dummy type");
		switch (name_map[name].second)
		{
		case is_type: throw err("redefined type: " + name);
		default: throw err("name conflicted: " + name);
		case is_none: name_map[name] = { type, is_type }; 
		}
	}
	void add_alloc(llvm::Value* alloc, const std::string& name)
	{
		if (name == "") throw err("cannot define a dummy variable");
		switch (name_map[name].second)
		{
		case is_alloc: throw err("redefined variable: " + name);
		default: throw err("name conflicted: " + name);
		case is_none: name_map[name] = { alloc, is_alloc }; 
		}
	}
	void add_func(llvm::Function* func, const std::string& name)
	{
		if (name == "") throw err("cannot define a dummy function");
		switch (name_map[name].second)
		{
		case is_func: throw err("redefined function: " + name);
		default: throw err("name conflicted: " + name);
		case is_none: name_map[name] = { func, is_func }; 
		}
	}
	// get type
	AST_result get_type(const std::string& name)
	{
		switch (name_map[name].second)
		{
		case is_type: return AST_result(reinterpret_cast<llvm::Type*>(name_map[name].first));
		case is_none: if (parent_namespace) return parent_namespace->get_type(name);
			else throw err("undefined type: " + name);
		default: throw err("name \"" + name + "\" in this context is not typename");
		}
	}
	AST_result get_var(const std::string& name)
	{
		switch (name_map[name].second)
		{
		case is_alloc: return AST_result(reinterpret_cast<llvm::Value*>(name_map[name].first), true);
		case is_none: if (parent_namespace) return parent_namespace->get_var(name);
			else throw err("undefined variable: " + name);
		default: throw err("name \"" + name + "\" in this context is not variable");
		}
	}
	AST_result get_func(const std::string& name)
	{
		switch (name_map[name].second)
		{
		case is_func: return AST_result(reinterpret_cast<llvm::Function*>(name_map[name].first), false);
		case is_none: if (parent_namespace) return parent_namespace->get_func(name);
			else throw err("undefined function: " + name);
		default: throw err("name \"" + name + "\" in this context is not function");
		}
	}
	AST_result get_id(const std::string& name)
	{
		switch (name_map[name].second)
		{
		case is_type: return AST_result(reinterpret_cast<llvm::Type*>(name_map[name].first));
		case is_alloc: return AST_result(reinterpret_cast<llvm::Value*>(name_map[name].first), true);
		case is_func: return AST_result(reinterpret_cast<llvm::Function*>(name_map[name].first), false);
		case is_none: if (parent_namespace) return parent_namespace->get_id(name);
			else throw err("undefined identifier: " + name);
		}
	}
};

class AST_context: public AST_namespace
{
	llvm::BasicBlock* alloc_block = nullptr;
	llvm::BasicBlock* entry_block = nullptr;
	llvm::BasicBlock* src_block = nullptr;
	llvm::BasicBlock* block = nullptr;
	llvm::Function* function = nullptr;
	std::set<llvm::BasicBlock*> need_ret;
	//std::map<std::string, llvm::Type*> types;
	//std::map<std::string, llvm::Value*> vars;
	//std::map<std::string, llvm::Function*> funcs;
public:
	llvm::BasicBlock* loop_end = nullptr;
	llvm::BasicBlock* loop_next = nullptr;
	llvm::Type* current_type = nullptr;
	std::string current_name;
	std::vector<std::string> function_param_name;
	bool collect_param_name = false;
	AST_context() {}
	AST_context(AST_context* p, llvm::Function* F): AST_namespace(p),
		alloc_block(llvm::BasicBlock::Create(llvm::getGlobalContext(), "alloc", F)),
		entry_block(llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry", F)),
		block(entry_block),
		need_ret({entry_block}),
		function(F)
	{ lBuilder.SetInsertPoint(block); }
	/*AST_context(AST_context* p, const std::string& block_name):
		parent(p),
		alloc_block(p->alloc_block),
		entry_block(p->entry_block),
		src_block(nullptr),
		block(llvm::BasicBlock::Create(llvm::getGlobalContext(), block_name, p->function)),
		need_ret({block}),
		function(p->function)
	{}*/
	AST_context(AST_context* p): AST_namespace(p),
		alloc_block(p->alloc_block),
		entry_block(p->entry_block),
		src_block(p->block),
		block(p->block),
		need_ret({block}),
		loop_end(p->loop_end),
		loop_next(p->loop_next),
		function(p->function)
	{}
	~AST_context()
	{
		if (src_block)
		{
			auto parent = static_cast<AST_context*>(parent_namespace);
			if (parent->need_ret.count(parent->block))
			{
				parent->need_ret.erase(parent->block);
				if (need_ret.count(block)) parent->need_ret.insert(block);
			}
			parent->block = block;
		}
	}
	
public:
	void activate() { lBuilder.SetInsertPoint(block); }
	void set_block(llvm::BasicBlock* b)
	{
		function->getBasicBlockList().push_back(block = b);
		activate();
	}
	void leave_function(llvm::Value* ret = nullptr)
	{
		need_ret.erase(block);
		if (!ret)
		{
			if (function->getReturnType() == void_type) lBuilder.CreateRetVoid();
			else throw err("function return without value");
		}
		else lBuilder.CreateRet(create_implicit_cast(ret, function->getReturnType()));		
	}
	void cond_jump_to(llvm::Value* cond, llvm::BasicBlock* b1, llvm::BasicBlock* b2)
	{
		lBuilder.CreateCondBr(create_implicit_cast(cond, bool_type), b1, b2);
		if (need_ret.count(block))
		{
			need_ret.insert(b1); need_ret.insert(b2); need_ret.erase(block);
		}
	}
	void jump_to(llvm::BasicBlock* b)
	{
		lBuilder.CreateBr(b);
		if (need_ret.count(block))
		{
			need_ret.insert(b); need_ret.erase(block);
		}
	}
	void finish_func()
	{
		if (function->getReturnType() != void_type)
		{
			if (need_ret.count(block)) throw err("function may return without value");;
		}
		else leave_function();
		lBuilder.SetInsertPoint(alloc_block);
		lBuilder.CreateBr(entry_block);
		llvm::verifyFunction(*function);
		function->dump();
	}
	llvm::Value* alloc_var(llvm::Type* type, const std::string& name, llvm::Value* init = nullptr)
	{
		llvm::Value* alloc;
		if (type == void_type) throw err("cannot create variable of void type");
		if (name == "") throw err("cannot alloc a dummy variable");
		if (init) init = create_implicit_cast(init, type);
		if (alloc_block)
		{
			lBuilder.SetInsertPoint(alloc_block);
			alloc = lBuilder.CreateAlloca(type);
			activate();
			if (init) lBuilder.CreateStore(init, alloc);
			add_alloc(alloc, name);
			return alloc;
		}
		else
		{
			alloc = new llvm::GlobalVariable(*lModule, type, false, 
				llvm::GlobalValue::ExternalLinkage, static_cast<llvm::Constant*>(init));
			add_alloc(alloc, name);
			return alloc;
		}
	}
	llvm::BasicBlock* get_block() { return block; }
	llvm::Function* get_function() { return function; }
public:
	static llvm::BasicBlock* new_block(const std::string& block_name)
	{
		return llvm::BasicBlock::Create(llvm::getGlobalContext(), block_name);
	}
};

}

#endif
