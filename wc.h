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
		{ "void", "void", {word} },
		{ "int", "int", {word} },
		{ "float", "float", {word} },
		{ "char", "char", {word} },
		{ "bool", "bool", {word} },
		// key words
		{ "while", "while", {word, no_attr} },
		{ "if", "if", {word, no_attr} },
		{ "else", "else", {word, no_attr} },
		{ "for", "for", {word, no_attr} },
		{ "true", "true", {word} },
		{ "false", "false", {word} },
		//
		{ "typedef", "typedef", {word, no_attr} },
		{ "struct", "struct", {word, no_attr} },
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
				return AST_result(ConstantFP::get(float_type, val), false);
			throw err("invalid float literal: ", T.data);
		}},
		{ "Scientific", [](term_node& T, AST_context*){
			double val;
			if (sscanf(T.data.attr->value.c_str(), "%lf", &val) == 1)
				return AST_result(ConstantFP::get(float_type, val), false);
			throw err("invalid float literal: ", T.data);
		}},
		{ "Char", [](term_node& T, AST_context*){
			auto src = T.data.attr->value.c_str() + 1;
			if (*src != '\\') return AST_result(lBuilder.getInt8(*src), false);
			int src_char;
			if (*++src == 'x' || *src == 'X')
			{
				++src; if (sscanf(src, "%x", &src_char) != 1) throw err("invalid char literal");
				return AST_result(lBuilder.getInt8(src_char), false);
			}
			if (sscanf(src, "%o", &src_char) != 1) throw err("invalid char literal");
			return AST_result(lBuilder.getInt8(src_char), false);
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
		{ "void", [](term_node&, AST_context*){ return AST_result(void_type); } },
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
			RHS = create_implicit_cast(RHS, static_cast<AllocaInst*>(LHS)->getAllocatedType());
			lBuilder.CreateStore(RHS, LHS);
			return val;
		}},
		{ "%/=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_lvalue();
			auto LHS = val.get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto type = LHS->getType();
			create_implicit_cast(RHS, type);
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
			create_implicit_cast(RHS, type);
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
			create_implicit_cast(RHS, type);
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
			create_implicit_cast(RHS, type);
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
			create_implicit_cast(RHS, type);
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
			create_implicit_cast(RHS, type);
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
			create_implicit_cast(RHS, type);
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
			create_implicit_cast(RHS, type);
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
			create_implicit_cast(RHS, type);
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
			create_implicit_cast(RHS, type);
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
			auto then_block = AST_context::new_block("then");
			auto else_block = AST_context::new_block("else");
			auto merge_block = AST_context::new_block("endif");
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
		{ "*%", right_asl, [](gen_node& syntax_node, AST_context* context)->AST_result{
			auto data = syntax_node[0].code_gen(context).get_among<PointerType, ArrayType>();
			switch (data.second)
			{
			case 0: return AST_result(data.first, true);
			case 1: vector<Value*> idx = { ConstantInt::get(int_type, 0), ConstantInt::get(int_type, 0) };
				return AST_result(GetElementPtrInst::Create(data.first, idx, "", context->get_block()), true);
			}
		}},
		{ "&%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto RHS = syntax_node[0].code_gen(context).get_lvalue();
			return AST_result(RHS, false);
		}},
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
			return AST_result(lBuilder.CreateNot(create_implicit_cast(RHS, bool_type)), false);
		}},
		/*{
			"~%", right_asl
		}*/
	},
	
	{
		{ "% [ %Expr ]", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto data = syntax_node[0].code_gen(context).get_among<PointerType, ArrayType>();
			auto index = create_implicit_cast(syntax_node[1].code_gen(context).get_rvalue(), int_type);
			vector<Value*> idx = { ConstantInt::get(int_type, 0), index };
			return AST_result(GetElementPtrInst::Create(data.first, idx, "", context->get_block()), true);
		}},
		{ "% ( %$ )", left_asl, [](gen_node& syntax_node, AST_context* context){	
			auto function = syntax_node[0].code_gen(context).get_function();
			auto params = syntax_node[1].code_gen(context).get_data<std::vector<Value*>>();
			auto function_proto = function->getFunctionType();
			if (function_proto->getNumParams() != params->size())
				throw err("function param number mismatch");
			for (unsigned i = 0; i < params->size(); ++i)
				(*params)[i] = create_implicit_cast((*params)[i], function_proto->getParamType(i));
			auto call_inst = lBuilder.CreateCall(function, *params);
			delete params;
			return AST_result(call_inst, false);
		}},
		/*{
			"%.%", left_asl
		},*/
	},
};

using function_params = vector<Type*>;

parser::init_rules mparse_rules = 
{	// Basic
	{ "S", {
		{ "S GlobalItem", parser::expand },
		{ "", parser::empty }
	}},
	{ "GlobalItem", {
		{ "Function", parser::forward },
		{ "TypeDefine;", parser::forward },
		{ "StructDefine;", parser::forward },
		{ "GlobalVarDefine;", parser::forward }
	}},
	{ "Stmt", {
		{ "while ( Expr ) Stmt", [](gen_node& syntax_node, AST_context* context){
			auto loop_end = context->loop_end;
			auto loop_next = context->loop_next;
			auto cond_block = context->loop_next = AST_context::new_block("while_cond");
			auto body_block = AST_context::new_block("while_body");
			auto merge_block = context->loop_end = AST_context::new_block("endwhile");
			
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
		{ "if ( Expr ) Stmt else Stmt", [](gen_node& syntax_node, AST_context* context){	
			auto then_block = AST_context::new_block("then");
			auto else_block = AST_context::new_block("else");
			auto merge_block = AST_context::new_block("endif");
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
		{ "if ( Expr ) Stmt", [](gen_node& syntax_node, AST_context* context){	
			auto then_block = AST_context::new_block("then");
			auto merge_block = AST_context::new_block("endif");
			context->cond_jump_to(syntax_node[0].code_gen(context).get_rvalue(), then_block, merge_block);
			
			context->set_block(then_block);
			syntax_node[1].code_gen(context);
			context->jump_to(merge_block);
			
			context->set_block(merge_block);
			return AST_result();
		}},
		{ "LocalVarDefine ;", parser::forward },
		{ "TypeDefine ;", parser::forward },
		{ "Expr ;", parser::forward },
		{ "{ Block }", [](gen_node& syntax_node, AST_context* context){
			AST_context new_context(context);
			syntax_node[0].code_gen(&new_context);
			return AST_result();
		}},
		{ "return expr ;", [](gen_node& syntax_node, AST_context* context){
			context->leave_function(syntax_node[1].code_gen(context).get_rvalue());
			return AST_result();
		}},
		{ "return ;", parser::forward },
		{ "break ;", parser::forward },
		{ "continue ;", parser::forward },
		{ ";", parser::empty }
	}},
	{ "Block", {
		{ "Block Stmt", parser::expand },
		{ "", parser::empty }
	}},
	
	// Expressions
	{ "Expr", {
		{ "Expr, expr", [](gen_node& syntax_node, AST_context* context){
			syntax_node[0].code_gen(context);
			return syntax_node[1].code_gen(context);
		}},
		{ "expr", parser::forward }
	}},
	{ "ConstExpr", {
		{ "ConstExpr, constexpr", [](gen_node& syntax_node, AST_context* context){
			syntax_node[0].code_gen(context);
			return syntax_node[1].code_gen(context);
		}},
		{ "constexpr", parser::forward }
	}},
	{ "TypeExpr", {
		{ "* TypeExpr", [](gen_node& syntax_node, AST_context* context){
			auto& base_type_name = type_names[context->current_type];
			context->current_type = PointerType::getUnqual(context->current_type);
			type_names[context->current_type] = base_type_name + " ptr";
			return syntax_node[0].code_gen(context);
		}},
		{ "TypeExpr0", parser::forward }
	}},
	{ "TypeExpr0", {
		{ "TypeExpr0 ( FunctionParams )", [](gen_node& syntax_node, AST_context* context){
			if (context->current_type->isArrayTy())
				throw err("function cannot return an array");
			if (context->current_type->isFunctionTy())
				throw err("function cannot return a function");
			
			auto params = syntax_node[1].code_gen(context).get_data<function_params>();
			auto& base_type_name = type_names[context->current_type];
			context->current_type = FunctionType::get(context->current_type, *params, false);	// cannot return an array
			type_names[context->current_type] = base_type_name + "(";
			if (params->size())
			{
				type_names[context->current_type] += type_names[(*params)[0]];
				for (auto itr = params->begin() + 1; itr != params->end(); ++itr)
					type_names[context->current_type] += ", " + type_names[*itr];
			}
			type_names[context->current_type] += ")";
			delete params;
			return syntax_node[0].code_gen(context);
		}},
		{ "TypeExpr0 [ ConstExpr ]", [](gen_node& syntax_node, AST_context* context){
			if (context->current_type == void_type)
				throw err("cannot create array of void type");
			if (context->current_type->isFunctionTy())
				throw err("cannot create array of function");
			auto size = static_cast<ConstantInt*>(create_implicit_cast(
				syntax_node[1].code_gen(context).get_rvalue(), int_type));
			if (size->isNegative()) throw err("negative array size");
			auto& base_type_name = type_names[context->current_type];
			//auto elem_type = context->current_type;
			context->current_type = ArrayType::get(context->current_type, size->getZExtValue());
			// record typename
			type_names[context->current_type] = base_type_name + "[" + [](int x)->string
			{
				char s[20];	sprintf(s, "%d", x); return s;
			}(size->getZExtValue()) + "]";
			// create implicit cast to pointer
/*			auto pointer_type = PointerType::getUnqual(elem_type);
			implicit_casts[{ context->current_type, pointer_type }] = [](Value* v)->Value*
			{
				//return GetElementPtrInst::Create(array, idx, "", context->get_block()), true);
			};*/
			return syntax_node[0].code_gen(context);
		}},
		{ "Id", [](gen_node& syntax_node, AST_context* context){
			context->current_name = static_cast<term_node&>(syntax_node[0]).data.attr->value;
			return AST_result();
		}},
		{ "( TypeExpr )", parser::forward },
		{ "", [](gen_node& syntax_node, AST_context* context){
			context->current_name = "";
			return AST_result();
		}}		// dummy
	}},
	{ "Type", {
		{ "void", parser::forward },
		{ "int", parser::forward },
		{ "float", parser::forward },
		{ "char", parser::forward },
		{ "bool", parser::forward },
		{ "Id", [](gen_node& syntax_node, AST_context* context){
			return context->get_type(static_cast<term_node&>(syntax_node[0]).data.attr->value);
		}}
	}},
	
	// Function
	{ "FunctionParams", {
		{ "FunctionParamList", [](gen_node& syntax_node, AST_context* context){
			auto param = syntax_node[0].code_gen(context);
			context->collect_param_name = false;
			return param;
		}},
		{ "", [](gen_node&, AST_context* context){
			context->collect_param_name = false;
			return AST_result(new function_params);
		}}
	}},
	{ "FunctionParamList", {
		{ "FunctionParamList , FunctionParam", [](gen_node& syntax_node, AST_context* context){
			function_params* p = syntax_node[0].code_gen(context).get_data<function_params>();
			p->push_back(syntax_node[1].code_gen(context).get_type());
			return AST_result(p);
		}},
		{ "FunctionParam", [](gen_node& syntax_node, AST_context* context){
			function_params* p = new function_params;
			p->push_back(syntax_node[0].code_gen(context).get_type());
			return AST_result(p);
		}},
	}},
	{ "FunctionParam", {
		{ "Type TypeExpr", [](gen_node& syntax_node, AST_context* context){
			auto bak_type = context->current_type;
			auto bak_name = context->current_name;
			auto bak_collect = context->collect_param_name;
			
			context->collect_param_name = false;
			context->current_type = syntax_node[0].code_gen(context).get_type();
			syntax_node[1].code_gen(context);
			
			auto param = context->current_type;
			if (context->collect_param_name = bak_collect)
				context->function_param_name.push_back(context->current_name);
			context->current_type = bak_type;
			context->current_name = bak_name;
			return AST_result(param);
		}}
	}},
	{ "Function", {
		{ "Type TypeExpr { Block }", [](gen_node& syntax_node, AST_context* context){
			context->collect_param_name = true;
			context->current_type = syntax_node[0].code_gen(context).get_type();
			syntax_node[1].code_gen(context);
			if (!context->current_type->isFunctionTy()) throw err("not function type");
			Function* F = Function::Create(static_cast<FunctionType*>(context->current_type),
				Function::ExternalLinkage, context->current_name, lModule);
			context->add_func(F, context->current_name);
			AST_context new_context(context, F);
			new_context.activate();
			context->collect_param_name = false;
			/*unsigned i = 0;
			for (auto itr = F->arg_begin(); itr != F->arg_end(); ++itr, ++i)
			{	//itr->setName(p->second[i]->attr->value);
				lBuilder.CreateStore(itr, new_context.alloc_var((*p)[i], p->second[i]));
			}*/
			syntax_node[2].code_gen(&new_context);
			new_context.finish_func();
			return AST_result();
		}}
	}},
	
	// Defination
	{ "TypeDefine", {
		{ "CStyleTypeDefine", parser::forward },
		{ "CppStyleTypeDefine", parser::forward }
	}},
	{ "CStyleTypeDefine", {
		{ "CStyleTypeDefine, TypeExpr", [](gen_node& syntax_node, AST_context* context){
			auto base_type = context->current_type = syntax_node[0].code_gen(context).get_type();
			syntax_node[1].code_gen(context);
			context->add_type(context->current_type, context->current_name);
			return AST_result(base_type);
		}},
		{ "typedef Type TypeExpr", [](gen_node& syntax_node, AST_context* context){
			auto base_type = context->current_type = syntax_node[0].code_gen(context).get_type();
			syntax_node[1].code_gen(context);
			context->add_type(context->current_type, context->current_name);
			return AST_result(base_type);
		}}
	}},
	{ "CppStyleTypeDefine", {
		{ "typedef Id = Type TypeExpr", [](gen_node& syntax_node, AST_context* context){
			auto base_type = context->current_type = syntax_node[1].code_gen(context).get_type();
			syntax_node[2].code_gen(context);
			if (context->current_name != "") throw err("type expression not dummy");
			context->add_type(context->current_type, static_cast<term_node&>(syntax_node[0]).data.attr->value);
			return AST_result();
		}}
	}},
	{ "GlobalVarDefine", {
		{ "GlobalVarDefine, TypeExpr GlobalInitExpr", [](gen_node& syntax_node, AST_context* context){
			Type* base_type = syntax_node[0].code_gen(context).get_type();
			context->current_type = base_type;
			syntax_node[1].code_gen(context);
			auto init_expr = syntax_node[2].code_gen(context);
			Constant* init = static_cast<Constant*>(init_expr.flag != AST_result::is_none ?
				init_expr.get_rvalue() : nullptr);		// init this value implicitly by default
			context->alloc_var(context->current_type, context->current_name, init);
			return AST_result(base_type);
		}},
		{ "Type TypeExpr GlobalInitExpr", [](gen_node& syntax_node, AST_context* context){
			Type* base_type = syntax_node[0].code_gen(context).get_type();
			context->current_type = base_type;
			syntax_node[1].code_gen(context);
			auto init_expr = syntax_node[2].code_gen(context);
			Constant* init = static_cast<Constant*>(init_expr.flag != AST_result::is_none ?
				init_expr.get_rvalue() : nullptr);		// init this value implicitly by default
			context->alloc_var(context->current_type, context->current_name, init);
			return AST_result(base_type);
		}}
	}},
	{ "GlobalInitExpr", {
		{ "= ConstExpr", parser::forward },
		{ "", parser::empty }
	}},
	{ "LocalVarDefine", {
		{ "LocalVarDefine, TypeExpr InitExpr", [](gen_node& syntax_node, AST_context* context){
			Type* base_type = syntax_node[0].code_gen(context).get_type();
			context->current_type = base_type;
			syntax_node[1].code_gen(context);
			auto init_expr = syntax_node[2].code_gen(context);
			auto init = init_expr.flag != AST_result::is_none ? init_expr.get_rvalue() : nullptr;		// no init
			context->alloc_var(context->current_type, context->current_name, init);
			return AST_result(base_type);
		}},
		{ "Type TypeExpr InitExpr", [](gen_node& syntax_node, AST_context* context){
			Type* base_type = syntax_node[0].code_gen(context).get_type();
			context->current_type = base_type;
			syntax_node[1].code_gen(context);
			auto init_expr = syntax_node[2].code_gen(context);
			auto init = init_expr.flag != AST_result::is_none ? init_expr.get_rvalue() : nullptr;		// no init
			context->alloc_var(context->current_type, context->current_name, init);
			return AST_result(base_type);
		}}
	}},
	{ "InitExpr", {
		{ "= Expr", parser::forward },
		{ "", parser::empty }
	}},
	
	// Struct
	{ "StructDefine", {
		{ "struct Id { StructInterface }", [](gen_node& syntax_node, AST_context* context){
			auto& class_name =  static_cast<term_node&>(syntax_node[0]).data.attr->value;
			auto elems = syntax_node[1].code_gen(context).get_data<vector<Type*>>();
			auto class_type = StructType::create(*elems, class_name);
			context->add_type(class_type, class_name);
			/*for (auto itr = class_type->element_begin(); itr != class_type->element_end(); ++itr)
			{
				cout << (*itr)->getTypeID() << endl;
			}*/
			type_names[class_type] = class_name;
			delete elems;
			return AST_result();
		}},
	}},
	{ "StructInterface", {
		{ "StructInterface StructInterfaceItem", [](gen_node& syntax_node, AST_context* context){
			auto elems = syntax_node[0].code_gen(context).get_data<vector<Type*>>();
			elems->push_back(syntax_node[1].code_gen(context).get_type());
			return AST_result(elems);
		}},
		{ "", [](gen_node& syntax_node, AST_context* context){ return AST_result(new vector<Type*>); } }
	}},
	{ "StructInterfaceItem", {
		{ "Type Id ;", [](gen_node& syntax_node, AST_context* context){
			auto type = syntax_node[0].code_gen(context).get_type();
			return AST_result(type);
		}},
	}},
	
	{ "exprelem", {
		{ "( Expr )", parser::forward },
		{ "Id", parser::forward },
		{ "Dec", parser::forward },
		{ "Hex", parser::forward },
		{ "Oct", parser::forward },
		{ "Float", parser::forward },
		{ "Scientific", parser::forward },
		{ "Char", parser::forward },
		{ "true", parser::forward },
		{ "false", parser::forward },
	}},
	{ "constexprelem", {
		{ "( ConstExpr )", parser::forward },
		//{ "Id", parser::forward },
		{ "Dec", parser::forward },
		{ "Hex", parser::forward },
		{ "Oct", parser::forward },
		{ "Float", parser::forward },
		{ "Scientific", parser::forward },
		{ "Char", parser::forward },
		{ "true", parser::forward },
		{ "false", parser::forward },
	}},
};

#endif
