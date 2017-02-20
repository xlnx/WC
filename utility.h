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

enum ltype { integer, floating_point, function, array, pointer, wstruct, overload, void_pointer };
struct function_meta
{
	llvm::Function* ptr = nullptr;
	enum { is_function, is_method } flag;
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

using function_attr = std::set<unsigned>;
const unsigned is_method = 1;
const unsigned is_virtual = 2;

class AST_result
{	
	void* value = nullptr;
	unsigned attr = 0;
public:
	enum result_type { is_none = 0, is_type, is_lvalue, is_rvalue, is_overload, is_custom, is_attr };
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
	explicit AST_result(void* p):
		value(p),
		flag(is_custom) 
	{}
	explicit AST_result(unsigned n):
		attr(n),
		flag(is_attr)
	{}
public:
	llvm::Type* get_type() const;
	llvm::Value* get_lvalue() const;
	llvm::Value* get_rvalue() const;
	unsigned get_attr() const
		{ if (flag != is_attr) throw err("target is not attribute type"); return attr; }
	
	template <typename T>
		T* get_data() const
		{
			if (flag != is_custom) throw err("invalid custom data, target is " + type_map[flag]);
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
			throw err("cannot get value, target is " + type_map[flag]);
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

AST_result::type_map_type AST_result::type_map =
{
	AST_result::type_map_type::value_type(is_none, "none"),
	AST_result::type_map_type::value_type(is_type, "type"),
	AST_result::type_map_type::value_type(is_lvalue, "lvalue"),
	AST_result::type_map_type::value_type(is_rvalue, "rvalue"),
	AST_result::type_map_type::value_type(is_overload, "overload"),
	AST_result::type_map_type::value_type(is_custom, "custom")
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
	llvm::Value* AST_result::get_casted<ltype::array>() const
		{ return nullptr; }
	
template <>
	llvm::Value* AST_result::get<ltype::function>() const
	{
		switch (flag)
		{
		case is_overload: {
			auto ptr = reinterpret_cast<overload_map_type*>(value);
			if (ptr->size() == 1) return reinterpret_cast<llvm::Value*>(&ptr->begin()->second);
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
			{
				return lBuilder.CreateLoad(lBuilder.CreateLoad(ptr, "Load"), "Load");
			} break;
		case is_rvalue:
			type = ptr->getType();
			if (type->isPointerTy() && static_cast<llvm::PointerType*>(type)->getElementType()->isFunctionTy())
			{
				return lBuilder.CreateLoad(ptr, "Load");
			}break;
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
	llvm::Value* AST_result::get_casted<ltype::overload>() const
		{ return nullptr; }
	
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
	llvm::Value* AST_result::get_casted<ltype::integer>() const
		{ return nullptr; }
	
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
	llvm::Value* AST_result::get_casted<ltype::floating_point>() const
		{ return nullptr; }
		
template <>
	llvm::Value* AST_result::get<ltype::wstruct>() const
	{
		auto ptr = reinterpret_cast<llvm::Value*>(value);
		switch (flag)
		{	// cast lvalue to rvalue
		case is_lvalue: if (static_cast<llvm::AllocaInst*>(ptr)->getAllocatedType()->isStructTy())
			return ptr; break;
		}
		return nullptr;
	}
template <>
	llvm::Value* AST_result::get_casted<ltype::wstruct>() const
		{ return nullptr; }

class AST_namespace;
class AST_struct_context;
class AST_namespace
{
	friend class AST_struct_context;
	enum mapped_value_type { is_none = 0, is_type, is_alloc, is_constant, is_overload_func };
	std::map<std::string, std::pair<void*, mapped_value_type>> name_map;
	std::map<llvm::StructType*, AST_struct_context*> typed_namespace_map;
	AST_namespace* parent_namespace = nullptr;
public:
	AST_namespace(AST_namespace* p):
		parent_namespace(p)
	{}
	virtual ~AST_namespace() = default;
public:
	void add_type(llvm::Type* type, const std::string& name);
	void add_alloc(llvm::Value* alloc, const std::string& name);
	void add_constant(llvm::Value* constant, const std::string& name);
	virtual void add_func(llvm::Function* func, const std::string& name, function_attr* fnattr = nullptr);
	// get type
	AST_struct_context* get_namespace(llvm::StructType* p);
	AST_struct_context* get_namespace(llvm::Value* p);
	virtual AST_result get_id(const std::string& name, bool precise = false);
	virtual AST_result get_type(const std::string& name);
	virtual AST_result get_var(const std::string& name);
};

class AST_context;
class AST_context: public AST_namespace
{
protected:
	AST_context* parent;
public:
	bool collect_param_name = false;
	std::vector<std::string> function_param_name;
public:
	AST_context(AST_context* p):
		AST_namespace(p),
		parent(p)
	{}
public:
	virtual void alloc_var(llvm::Type* type, const std::string& name, llvm::Value* init) = 0;
	static llvm::BasicBlock* new_block(const std::string& block_name)
		{ return llvm::BasicBlock::Create(llvm::getGlobalContext(), block_name); }
};

class AST_struct_context;
class AST_struct_context: public AST_context
{
	std::vector<llvm::Type*> elems;
	std::map<std::string, unsigned> idx_lookup;
protected:
	static const unsigned not_settled;
	static const unsigned confirmed;
	static const unsigned confirmed_not;
public:
	unsigned has_vbptr = 0;
	llvm::Value* selected = nullptr;
	llvm::StructType* type = nullptr;
	AST_struct_context* base = nullptr;
	AST_struct_context(AST_context* p, AST_struct_context* b = nullptr):
		AST_context(p),
		base(b)
	{}
public:
	void alloc_var(llvm::Type* type, const std::string& name, llvm::Value* init) override
	{
		elems.push_back(type);
		idx_lookup[name] = elems.size();
		name_map[name].second = is_alloc;
	}
	void finish_struct(const std::string& name)
	{
		if (base)
		{
			elems.insert(elems.begin(), base->type);
			for (auto& v: idx_lookup) ++v.second;
		}
		type = llvm::StructType::create(elems, name);
		parent->add_type(type, name);
		parent->typed_namespace_map[type] = this;
		type_names[type] = name;
	}
	AST_result get_id(const std::string& name, bool precise = false) override
	{
		switch (name_map[name].second)
		{
		case is_type: return get_type(name);
		case is_alloc: return get_var(name);
		case is_overload_func: {
			auto map = reinterpret_cast<overload_map_type*>(name_map[name].first);
			for (auto& f: *map) f.second.object = selected;
			selected = nullptr;
			return AST_result(map);
		}
		case is_constant: return AST_result(reinterpret_cast<llvm::Value*>(name_map[name].first), false);
		case is_none: if (base){
				auto sel_back = base->selected;
				std::vector<llvm::Value*> idxs = { lBuilder.getInt64(0), lBuilder.getInt32(0) };
				base->selected = selected ? llvm::GetElementPtrInst::CreateInBounds(
					selected, idxs, "PBase", lBuilder.GetInsertBlock()) : nullptr;
				auto res = base->get_id(name);
				base->selected = sel_back;
				selected = nullptr;
				return res;
			}
			if (parent_namespace && !precise) return parent_namespace->get_id(name);
			else throw err("undefined identifier " + name + " in this namespace");
		}
	}
protected:
	AST_result get_var(const std::string& name) override
	{
		if (!selected) throw err("class object not selected");
		if (auto idx = idx_lookup[name])
		{
			std::vector<llvm::Value*> idxs = { lBuilder.getInt64(0), lBuilder.getInt32(idx - 1) };
			return AST_result(llvm::GetElementPtrInst::CreateInBounds(
				selected, idxs, "PElem", lBuilder.GetInsertBlock()), true);
		}
		throw err("class has no variable named " + name);
	}
};

const unsigned AST_struct_context::not_settled = 0;
const unsigned AST_struct_context::confirmed = 1;
const unsigned AST_struct_context::confirmed_not = 2;

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
		auto alloc = new llvm::GlobalVariable(*lModule, type, false, 
			llvm::GlobalValue::ExternalLinkage, static_cast<llvm::Constant*>(init));
		add_alloc(alloc, name);
	}
};

class AST_local_context: public AST_context
{
protected:
	llvm::BasicBlock* block;
	virtual llvm::BasicBlock* get_alloc_block() const
		{ return static_cast<AST_local_context*>(parent)->get_alloc_block(); }
	virtual llvm::Function* get_local_function()
		{ return static_cast<AST_local_context*>(parent)->get_local_function(); }
protected:
	AST_local_context(AST_context* p):
		AST_context(p),
		block(llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry"))
	{ activate(); }
public:
	AST_local_context(AST_local_context* p):
		AST_context(p),
		block(llvm::BasicBlock::Create(llvm::getGlobalContext(), "block"))
	{ p->make_br(block); set_block(block); }
	virtual ~AST_local_context() override
		{ static_cast<AST_local_context*>(parent)->block = block; }
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
		{ static_cast<AST_local_context*>(parent)->make_break(); }
	virtual void make_continue()
		{ static_cast<AST_local_context*>(parent)->make_continue(); }
	virtual void make_return(llvm::Value* ret = nullptr)
		{ static_cast<AST_local_context*>(parent)->make_return(ret); }
	void alloc_var(llvm::Type* type, const std::string& name, llvm::Value* init) override
	{
		if (name == "")
			throw err("cannot alloc a dummy variable");
		if (init)
			init = create_implicit_cast(init, type);
		lBuilder.SetInsertPoint(get_alloc_block());
		auto alloc = lBuilder.CreateAlloca(type);
		activate();
		if (init) lBuilder.CreateStore(init, alloc);
		add_alloc(alloc, name);
	}
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
	void make_break() override
		{ make_br(switch_end); }
};

class AST_function_context: public AST_local_context
{
	llvm::BasicBlock* alloc_block;
	llvm::BasicBlock* entry_block;
	llvm::BasicBlock* return_block;
	llvm::Value* retval;
protected:
	llvm::Function* function;
	llvm::BasicBlock* get_alloc_block() const override
		{ return alloc_block; }
	llvm::Function* get_local_function() override
		{ return function; }
public:
	AST_function_context(AST_context* p, llvm::Function* F, const std::string& name, function_attr* fnattr = nullptr):
		AST_local_context(p),
		function(F),
		alloc_block(llvm::BasicBlock::Create(llvm::getGlobalContext(), "alloc", F)),
		entry_block(block),
		return_block(llvm::BasicBlock::Create(llvm::getGlobalContext(), "return"))
	{
		p->add_func(F, name, fnattr);
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
		{
			itr->setName(parent->function_param_name[i]);
			alloc_var(itr->getType(), parent->function_param_name[i], itr);
		}
	}
};

class AST_method_context: public AST_function_context
{
public:
	AST_method_context(AST_struct_context* p, llvm::FunctionType* ft, const std::string& name, function_attr* fnattr):
		AST_function_context(p, fn2method(p->type, ft, name), name, (fnattr->insert(is_method), fnattr))
	{}
	~AST_method_context() override
		{ static_cast<AST_struct_context*>(parent)->selected = nullptr; }
	void register_args() override
	{
		unsigned i = 0;
		add_constant(function->arg_begin(), "this");
		for (auto itr = function->arg_begin(); ++itr != function->arg_end(); ++i)
		{
			itr->setName(parent->function_param_name[i]);
			alloc_var(itr->getType(), parent->function_param_name[i], itr);
		}
		static_cast<AST_struct_context*>(parent)->selected = function->arg_begin();
	}
private:
	static llvm::Function* fn2method(llvm::StructType* st, llvm::FunctionType* ft, const std::string& name)
	{	
		std::vector<llvm::Type*> args = { llvm::PointerType::getUnqual(st) };
		for (auto itr = ft->param_begin(); itr != ft->param_end(); ++itr)
			args.push_back(*itr);
		ft = llvm::FunctionType::get(ft->getReturnType(), args, false);
		return llvm::Function::Create(ft, llvm::Function::ExternalLinkage, type_names[st] + "." + name, lModule);
	}
};

}

#include "utility.cpp"

#endif
