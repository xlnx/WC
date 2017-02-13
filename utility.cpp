// AST_result
namespace lr_parser
{

llvm::Type* AST_result::get_type() const
{
	if (flag != is_type) throw err("invalid typename"); 
	return reinterpret_cast<llvm::Type*>(value);
}

llvm::Value* AST_result::get_rvalue() const
{
	switch (flag)
	{	// cast lvalue to rvalue
	case is_lvalue: return lBuilder.CreateLoad(reinterpret_cast<llvm::Value*>(value));
	case is_rvalue: return reinterpret_cast<llvm::Value*>(value);
	default: throw err("invalid rvalue");
	}
}

llvm::Value* AST_result::get_lvalue() const
{
	if (flag == is_rvalue) throw err("cannot take address of expression");
	if (flag != is_lvalue) throw err("invalid lvalue expression");
	return reinterpret_cast<llvm::Value*>(value);
}

llvm::Function* AST_result::get_function() const
{
	if (auto func_ptr = get<llvm::FunctionType>())
		return static_cast<llvm::Function*>(func_ptr);
	throw err("invalid function");
}
	
llvm::AllocaInst* AST_result::get_array() const
{
	if (auto array_ptr = get<llvm::ArrayType>())
	 	return static_cast<llvm::AllocaInst*>(array_ptr);
	throw err("invalid array");
}

// AST_namespace
void AST_namespace::add_type(llvm::Type* type, const std::string& name)
{
	if (name == "") throw err("cannot define a dummy type");
	switch (name_map[name].second)
	{
	case is_type: throw err("redefined type: " + name);
	default: throw err("name conflicted: " + name);
	case is_none: name_map[name] = { type, is_type }; 
	}
}

void AST_namespace::add_alloc(llvm::Value* alloc, const std::string& name)
{
	if (name == "") throw err("cannot define a dummy variable");
	switch (name_map[name].second)
	{
	case is_alloc: throw err("redefined variable: " + name);
	default: throw err("name conflicted: " + name);
	case is_none: name_map[name] = { alloc, is_alloc }; 
	}
}
void AST_namespace::add_func(llvm::Function* func, const std::string& name)
{
	if (name == "") throw err("cannot define a dummy function");
	switch (name_map[name].second)
	{
	case is_func: throw err("redefined function: " + name);
	default: throw err("name conflicted: " + name);
	case is_none: name_map[name] = { func, is_func }; 
	}
}

AST_result AST_namespace::get_type(const std::string& name)
{
	switch (name_map[name].second)
	{
	case is_type: return AST_result(reinterpret_cast<llvm::Type*>(name_map[name].first));
	case is_none: if (parent_namespace) return parent_namespace->get_type(name);
		else throw err("undefined type: " + name);
	default: throw err("name \"" + name + "\" in this context is not typename");
	}
}

AST_result AST_namespace::get_var(const std::string& name)
{
	switch (name_map[name].second)
	{
	case is_alloc: return AST_result(reinterpret_cast<llvm::Value*>(name_map[name].first), true);
	case is_none: if (parent_namespace) return parent_namespace->get_var(name);
		else throw err("undefined variable: " + name);
	default: throw err("name \"" + name + "\" in this context is not variable");
	}
}

AST_result AST_namespace::get_func(const std::string& name)
{
	switch (name_map[name].second)
	{
	case is_func: return AST_result(reinterpret_cast<llvm::Function*>(name_map[name].first), false);
	case is_none: if (parent_namespace) return parent_namespace->get_func(name);
		else throw err("undefined function: " + name);
	default: throw err("name \"" + name + "\" in this context is not function");
	}
}

AST_result AST_namespace::get_id(const std::string& name)
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

// AST_context
AST_context::~AST_context()
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

void AST_context::leave_function(llvm::Value* ret)
{
	need_ret.erase(block);
	if (!ret)
	{
		if (function->getReturnType() == void_type) lBuilder.CreateRetVoid();
		else throw err("function return without value");
	}
	else lBuilder.CreateRet(create_implicit_cast(ret, function->getReturnType()));		
}

void AST_context::cond_jump_to(llvm::Value* cond, llvm::BasicBlock* b1, llvm::BasicBlock* b2)
{
	lBuilder.CreateCondBr(create_implicit_cast(cond, bool_type), b1, b2);
	if (need_ret.count(block))
	{
		need_ret.insert(b1); need_ret.insert(b2); need_ret.erase(block);
	}
}

void AST_context::jump_to(llvm::BasicBlock* b)
{
	lBuilder.CreateBr(b);
	if (need_ret.count(block))
	{
		need_ret.insert(b); need_ret.erase(block);
	}
}

void AST_context::finish_func()
{
	if (function->getReturnType() != void_type)
	{
		if (need_ret.count(block)) throw err("function may return without value");;
	}
	else 
	{
		if (need_ret.count(block)) leave_function();
	}
	lBuilder.SetInsertPoint(alloc_block);
	lBuilder.CreateBr(entry_block);
	llvm::verifyFunction(*function);
	function->dump();
}

llvm::Value* AST_context::alloc_var(llvm::Type* type, const std::string& name, llvm::Value* init)
{
	if (type == void_type) throw err("cannot create variable of void type");
	if (name == "") throw err("cannot alloc a dummy variable");
	if (init) init = create_implicit_cast(init, type);
	llvm::Value* alloc;
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

}
