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
auto void_ptr_type = llvm::PointerType::getUnqual(lBuilder.getIntNTy((1<<23)-1));
auto int_type = lBuilder.getInt32Ty();
auto float_type = lBuilder.getDoubleTy();
auto char_type = lBuilder.getInt8Ty();
auto bool_type = lBuilder.getInt1Ty();

using type_name_lookup = std::map<llvm::Type*, std::string>;
using type_item = type_name_lookup::value_type;
type_name_lookup type_names =
{
	type_item(void_type, "void"),
	type_item(void_ptr_type, "ptr"),
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
		cast_item(float_type, [](llvm::Value* v){ return lBuilder.CreateFPToSI(v, int_type, "FPToSI"); } ),	// float->int
		cast_item(char_type, [](llvm::Value* v){ return lBuilder.CreateSExt(v, int_type, "SExt"); } ),		// char->int
		cast_item(bool_type, [](llvm::Value* v){ return lBuilder.CreateZExt(v, int_type, "ZExt"); } ),		// bool->int
	}),
	dest_item(char_type, {
		cast_item(int_type, [](llvm::Value* v){ return lBuilder.CreateTrunc(v, char_type, "Trunc"); } ),		// int->char
		cast_item(float_type, [](llvm::Value* v){ return lBuilder.CreateFPToSI(v, char_type, "FPToSI"); } ),	// float->char
		cast_item(bool_type, [](llvm::Value* v){ return lBuilder.CreateZExt(v, char_type, "ZExt"); } ),		// bool->char
	}),
	dest_item(bool_type, {
		cast_item(int_type, [](llvm::Value* v){ return lBuilder.CreateICmpNE(v, llvm::ConstantInt::get(int_type, 0), "ICmpNE"); } ),
		cast_item(float_type, [](llvm::Value* v){ return lBuilder.CreateFCmpONE(v, llvm::ConstantFP::get(float_type, 0), "FCmpONE"); } ),
		cast_item(char_type, [](llvm::Value* v){ return lBuilder.CreateICmpNE(v, llvm::ConstantInt::get(char_type, 0), "ICmpNE"); } ),
	}),
	dest_item(float_type, {
		cast_item(int_type, [](llvm::Value* v){ return lBuilder.CreateSIToFP(v, float_type, "SIToFP"); } ),
		cast_item(bool_type, [](llvm::Value* v){ return lBuilder.CreateUIToFP(v, float_type, "UIToFP"); } ),
		cast_item(char_type, [](llvm::Value* v){ return lBuilder.CreateSIToFP(v, float_type, "SIToFP"); } ),
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

struct init_item
{
	void* value;
	enum {is_constant, is_init_list} flag;
};
using init_vec = std::vector<init_item>;

llvm::Value* create_implicit_cast(llvm::Value* value, llvm::Type* type);
llvm::Value* create_cast(llvm::Value* value, llvm::Type* type);
llvm::Type* get_binary_sync_type(llvm::Value* LHS, llvm::Value* RHS);
llvm::Type* binary_sync_cast(llvm::Value*& LHS, llvm::Value*& RHS, llvm::Type* type = nullptr);
llvm::Value* get_struct_member(llvm::Value* agg, unsigned idx);
llvm::Constant* create_initializer_list(llvm::Type* type, init_vec* init);

enum ltype { integer, floating_point, function, array, pointer, wstruct, overload, void_pointer,
	init_list, rvalue, lvalue, template_func, template_class/*, type*/ };
using ltype_map_type = std::map<ltype, std::string>;
using ltype_item = ltype_map_type::value_type;
static ltype_map_type err_msg =
{
	ltype_item(ltype::integer, "integer"),
	ltype_item(ltype::floating_point, "floating point"),
	ltype_item(ltype::function, "function pointer"),
	ltype_item(ltype::array, "array"),
	ltype_item(ltype::pointer, "pointer"),
	ltype_item(ltype::wstruct, "class"),
	ltype_item(ltype::overload, "overloaded functions"),
	ltype_item(ltype::void_pointer, "generic pointer"),
	ltype_item(ltype::init_list, "initializer list"),
	ltype_item(ltype::rvalue, "rvalue"),
	ltype_item(ltype::lvalue, "lvalue"),
	ltype_item(ltype::template_func, "function template"),
	ltype_item(ltype::template_class, "class template"),
	//ltype_item(ltype::type, "typename"),
};
struct function_meta
{
	llvm::Function* ptr = nullptr;
	enum { is_function, is_method } flag;
	unsigned vtable_id = 0;
	llvm::Value* object = nullptr;
	explicit operator bool () const { return ptr; }
};
using func_sig = std::vector<llvm::Type*>;
func_sig gen_sig(llvm::FunctionType* ft)
{
	func_sig result;
	for (auto itr = ft->param_begin(); itr != ft->param_end(); ++itr)
		result.push_back(*itr);
	return result;
}
using overload_map_type = std::map<func_sig, function_meta>;

using function_params = std::vector<llvm::Type*>;
class template_param_item
{
	void* value;
	enum { is_constant_item, is_type_item } flag;
public:
	template_param_item(llvm::Constant* constant_init):
		value(reinterpret_cast<void*>(constant_init)),
		flag(is_constant_item)
	{}
	template_param_item(llvm::Type* type_init):
		value(reinterpret_cast<void*>(type_init)),
		flag(is_type_item)
	{}
public:
	bool operator < (const template_param_item& other) const
	{
		return value < other.value;
	}
	llvm::Value* get_constant() const
	{
		if (flag != is_constant_item)
			throw err("expected constant value but given typename");
		return reinterpret_cast<llvm::Value*>(value);
	}
	llvm::Type* get_type() const
	{
		if (flag != is_type_item)
			throw err("expected typename but given constant");
		return reinterpret_cast<llvm::Type*>(value);
	}
};
class AST_context;
using template_params = std::vector<template_param_item>;
using template_args_type = std::vector<std::pair<llvm::Type*, std::string>>;
class template_func_meta
{
	template_args_type template_args;
	function_params template_func_params;
	AST& syntax_node;
	std::map<function_params, llvm::Function*> rlist;
public:
	template_func_meta(template_args_type* ta, function_params* params, AST& sn):
		template_args(*ta),
		template_func_params(*params),
		syntax_node(sn)
	{}
	llvm::Function* get_function(const std::vector<llvm::Value*>& params,
		AST_context* context, template_params* ta = nullptr);
};

class template_class_meta
{
	template_args_type template_args;
	AST& syntax_node;
	std::map<template_params, llvm::StructType*> rlist;
public:
	template_class_meta(template_args_type* ta, AST& sn):
		template_args(*ta),
		syntax_node(sn)
	{}
	llvm::StructType* generate_class(const template_params& params, AST_context* context);
};

using function_attr = std::set<unsigned>;
const unsigned is_method = 1;
const unsigned is_virtual = 2;
const unsigned is_override = 4;

const unsigned is_this = 4;
const unsigned is_private = 0;		// 00
const unsigned is_protected = 1;	// 10
const unsigned is_public = 3;		// 11
const unsigned is_inh_visible = 1 | is_this;		//
const unsigned is_out_visible = 2;

template <typename T>
	T concat(const T& s)
		{ return s; }
template <typename T, typename...Args>
	T concat(const T& s, const Args&... args)
		{ return s + ", " + concat(args...); }

class AST_result
{
	void* value = nullptr;
	unsigned attr = 0;
public:
	enum result_type { is_none = 0, is_type, is_lvalue, is_rvalue,
			is_overload, is_custom, is_attr, is_init_list, is_template_function, is_template_class };
	using type_map_type = std::map<result_type, std::string>;
	static type_map_type type_map;
	result_type flag = is_none;
	explicit operator bool () const { return flag; }
public:
	explicit AST_result() = default;
	explicit AST_result(llvm::Type* p):
		value(p),
		flag(is_type)
	{}
	explicit AST_result(llvm::Value* p, bool islvalue):
		value(p),
		flag(islvalue ? is_lvalue : is_rvalue)
	{}
	explicit AST_result(overload_map_type* p):
		value(p),
		flag(is_overload)
	{}
	explicit AST_result(template_func_meta* p):
		value(p),
		flag(is_template_function)
	{}
	explicit AST_result(template_class_meta* p):
		value(p),
		flag(is_template_class)
	{}
	explicit AST_result(void* p):
		value(p),
		flag(is_custom)
	{}
	explicit AST_result(unsigned n):
		attr(n),
		flag(is_attr)
	{}
	explicit AST_result(init_vec* p):
		value(p),
		flag(is_init_list)
	{}
public:
	llvm::Type* get_type() const;
	//llvm::Value* get_lvalue() const;
	//llvm::Value* get_rvalue() const;
	unsigned get_attr() const
		{ if (flag != is_attr) throw err("target is not attribute type"); return attr; }

	template <typename T>
		T* get_data() const
		{
			if (flag != is_custom) throw err("expected custom data, target is " + type_map[flag]);
			return reinterpret_cast<T*>(value);
		}
	template <ltype...U>
		std::pair<llvm::Value*, unsigned> get_among() const
		{
			std::pair<llvm::Value*, unsigned> result;
			if (get_hp<false, 0, U...>(result)) return result;
			if (get_hp<true, 0, U...>(result)) return result;
			throw err("expected " + concat(err_msg[U]...) + ", target is " + type_map[flag]);
		}
	template <ltype...U>
		llvm::Value* get_any_among() const
		{
			std::pair<llvm::Value*, unsigned> result;
			if (get_hp<false, 0, U...>(result)) return result.first;
			if (get_hp<true, 0, U...>(result)) return result.first;
			return nullptr;
		}
	template <ltype T>
		llvm::Value* get_as() const
		{
			if (auto result = get<T>()) return result;
			throw err("expected " + err_msg[T] + ", target is " + type_map[flag]);
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
		llvm::Value* get_casted() const
		{ return nullptr; }
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

AST_result::type_map_type AST_result::type_map =
{
	AST_result::type_map_type::value_type(is_none, "none"),
	AST_result::type_map_type::value_type(is_type, "type"),
	AST_result::type_map_type::value_type(is_lvalue, "lvalue"),
	AST_result::type_map_type::value_type(is_rvalue, "rvalue"),
	AST_result::type_map_type::value_type(is_overload, "overload"),
	AST_result::type_map_type::value_type(is_custom, "custom"),
	AST_result::type_map_type::value_type(is_attr, "function attribute"),
	AST_result::type_map_type::value_type(is_init_list, "initializer list"),
};

// GetType Spec
template <>
	llvm::Value* AST_result::get<ltype::pointer>() const
	{
		auto ptr = reinterpret_cast<llvm::Value*>(value);
		switch (flag)
		{	// cast lvalue to rvalue
		case is_lvalue:
			if (static_cast<llvm::AllocaInst*>(ptr)->getAllocatedType()->isPointerTy())
				if (static_cast<llvm::AllocaInst*>(ptr)->getAllocatedType() != void_ptr_type)
					return lBuilder.CreateLoad(ptr, "Load"); break;
		case is_rvalue: if (ptr->getType()->isPointerTy() && ptr->getType() != void_ptr_type) return ptr; break;
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
				return llvm::GetElementPtrInst::CreateInBounds(ptr, idx, "Decay", lBuilder.GetInsertBlock());
			} break;
		case is_rvalue: if (ptr->getType()->isFunctionTy()) return ptr; break;
		}
		return nullptr;
	}

template <>
	llvm::Value* AST_result::get<ltype::void_pointer>() const
	{
		auto ptr = reinterpret_cast<llvm::Value*>(value);
		switch (flag)
		{	// cast lvalue to rvalue
		case is_lvalue:
			if (static_cast<llvm::AllocaInst*>(ptr)->getAllocatedType()->isPointerTy())
				if (static_cast<llvm::AllocaInst*>(ptr)->getAllocatedType() == void_ptr_type)
					return lBuilder.CreateLoad(ptr, "Load"); break;
		case is_rvalue: if (ptr->getType()->isPointerTy() && ptr->getType() == void_ptr_type) return ptr; break;
		}
		return nullptr;
	}
template <>
	llvm::Value* AST_result::get_casted<ltype::void_pointer>() const
	{
		auto ptr = get<ltype::pointer>();
		if (!ptr) ptr = get_casted<ltype::pointer>();
		if (ptr) return new llvm::BitCastInst(ptr, void_ptr_type, "BitCast", lBuilder.GetInsertBlock());
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
	llvm::Value* AST_result::get<ltype::function>() const
	{
		switch (flag)
		{
		case is_overload: {
			auto ptr = reinterpret_cast<overload_map_type*>(value);
			if (ptr->size() == 1)
			{
				auto & item = ptr->begin()->second;
				if (item.flag == function_meta::is_method)
					throw err("cannot create reference to a class method");
				return reinterpret_cast<llvm::Value*>(item.ptr);
			}
			throw err("ambigious reference to overloaded function");
		}
		case is_rvalue: {
			auto ptr = reinterpret_cast<llvm::Value*>(value);
			if (ptr->getType()->isPointerTy() && static_cast<llvm::PointerType*>(
				ptr->getType())->getElementType()->isFunctionTy()) return ptr;
		}
		default: return nullptr;
		}
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
				return lBuilder.CreateLoad(ptr, "Load"); break;
		case is_rvalue:
			type = ptr->getType();
			if (type->isPointerTy() && static_cast<llvm::PointerType*>(type)->getElementType()->isFunctionTy())
				return ptr; break;
		}
		return nullptr;
	}

template <>
	llvm::Value* AST_result::get<ltype::overload>() const
	{
		if (flag == is_overload)
			return reinterpret_cast<llvm::Value*>(value);
		return nullptr;
	}

template <>
	llvm::Value* AST_result::get<ltype::integer>() const
	{
		auto ptr = reinterpret_cast<llvm::Value*>(value);
		switch (flag)
		{	// cast lvalue to rvalue
		case is_lvalue: if (static_cast<llvm::AllocaInst*>(ptr)->getAllocatedType()->isIntegerTy())
			return lBuilder.CreateLoad(ptr, "Load"); break;
		case is_rvalue: if (ptr->getType()->isIntegerTy()) return ptr; break;
		}
		return nullptr;
	}

template <>
	llvm::Value* AST_result::get<ltype::floating_point>() const
	{
		auto ptr = reinterpret_cast<llvm::Value*>(value);
		switch (flag)
		{	// cast lvalue to rvalue
		case is_lvalue: if (static_cast<llvm::AllocaInst*>(ptr)->getAllocatedType()->isFloatingPointTy())
			return lBuilder.CreateLoad(ptr, "Load"); break;
		case is_rvalue: if (ptr->getType()->isFloatingPointTy()) return ptr; break;
		}
		return nullptr;
	}

template <>
	llvm::Value* AST_result::get<ltype::wstruct>() const
	{
		auto ptr = reinterpret_cast<llvm::Value*>(value);
		switch (flag)
		{	// cast lvalue to rvalue
		case is_lvalue: if (static_cast<llvm::AllocaInst*>(ptr)->getAllocatedType()->isStructTy())
			return ptr; break;
		case is_rvalue: if (ptr->getType()->isStructTy()) return ptr; break;
		}
		return nullptr;
	}

template <>
	llvm::Value* AST_result::get<ltype::init_list>() const
	{
		if (flag == is_init_list)
			return reinterpret_cast<llvm::Value*>(value);
		return nullptr;
	}

template <>
	llvm::Value* AST_result::get<ltype::lvalue>() const
	{
		if (flag == is_lvalue) return reinterpret_cast<llvm::Value*>(value);
		return nullptr;
	}

template <>
	llvm::Value* AST_result::get<ltype::rvalue>() const
	{
		return get_any_among<ltype::integer, ltype::floating_point,
			ltype::pointer, ltype::function, ltype::wstruct, void_pointer>();
	}

template <>
	llvm::Value* AST_result::get<ltype::template_func>() const
	{
		if (flag == is_template_function) return reinterpret_cast<llvm::Value*>(value);
		return nullptr;
	}
template <>
	llvm::Value* AST_result::get<ltype::template_class>() const
	{
		if (flag == is_template_class) return reinterpret_cast<llvm::Value*>(value);
		return nullptr;
	}




class AST_namespace;
class AST_struct_context;
class AST_template_class_context;
class AST_namespace
{
	friend class AST_struct_context;
	friend class AST_template_class_context;
	std::map<llvm::StructType*, AST_struct_context*> typed_namespace_map;
	AST_namespace* parent_namespace = nullptr;
protected:
	enum mapped_value_type { is_none = 0, is_type, is_alloc,
		is_ref,		// add when i need lambda
		is_constant, is_overload_func, is_template_func, is_template_class };
	std::map<std::string, std::pair<void*, mapped_value_type>> name_map;
public:
	AST_namespace(AST_namespace* p):
		parent_namespace(p)
	{}
	virtual ~AST_namespace() = default;
public:
	void add_type(llvm::Type* type, const std::string& name);
	void add_alloc(llvm::Value* alloc, const std::string& name, bool is_reference = false);
	void add_constant(llvm::Value* constant, const std::string& name);
	void add_template_func(template_args_type* ta, function_params* params, const std::string& name, AST& syntax_node) {
		name_map[name].first = new template_func_meta(ta, params, syntax_node);
		name_map[name].second = is_template_func;
	}
	void add_template_class(template_args_type* ta, const std::string& name, AST& syntax_node) {
		name_map[name].first = new template_class_meta(ta, syntax_node);
		name_map[name].second = is_template_class;
	}
	virtual void add_func(llvm::Function* func, const std::string& name, function_attr* fnattr = nullptr);
	// get type
	AST_struct_context* get_namespace(llvm::StructType* p);
	AST_struct_context* get_namespace(llvm::Value* p);
	virtual AST_result get_id(const std::string& name, bool precise = false, unsigned helper = is_this | is_public);
	virtual AST_result get_type(const std::string& name);
	virtual AST_result get_var(const std::string& name);
};

class AST_context;
class AST_template_class_context;
class AST_context: public AST_namespace
{
	friend class AST_template_class_context;
protected:
	AST_context* parent;
public:
	llvm::Type* cur_type = nullptr;
	bool collect_param_name = false;
	std::vector<std::string> function_param_name;
public:
	AST_context(AST_context* p):
		AST_namespace(p),
		parent(p)
	{}
public:
	virtual void alloc_var(llvm::Type* type, const std::string& name, llvm::Value* init) = 0;
	virtual void add_ref(llvm::Value* alloc_ptr, const std::string& name) = 0;
	AST_context* get_global_context()
		{ return parent ? parent->get_global_context() : this; }
	static llvm::BasicBlock* new_block(const std::string& block_name)
		{ return llvm::BasicBlock::Create(llvm::getGlobalContext(), block_name); }
};

llvm::FunctionType* methodlify(llvm::FunctionType* ft);
llvm::FunctionType* functionlify(llvm::FunctionType* ft, llvm::StructType* st);

class AST_struct_context;
class AST_struct_context: public AST_context
{
	struct vmethod_data
	{
		llvm::Function* func;
		bool is_override;
		std::string name;
	};
	std::vector<vmethod_data> vmethod_list;
	using inh_vec = std::vector<AST_struct_context*>;
protected:
	std::map<std::string, unsigned> idx_lookup;
	std::map<std::string, unsigned> visibility_lookup;
	std::vector<llvm::Type*> elems;
	std::vector<llvm::Constant*> vmt;
public:
	unsigned visibility_hwnd = is_public;
	std::string sname;
	std::stack<llvm::Value*> selected;
	llvm::StructType* type = nullptr;
	bool chk_vptr = false;
	bool is_vclass = false;
	llvm::Value* vtable = nullptr;
	AST_struct_context* base = nullptr;
	AST_struct_context(AST_context* p, AST_struct_context* b = nullptr):
		AST_context(p),
		base(b)
	{}
public:
	llvm::Function* get_virtual_function(llvm::Value* obj, unsigned idx) const
	{
		llvm::Value* v = lBuilder.CreateLoad(
			get_struct_member(
				new llvm::BitCastInst(
					lBuilder.CreateLoad(
						get_struct_member(obj, base ? 1 : 0), "LoadVPtr"
					), vtable->getType(), "VMTCast", lBuilder.GetInsertBlock()
				), idx
			), "VMethod"
		);
		return static_cast<llvm::Function*>(v);
	}
	void verify_vmethod()
	{
		chk_vptr = true;
		is_vclass |= base && base->vtable;
	}
	void initialize(llvm::Value* vptr = nullptr)
	{
		if (selected.empty()) throw err("nothing is selected to initialize");
		if (!vptr) vptr = vtable;
		if (vtable)
		{
			llvm::Value* val = new llvm::BitCastInst(vptr, void_ptr_type, "BitCast", lBuilder.GetInsertBlock());
			lBuilder.CreateStore(val, get_struct_member(selected.top(), base ? 1 : 0));
		}
		if (base)
		{
			base->selected.push(get_struct_member(selected.top(), 0));
			base->initialize(vptr);
			base->selected.pop();
		}
	}
	void verify()
	{
		if (is_vclass)
		{
			if (base && base->vtable)
			{
				vmt = base->vmt;
				for (auto& v: vmethod_list) if (v.is_override)
				{	// base ->derived
					bool m_override = false;
					func_sig sig = gen_sig(methodlify(v.func->getFunctionType()));
					if (base->name_map[v.name].second == is_overload_func)
					{
						auto map = reinterpret_cast<overload_map_type*>(base->name_map[v.name].first);
						auto& stg = (*map)[sig];
						if (stg && stg.vtable_id)
						{
							m_override = true;
							if (static_cast<llvm::Function*>(vmt[stg.vtable_id - 1])->getReturnType() !=
								v.func->getReturnType()) throw err("override method returned a different type: " + v.name);
							vmt[stg.vtable_id - 1] = v.func;
							(*reinterpret_cast<overload_map_type*>(name_map[v.name].first))[sig].vtable_id = stg.vtable_id;
						}
					}
					if (!m_override) throw err("override method didn't override anything: " + v.name);
				}
				for (auto& dt: base->name_map)
				{
					if (dt.second.second == is_overload_func)
					{	// for each direct base vmethod
						for (auto& f: *reinterpret_cast<overload_map_type*>(dt.second.first))
						if (f.second.vtable_id)
						{
							if (name_map[dt.first].second == is_none)
							{
								auto ptr = new overload_map_type;
								(*ptr)[gen_sig(methodlify(f.second.ptr->getFunctionType()))] = f.second;
								name_map[dt.first] = make_pair(ptr, mapped_value_type::is_overload_func);
							}
							else if (auto map = reinterpret_cast<overload_map_type*>(name_map[dt.first].first))
							{
								auto stg = (*map)[gen_sig(methodlify(f.second.ptr->getFunctionType()))];
								if (stg && !stg.vtable_id) throw err("base class virtual method has the same function signature: " + dt.first);
								if (!stg) stg = f.second;
							}
							else throw err("cannot recover virtual method by name: " + dt.first);
						}
					}
				}
			}
			else for (auto& v: vmethod_list) if (v.is_override)
					throw err("override method didn't override anything: " + v.name);
			for (auto& v: vmethod_list) if (!v.is_override)
			{
				vmt.push_back(v.func);
				auto map = reinterpret_cast<overload_map_type*>(name_map[v.name].first);
				map->operator[](gen_sig(methodlify(v.func->getFunctionType()))).vtable_id = vmt.size();
			}
			auto cvtable = llvm::ConstantStruct::getAnon(vmt);
			vtable = new llvm::GlobalVariable(*lModule, cvtable->getType(), true,
				llvm::GlobalValue::ExternalLinkage, cvtable, "vtable");
		}
	}
	void alloc_var(llvm::Type* type, const std::string& name, llvm::Value* init) override
	{
		if (type->isFunctionTy()) throw err("cannot create unimplemented function in class context");
		elems.push_back(type);
		if (idx_lookup[name]) throw err("redeclared identifier " + name);
		idx_lookup[name] = elems.size();
		name_map[name].second = is_alloc;
	}
	void add_ref(llvm::Value* alloc_ptr, const std::string& name) override
	{
		/* TODO */
	}
	void set_name_visibility(const std::string& name, unsigned visit_attr) {
		if (!name_map[name].second)
		{
			throw err("cannot set visibility for undefined identifier");
		}
		visibility_lookup[name] = visit_attr | is_this;
	}
	virtual void finish_struct(const std::string& name)
	{
		sname = name;
		if (is_vclass)
		{
			elems.insert(elems.begin(), void_ptr_type);
			for (auto&v : idx_lookup) ++v.second;
		}
		if (base)
		{
			elems.insert(elems.begin(), base->type);
			for (auto& v: idx_lookup) ++v.second;
		}
		if (elems.empty())
		{
			elems.push_back(char_type);
		}
		type = llvm::StructType::create(elems, name);
		parent->add_type(type, name);
		parent->typed_namespace_map[type] = this;
		type_names[type] = name;
	}
	AST_result get_id(const std::string& name, bool precise = false, unsigned visibility = is_this | is_public)
	{
		if (name_map[name].second != is_none)
		{	// check for visibility
			if (precise)
			{
				if (!(visibility_lookup[name] & visibility & is_out_visible))
			 		throw err("identifier " + name + " is invisible in this scope");
			}
			else
			{
				if (!(visibility_lookup[name] & visibility & is_inh_visible))
			 		throw err("identifier " + name + " is invisible in this scope");
			}
		}
		switch (name_map[name].second)
		{
		case is_type: return get_type(name);
		case is_alloc: return get_var(name);
		case is_overload_func: {
			auto map = reinterpret_cast<overload_map_type*>(name_map[name].first);
			for (auto& f: *map) f.second.object = selected.top();
			return AST_result(map);
		}
		case is_constant: return AST_result(reinterpret_cast<llvm::Value*>(name_map[name].first), false);
		case is_none: if (base){
			if (!selected.empty()) base->selected.push(get_struct_member(selected.top(), 0));
			if (!precise && visibility & is_this) visibility &= ~is_this;
			else visibility &= visibility_hwnd;
			auto res = base->get_id(name, precise, visibility);
			if (!selected.empty()) base->selected.pop();
			return res;
		}
		if (parent_namespace && !precise) return parent_namespace->get_id(name, precise);
		else throw err("undefined identifier " + name + " in namespace: " + sname);
		}
	}
	void reg_vmethod(llvm::Function* f, const std::string& name, function_attr* at)
	{
		vmethod_list.push_back({ f, at->count(is_override), name } );
	}
protected:
	AST_result get_var(const std::string& name) override
	{
		if (selected.empty()) throw err("class object not selected");
		if (auto idx = idx_lookup[name])
			return AST_result(get_struct_member(selected.top(), idx - 1), true);
		throw err("class has no variable named " + name);
	}
};

class AST_template_class_context: public AST_struct_context
{
public:
	AST_template_class_context(AST_context* p, AST_struct_context* b = nullptr):
		AST_struct_context(p, b)
	{}
	void finish_struct(const std::string& name) override
	{
		sname = name;
		if (is_vclass)
		{
			elems.insert(elems.begin(), void_ptr_type);
			for (auto&v : idx_lookup) ++v.second;
		}
		if (base)
		{
			elems.insert(elems.begin(), base->type);
			for (auto& v: idx_lookup) ++v.second;
		}
		if (elems.empty())
		{
			elems.push_back(char_type);
		}
		type = llvm::StructType::create(elems, name);
		parent->parent->typed_namespace_map[type] = this;
		type_names[type] = name;
	}
};

class AST_template_context: public AST_context
{
public:
	llvm::Function** func_ptr = nullptr;
	AST_template_context(AST_context* p):
		AST_context(p)
	{}
	void alloc_var(llvm::Type* type, const std::string& name, llvm::Value* init = nullptr) override
		{ throw err("unable to alloc variable within template context"); }
	void add_ref(llvm::Value* alloc_ptr, const std::string& name) override
		{ throw err("unable to add reference within template context"); }
	void set_temporary_func(llvm::Function*& func_storage)
		{ func_ptr = &func_storage; }
};

class AST_global_context: public AST_context
{
public:
	AST_global_context():
		AST_context(nullptr)
	{}
	void alloc_var(llvm::Type* type, const std::string& name, llvm::Value* init = nullptr) override
	{
		if (name == "")
			throw err("cannot alloc a dummy variable");
		if (init)
			init = create_implicit_cast(init, type);
		if (type->isFunctionTy()) throw err("cannot create unimplemented function in local context");
		if (type->isStructTy())
			throw err("cannot define a global object, ctor required");
		auto alloc = new llvm::GlobalVariable(*lModule, type, false,
			llvm::GlobalValue::ExternalLinkage, static_cast<llvm::Constant*>(init));
		add_alloc(alloc, name);
	}
	void add_ref(llvm::Value* alloc_ptr, const std::string& name) override
	{
		if (name == "") throw err("cannot define a dummy reference");
		if (!alloc_ptr->getType()->isPointerTy()) throw err("target allocation not a pointer");
		auto alloc = new llvm::GlobalVariable(*lModule, alloc_ptr->getType(), false,
			llvm::GlobalValue::ExternalLinkage, static_cast<llvm::Constant*>(alloc_ptr));
		add_alloc(alloc, name, true);
	}
};

class AST_basic_local_context: public AST_context
{
protected:
	virtual llvm::BasicBlock* get_alloc_block() const
		{ return static_cast<AST_basic_local_context*>(parent)->get_alloc_block(); }
	virtual llvm::Function* get_local_function()
		{ return static_cast<AST_basic_local_context*>(parent)->get_local_function(); }
protected:
	AST_basic_local_context(AST_context* p):
		AST_context(p),
		block(llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry"))
	{ activate(); }
public:
	llvm::BasicBlock* block;
	AST_basic_local_context(AST_basic_local_context* p):
		AST_context(p),
		block(llvm::BasicBlock::Create(llvm::getGlobalContext(), "block"))
	{ p->make_br(block); set_block(block); }
	virtual ~AST_basic_local_context() override
	{}
public:
	void activate()
		{ lBuilder.SetInsertPoint(block); }
	llvm::BasicBlock* get_block()
		{ return block; }
	void set_block(llvm::BasicBlock* b)
		{ get_local_function()->getBasicBlockList().push_back(block = b); activate(); }
	void make_cond_br(llvm::Value* cond, llvm::BasicBlock* b1, llvm::BasicBlock* b2)
		{ lBuilder.CreateCondBr(create_implicit_cast(cond, bool_type), b1, b2); }
	void make_br(llvm::BasicBlock* b)
		{ lBuilder.CreateBr(b); }
	virtual void make_break()
		{ static_cast<AST_basic_local_context*>(parent)->make_break(); }
	virtual void make_continue()
		{ static_cast<AST_basic_local_context*>(parent)->make_continue(); }
	virtual void make_return(llvm::Value* ret = nullptr)
		{ static_cast<AST_basic_local_context*>(parent)->make_return(ret); }
	void alloc_var(llvm::Type* type, const std::string& name, llvm::Value* init) override
	{
		if (name == "") throw err("cannot alloc a dummy variable");
		if (type->isFunctionTy()) throw err("cannot create unimplemented function in local context");
		if (init) init = create_implicit_cast(init, type);
		lBuilder.SetInsertPoint(get_alloc_block());
		auto alloc = lBuilder.CreateAlloca(type);
		activate();
		if (type->isStructTy())
		{
			auto np = get_namespace(alloc);
			np->initialize();
			np->selected.pop();
		}
		if (init) lBuilder.CreateStore(init, alloc);
		add_alloc(alloc, name);
	}
	void add_ref(llvm::Value* alloc_ptr, const std::string& name) override
	{
		if (name == "") throw err("cannot define a dummy reference");
		if (!alloc_ptr->getType()->isPointerTy()) throw err("target allocation not a pointer");
		lBuilder.SetInsertPoint(get_alloc_block());
		auto alloc = lBuilder.CreateAlloca(alloc_ptr->getType());
		activate();
		lBuilder.CreateStore(alloc_ptr, alloc);
		add_alloc(alloc, name, true);
	}
};

class AST_local_context: public AST_basic_local_context
{
protected:
	AST_local_context(AST_context* p):
		AST_basic_local_context(p)
	{}
public:
	AST_local_context(AST_basic_local_context* p):
		AST_basic_local_context(p)
	{}
	virtual ~AST_local_context() override
		{ static_cast<AST_basic_local_context*>(parent)->block = block; }
};

class AST_loop_context: public AST_local_context
{
public:
	llvm::BasicBlock* loop_next;
	llvm::BasicBlock* loop_end;
public:
	AST_loop_context(AST_local_context* p):
		AST_local_context(p),
		loop_next(new_block("loop_next")),
		loop_end(new_block("loop_end"))
	{}
public:
	void make_break() override
		{ make_br(loop_end); }
	void make_continue() override
		{ make_br(loop_next); }
};

class AST_while_loop_context: public AST_loop_context
{
public:
	llvm::BasicBlock* while_body;
public:
	AST_while_loop_context(AST_local_context* p):
		AST_loop_context(p),
		while_body(new_block("while_body"))
	{}
};

class AST_switch_context: public AST_local_context
{
public:
	llvm::BasicBlock* switch_block;
	llvm::BasicBlock* switch_end;
	llvm::BasicBlock* default_block;
	std::vector<std::pair<llvm::ConstantInt*, llvm::BasicBlock*>> cases;
public:
	AST_switch_context(AST_local_context* p):
		AST_local_context(p),
		switch_block(new_block("switch_body")),
		switch_end(new_block("switch_end")),
		default_block(switch_end)
	{}
public:
	void alloc_var(llvm::Type* type, const std::string& name, llvm::Value* init) override
		{ throw err("unable to allocate variable within switch context"); }
	void add_ref(llvm::Value* alloc_ptr, const std::string& name) override
		{ throw err("unable to add reference within switch context"); }
	void make_break() override
		{ make_br(switch_end); }
};

class AST_function_context: public AST_basic_local_context
{
	llvm::BasicBlock* alloc_block;
	llvm::BasicBlock* entry_block;
	llvm::BasicBlock* return_block;
	llvm::BasicBlock* old_block;
	llvm::Value* retval;
protected:
	std::string fname;
	llvm::BasicBlock* get_alloc_block() const override
		{ return alloc_block; }
	llvm::Function* get_local_function() override
		{ return function; }
public:
	llvm::Function* function;
	AST_function_context(AST_context* p, llvm::Function* F, const std::string& name = "", function_attr* fnattr = nullptr):
		AST_basic_local_context((old_block = lBuilder.GetInsertBlock(), p)),
		function(F),
		fname(name),
		alloc_block(llvm::BasicBlock::Create(llvm::getGlobalContext(), "alloc", F)),
		entry_block(block),
		return_block(llvm::BasicBlock::Create(llvm::getGlobalContext(), "return"))
	{
		if (name != "") p->add_func(F, name, fnattr);
		F->getBasicBlockList().push_back(block);
		if (F->getReturnType() != void_type)
		{
			lBuilder.SetInsertPoint(alloc_block);
			retval = lBuilder.CreateAlloca(F->getReturnType());
			retval->setName("retval");
		}
		lBuilder.SetInsertPoint(block);
	}
	virtual ~AST_function_context() override;
public:
	void make_break() override
		{ throw err("break appears outside loop"); }
	void make_continue() override
		{ throw err("continue appears outside loop"); }
	void make_return(llvm::Value* ret = nullptr) override
	{
		if (!ret)
		{
			if (function->getReturnType() == void_type) make_br(return_block);
			else throw err("function return without value");
		}
		else
		{
			lBuilder.CreateStore(create_implicit_cast(ret, function->getReturnType()), retval);
			make_br(return_block);
		}
	}
	virtual void register_args()
	{
		unsigned i = 0;
		for (auto itr = function->arg_begin(); itr != function->arg_end(); ++itr, ++i)
			if (parent->function_param_name[i] != "")
		{
			itr->setName(parent->function_param_name[i]);
			alloc_var(itr->getType(), parent->function_param_name[i], itr);
		}
	}
};

class AST_method_context: public AST_function_context
{
	function_attr attr;
public:
	AST_method_context(AST_struct_context* p, llvm::FunctionType* ft, const std::string& name, function_attr* fnattr):
		AST_function_context(p, fn2method(p->type, ft, name), name, (fnattr->insert(is_method), fnattr)),
		attr(*fnattr)
	{}
	~AST_method_context() override
	{
		auto p = static_cast<AST_struct_context*>(parent);
		p->selected.pop();
	}
	void register_args() override
	{
		unsigned i = 0;
		add_constant(function->arg_begin(), "this");
		for (auto itr = function->arg_begin(); ++itr != function->arg_end(); ++i)
			if (parent->function_param_name[i] != "")
		{
			itr->setName(parent->function_param_name[i]);
			alloc_var(itr->getType(), parent->function_param_name[i], itr);
		}
		static_cast<AST_struct_context*>(parent)->selected.push(function->arg_begin());
	}
private:
	static llvm::Function* fn2method(llvm::StructType* st, llvm::FunctionType* ft, const std::string& name)
	{
		ft = functionlify(ft, st);
		return llvm::Function::Create(ft, llvm::Function::ExternalLinkage, type_names[st] + "." + name, lModule);
	}
};

}

#include "utility.cpp"

#endif
