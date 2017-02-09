#include <iostream>
#include <fstream>
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
			if (sscanf(T.data.attr->value.c_str(), "%d", &val) == 1) return lBuilder.getInt32(val);
			throw err("invalid integer literal: ", T.data);
		}},
		{ "Hex", [](term_node& T, AST_context*){
			int val;
			if (sscanf(T.data.attr->value.c_str(), "%x", &val) == 1) return lBuilder.getInt32(val);
			throw err("invalid integer literal: ", T.data);
		}},
		{ "Oct", [](term_node& T, AST_context*){
			int val;
			if (sscanf(T.data.attr->value.c_str(), "%o", &val) == 1) return lBuilder.getInt32(val);
			throw err("invalid integer literal: ", T.data);
		}},
		{ "Float", [](term_node& T, AST_context*){
			double val;
			if (sscanf(T.data.attr->value.c_str(), "%lf", &val) == 1)
			return ConstantFP::get(lBuilder.getDoubleTy(), val);
			throw err("invalid float literal: ", T.data);
		}},
		{ "Scientific", [](term_node& T, AST_context*){
			double val;
			if (sscanf(T.data.attr->value.c_str(), "%lf", &val) == 1)
			return ConstantFP::get(lBuilder.getDoubleTy(), val);
			throw err("invalid float literal: ", T.data);
		}},
		{ "Id", [](term_node& T, AST_context* context){
			return context->get_val(T.data.attr->value);
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

Type* sync_cast(Value*& LHS, Value*& RHS)
{	
	auto ltype = LHS->getType(), rtype = RHS->getType();
	if (cast_priority[ltype] < cast_priority[rtype])
	{
		LHS = create_static_cast(LHS, rtype);
	}
	else if (cast_priority[ltype] > cast_priority[rtype])
	{
		RHS = create_static_cast(RHS, ltype);
	}
	return LHS->getType();
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
		{
			"%=%", right_asl, [](gen_node& G, AST_context* context)->void*
			{  }
		}/*,
		{
			"%/=%", right_asl
		},
		{
			"%*=%", right_asl
		},
		{
			"%\\%=%", right_asl
		},
		{
			"%+=%", right_asl
		},
		{
			"%-=%", right_asl
		},
		{
			"%<<=%", right_asl
		},
		{
			"%>>=%", right_asl
		},
		{
			"%&=%", right_asl
		},
		{
			"%^=%", right_asl
		},
		{
			"%|=%", right_asl
		}*/
	},
	
	/*{
		{
			"%?%:%", right_asl
		}
	},
	
	{
		{
			"%||%", left_asl
		}
	},
	
	{
		{
			"%&&%", left_asl
		}
	},
	
	{
		{
			"%|%", left_asl
		}
	},
	
	{
		{
			"%^%", left_asl
		}
	},
	
	{
		{
			"%&%", left_asl
		}
	},*/
	
	{
		{
			"%==%", left_asl, [](gen_node& G, AST_context* context)->void*
			{
				auto* LHS = (Value*)G[0].code_gen(context);
				auto* RHS = (Value*)G[1].code_gen(context);
				auto key = sync_cast(LHS, RHS);
				if (key == int_type || key == bool_type || key == char_type)
				{
					return lBuilder.CreateICmpEQ(LHS, RHS);
				}
				if (key == float_type)
				{
					return lBuilder.CreateFCmpOEQ(LHS, RHS);
				}
				throw err("unknown operator == for type: " + type_names[key]);
			}
		},
		{
			"%!=%", left_asl, [](gen_node& G, AST_context* context)->void*
			{
				auto* LHS = (Value*)G[0].code_gen(context);
				auto* RHS = (Value*)G[1].code_gen(context);
				auto key = sync_cast(LHS, RHS);
				if (key == int_type || key == bool_type || key == char_type)
				{
					return lBuilder.CreateICmpNE(LHS, RHS);
				}
				if (key == float_type)
				{
					return lBuilder.CreateFCmpONE(LHS, RHS);
				}
				throw err("unknown operator != for type: " + type_names[key]);
			}
		}
	},
	
	{
		{
			"%>%", left_asl, [](gen_node& G, AST_context* context)->void*
			{
				auto* LHS = (Value*)G[0].code_gen(context);
				auto* RHS = (Value*)G[1].code_gen(context);
				auto key = sync_cast(LHS, RHS);
				if (key == int_type || key == bool_type || key == char_type)
				{
					return lBuilder.CreateICmpSGT(LHS, RHS);
				}
				if (key == float_type)
				{
					return lBuilder.CreateFCmpOGT(LHS, RHS);
				}
				throw err("unknown operator > for type: " + type_names[key]);
			}
		},
		{
			"%>=%", left_asl, [](gen_node& G, AST_context* context)->void*
			{
				auto* LHS = (Value*)G[0].code_gen(context);
				auto* RHS = (Value*)G[1].code_gen(context);
				auto key = sync_cast(LHS, RHS);
				if (key == int_type || key == bool_type || key == char_type)
				{
					return lBuilder.CreateICmpSGE(LHS, RHS);
				}
				if (key == float_type)
				{
					return lBuilder.CreateFCmpOGE(LHS, RHS);
				}
				throw err("unknown operator >= for type: " + type_names[key]);
			}
		},
		{
			"%<%", left_asl, [](gen_node& G, AST_context* context)->void*
			{
				auto* LHS = (Value*)G[0].code_gen(context);
				auto* RHS = (Value*)G[1].code_gen(context);
				auto key = sync_cast(LHS, RHS);
				if (key == int_type || key == bool_type || key == char_type)
				{
					return lBuilder.CreateICmpSLT(LHS, RHS);
				}
				if (key == float_type)
				{
					return lBuilder.CreateFCmpOLT(LHS, RHS);
				}
				throw err("unknown operator < for type: " + type_names[key]);
			}
		},
		{
			"%<=%", left_asl, [](gen_node& G, AST_context* context)->void*
			{
				auto* LHS = (Value*)G[0].code_gen(context);
				auto* RHS = (Value*)G[1].code_gen(context);
				auto key = sync_cast(LHS, RHS);
				if (key == int_type || key == bool_type || key == char_type)
				{
					return lBuilder.CreateICmpSLE(LHS, RHS);
				}
				if (key == float_type)
				{
					return lBuilder.CreateFCmpOLE(LHS, RHS);
				}
				throw err("unknown operator <= for type: " + type_names[key]);
			}
		}
	},
	
	{
		{
			"%<<%", left_asl, [](gen_node& G, AST_context* context)->void*
			{
				auto* LHS = (Value*)G[0].code_gen(context);
				auto* RHS = (Value*)G[1].code_gen(context);
				auto key = sync_cast(LHS, RHS);
				if (key == int_type || key == bool_type || key == char_type)
				{
					return lBuilder.CreateShl(LHS, RHS);
				}
				throw err("unknown operator << for type: " + type_names[key]);
			}
		},
		{
			"%>>%", left_asl, [](gen_node& G, AST_context* context)->void*
			{
				auto* LHS = (Value*)G[0].code_gen(context);
				auto* RHS = (Value*)G[1].code_gen(context);
				auto key = sync_cast(LHS, RHS);
				if (key == int_type || key == bool_type || key == char_type)
				{
					return lBuilder.CreateAShr(LHS, RHS);
				}
				throw err("unknown operator >> for type: " + type_names[key]);
			}
		}
	},
	
	{
		{
			"%+%", left_asl, [](gen_node& G, AST_context* context)->void*
			{
				auto* LHS = (Value*)G[0].code_gen(context);
				auto* RHS = (Value*)G[1].code_gen(context);
				auto key = sync_cast(LHS, RHS);
				if (key == int_type || key == bool_type || key == char_type)
				{
					return lBuilder.CreateAdd(LHS, RHS);
				}
				else if (key == float_type)
				{
					return lBuilder.CreateFAdd(LHS, RHS);
				}
				throw err("unknown operator + for type: " + type_names[key]);
			}
		},
		{
			"%-%", left_asl, [](gen_node& G, AST_context* context)->void*
			{
				auto* LHS = (Value*)G[0].code_gen(context);
				auto* RHS = (Value*)G[1].code_gen(context);
				auto key = sync_cast(LHS, RHS);
				if (key == int_type || key == bool_type || key == char_type)
				{
					return lBuilder.CreateSub(LHS, RHS);
				}
				else if (key == float_type)
				{
					return lBuilder.CreateFSub(LHS, RHS);
				}
				throw err("unknown operator - for type: " + type_names[key]);
			}
		}
	},
	
	{
		{
			"%/%", left_asl, [](gen_node& G, AST_context* context)->void*
			{
				auto* LHS = (Value*)G[0].code_gen(context);
				auto* RHS = (Value*)G[1].code_gen(context);
				auto key = sync_cast(LHS, RHS);
				if (key == int_type || key == bool_type || key == char_type)
				{
					return lBuilder.CreateSDiv(LHS, RHS);
				}
				else if (key == float_type)
				{
					return lBuilder.CreateFDiv(LHS, RHS);
				}
				throw err("unknown operator / for type: " + type_names[key]);
			}
		},
		{
			"%*%", left_asl, [](gen_node& G, AST_context* context)->void*
			{	
				auto* LHS = (Value*)G[0].code_gen(context);
				auto* RHS = (Value*)G[1].code_gen(context);
				auto key = sync_cast(LHS, RHS);
				if (key == int_type || key == bool_type || key == char_type)
				{
					return lBuilder.CreateMul(LHS, RHS);
				}
				else if (key == float_type)
				{
					return lBuilder.CreateFMul(LHS, RHS);
				}
				throw err("unknown operator * for type: " + type_names[key]);
			}
		},
		{
			"%\\%%", left_asl, [](gen_node& G, AST_context* context)->void*
			{	
				auto* LHS = (Value*)G[0].code_gen(context);
				auto* RHS = (Value*)G[1].code_gen(context);
				auto key = sync_cast(LHS, RHS);
				if (key == int_type || key == bool_type || key == char_type)
				{
					return lBuilder.CreateSRem(LHS, RHS);
				}
				throw err("unknown operator % for type: " + type_names[key]);
			}
		}
	},
	
	/*{
		{
			"-%", right_asl
		},
		{
			"++%", right_asl
		},
		{
			"--%", right_asl
		},
		{
			"*%", right_asl
		},
		{
			"&%", right_asl
		},
		{
			"!%", right_asl
		},
		{
			"~%", right_asl
		},
		{
			"sizeof %", right_asl
		}
	},*/
	
	{
		/*{
			"%++", left_asl
		},
		{
			"%--", left_asl
		},
		{
			"%[%pk]", left_asl
		},*/
		{
			"%(%pk)", left_asl, [](gen_node& G, AST_context* context)->void*
			{
				Value* func = (Value*)G[0].code_gen(context);
				auto params = (std::vector<Value*>*)G[1].code_gen(context);
				auto call_inst = lBuilder.CreateCall((Function*)func, *params);
				delete params;
				return call_inst;
				//throw err("unknown operator % for type: " + type_names[key]);
			}
		},
		/*{
			"%.%", left_asl
		},
		{
			"%->%", left_asl
		},*/
	},
};

using function_params = pair<vector<Type*>, vector<token*>>;

parser::init_rules mparse_rules = 
{
	{
		"S", 
		{
			{ "S GlobalItem", parser::expand },
			{ "", parser::empty }
		}
	},
	{
		"GlobalItem",
		{
			{ "Function", parser::forward },
			{ "TypeDefine", parser::forward },
			{ "GlobalVarDefine", parser::forward }
		}
	},
	{
		"TypeDefine",
		{
			{ "type Id = Type;", [](gen_node& G, AST_context* context)->void*{
				context->add_type(G(0).data.attr->value, (Type*)G[1].code_gen(context));
				return nullptr;
			}}
		}
	},
	{
		"Function",
		{
			{ "Type Id ( FunctionParams ) { BlockCodes }", [](gen_node& G, AST_context* context)->void*{
				function_params* p = (function_params*)G[2].code_gen(context);
				auto& name = G(1).data.attr->value;
				FunctionType *FT = FunctionType::get((Type*)G[0].code_gen(context), p->first, false);
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
				return F;
			}}
		}
	},
	{
		"FunctionParams",
		{
			{ "FunctionParamList", parser::forward },
			{ "", [](gen_node&, AST_context* context)->void*{ return new function_params; } }
		}
	},
	{
		"FunctionParamList",
		{
			{ "FunctionParamList , Type Id", [](gen_node& G, AST_context* context)->void*{
				function_params* p = (function_params*)(G[0].code_gen(context));
				p->first.push_back((Type*)G[1].code_gen(context));
				p->second.push_back(&G(2).data);
				return p;
			}},
			{ "Type Id", [](gen_node& G, AST_context* context)->void*{
				function_params* p = new function_params;
				p->first.push_back((Type*)G[0].code_gen(context));
				p->second.push_back(&G(1).data);
				return p;
			}},
		}
	},
	{
		"Block",
		{
			{ "{ BlockCodes }", parser::forward }	/**/
		}
	},
	{
		"BlockCodes",
		{
			{ "BlockCodes BlockCodeItem", parser::expand },
			{ "", parser::empty }
		}
	},
	{
		"BlockCodeItem",
		{
			{ "LocalVarDefine", parser::forward },
			{ "TypeDefine", parser::forward },
			{ "Stmt", parser::forward }
		}
	},
	{
		"LocalVarDefine",
		{
			{ "Type Id LocalInitExpr ;", [](gen_node& G, AST_context* context)->void*{
				Value* init = (Value*)G[2].code_gen(context);
				Type* type = (Type*)G[0].code_gen(context);
				Value* v = init ? create_static_cast(init, type) : initialize(type);
				Value* var = lBuilder.CreateAlloca(type);
				lBuilder.CreateStore(v, var);
				context->add_var(G(1).data.attr->value, var);
				return nullptr;
			}}
		}
	},
	{
		"GlobalVarDefine",
		{
			{ "Type Id GlobalInitExpr ;", [](gen_node& G, AST_context* context)->void*{
				Constant* init = (Constant*)G[2].code_gen(context);
				Type* type = (Type*)G[0].code_gen(context);
				Constant* v = (Constant*)(init ? create_static_cast(init, type) : initialize(type));
				Value* var = new GlobalVariable(*lModule, type, false, GlobalValue::ExternalLinkage, v);
				lBuilder.CreateStore(v, var);
				context->add_var(G(1).data.attr->value, var);
				return nullptr;
			}}
		}
	},
	{
		"LocalInitExpr",
		{
			{ " = expr", parser::forward },
			{ "", parser::empty }
		}
	},
	{
		"GlobalInitExpr",
		{
			{ " = number", parser::forward },
			{ "", parser::empty }
		}
	},
	{
		"Type",
		{
			{ "int", [](gen_node&, AST_context*)->void*{ return int_type; } },
			{ "float", [](gen_node&, AST_context*)->void*{ return float_type; } },
			{ "char", [](gen_node&, AST_context*)->void*{ return char_type; } },
			{ "bool", [](gen_node&, AST_context*)->void*{ return bool_type; } },
			{ "Id", [](gen_node& G, AST_context* context)->void*{ return context->get_type(G(0).data.attr->value); } }
		}
	},
	{
		"Stmt",
		{
			{
				"while ( expr ) Stmt", parser::forward
				/*[](gen_node& n)
				{
					
				}*/
			},
			{
				"if ( expr ) else Stmt", [](gen_node& G, AST_context* context)->void*
				{
					/*Value* condV = (Value*)G[0].code_gen(context);
					if ()
					Builder.CreateCondBr(condV, thenBB, elseBB);*/
				}
			},
			{
				"if ( expr ) Stmt", parser::forward
				/*[](gen_node& n)
				{
					
				}*/
			},
			{ "expr;", parser::forward },
			{ "Block", parser::forward },
			{ ";", parser::empty }
		}
	},
	{
		"number",
		{
			{ "Dec", parser::forward },
			{ "Hex", parser::forward },
			{ "Oct", parser::forward },
			{ "Float", parser::forward },
			{ "Scientific", parser::forward }
		}
	}
};

// main
const int buff_len = 65536;
char buff[buff_len];

int main(int argc, char *argv[])
{
	try
	{
		parser mparser(mlex_rules, mparse_rules, mexpr_rules);
		if (argc > 1 && argc < 4)
		{
			string output_file_name = argc == 3 ? argv[2] : string(argv[1]) + ".ll";
			ifstream in(argv[1]);
			auto ptr = buff;
			while ((*ptr++ = in.get()) != EOF);
			ptr[-1] = 0;
			try
			{
				mparser.parse(buff);
			}
			catch (const err& e)	// poly
			{
				e.alert();
			}
			lModule->dump();
		}
		else
		{
			cerr << "usage: kc <file-name> [<output-file-name>]" << endl;
		}
	}
	catch (const err& e)	// poly
	{
		e.alert();
	}
}
