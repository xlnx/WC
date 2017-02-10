#ifndef __W_COMPILER__HEADER_FILE
#define __W_COMPILER__HEADER_FILE
#include <iostream>
#include <cstdio>
#include "parser.h"
using namespace std;
using namespace llvm;
using namespace lr_parser;

lexer::init_rules mlex_rules =
{
	{	// type
		{ "int", "int", {word} },
		{ "float", "float", {word} },
		{ "char", "char", {word} },
		{ "bool", "bool", {word} },
		// key words
		{ "while", "while", {word, no_attr} },
		{ "if", "if", {word, no_attr} },
		{ "else", "else", {word, no_attr} },
		{ "for", "for", {word, no_attr} },
		{ "true", "true", {word, no_attr} },
		{ "false", "false", {word, no_attr} },
		//
		{ "type", "type", {word, no_attr} },
		//
		{ "return", "return", {word} },
		{ "break", "break", {word} },
		{ "continue", "continue", {word} },
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
		}},
		{ "continue", [](term_node&, AST_context* context){
			if (!context->loop_next) throw err("continue appears outside loop");
			context->jump_to(context->loop_next);
			return AST_result();
		}},
		{ "break", [](term_node&, AST_context* context){
			if (!context->loop_end) throw err("break appears outside loop");
			context->jump_to(context->loop_end);
			return AST_result();
		}},
		{ "return", [](term_node&, AST_context* context){
			context->leave_function();
			return AST_result();
		}},
		{ "int", [](term_node&, AST_context*){ return AST_result(int_type); } },
		{ "float", [](term_node&, AST_context*){ return AST_result(float_type); } },
		{ "char", [](term_node&, AST_context*){ return AST_result(char_type); } },
		{ "bool", [](term_node&, AST_context*){ return AST_result(bool_type); } },
		{ "true", [](term_node&, AST_context*){ return AST_result(ConstantInt::get(bool_type, 1), false); } },
		{ "false", [](term_node&, AST_context*){ return AST_result(ConstantInt::get(bool_type, 0), false); } }
	}
};

parser::expr_init_rules mexpr_rules = 
{
	{
		{ "%=%", right_asl, [](gen_node& syntax_node, AST_context* context)->AST_result{
			auto val = syntax_node[0].code_gen(context);
			auto LHS = val.get_lvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			RHS = create_static_cast(RHS, static_cast<AllocaInst*>(LHS)->getAllocatedType());
			lBuilder.CreateStore(RHS, LHS);
			return val;
		}},
		{ "%/=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_lvalue();
			auto LHS = val.get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto type = LHS->getType();
			create_static_cast(RHS, type);
			if (type == int_type || type == bool_type || type == char_type)
			{
				lBuilder.CreateStore(lBuilder.CreateSDiv(LHS, RHS), alloc);
				return val;
			}
			else if (type == float_type)
			{
				lBuilder.CreateStore(lBuilder.CreateFDiv(LHS, RHS), alloc);
				return val;
			}
			throw err("unknown operator /= for type: " + type_names[type]);
		}},
		{ "%*=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_lvalue();
			auto LHS = val.get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto type = LHS->getType();
			create_static_cast(RHS, type);
			if (type == int_type || type == bool_type || type == char_type)
			{
				lBuilder.CreateStore(lBuilder.CreateMul(LHS, RHS), alloc);
				return val;
			}
			else if (type == float_type)
			{
				lBuilder.CreateStore(lBuilder.CreateFMul(LHS, RHS), alloc);
				return val;
			}
			throw err("unknown operator *= for type: " + type_names[type]);
		}},
		{ "%\\%=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_lvalue();
			auto LHS = val.get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto type = LHS->getType();
			create_static_cast(RHS, type);
			if (type == int_type || type == bool_type || type == char_type)
			{
				lBuilder.CreateStore(lBuilder.CreateSRem(LHS, RHS), alloc);
				return val;
			}
			throw err("unknown operator %= for type: " + type_names[type]);
		}},
		{ "%+=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_lvalue();
			auto LHS = val.get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto type = LHS->getType();
			create_static_cast(RHS, type);
			if (type == int_type || type == bool_type || type == char_type)
			{
				lBuilder.CreateStore(lBuilder.CreateAdd(LHS, RHS), alloc);
				return val;
			}
			else if (type == float_type)
			{
				lBuilder.CreateStore(lBuilder.CreateFAdd(LHS, RHS), alloc);
				return val;
			}
			throw err("unknown operator += for type: " + type_names[type]);
		}},
		{ "%-=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_lvalue();
			auto LHS = val.get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto type = LHS->getType();
			create_static_cast(RHS, type);
			if (type == int_type || type == bool_type || type == char_type)
			{
				lBuilder.CreateStore(lBuilder.CreateSub(LHS, RHS), alloc);
				return val;
			}
			else if (type == float_type)
			{
				lBuilder.CreateStore(lBuilder.CreateFSub(LHS, RHS), alloc);
				return val;
			}
			throw err("unknown operator -= for type: " + type_names[type]);
		}},
		{ "%<<=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_lvalue();
			auto LHS = val.get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto type = LHS->getType();
			create_static_cast(RHS, type);
			if (type == int_type || type == bool_type || type == char_type)
			{
				lBuilder.CreateStore(lBuilder.CreateShl(LHS, RHS), alloc);
				return val;
			}
			throw err("unknown operator <<= for type: " + type_names[type]);
		}},
		{ "%>>=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_lvalue();
			auto LHS = val.get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto type = LHS->getType();
			create_static_cast(RHS, type);
			if (type == int_type || type == bool_type || type == char_type)
			{
				lBuilder.CreateStore(lBuilder.CreateAShr(LHS, RHS), alloc);
				return val;
			}
			throw err("unknown operator >>= for type: " + type_names[type]);
		}},
		{ "%&=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_lvalue();
			auto LHS = val.get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto type = LHS->getType();
			create_static_cast(RHS, type);
			if (type == int_type || type == bool_type || type == char_type)
			{
				lBuilder.CreateStore(lBuilder.CreateAnd(LHS, RHS), alloc);
				return val;
			}
			throw err("unknown operator &= for type: " + type_names[type]);
		}},
		{ "%^=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_lvalue();
			auto LHS = val.get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto type = LHS->getType();
			create_static_cast(RHS, type);
			if (type == int_type || type == bool_type || type == char_type)
			{
				lBuilder.CreateStore(lBuilder.CreateXor(LHS, RHS), alloc);
				return val;
			}
			throw err("unknown operator ^= for type: " + type_names[type]);
		}},
		{ "%|=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_lvalue();
			auto LHS = val.get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto type = LHS->getType();
			create_static_cast(RHS, type);
			if (type == int_type || type == bool_type || type == char_type)
			{
				lBuilder.CreateStore(lBuilder.CreateOr(LHS, RHS), alloc);
				return val;
			}
			throw err("unknown operator |= for type: " + type_names[type]);
		}}
	},
	
	{
		{ "%?%:%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto then_block = context->new_block("then");
			auto else_block = context->new_block("else");
			auto merge_block = context->new_block("endif");
			context->cond_jump_to(syntax_node[0].code_gen(context).get_rvalue(), then_block, else_block);
			
			context->set_block(then_block);
			Value* then_value = syntax_node[1].code_gen(context).get_rvalue();
			then_block = context->get_block();
			context->jump_to(merge_block);
			
			context->set_block(else_block);
			Value* else_value = syntax_node[2].code_gen(context).get_rvalue();
			then_block = context->get_block();
			context->jump_to(merge_block);
			
			context->set_block(merge_block);
			PHINode* PN = lBuilder.CreatePHI(get_binary_sync_type(then_value, else_value), 2);
			PN->addIncoming(then_value, then_block);
			PN->addIncoming(else_value, else_block);
			return AST_result(PN, false); 
		}}
	},
	
	{
		{ "%||%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS, bool_type);
			return AST_result(lBuilder.CreateOr(LHS, RHS), false);
		}}
	},
	
	{
		{ "%&&%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS, bool_type);
			return AST_result(lBuilder.CreateAnd(LHS, RHS), false);
		}}
	},
	
	{
		{ "%|%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateOr(LHS, RHS), false);
			}
			throw err("unknown operator | for type: " + type_names[key]);
		}}
	},
	
	{
		{ "%^%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateXor(LHS, RHS), false);
			}
			throw err("unknown operator ^ for type: " + type_names[key]);
		}}
	},
	
	{
		{ "%&%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateAnd(LHS, RHS), false);
			}
			throw err("unknown operator & for type: " + type_names[key]);
		}}
	},
	
	{
		{ "%==%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateICmpEQ(LHS, RHS), false);
			}
			if (key == float_type)
			{
				return AST_result(lBuilder.CreateFCmpOEQ(LHS, RHS), false);
			}
			throw err("unknown operator == for type: " + type_names[key]);
		}},
		{ "%!=%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateICmpNE(LHS, RHS), false);
			}
			if (key == float_type)
			{
				return AST_result(lBuilder.CreateFCmpONE(LHS, RHS), false);
			}
			throw err("unknown operator != for type: " + type_names[key]);
		}}
	},
	
	{
		{ "%>%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateICmpSGT(LHS, RHS), false);
			}
			if (key == float_type)
			{
				return AST_result(lBuilder.CreateFCmpOGT(LHS, RHS), false);
			}
			throw err("unknown operator > for type: " + type_names[key]);
		}},
		{ "%>=%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateICmpSGE(LHS, RHS), false);
			}
			if (key == float_type)
			{
				return AST_result(lBuilder.CreateFCmpOGE(LHS, RHS), false);
			}
			throw err("unknown operator >= for type: " + type_names[key]);
		}},
		{ "%<%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateICmpSLT(LHS, RHS), false);
			}
			if (key == float_type)
			{
				return AST_result(lBuilder.CreateFCmpOLT(LHS, RHS), false);
			}
			throw err("unknown operator < for type: " + type_names[key]);
		}},
		{ "%<=%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateICmpSLE(LHS, RHS), false);
			}
			if (key == float_type)
			{
				return AST_result(lBuilder.CreateFCmpOLE(LHS, RHS), false);
			}
			throw err("unknown operator <= for type: " + type_names[key]);
		}}
	},
	
	{
		{ "%<<%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateShl(LHS, RHS), false);
			}
			throw err("unknown operator << for type: " + type_names[key]);
		}},
		{ "%>>%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateAShr(LHS, RHS), false);
			}
			throw err("unknown operator >> for type: " + type_names[key]);
		}}
	},
	
	{
		{ "%+%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateAdd(LHS, RHS), false);
			}
			else if (key == float_type)
			{
				return AST_result(lBuilder.CreateFAdd(LHS, RHS), false);
			}
			throw err("unknown operator + for type: " + type_names[key]);
		}},
		{ "%-%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateSub(LHS, RHS), false);
			}
			else if (key == float_type)
			{
				return AST_result(lBuilder.CreateFSub(LHS, RHS), false);
			}
			throw err("unknown operator - for type: " + type_names[key]);
		}}
	},
	
	{
		{ "%/%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateSDiv(LHS, RHS), false);
			}
			else if (key == float_type)
			{
				return AST_result(lBuilder.CreateFDiv(LHS, RHS), false);
			}
			throw err("unknown operator / for type: " + type_names[key]);
		}},
		{ "%*%", left_asl, [](gen_node& syntax_node, AST_context* context){	
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateMul(LHS, RHS), false);
			}
			else if (key == float_type)
			{
				return AST_result(lBuilder.CreateFMul(LHS, RHS), false);
			}
			throw err("unknown operator * for type: " + type_names[key]);
		}},
		{ "%\\%%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateSRem(LHS, RHS), false);
			}
			throw err("unknown operator % for type: " + type_names[key]);
		}}
	},
	
	{
		{ "-%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto RHS = syntax_node[0].code_gen(context).get_rvalue();
			auto key = RHS->getType();
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateNeg(RHS), false);
			}
			else if (key == float_type)
			{
				return AST_result(lBuilder.CreateFNeg(RHS), false);
			}
			throw err("unknown operator - for type: " + type_names[key]);
		}},
		{ "!%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto RHS = syntax_node[0].code_gen(context).get_rvalue();
			return AST_result(lBuilder.CreateNot(create_static_cast(RHS, bool_type)), false);
		}},
		/*{
			"~%", right_asl
		}*/
	},
	
	{
		{ "%[%pk]", left_asl, parser::forward },
		{ "%(%pk)", left_asl, [](gen_node& syntax_node, AST_context* context){	
			auto function = syntax_node[0].code_gen(context).get_function();
			auto params = syntax_node[1].code_gen(context).get_data<std::vector<Value*>>();
			auto function_proto = function->getFunctionType();
			if (function_proto->getNumParams() != params->size())
				throw err("function param number mismatch");
			for (unsigned i = 0; i < params->size(); ++i)
				(*params)[i] = create_static_cast((*params)[i], function_proto->getParamType(i));
			auto call_inst = lBuilder.CreateCall(function, *params);
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
		{ "type Id = Type;", [](gen_node& syntax_node, AST_context* context){
			context->add_type(static_cast<term_node&>(syntax_node[0]).data.attr->value,
				syntax_node[1].code_gen(context).get_type());
			return AST_result();
		}}
	}},
	{ "Function", {
		{ "Type Id ( FunctionParams ) { Block }", [](gen_node& syntax_node, AST_context* context){
			function_params* p = syntax_node[2].code_gen(context).get_data<function_params>();
			auto& name = static_cast<term_node&>(syntax_node[1]).data.attr->value;
			FunctionType *FT = FunctionType::get(syntax_node[0].code_gen(context).get_type(), p->first, false);
			Function* F = Function::Create(FT, Function::ExternalLinkage, name, lModule);
			context->add_func(name, F);
			AST_context new_context(context, F);
			new_context.activate();
			unsigned i = 0;
			for (auto itr = F->arg_begin(); itr != F->arg_end(); ++itr, ++i)
			{	//itr->setName(p->second[i]->attr->value);
				lBuilder.CreateStore(itr, new_context.alloc_var(p->first[i], p->second[i]->attr->value));
			}
			delete p;
			syntax_node[3].code_gen(&new_context);
			new_context.finish_func();
			return AST_result();
		}},
		{ "Id ( FunctionParams ) { Block }", [](gen_node& syntax_node, AST_context* context){
			function_params* p = syntax_node[1].code_gen(context).get_data<function_params>();
			auto& name = static_cast<term_node&>(syntax_node[0]).data.attr->value;
			FunctionType *FT = FunctionType::get(void_type, p->first, false);
			Function* F = Function::Create(FT, Function::ExternalLinkage, name, lModule);
			context->add_func(name, F);
			AST_context new_context(context, F);
			new_context.activate();
			unsigned i = 0;
			for (auto itr = F->arg_begin(); itr != F->arg_end(); ++itr, ++i)
			{	//itr->setName(p->second[i]->attr->value);
				lBuilder.CreateStore(itr, new_context.alloc_var(p->first[i], p->second[i]->attr->value));
			}
			delete p;
			syntax_node[2].code_gen(&new_context);
			new_context.finish_func();
			return AST_result();
		}}
	}},
	{ "FunctionParams", {
		{ "FunctionParamList", parser::forward },
		{ "", [](gen_node&, AST_context* context){ return AST_result(new function_params); } }
	}},
	{ "FunctionParamList", {
		{ "FunctionParamList , Type Id", [](gen_node& syntax_node, AST_context* context){
			function_params* p = syntax_node[0].code_gen(context).get_data<function_params>();
			p->first.push_back(syntax_node[1].code_gen(context).get_type());
			p->second.push_back(&static_cast<term_node&>(syntax_node[2]).data);
			return AST_result(p);
		}},
		{ "Type Id", [](gen_node& syntax_node, AST_context* context){
			function_params* p = new function_params;
			p->first.push_back(syntax_node[0].code_gen(context).get_type());
			p->second.push_back(&static_cast<term_node&>(syntax_node[1]).data);
			return AST_result(p);
		}},
	}},
	{ "GlobalVarDefine", {
		{ "Type Id GlobalInitExpr;", [](gen_node& syntax_node, AST_context* context){
			Type* type = syntax_node[0].code_gen(context).get_type();
			auto init_expr = syntax_node[2].code_gen(context);
			Constant* init = static_cast<Constant*>(init_expr.flag != AST_result::is_none ?
				create_static_cast(init_expr.get_rvalue(), type) : initialize(type));
			lBuilder.CreateStore(init, context->alloc_var(type, static_cast<term_node&>(syntax_node[1]).data.attr->value));
			return AST_result();
		}}
	}},
	{ "InitExpr", {
		{ "= expr", parser::forward },
		{ "", parser::empty }
	}},
	{ "GlobalInitExpr", {
		{ "= constexpr", parser::forward },
		{ "", parser::empty }
	}},
	{ "Type", {
		{ "Type [ constexpr ]", [](gen_node& syntax_node, AST_context* context){
			return AST_result(/*llvm::ArrayType::get(syntax_node[0].code_gen(context).get_type(),
				syntax_node[1].code_gen(context).get_rvalue())*/);
		}}, 
		{ "int", parser::forward },
		{ "float", parser::forward },
		{ "char", parser::forward },
		{ "bool", parser::forward },
		{ "Id", [](gen_node& syntax_node, AST_context* context){
			return context->get_type(static_cast<term_node&>(syntax_node[0]).data.attr->value);
		}}
	}},
	{ "Stmt", {
		{ "while ( expr ) Stmt", [](gen_node& syntax_node, AST_context* context){
			auto loop_end = context->loop_end;
			auto loop_next = context->loop_next;
			auto cond_block = context->loop_next = context->new_block("while_cond");
			auto body_block = context->new_block("while_body");
			auto merge_block = context->loop_end = context->new_block("endwhile");
			
			context->set_block(cond_block);
			context->cond_jump_to(syntax_node[0].code_gen(context).get_rvalue(), body_block, merge_block);
			
			context->set_block(body_block);
			syntax_node[1].code_gen(context);
			context->jump_to(cond_block);
			
			context->set_block(merge_block);
			context->loop_end = loop_end;
			context->loop_next = loop_next;
			return AST_result();
		}},
		{ "if ( expr ) Stmt else Stmt", [](gen_node& syntax_node, AST_context* context){	
			auto then_block = context->new_block("then");
			auto else_block = context->new_block("else");
			auto merge_block = context->new_block("endif");
			context->cond_jump_to(syntax_node[0].code_gen(context).get_rvalue(), then_block, else_block);
			
			context->set_block(then_block);
			syntax_node[1].code_gen(context);
			context->jump_to(merge_block);
			
			context->set_block(else_block);
			syntax_node[2].code_gen(context);
			context->jump_to(merge_block);
			
			context->set_block(merge_block);
			return AST_result();
		}},
		{ "Type Id InitExpr;", [](gen_node& syntax_node, AST_context* context){
			Type* type = syntax_node[0].code_gen(context).get_type();
			auto init_expr = syntax_node[2].code_gen(context);
			Value* init = init_expr.flag != AST_result::is_none ?
				create_static_cast(init_expr.get_rvalue(), type) : initialize(type);
			auto alloc = context->alloc_var(type, static_cast<term_node&>(syntax_node[1]).data.attr->value);
			lBuilder.CreateStore(init, alloc);
			return AST_result();
		}},
		{ "TypeDefine", parser::forward },
		{ "expr;", parser::forward },
		{ "{ Block }", [](gen_node& syntax_node, AST_context* context){
			AST_context new_context(context);
			syntax_node[0].code_gen(&new_context);
			return AST_result();
		}},
		{ "return expr;", [](gen_node& syntax_node, AST_context* context){
			context->leave_function(syntax_node[0].code_gen(context).get_rvalue());
			return AST_result();
		}},
		{ "return;", parser::forward },
		{ "break;", parser::forward },
		{ "continue;", parser::forward },
		{ ";", parser::empty }
	}},
	{ "Block", {
		{ "Block Stmt", parser::expand },
		{ "", parser::empty }
	}},
	{ "exprelem", {
		{ "( expr )", parser::forward },
		{ "Id", parser::forward },
		{ "Dec", parser::forward },
		{ "Hex", parser::forward },
		{ "Oct", parser::forward },
		{ "Float", parser::forward },
		{ "Scientific", parser::forward },
		{ "true", parser::forward },
		{ "false", parser::forward },
	}},
	{ "constexprelem", {
		{ "( expr )", parser::forward },
		//{ "Id", parser::forward },
		{ "Dec", parser::forward },
		{ "Hex", parser::forward },
		{ "Oct", parser::forward },
		{ "Float", parser::forward },
		{ "Scientific", parser::forward },
		{ "true", parser::forward },
		{ "false", parser::forward },
	}}
};

#endif
