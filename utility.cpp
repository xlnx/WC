namespace lr_parser
{

void err::alert() const
{
	std::cerr << "wc: ";
	(ptr ? std::cerr << ln + 1 << ": " << col << ": " : std::cerr) << what() << std::endl;
	if (ptr)
	{
		pchar p = ptr;
		while (*p && *p != '\n') std::cerr << *p++;
		std::cerr << std::endl;
		for (p = ptr; p < ptr + col; ++p) std::cerr << (*p != '\t' ? ' ' : '\t');
		std::cerr << '^';
	}
}

static llvm::Value* try_create_implicit_cast(llvm::Value* value, llvm::Type* type)
{
	llvm::Type* cur_type = value->getType();
	if (cur_type != type)
	{
		if (implicit_casts[type][cur_type]) return implicit_casts[type][cur_type](value);
		AST_result res(value, false);
		if (type == int_type || type == char_type || type == bool_type) return res.cast_to<ltype::integer>(type);
		if (type == float_type) return res.cast_to<ltype::floating_point>(type);
		if (type->isFunctionTy()) return res.cast_to<ltype::function>(type);
		if (type->isPointerTy())
		{
			if (type == void_ptr_type) return res.cast_to<ltype::void_pointer>(type);
			return res.cast_to<ltype::pointer>(type);
		}
		if (type->isArrayTy()) return res.cast_to<ltype::array>(type);
		return nullptr;
	}
	return value;
}

llvm::Value* create_implicit_cast(llvm::Value* value, llvm::Type* type)
{
	if (auto cast = try_create_implicit_cast(value, type)) return cast;
	throw err("cannot cast " + type_names[value->getType()] + " to " + type_names[type] + " implicitly");
}

llvm::Value* create_cast(llvm::Value* value, llvm::Type* type)
{
	if (auto cast = try_create_implicit_cast(value, type)) return cast;
	throw err("cannot cast " + type_names[value->getType()] + " to " + type_names[type] + " explicitly");
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

llvm::Value* get_struct_member(llvm::Value* agg, unsigned idx)
{
	std::vector<llvm::Value*> idxs = { lBuilder.getInt64(0), lBuilder.getInt32(idx) };
	return llvm::GetElementPtrInst::CreateInBounds(agg, idxs, "PMember", lBuilder.GetInsertBlock());
}

llvm::Constant* create_initializer_list(llvm::Type* type, init_vec* init)
{
	if (!type)
	{
		std::vector<llvm::Constant*> vec;
		for (auto& item: *init)
		{
			if (item.flag == init_item::is_constant)
			{
				vec.push_back(reinterpret_cast<llvm::Constant*>(item.value));
			}
			else
			{
				vec.push_back(create_initializer_list(type, reinterpret_cast<init_vec*>(item.value)));
			}
			if (vec.back()->getType() != vec.front()->getType())
				throw err("annoymous array element type conflicted");
		}
		delete init;
		return llvm::ConstantArray::get(llvm::ArrayType::get(vec.front()->getType(), vec.size()), vec);
	}
	else
	{
		if (type->isArrayTy())
		{
			auto at = static_cast<llvm::ArrayType*>(type);
			std::vector<llvm::Constant*> vec;
			auto elem_ty = at->getElementType();
			auto eitr = init->begin();
			for (unsigned i = 0; i != at->getNumElements(); ++i, ++eitr)
			{
				if (eitr >= init->end()) throw err("not every member of " + type_names[type] + " is initialized");
				else if (eitr->flag == init_item::is_constant)
				{
					vec.push_back(static_cast<llvm::Constant*>(
						create_implicit_cast(reinterpret_cast<llvm::Constant*>(eitr->value), elem_ty)));
				}
				else
				{
					vec.push_back(create_initializer_list(elem_ty, reinterpret_cast<init_vec*>(eitr->value)));
				}
			}
			delete init;
			return llvm::ConstantArray::get(at, vec);
		}
		else if (type->isStructTy())
		{
			auto st = static_cast<llvm::StructType*>(type);
			std::vector<llvm::Constant*> vec;
			auto eitr = init->begin();
			for (auto itr = st->element_begin(); itr != st->element_end(); ++itr, ++eitr)
			{
				if (eitr >= init->end()) throw err("not every member of " + type_names[type] + " is initialized");
				else if (eitr->flag == init_item::is_constant)
				{
					vec.push_back(static_cast<llvm::Constant*>(
						create_implicit_cast(reinterpret_cast<llvm::Constant*>(eitr->value), *itr)));
				}
				else
				{
					vec.push_back(create_initializer_list(*itr, reinterpret_cast<init_vec*>(eitr->value)));
				}
			}
			delete init;
			return llvm::ConstantStruct::get(st, vec);
		}
		else throw err("initializer list can only be assigned to aggregrate type");
	}
}



llvm::Type* AST_result::get_type() const
{
	if (flag != is_type) throw err("invalid typename"); 
	return reinterpret_cast<llvm::Type*>(value);
}

llvm::FunctionType* methodlify(llvm::FunctionType* type)
{
	std::vector<llvm::Type*> args;
	for (auto itr = type->param_begin(); ++itr != type->param_end();)
		args.push_back(*itr);
	return llvm::FunctionType::get(type->getReturnType(), args, false);
}

llvm::FunctionType* functionlify(llvm::FunctionType* ft, llvm::StructType* st)
{
	std::vector<llvm::Type*> args = { llvm::PointerType::getUnqual(st) };
	for (auto itr = ft->param_begin(); itr != ft->param_end(); ++itr)
		args.push_back(*itr);
	return llvm::FunctionType::get(ft->getReturnType(), args, false);
}


// AST_namespace
void AST_namespace::add_type(llvm::Type* type, const std::string& name)
{
	if (name == "") throw err("cannot define a dummy type");
	switch (name_map[name].second)
	{
	case is_type: throw err("redefined type: " + name);
	default: throw err("name conflicted: " + name);
	case is_none: name_map[name].second = is_type; name_map[name].first = type;
	}
}

void AST_namespace::add_alloc(llvm::Value* alloc, const std::string& name, bool is_reference)
{
	if (name == "") throw err("cannot define a dummy variable");
	switch (name_map[name].second)
	{
	case is_alloc: throw err("redefined variable: " + name);
	default: throw err("name conflicted: " + name);
	case is_none: name_map[name].second = is_reference ? is_ref : is_alloc;
		name_map[name].first = alloc; alloc->setName(name); 
	}
}

void AST_namespace::add_constant(llvm::Value* constant, const std::string& name)
{
	if (name == "") throw err("cannot define a dummy variable");
	switch (name_map[name].second)
	{
	case is_constant: throw err("redefined constant: " + name);
	default: throw err("name conflicted: " + name);
	case is_none: name_map[name].second = is_constant; name_map[name].first = constant;
	}
}

void AST_namespace::add_func(llvm::Function* func, const std::string& name, function_attr* fnattr)
{
	if (name == "") throw err("cannot define a dummy function");
	auto type = func->getFunctionType();
	if (fnattr && fnattr->count(is_method))
		type = methodlify(type);
	switch (name_map[name].second)
	{
	case is_overload_func: {
		auto map = reinterpret_cast<overload_map_type*>(name_map[name].first);
		auto& fndata = map->operator[](gen_sig(type));
		if (fndata) throw err("redefined function: " + name + " with signature " + type_names[type]);
		fndata.ptr = func;
		if (fnattr && fnattr->count(is_method))
		{
			fndata.flag = function_meta::is_method;
			if (fnattr->count(is_virtual)) static_cast<AST_struct_context*>(this)->reg_vmethod(func, name, fnattr);
			//fndata.object = static_cast<AST_struct_context*>(this)->selected;
		}
		else fndata.flag = function_meta::is_function; break;
	}
	default: throw err("name conflicted: " + name);
	case is_none: {
		name_map[name].second = is_overload_func;
		auto map = new overload_map_type;
		auto& fndata = map->operator[](gen_sig(type));
		fndata.ptr = func;
		if (fnattr && fnattr->count(is_method))
		{
			fndata.flag = function_meta::is_method;
			if (fnattr->count(is_virtual)) static_cast<AST_struct_context*>(this)->reg_vmethod(func, name, fnattr);
			//fndata.parent = static_cast<AST_struct_context*>(this);
		}
		else fndata.flag = function_meta::is_function;
		name_map[name].first = map;
	}
	}
}

AST_struct_context* AST_namespace::get_namespace(llvm::StructType* p)
{
	if (!p->isStructTy()) throw err("target not struct type");
	if (auto ptr = typed_namespace_map[p])
	{
		return ptr;
	}
	else
	{
		if (!parent_namespace) throw err("target type not found in this namespace");
		return parent_namespace->get_namespace(p);
	}
}

AST_struct_context* AST_namespace::get_namespace(llvm::Value* p)
{
	if (!p->getType()->isPointerTy() && !static_cast<llvm::PointerType*>(p->getType())->
		getElementType()->isStructTy()) throw err("target not struct type");
	if (auto ptr = typed_namespace_map[static_cast<llvm::StructType*>(
		static_cast<llvm::PointerType*>(p->getType())->getElementType())])
	{
		ptr->selected.push(p);
		return ptr;
	}
	else
	{
		if (!parent_namespace) throw err("target type not found in this namespace");
		return parent_namespace->get_namespace(p);
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
	case is_ref: return AST_result(lBuilder.CreateLoad(reinterpret_cast<llvm::Value*>(name_map[name].first), "LoadRef"), true);
	case is_overload_func: return AST_result(reinterpret_cast<overload_map_type*>(name_map[name].first));
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

AST_result AST_namespace::get_id(const std::string& name, bool precise, unsigned helper)
{
	switch (name_map[name].second)
	{
	case is_type: return get_type(name);
	case is_alloc: case is_ref: return get_var(name);
	//case is_ref: return lBuilder.CreateLoad(get_var(name));
	case is_overload_func: return AST_result(reinterpret_cast<overload_map_type*>(name_map[name].first));
	case is_constant: return AST_result(reinterpret_cast<llvm::Value*>(name_map[name].first), false);
	case is_none: if (parent_namespace && !precise) return parent_namespace->get_id(name);
		else throw err("undefined identifier " + name + " in this namespace");
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
