namespace lr_parser
{

void err::alert() const
{
	std::cerr << "wc: ";
	(ptr ? std::cerr << "[" << ln << ", " << col << "]: " : std::cerr) << what() << std::endl;
	if (ptr)
	{
		pchar p = ptr;
		while (*p && *p != '\n') std::cerr << *p++;
		std::cerr << std::endl;
		for (p = ptr; p < ptr + col; ++p) std::cerr << (*p != '\t' ? ' ' : '\t');
		std::cerr << '^';
	}
}

llvm::Value* create_implicit_cast(llvm::Value* value, llvm::Type* type)
{
	llvm::Type* cur_type = value->getType();
	if (cur_type != type)
	{
		if (implicit_casts[type][cur_type]) return implicit_casts[type][cur_type](value);
		AST_result res(value, false);
		if (type == int_type || type == char_type || type == bool_type) return res.cast_to<ltype::integer>(type);
		if (type == float_type) return res.cast_to<ltype::floating_point>(type);
		if (type->isPointerTy()) return res.cast_to<ltype::pointer>(type);
		if (type->isFunctionTy()) return res.cast_to<ltype::function>(type);
		if (type->isArrayTy()) return res.cast_to<ltype::array>(type);
		throw err("unexpected typename in casting");
	}
	return value;
}

llvm::Type* get_binary_sync_type(llvm::Value* LHS, llvm::Value* RHS)
{
	auto ltype = LHS->getType(), rtype = RHS->getType();
	if (cast_priority[ltype] < cast_priority[rtype]) return rtype;
	return ltype;
}

llvm::Type* binary_sync_cast(llvm::Value*& LHS, llvm::Value*& RHS, llvm::Type* type)
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

llvm::Type* AST_result::get_type() const
{
	if (flag != is_type) throw err("invalid typename"); 
	return reinterpret_cast<llvm::Type*>(value);
}

llvm::Value* AST_result::get_lvalue() const
{
	if (flag == is_rvalue) throw err("cannot take address of expression");
	if (flag != is_lvalue) throw err("invalid lvalue expression");
	return reinterpret_cast<llvm::Value*>(value);
}

llvm::Value* AST_result::get_rvalue() const
{
	return get_any_among<ltype::integer, ltype::floating_point, ltype::pointer, ltype::function>(); 
}

// AST_namespace
void AST_namespace::add_type(llvm::Type* type, const std::string& name)
{
	if (name == "") throw err("cannot define a dummy type");
	switch (name_map[name].second)
	{
	case is_type: throw err("redefined type: " + name);
	default: throw err("name conflicted: " + name);
	case is_none: name_map[name].second = is_type; name_map[name].first.ptr = type;
	}
}

void AST_namespace::add_alloc(llvm::Value* alloc, const std::string& name)
{
	if (name == "") throw err("cannot define a dummy variable");
	switch (name_map[name].second)
	{
	case is_alloc: throw err("redefined variable: " + name);
	default: throw err("name conflicted: " + name);
	case is_none: name_map[name].second = is_alloc; name_map[name].first.ptr = alloc; alloc->setName(name); 
	}
}
void AST_namespace::add_func(llvm::Function* func, const std::string& name)
{
	if (name == "") throw err("cannot define a dummy function");
	auto type = func->getType();
	switch (name_map[name].second)
	{
	case is_overload_func:
		if (name_map[name].first.overload_map[type])
			throw err("redefined function: " + name + " with signature " + type_names[type]);
		name_map[name].first.overload_map[type] = func; break;
	default: throw err("name conflicted: " + name);
	case is_none: name_map[name].second = is_overload_func;
		name_map[name].first.ptr = func;//overload_map[type] = func;
	}
}

AST_result AST_namespace::get_type(const std::string& name)
{
	switch (name_map[name].second)
	{
	case is_type: return AST_result(reinterpret_cast<llvm::Type*>(name_map[name].first.ptr));
	case is_none: if (parent_namespace) return parent_namespace->get_type(name);
		else throw err("undefined type: " + name);
	default: throw err("name \"" + name + "\" in this context is not typename");
	}
}

AST_result AST_namespace::get_var(const std::string& name)
{
	switch (name_map[name].second)
	{
	case is_alloc: return AST_result(reinterpret_cast<llvm::Value*>(name_map[name].first.ptr), true);
	case is_overload_func: return AST_result(reinterpret_cast<llvm::Value*>(name_map[name].first.ptr), false);
	case is_none: if (parent_namespace) return parent_namespace->get_var(name);
		else throw err("undefined variable: " + name);
	default: throw err("name \"" + name + "\" in this context is not variable");
	}
}

/*AST_result AST_namespace::get_func(const std::string& name)
{
	switch (name_map[name].second)
	{
	case is_func: return AST_result(reinterpret_cast<llvm::Function*>(name_map[name].first), false);
	case is_none: if (parent_namespace) return parent_namespace->get_func(name);
		else throw err("undefined function: " + name);
	default: throw err("name \"" + name + "\" in this context is not function");
	}
}*/

AST_result AST_namespace::get_id(const std::string& name)
{
	switch (name_map[name].second)
	{
	case is_type: return AST_result(reinterpret_cast<llvm::Type*>(name_map[name].first.ptr));
	case is_alloc: return AST_result(reinterpret_cast<llvm::Value*>(name_map[name].first.ptr), true);
	case is_overload_func: return AST_result(reinterpret_cast<llvm::Function*>(name_map[name].first.ptr), false/*overload_map*/);
	case is_none: if (parent_namespace) return parent_namespace->get_id(name);
		else throw err("undefined identifier: " + name);
	}
}

// AST_context
AST_function_context::~AST_function_context()
{
	lBuilder.SetInsertPoint(alloc_block);
	lBuilder.CreateBr(entry_block);
	
	function->getBasicBlockList().push_back(return_block);
	lBuilder.SetInsertPoint(block);
	lBuilder.CreateBr(return_block);
	
	lBuilder.SetInsertPoint(return_block);
	if (function->getReturnType() == void_type)
		lBuilder.CreateRetVoid();
	else
		lBuilder.CreateRet(lBuilder.CreateLoad(retval, "retval_load"));
	function->setCallingConv(llvm::CallingConv::C);
	
	llvm::AttributeSet function_attrs;
	llvm::SmallVector<llvm::AttributeSet, 4> attrs;
	llvm::AttributeSet PAS;
	{
		llvm::AttrBuilder B;
		B.addAttribute(llvm::Attribute::NoUnwind);
		B.addAttribute(llvm::Attribute::NoInline);
		PAS = llvm::AttributeSet::get(lModule->getContext(), ~0U, B);
	}
	attrs.push_back(PAS);
	function_attrs = llvm::AttributeSet::get(lModule->getContext(), attrs);
	function->setAttributes(function_attrs);
	
	llvm::verifyFunction(*function);
	#ifdef WC_DEBUG
	function->dump();
	#endif
}

}
