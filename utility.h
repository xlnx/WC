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
	token(const std::string& n,
		unsigned line,
		unsigned column,
		pchar src_ptr,
		const pointer& attr_ptr = nullptr):
		name(n),
		ln(line),
		col(column),
		ptr(src_ptr),
		attr(attr_ptr)
	{}
	explicit operator bool() const
		{ return name != ""; }
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
	friend struct err;
	sub_nodes sub;
public:
	AST(token& T):
		ln(T.ln),
		col(T.col),
		ptr(T.ptr)
	{} 
	virtual AST_result code_gen(AST_context* context) = 0;
	void append_child(AST* p)
		{ sub.push_back(p); }
	AST& operator[](int i)
		{ return *sub[i]; }
	void destroy()
		{ for (auto& n: sub) n->destroy(); delete this; }
private:
	unsigned ln;
	unsigned col;
	pchar ptr;
};

AST* cur_node = nullptr;

struct err:std::logic_error
{
	err(const std::string& s, AST& T):
		std::logic_error(s),
		ln(T.ln),
		col(T.col),
		ptr(T.ptr)
	{}
	err(const std::string& s, token& T):
		std::logic_error(s),
		ln(T.ln),
		col(T.col),
		ptr(T.ptr)
	{}
	err(const std::string& s): 
		std::logic_error(s),
		ln(cur_node?cur_node->ln:0),
		col(cur_node?cur_node->col:0),
		ptr(cur_node?cur_node->ptr:nullptr)
	{}
	err(const std::string& s,
		unsigned LN,
		unsigned COL,
		pchar PTR):
		std::logic_error(s),
		ln(LN),
		col(COL),
		ptr(PTR)
	{}
	virtual void alert() const;
protected:
	unsigned ln;
	unsigned col;
	pchar ptr;
};

static llvm::Module *lModule = new llvm::Module("LRparser", llvm::getGlobalContext());
static llvm::IRBuilder<> lBuilder(llvm::getGlobalContext());

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
using implicit_cast_lookup = std::map<llvm::Type*, std::function<llvm::Value*(llvm::Value*)>>;
using cast_dest_lookup = std::map<llvm::Type*, implicit_cast_lookup>;
using cast_item = implicit_cast_lookup::value_type;
using dest_item = cast_dest_lookup::value_type;
cast_dest_lookup implicit_casts =
{	// cast from int
	dest_item(int_type, {
		cast_item(float_type, [](llvm::Value* v){ return lBuilder.CreateFPToSI(v, int_type); } ),	// float->int
		cast_item(char_type, [](llvm::Value* v){ return lBuilder.CreateSExt(v, int_type); } ),		// char->int
		cast_item(bool_type, [](llvm::Value* v){ return lBuilder.CreateZExt(v, int_type); } ),		// bool->int
	}),
	dest_item(char_type, {
		cast_item(int_type, [](llvm::Value* v){ return lBuilder.CreateTrunc(v, char_type); } ),		// int->char
		cast_item(float_type, [](llvm::Value* v){ return lBuilder.CreateFPToSI(v, char_type); } ),	// float->char
		cast_item(bool_type, [](llvm::Value* v){ return lBuilder.CreateZExt(v, char_type); } ),		// bool->char
	}),
	dest_item(bool_type, {
		cast_item(int_type, [](llvm::Value* v){ return lBuilder.CreateICmpNE(v, llvm::ConstantInt::get(int_type, 0)); } ),
		cast_item(float_type, [](llvm::Value* v){ return lBuilder.CreateFCmpONE(v, llvm::ConstantFP::get(float_type, 0) ); } ),
		cast_item(char_type, [](llvm::Value* v){ return lBuilder.CreateICmpNE(v, llvm::ConstantInt::get(char_type, 0)); } ),
	}),
	dest_item(float_type, {
		cast_item(int_type, [](llvm::Value* v){ return lBuilder.CreateSIToFP(v, float_type); } ),
		cast_item(bool_type, [](llvm::Value* v){ return lBuilder.CreateUIToFP(v, float_type); } ),
		cast_item(char_type, [](llvm::Value* v){ return lBuilder.CreateSIToFP(v, float_type); } ),
	})
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

llvm::Value* create_implicit_cast(llvm::Value* value, llvm::Type* type);
llvm::Type* get_binary_sync_type(llvm::Value* LHS, llvm::Value* RHS);
llvm::Type* binary_sync_cast(llvm::Value*& LHS, llvm::Value*& RHS, llvm::Type* type = nullptr);

enum ltype { integer, floating_point, function, array, pointer };

class AST_result
{
	void* value = nullptr;
public:
	using result_type = enum { is_none = 0, is_type, is_lvalue, is_rvalue, is_custom };
	result_type flag = is_none;
public:
	explicit AST_result() = default;
	explicit AST_result(llvm::Type* p):
		value(p),
		flag(is_type)
	{}
	AST_result(llvm::Value* p,
		bool islvalue):
		value(p),
		flag(islvalue ? is_lvalue : is_rvalue)
	{}
	explicit AST_result(void* p):
		value(p),
		flag(is_custom) 
	{}
public:
	llvm::Type* get_type() const;
	llvm::Value* get_lvalue() const;
	llvm::Value* get_rvalue() const;
	
	template <typename T>
		T* get_data() const
		{
			if (flag != is_custom) throw err("invalid custom data");
			return reinterpret_cast<T*>(value);
		}
	template <ltype...U>
		std::pair<llvm::Value*, unsigned> get_among() const
		{
			std::pair<llvm::Value*, unsigned> result;
			if (get_hp<false, 0, U...>(result)) return result;
			if (get_hp<true, 0, U...>(result)) return result;
			throw err("cannot get value among ");
		}
	template <ltype...U>
		llvm::Value* get_any_among() const
			{ return get_among<U...>().first; }
	template <ltype T>
		llvm::Value* get_as() const
		{
			if (auto result = get<T>()) return result;
			throw err("cannot get value as ");
		}
	template <ltype T>
		llvm::Value* cast_to(llvm::Type* type) const
		{
			if (auto res = get_casted<T>()) return res;
			throw err("cannot cast " + type_names[reinterpret_cast<llvm::Value*>(value)->getType()]
				+ " to " + type_names[type] + " implicitly");
		}
	template <ltype>
		llvm::Value* get() const;
private:
	template <ltype>
		llvm::Value* get_casted() const;
	template <bool casted, unsigned index, ltype T, ltype...U>
		bool get_hp(std::pair<llvm::Value*, unsigned>& result) const
		{
			if (auto v = casted ? get_casted<T>() : get<T>())
			{
				result = { v, index };
				return true;
			}
			return get_hp<casted, index + 1, U...>(result);
		}
	template <bool, unsigned>
		bool get_hp(std::pair<llvm::Value*, unsigned>&) const
			{ return false; }
};

// GetType Spec
template <>
	llvm::Value* AST_result::get<ltype::pointer>() const
	{	
		auto ptr = reinterpret_cast<llvm::Value*>(value);
		switch (flag)
		{	// cast lvalue to rvalue
		case is_lvalue: if (static_cast<llvm::AllocaInst*>(ptr)->getAllocatedType()->isPointerTy())
			return lBuilder.CreateLoad(ptr); break;
		case is_rvalue: if (ptr->getType()->isPointerTy()) return ptr; break;
		}
		return nullptr;
	}
template <>
	llvm::Value* AST_result::get_casted<ltype::pointer>() const
	{
		auto ptr = reinterpret_cast<llvm::Value*>(value);
		switch (flag)
		{	// cast lvalue to rvalue
		case is_lvalue:
			if (static_cast<llvm::AllocaInst*>(ptr)->getAllocatedType()->isArrayTy())
			{
				std::vector<llvm::Value*> idx = { llvm::ConstantInt::get(int_type, 0),
					llvm::ConstantInt::get(int_type, 0) };
				return llvm::GetElementPtrInst::CreateInBounds(ptr, idx, "", lBuilder.GetInsertBlock());
			} break;
		case is_rvalue: if (ptr->getType()->isFunctionTy()) return ptr; break;
		}
		return nullptr;
	}
	
template <>
	llvm::Value* AST_result::get<ltype::array>() const
	{
		auto ptr = reinterpret_cast<llvm::Value*>(value);
		if (flag == is_lvalue && static_cast<llvm::AllocaInst*>(ptr)->getAllocatedType()->isArrayTy()) 
			return ptr;
		return nullptr;
	}
template <>
	llvm::Value* AST_result::get_casted<ltype::array>() const
		{ return nullptr; }
	
template <>
	llvm::Value* AST_result::get<ltype::function>() const
	{	
		auto ptr = reinterpret_cast<llvm::Value*>(value);
		if (flag == is_rvalue && ptr->getType()->isPointerTy() &&
			static_cast<llvm::PointerType*>(ptr->getType())->getElementType()->isFunctionTy()) 
				return ptr;
		return nullptr;
	}
template <>
	llvm::Value* AST_result::get_casted<ltype::function>() const
	{
		auto ptr = reinterpret_cast<llvm::Value*>(value);
		llvm::Type* type;
		switch (flag)
		{	// cast lvalue to rvalue
		case is_lvalue: 
			type = static_cast<llvm::AllocaInst*>(ptr)->getAllocatedType();
			if (type->isPointerTy() && static_cast<llvm::PointerType*>(type)->getElementType()->isFunctionTy())
			{
				return lBuilder.CreateLoad(lBuilder.CreateLoad(ptr));
			} break;
		case is_rvalue:
			type = ptr->getType();
			if (type->isPointerTy() && static_cast<llvm::PointerType*>(type)->getElementType()->isFunctionTy())
			{
				return lBuilder.CreateLoad(ptr);
			}break;
		}
		return nullptr;
	}
	
template <>
	llvm::Value* AST_result::get<ltype::integer>() const
	{
		auto ptr = reinterpret_cast<llvm::Value*>(value);
		switch (flag)
		{	// cast lvalue to rvalue
		case is_lvalue: if (static_cast<llvm::AllocaInst*>(ptr)->getAllocatedType()->isIntegerTy())
			return lBuilder.CreateLoad(ptr); break;
		case is_rvalue: if (ptr->getType()->isIntegerTy()) return ptr; break;
		}
		return nullptr;
	}
template <>
	llvm::Value* AST_result::get_casted<ltype::integer>() const
		{ return nullptr; }
	
template <>
	llvm::Value* AST_result::get<ltype::floating_point>() const
	{
		auto ptr = reinterpret_cast<llvm::Value*>(value);
		switch (flag)
		{	// cast lvalue to rvalue
		case is_lvalue: if (static_cast<llvm::AllocaInst*>(ptr)->getAllocatedType()->isFloatingPointTy())
			return lBuilder.CreateLoad(ptr); break;
		case is_rvalue: if (ptr->getType()->isFloatingPointTy()) return ptr; break;
		}
		return nullptr;
	}
template <>
	llvm::Value* AST_result::get_casted<ltype::floating_point>() const
		{ return nullptr; }

class AST_namespace;
class AST_namespace
{
	enum mapped_value_type { is_none = 0, is_type, is_alloc, is_func};
	std::map<std::string, std::pair<void*, mapped_value_type>> name_map;
protected:
	AST_namespace* parent_namespace = nullptr;
public:
	AST_namespace() = default;
	AST_namespace(AST_namespace* p):
		parent_namespace(p)
	{}
	virtual ~AST_namespace() = default;
public:
	void add_type(llvm::Type* type, const std::string& name);
	void add_alloc(llvm::Value* alloc, const std::string& name);
	void add_func(llvm::Function* func, const std::string& name);
	// get type
	AST_result get_type(const std::string& name);
	AST_result get_var(const std::string& name);
	AST_result get_func(const std::string& name);
	AST_result get_id(const std::string& name);
};

class AST_context: public AST_namespace
{
	llvm::BasicBlock* alloc_block = nullptr;
	llvm::BasicBlock* entry_block = nullptr;
	llvm::BasicBlock* src_block = nullptr;
	llvm::BasicBlock* block = nullptr;
	llvm::Function* function = nullptr;
	std::set<llvm::BasicBlock*> need_ret;
public:
	llvm::BasicBlock* loop_end = nullptr;
	llvm::BasicBlock* loop_next = nullptr;
	llvm::Type* current_type = nullptr;
	std::string current_name;
	std::vector<std::string> function_param_name;
	bool collect_param_name = false;
public:
	AST_context() {}
	AST_context(AST_context* p, llvm::Function* F): AST_namespace(p),
		alloc_block(llvm::BasicBlock::Create(llvm::getGlobalContext(), "alloc", F)),
		entry_block(llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry", F)),
		block(entry_block),
		need_ret({entry_block}),
		function(F)
	{ lBuilder.SetInsertPoint(block); }
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
	virtual ~AST_context();
public:
	void activate()
		{ lBuilder.SetInsertPoint(block); }
	void set_block(llvm::BasicBlock* b)
		{ function->getBasicBlockList().push_back(block = b); activate(); }
	llvm::BasicBlock* get_block()
		{ return block; }
	llvm::Function* get_function()
		{ return function; }
	
	llvm::Value* alloc_var(llvm::Type* type, const std::string& name, llvm::Value* init = nullptr);
	void cond_jump_to(llvm::Value* cond, llvm::BasicBlock* b1, llvm::BasicBlock* b2);
	void jump_to(llvm::BasicBlock* b);
	void leave_function(llvm::Value* ret = nullptr);
	void finish_func();
public:
	static llvm::BasicBlock* new_block(const std::string& block_name)
		{ return llvm::BasicBlock::Create(llvm::getGlobalContext(), block_name); }
};

}

#include "utility.cpp"

#endif
