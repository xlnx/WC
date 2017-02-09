#include <iostream>
#include <cstdio>
#include "parser.h"
using namespace std;
using namespace llvm;
using namespace lr_parser;

lexer::init_rules mlex_rules =
{
	{	// type
		{ "int", "int", {word, no_attr} },
		{ "float", "float", {word, no_attr} },
		{ "char", "char", {word, no_attr} },
		{ "bool", "bool", {word, no_attr} },
		// key words
		{ "while", "while", {word, no_attr} },
		{ "if", "if", {word, no_attr} },
		{ "else", "else", {word, no_attr} },
		{ "for", "for", {word, no_attr} },
		//
		{ "type", "type", {word, no_attr} },
		// identifier
		{ "Id", "[a-zA-Z_]\\w*", {word} },
		// number literal
		{ "Float", "\\d+\\.\\d+", {word} },
		{ "Scientific", "\\d+e-?\\d+", {word, ignore_case} },
		{ "Dec", "[1-9]\\d*", {word} },
		{ "Hex", "0x[0-9a-fA-F]+", {word, ignore_case} },
		{ "Oct", "0[0-7]*", {word} }, 
		
		{ ";", ";", {no_attr} },
		{ ",", ",", {no_attr} },
		{ "(", "\\(", {no_attr} },
		{ ")", "\\)", {no_attr} },
		{ "{", "\\{", {no_attr} },
		{ "}", "\\}", {no_attr} },
		// string literal
		//{"lstring", "\"\"\"[^]*?[^\\\\]{1}\"\"\""},
		{ "String", "\"\"|\"(?:\\\\\\\\|.)*?[^\\\\]\"", {} },
		{ "Char", "\'\'|\'(?:\\\\\\\\|.)*?[^\\\\]\'", {} }
	},
	{
		{ "Dec", [](term_node& T, AST_context*){
			int val;
			if (sscanf(T.data.attr->value.c_str(), "%d", &val) == 1)
			return AST_result(lBuilder.getInt32(val), false);
			throw err("invalid integer literal: ", T.data);
		}},
		{ "Hex", [](term_node& T, AST_context*){
			int val;
			if (sscanf(T.data.attr->value.c_str(), "%x", &val) == 1)
			return AST_result(lBuilder.getInt32(val), false);
			throw err("invalid integer literal: ", T.data);
		}},
		{ "Oct", [](term_node& T, AST_context*){
			int val;
			if (sscanf(T.data.attr->value.c_str(), "%o", &val) == 1)
			return AST_result(lBuilder.getInt32(val), false);
			throw err("invalid integer literal: ", T.data);
		}},
		{ "Float", [](term_node& T, AST_context*){
			double val;
			if (sscanf(T.data.attr->value.c_str(), "%lf", &val) == 1)
			return AST_result(ConstantFP::get(lBuilder.getDoubleTy(), val), false);
			throw err("invalid float literal: ", T.data);
		}},
		{ "Scientific", [](term_node& T, AST_context*){
			double val;
			if (sscanf(T.data.attr->value.c_str(), "%lf", &val) == 1)
			return AST_result(ConstantFP::get(lBuilder.getDoubleTy(), val), false);
			throw err("invalid float literal: ", T.data);
		}},
		{ "Id", [](term_node& T, AST_context* context){
			return context->get_id(T.data.attr->value);
		}}
	}
};

auto int_type = lBuilder.getIntNTy(32);
auto float_type = lBuilder.getDoubleTy();
auto char_type = lBuilder.getIntNTy(8);
auto bool_type = lBuilder.getIntNTy(1);

using type_name_lookup = std::map<Type*, std::string>;
using type_item = type_name_lookup::value_type;
type_name_lookup type_names = 
{
	type_item(int_type, "int"),
	type_item(float_type, "float"),
	type_item(char_type, "char"),
	type_item(bool_type, "bool")
};

// use this table to create static cast command
using static_cast_lookup = std::map<std::pair<Type*, Type*>, std::function<Value*(Value*)>>;
using cast_item = static_cast_lookup::value_type;
static_cast_lookup static_casts =
{	// cast from int
	cast_item({int_type, float_type}, [](Value* v){ return lBuilder.CreateSIToFP(v, float_type); } ),
	cast_item({int_type, char_type}, [](Value* v){ return lBuilder.CreateTrunc(v, char_type); } ),
	cast_item({int_type, bool_type}, [](Value* v){ return lBuilder.CreateTrunc(v, bool_type); } ),
	// cast from float
	cast_item({float_type, int_type}, [](Value* v){ return lBuilder.CreateFPToSI(v, int_type); } ),
	cast_item({float_type, char_type}, [](Value* v){ return lBuilder.CreateFPToSI(v, char_type); } ),	// disable
	cast_item({float_type, bool_type}, [](Value* v){ return lBuilder.CreateFPToSI(v, bool_type); } ),
	// cast from char
	cast_item({char_type, int_type}, [](Value* v){ return lBuilder.CreateSExt(v, int_type); } ),
	cast_item({char_type, float_type}, [](Value* v){ return lBuilder.CreateSIToFP(v, float_type); } ),
	cast_item({char_type, bool_type}, [](Value* v){ return lBuilder.CreateTrunc(v, bool_type); } ),
	// cast from bool
	cast_item({bool_type, int_type}, [](Value* v){ return lBuilder.CreateSExt(v, int_type); } ),
	cast_item({bool_type, float_type}, [](Value* v){ return lBuilder.CreateSIToFP(v, float_type); } ),
	cast_item({bool_type, char_type}, [](Value* v){ return lBuilder.CreateSExt(v, char_type); } ),
};

using cast_priority_map = std::map<Type*, unsigned>;
using priority_item = cast_priority_map::value_type;
cast_priority_map cast_priority =
{
	priority_item(bool_type, 0),
	priority_item(char_type, 1),
	priority_item(int_type, 2),
	priority_item(float_type, 3)
};

Value* create_static_cast(Value* value, Type* type)
{
	Type* cur_type = value->getType();
	if (cur_type != type)
	{
		std::pair<Type*, Type*> key = {cur_type, type};
		if (static_casts[key]) return static_casts[key](value);
		throw err("cannot cast " + type_names[cur_type] + " to " + type_names[type] + " automatically");
	}
	return value;
}

AST_result rvalue(AST_result data)
{
	switch (data.flag)
	{	// cast lvalue to rvalue
	case AST_result::is_lvalue: data.value = lBuilder.CreateLoad(data.value);
	case AST_result::is_rvalue: break;
	default: throw err("invalid value");
	}
	return data;
}

AST_result lvalue(AST_result data)
{
	if (data.flag != AST_result::is_lvalue)
		throw err("value cannot be assigned to");
	return data;
}

AST_result func(AST_result data)
{
	if (data.flag != AST_result::is_function)
		throw err("invalid function");
	return data;
}

Type* binary_sync_cast(AST_result& LHS, AST_result& RHS, Type* type = nullptr)
{
	if (!type)
	{
		auto ltype = LHS.value->getType(), rtype = RHS.value->getType();
		if (cast_priority[ltype] < cast_priority[rtype])
		{
			LHS.value = create_static_cast(LHS.value, rtype);
		}
		else if (cast_priority[ltype] > cast_priority[rtype])
		{
			RHS.value = create_static_cast(RHS.value, ltype);
		}
	}
	else
	{
		LHS.value = create_static_cast(LHS.value, type);
		RHS.value = create_static_cast(RHS.value, type);
	}
	return LHS.value->getType();
}

Value* initialize(Type* type)
{
	if (type->isDoubleTy()) return ConstantFP::get(lBuilder.getDoubleTy(), 0);
	if (type->isIntegerTy(32)) return ConstantInt::get(lBuilder.getIntNTy(32), 0);
	throw err("cannot initialize variable of type: " + type_names[type]);
}

parser::expr_init_rules mexpr_rules = 
{
	{
		{ "%=%", right_asl, [](gen_node& G, AST_context* context)->AST_result{
			auto LHS = lvalue(G[0].code_gen(context));
			auto RHS = rvalue(G[1].code_gen(context));
			RHS.value = create_static_cast(RHS.value, static_cast<AllocaInst*>(LHS.value)->getAllocatedType());
			lBuilder.CreateStore(RHS.value, LHS.value);
			return LHS;
		}},
		{ "%/=%", right_asl, [](gen_node& G, AST_context* context){
			auto val = G[0].code_gen(context);
			auto LHS = rvalue(val);
			val = lvalue(val);
			auto RHS = rvalue(G[1].code_gen(context));
			auto type = LHS.value->getType();
			create_static_cast(RHS.value, type);
			if (type == int_type || type == bool_type || type == char_type)
			{
				lBuilder.CreateStore(lBuilder.CreateSDiv(LHS.value, RHS.value), val.value);
				return val;
			}
			else if (type == float_type)
			{
				lBuilder.CreateStore(lBuilder.CreateFDiv(LHS.value, RHS.value), val.value);
				return val;
			}
			throw err("unknown operator /= for type: " + type_names[type]);
		}},
		{ "%*=%", right_asl, [](gen_node& G, AST_context* context){
			auto val = G[0].code_gen(context);
			auto LHS = rvalue(val);
			val = lvalue(val);
			auto RHS = rvalue(G[1].code_gen(context));
			auto type = LHS.value->getType();
			create_static_cast(RHS.value, type);
			if (type == int_type || type == bool_type || type == char_type)
			{
				lBuilder.CreateStore(lBuilder.CreateMul(LHS.value, RHS.value), val.value);
				return val;
			}
			else if (type == float_type)
			{
				lBuilder.CreateStore(lBuilder.CreateFMul(LHS.value, RHS.value), val.value);
				return val;
			}
			throw err("unknown operator *= for type: " + type_names[type]);
		}},
		{ "%\\%=%", right_asl, [](gen_node& G, AST_context* context){
			auto val = G[0].code_gen(context);
			auto LHS = rvalue(val);
			val = lvalue(val);
			auto RHS = rvalue(G[1].code_gen(context));
			auto type = LHS.value->getType();
			create_static_cast(RHS.value, type);
			if (type == int_type || type == bool_type || type == char_type)
			{
				lBuilder.CreateStore(lBuilder.CreateSRem(LHS.value, RHS.value), val.value);
				return val;
			}
			throw err("unknown operator %= for type: " + type_names[type]);
		}},
		{ "%+=%", right_asl, [](gen_node& G, AST_context* context){
			auto val = G[0].code_gen(context);
			auto LHS = rvalue(val);
			val = lvalue(val);
			auto RHS = rvalue(G[1].code_gen(context));
			auto type = LHS.value->getType();
			create_static_cast(RHS.value, type);
			if (type == int_type || type == bool_type || type == char_type)
			{
				lBuilder.CreateStore(lBuilder.CreateAdd(LHS.value, RHS.value), val.value);
				return val;
			}
			else if (type == float_type)
			{
				lBuilder.CreateStore(lBuilder.CreateFAdd(LHS.value, RHS.value), val.value);
				return val;
			}
			throw err("unknown operator += for type: " + type_names[type]);
		}},
		{ "%-=%", right_asl, [](gen_node& G, AST_context* context){
			auto val = G[0].code_gen(context);
			auto LHS = rvalue(val);
			val = lvalue(val);
			auto RHS = rvalue(G[1].code_gen(context));
			auto type = LHS.value->getType();
			create_static_cast(RHS.value, type);
			if (type == int_type || type == bool_type || type == char_type)
			{
				lBuilder.CreateStore(lBuilder.CreateSub(LHS.value, RHS.value), val.value);
				return val;
			}
			else if (type == float_type)
			{
				lBuilder.CreateStore(lBuilder.CreateFSub(LHS.value, RHS.value), val.value);
				return val;
			}
			throw err("unknown operator -= for type: " + type_names[type]);
		}},
		{ "%<<=%", right_asl, [](gen_node& G, AST_context* context){
			auto val = G[0].code_gen(context);
			auto LHS = rvalue(val);
			val = lvalue(val);
			auto RHS = rvalue(G[1].code_gen(context));
			auto type = LHS.value->getType();
			create_static_cast(RHS.value, type);
			if (type == int_type || type == bool_type || type == char_type)
			{
				lBuilder.CreateStore(lBuilder.CreateShl(LHS.value, RHS.value), val.value);
				return val;
			}
			throw err("unknown operator <<= for type: " + type_names[type]);
		}},
		{ "%>>=%", right_asl, [](gen_node& G, AST_context* context){
			auto val = G[0].code_gen(context);
			auto LHS = rvalue(val);
			val = lvalue(val);
			auto RHS = rvalue(G[1].code_gen(context));
			auto type = LHS.value->getType();
			create_static_cast(RHS.value, type);
			if (type == int_type || type == bool_type || type == char_type)
			{
				lBuilder.CreateStore(lBuilder.CreateAShr(LHS.value, RHS.value), val.value);
				return val;
			}
			throw err("unknown operator >>= for type: " + type_names[type]);
		}},
		{ "%&=%", right_asl, [](gen_node& G, AST_context* context){
			auto val = G[0].code_gen(context);
			auto LHS = rvalue(val);
			val = lvalue(val);
			auto RHS = rvalue(G[1].code_gen(context));
			auto type = LHS.value->getType();
			create_static_cast(RHS.value, type);
			if (type == int_type || type == bool_type || type == char_type)
			{
				lBuilder.CreateStore(lBuilder.CreateAnd(LHS.value, RHS.value), val.value);
				return val;
			}
			throw err("unknown operator &= for type: " + type_names[type]);
		}},
		{ "%^=%", right_asl, [](gen_node& G, AST_context* context){
			auto val = G[0].code_gen(context);
			auto LHS = rvalue(val);
			val = lvalue(val);
			auto RHS = rvalue(G[1].code_gen(context));
			auto type = LHS.value->getType();
			create_static_cast(RHS.value, type);
			if (type == int_type || type == bool_type || type == char_type)
			{
				lBuilder.CreateStore(lBuilder.CreateXor(LHS.value, RHS.value), val.value);
				return val;
			}
			throw err("unknown operator ^= for type: " + type_names[type]);
		}},
		{ "%|=%", right_asl, [](gen_node& G, AST_context* context){
			auto val = G[0].code_gen(context);
			auto LHS = rvalue(val);
			val = lvalue(val);
			auto RHS = rvalue(G[1].code_gen(context));
			auto type = LHS.value->getType();
			create_static_cast(RHS.value, type);
			if (type == int_type || type == bool_type || type == char_type)
			{
				lBuilder.CreateStore(lBuilder.CreateOr(LHS.value, RHS.value), val.value);
				return val;
			}
			throw err("unknown operator |= for type: " + type_names[type]);
		}}
	},
	
	/*{
		{
			"%?%:%", right_asl
		}
	},*/
	
	{
		{ "%||%", left_asl, [](gen_node& G, AST_context* context){
			auto LHS = rvalue(G[0].code_gen(context));
			auto RHS = rvalue(G[1].code_gen(context));
			auto key = binary_sync_cast(LHS, RHS, bool_type);
			return AST_result(lBuilder.CreateOr(LHS.value, RHS.value), false);
		}}
	},
	
	{
		{ "%&&%", left_asl, [](gen_node& G, AST_context* context){
			auto LHS = rvalue(G[0].code_gen(context));
			auto RHS = rvalue(G[1].code_gen(context));
			auto key = binary_sync_cast(LHS, RHS, bool_type);
			return AST_result(lBuilder.CreateAnd(LHS.value, RHS.value), false);
		}}
	},
	
	{
		{ "%|%", left_asl, [](gen_node& G, AST_context* context){
			auto LHS = rvalue(G[0].code_gen(context));
			auto RHS = rvalue(G[1].code_gen(context));
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateOr(LHS.value, RHS.value), false);
			}
			throw err("unknown operator | for type: " + type_names[key]);
		}}
	},
	
	{
		{ "%^%", left_asl, [](gen_node& G, AST_context* context){
			auto LHS = rvalue(G[0].code_gen(context));
			auto RHS = rvalue(G[1].code_gen(context));
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateXor(LHS.value, RHS.value), false);
			}
			throw err("unknown operator ^ for type: " + type_names[key]);
		}}
	},
	
	{
		{ "%&%", left_asl, [](gen_node& G, AST_context* context){
			auto LHS = rvalue(G[0].code_gen(context));
			auto RHS = rvalue(G[1].code_gen(context));
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateAnd(LHS.value, RHS.value), false);
			}
			throw err("unknown operator & for type: " + type_names[key]);
		}}
	},
	
	{
		{ "%==%", left_asl, [](gen_node& G, AST_context* context){
			auto LHS = rvalue(G[0].code_gen(context));
			auto RHS = rvalue(G[1].code_gen(context));
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateICmpEQ(LHS.value, RHS.value), false);
			}
			if (key == float_type)
			{
				return AST_result(lBuilder.CreateFCmpOEQ(LHS.value, RHS.value), false);
			}
			throw err("unknown operator == for type: " + type_names[key]);
		}},
		{ "%!=%", left_asl, [](gen_node& G, AST_context* context){
			auto LHS = rvalue(G[0].code_gen(context));
			auto RHS = rvalue(G[1].code_gen(context));
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateICmpNE(LHS.value, RHS.value), false);
			}
			if (key == float_type)
			{
				return AST_result(lBuilder.CreateFCmpONE(LHS.value, RHS.value), false);
			}
			throw err("unknown operator != for type: " + type_names[key]);
		}}
	},
	
	{
		{ "%>%", left_asl, [](gen_node& G, AST_context* context){
			auto LHS = rvalue(G[0].code_gen(context));
			auto RHS = rvalue(G[1].code_gen(context));
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateICmpSGT(LHS.value, RHS.value), false);
			}
			if (key == float_type)
			{
				return AST_result(lBuilder.CreateFCmpOGT(LHS.value, RHS.value), false);
			}
			throw err("unknown operator > for type: " + type_names[key]);
		}},
		{ "%>=%", left_asl, [](gen_node& G, AST_context* context){
			auto LHS = rvalue(G[0].code_gen(context));
			auto RHS = rvalue(G[1].code_gen(context));
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateICmpSGE(LHS.value, RHS.value), false);
			}
			if (key == float_type)
			{
				return AST_result(lBuilder.CreateFCmpOGE(LHS.value, RHS.value), false);
			}
			throw err("unknown operator >= for type: " + type_names[key]);
		}},
		{ "%<%", left_asl, [](gen_node& G, AST_context* context){
			auto LHS = rvalue(G[0].code_gen(context));
			auto RHS = rvalue(G[1].code_gen(context));
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateICmpSLT(LHS.value, RHS.value), false);
			}
			if (key == float_type)
			{
				return AST_result(lBuilder.CreateFCmpOLT(LHS.value, RHS.value), false);
			}
			throw err("unknown operator < for type: " + type_names[key]);
		}},
		{ "%<=%", left_asl, [](gen_node& G, AST_context* context){
			auto LHS = rvalue(G[0].code_gen(context));
			auto RHS = rvalue(G[1].code_gen(context));
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateICmpSLE(LHS.value, RHS.value), false);
			}
			if (key == float_type)
			{
				return AST_result(lBuilder.CreateFCmpOLE(LHS.value, RHS.value), false);
			}
			throw err("unknown operator <= for type: " + type_names[key]);
		}}
	},
	
	{
		{ "%<<%", left_asl, [](gen_node& G, AST_context* context){
			auto LHS = rvalue(G[0].code_gen(context));
			auto RHS = rvalue(G[1].code_gen(context));
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateShl(LHS.value, RHS.value), false);
			}
			throw err("unknown operator << for type: " + type_names[key]);
		}},
		{ "%>>%", left_asl, [](gen_node& G, AST_context* context){
			auto LHS = rvalue(G[0].code_gen(context));
			auto RHS = rvalue(G[1].code_gen(context));
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateAShr(LHS.value, RHS.value), false);
			}
			throw err("unknown operator >> for type: " + type_names[key]);
		}}
	},
	
	{
		{ "%+%", left_asl, [](gen_node& G, AST_context* context){
			auto LHS = rvalue(G[0].code_gen(context));
			auto RHS = rvalue(G[1].code_gen(context));
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateAdd(LHS.value, RHS.value), false);
			}
			else if (key == float_type)
			{
				return AST_result(lBuilder.CreateFAdd(LHS.value, RHS.value), false);
			}
			throw err("unknown operator + for type: " + type_names[key]);
		}},
		{ "%-%", left_asl, [](gen_node& G, AST_context* context){
			auto LHS = rvalue(G[0].code_gen(context));
			auto RHS = rvalue(G[1].code_gen(context));
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateSub(LHS.value, RHS.value), false);
			}
			else if (key == float_type)
			{
				return AST_result(lBuilder.CreateFSub(LHS.value, RHS.value), false);
			}
			throw err("unknown operator - for type: " + type_names[key]);
		}}
	},
	
	{
		{ "%/%", left_asl, [](gen_node& G, AST_context* context){
			auto LHS = rvalue(G[0].code_gen(context));
			auto RHS = rvalue(G[1].code_gen(context));
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateSDiv(LHS.value, RHS.value), false);
			}
			else if (key == float_type)
			{
				return AST_result(lBuilder.CreateFDiv(LHS.value, RHS.value), false);
			}
			throw err("unknown operator / for type: " + type_names[key]);
		}},
		{ "%*%", left_asl, [](gen_node& G, AST_context* context){	
			auto LHS = rvalue(G[0].code_gen(context));
			auto RHS = rvalue(G[1].code_gen(context));
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateMul(LHS.value, RHS.value), false);
			}
			else if (key == float_type)
			{
				return AST_result(lBuilder.CreateFMul(LHS.value, RHS.value), false);
			}
			throw err("unknown operator * for type: " + type_names[key]);
		}},
		{ "%\\%%", left_asl, [](gen_node& G, AST_context* context){
			auto LHS = rvalue(G[0].code_gen(context));
			auto RHS = rvalue(G[1].code_gen(context));
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateSRem(LHS.value, RHS.value), false);
			}
			throw err("unknown operator % for type: " + type_names[key]);
		}}
	},
	
	{
		{ "-%", right_asl, [](gen_node& G, AST_context* context){
			auto RHS = rvalue(G[0].code_gen(context));
			auto key = RHS.value->getType();
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateNeg(RHS.value), false);
			}
			else if (key == float_type)
			{
				return AST_result(lBuilder.CreateFNeg(RHS.value), false);
			}
			throw err("unknown operator - for type: " + type_names[key]);
		}},
		{ "!%", right_asl, [](gen_node& G, AST_context* context){
			auto RHS = rvalue(G[0].code_gen(context));
			create_static_cast(RHS.value, bool_type);
			return AST_result(lBuilder.CreateNot(RHS.value), false);
		}},
		/*{
			"~%", right_asl
		}*/
	},
	
	{
		/*
		{
			"%[%pk]", left_asl
		},*/
		{ "%(%pk)", left_asl, [](gen_node& G, AST_context* context){	
			auto key = func(G[0].code_gen(context));
			auto params = reinterpret_cast<std::vector<Value*>*>(G[1].code_gen(context).custom_data);
			auto call_inst = lBuilder.CreateCall(key.function, *params);
			delete params;
			return AST_result(call_inst, false);
		}},
		/*{
			"%.%", left_asl
		},*/
	},
};

using function_params = pair<vector<Type*>, vector<token*>>;

parser::init_rules mparse_rules = 
{
	{ "S", {
		{ "S GlobalItem", parser::expand },
		{ "", parser::empty }
	}},
	{ "GlobalItem", {
		{ "Function", parser::forward },
		{ "TypeDefine", parser::forward },
		{ "GlobalVarDefine", parser::forward }
	}},
	{ "TypeDefine", {
		{ "type Id = Type;", [](gen_node& G, AST_context* context){
			context->add_type(G(0).data.attr->value, G[1].code_gen(context).type);
			return AST_result();
		}}
	}},
	{ "Function", {
		{ "Type Id ( FunctionParams ) { BlockCodes }", [](gen_node& G, AST_context* context){
			function_params* p = reinterpret_cast<function_params*>(G[2].code_gen(context).custom_data);
			auto& name = G(1).data.attr->value;
			FunctionType *FT = FunctionType::get(G[0].code_gen(context).type, p->first, false);
			Function* F = Function::Create(FT, Function::ExternalLinkage, name, lModule);
			if (F->getName() != name)
			{
				F->eraseFromParent();
				delete p;
				throw err("function " + name + " already exists", G(1).data);
			}
			context->add_func(name, F);
			AST_context* new_context = new AST_context(context, F);
			lBuilder.SetInsertPoint(new_context->block);
			unsigned i = 0;
			for (auto itr = F->arg_begin(); itr != F->arg_end(); ++itr, ++i)
			{
				Value* alloc = lBuilder.CreateAlloca(p->first[i]);
				itr->setName(p->second[i]->attr->value);
				lBuilder.CreateStore(itr, alloc);
				new_context->add_var(p->second[i]->attr->value, alloc);
			}
			delete p;
			G[3].code_gen(new_context);
			verifyFunction(*F);
			delete new_context;
			F->dump();
			return AST_result();
		}}
	}},
	{ "FunctionParams", {
		{ "FunctionParamList", parser::forward },
		{ "", [](gen_node&, AST_context* context){ return AST_result(new function_params); } }
	}},
	{ "FunctionParamList", {
		{ "FunctionParamList , Type Id", [](gen_node& G, AST_context* context){
			function_params* p = reinterpret_cast<function_params*>(G[0].code_gen(context).custom_data);
			p->first.push_back(G[1].code_gen(context).type);
			p->second.push_back(&G(2).data);
			return AST_result(p);
		}},
		{ "Type Id", [](gen_node& G, AST_context* context){
			function_params* p = new function_params;
			p->first.push_back(G[0].code_gen(context).type);
			p->second.push_back(&G(1).data);
			return AST_result(p);
		}},
	}},
	{ "Block", {
		{ "{ BlockCodes }", parser::forward }	/**/
	}},
	{ "BlockCodes", {
		{ "BlockCodes BlockCodeItem", parser::expand },
		{ "", parser::empty }
	}},
	{ "BlockCodeItem", {
		{ "LocalVarDefine", parser::forward },
		{ "TypeDefine", parser::forward },
		{ "Stmt", parser::forward }
	}},
	{ "LocalVarDefine", {
		{ "Type Id LocalInitExpr ;", [](gen_node& G, AST_context* context){
			Value* init = rvalue(G[2].code_gen(context)).value;
			Type* type = G[0].code_gen(context).type;
			Value* v = init ? create_static_cast(init, type) : initialize(type);
			Value* var = lBuilder.CreateAlloca(type);
			lBuilder.CreateStore(v, var);
			context->add_var(G(1).data.attr->value, var);
			return AST_result();
		}}
	}},
	{ "GlobalVarDefine", {
		{ "Type Id GlobalInitExpr ;", [](gen_node& G, AST_context* context){
			Constant* init = (Constant*)rvalue(G[2].code_gen(context)).value;
			Type* type = G[0].code_gen(context).type;
			Constant* v = (Constant*)(init ? create_static_cast(init, type) : initialize(type));
			Value* var = new GlobalVariable(*lModule, type, false, GlobalValue::ExternalLinkage, v);
			lBuilder.CreateStore(v, var);
			context->add_var(G(1).data.attr->value, var);
			return AST_result();
		}}
	}},
	{ "LocalInitExpr", {
		{ " = expr", parser::forward },
		{ "", parser::empty }
	}},
	{ "GlobalInitExpr", {
		{ " = number", parser::forward },
		{ "", parser::empty }
	}},
	{ "Type", {
		{ "int", [](gen_node&, AST_context*){ return AST_result(int_type); } },
		{ "float", [](gen_node&, AST_context*){ return AST_result(float_type); } },
		{ "char", [](gen_node&, AST_context*){ return AST_result(char_type); } },
		{ "bool", [](gen_node&, AST_context*){ return AST_result(bool_type); } },
		{ "Id", [](gen_node& G, AST_context* context){ return context->get_type(G(0).data.attr->value); } }
	}},
	{ "Stmt", {
		{ "while ( expr ) Stmt", parser::forward
		},
		{ "if ( expr ) else Stmt", [](gen_node& G, AST_context* context)->AST_result{
			/*Value* condV = (Value*)G[0].code_gen(context);
			if ()
			Builder.CreateCondBr(condV, thenBB, elseBB);*/
		}},
		{ "if ( expr ) Stmt", parser::forward
		},
		{ "expr;", parser::forward },
		{ "Block", parser::forward },
		{ ";", parser::empty }
	}},
	{ "number", {
		{ "Dec", parser::forward },
		{ "Hex", parser::forward },
		{ "Oct", parser::forward },
		{ "Float", parser::forward },
		{ "Scientific", parser::forward }
	}}
};

// main
const int buff_len = 2048;
char buff[buff_len];

int main()
{
	try
	{
		parser mparser(mlex_rules, mparse_rules, mexpr_rules);
		while (1)
		{
			printf(">>> ");
			auto* ptr = buff;
			while (scanf("%[^\n]s", ptr) == 1)
			{
				fflush(stdin);
				ptr += strlen(ptr);
				*ptr++ = '\n';
				*ptr = 0;
			}
			if (!*buff) break;
			try
			{
				mparser.parse(buff);
				//mparser.parse(tokens);
			}
			catch (const err& e)	// poly
			{
				e.alert();
			}
			fflush(stdin);
			*buff = 0;
		}
		lModule->dump();
		system("pause");
	}
	catch (const err& e)	// poly
	{
		e.alert();
	}
}
