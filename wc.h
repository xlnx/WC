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
		{ "finally", "finally", {word, no_attr} },
		{ "for", "for", {word, no_attr} },
		{ "do", "do", {word, no_attr} },
		{ "ptr", "ptr", {word, no_attr} },
		{ "pointer", "pointer", {word, no_attr} },
		{ "fn", "fn", {word, no_attr} },
		{ "arr", "arr", {word, no_attr} },
		{ "virtual", "virtual", {word, no_attr} },
		{ "switch", "switch", {word, no_attr} },
		{ "case", "case", {word, no_attr} },
		{ "default", "default", {word, no_attr} },
		{ "true", "true", {word} },
		{ "false", "false", {word} },
		//
		{ "typedef", "typedef", {word, no_attr} },
		{ "class", "class", {word, no_attr} },
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
			static_cast<AST_local_context*>(context)->make_continue();
			return AST_result();
		}},
		{ "break", [](term_node&, AST_context* context){
			static_cast<AST_local_context*>(context)->make_break();
			return AST_result();
		}},
		{ "return", [](term_node&, AST_context* context){
			static_cast<AST_local_context*>(context)->make_return();
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
			RHS = create_implicit_cast(RHS, static_cast<AllocaInst*>(LHS)->getAllocatedType());
			lBuilder.CreateStore(RHS, LHS);
			return val;
		}},
		{ "%/=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_lvalue();
			auto LHS = val.get_among<ltype::integer, ltype::floating_point>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer, ltype::floating_point>();
			create_implicit_cast(RHS.first, LHS.first->getType());
			switch (LHS.second)
			{
			case 0: lBuilder.CreateStore(lBuilder.CreateSDiv(LHS.first, RHS.first, "SDiv"), alloc); return val;
			case 1: lBuilder.CreateStore(lBuilder.CreateFDiv(LHS.first, RHS.first, "FDiv"), alloc); return val;
			}
		}},
		{ "%*=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_lvalue();
			auto LHS = val.get_among<ltype::integer, ltype::floating_point>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer, ltype::floating_point>();
			create_implicit_cast(RHS.first, LHS.first->getType());
			switch (LHS.second)
			{
			case 0: lBuilder.CreateStore(lBuilder.CreateMul(LHS.first, RHS.first, "Mul"), alloc); return val;
			case 1: lBuilder.CreateStore(lBuilder.CreateFMul(LHS.first, RHS.first, "FMul"), alloc); return val;
			}
		}},
		{ "%\\%=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_lvalue();
			auto LHS = val.get_among<ltype::integer>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer>();
			create_implicit_cast(RHS.first, LHS.first->getType());
			switch (LHS.second)
			{
			case 0: lBuilder.CreateStore(lBuilder.CreateSRem(LHS.first, RHS.first, "SRem"), alloc); return val;
			}
		}},
		{ "%+=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_lvalue();
			auto LHS = val.get_among<ltype::integer, ltype::floating_point>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer, ltype::floating_point>();
			create_implicit_cast(RHS.first, LHS.first->getType());
			switch (LHS.second)
			{
			case 0: lBuilder.CreateStore(lBuilder.CreateAdd(LHS.first, RHS.first, "Add"), alloc); return val;
			case 1: lBuilder.CreateStore(lBuilder.CreateFAdd(LHS.first, RHS.first, "FAdd"), alloc); return val;
			}
		}},
		{ "%-=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_lvalue();
			auto LHS = val.get_among<ltype::integer, ltype::floating_point>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer, ltype::floating_point>();
			create_implicit_cast(RHS.first, LHS.first->getType());
			switch (LHS.second)
			{
			case 0: lBuilder.CreateStore(lBuilder.CreateSub(LHS.first, RHS.first, "Sub"), alloc); return val;
			case 1: lBuilder.CreateStore(lBuilder.CreateFSub(LHS.first, RHS.first, "FSub"), alloc); return val;
			}
		}},
		{ "%<<=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_lvalue();
			auto LHS = val.get_among<ltype::integer>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer>();
			create_implicit_cast(RHS.first, LHS.first->getType());
			switch (LHS.second)
			{
			case 0: lBuilder.CreateStore(lBuilder.CreateShl(LHS.first, RHS.first, "Shl"), alloc); return val;
			}
		}},
		{ "%>>=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_lvalue();
			auto LHS = val.get_among<ltype::integer>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer>();
			create_implicit_cast(RHS.first, LHS.first->getType());
			switch (LHS.second)
			{
			case 0: lBuilder.CreateStore(lBuilder.CreateAShr(LHS.first, RHS.first, "AShr"), alloc); return val;
			}
		}},
		{ "%&=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_lvalue();
			auto LHS = val.get_among<ltype::integer>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer>();
			create_implicit_cast(RHS.first, LHS.first->getType());
			switch (LHS.second)
			{
			case 0: lBuilder.CreateStore(lBuilder.CreateAnd(LHS.first, RHS.first, "And"), alloc); return val;
			}
		}},
		{ "%^=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_lvalue();
			auto LHS = val.get_among<ltype::integer>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer>();
			create_implicit_cast(RHS.first, LHS.first->getType());
			switch (LHS.second)
			{
			case 0: lBuilder.CreateStore(lBuilder.CreateXor(LHS.first, RHS.first, "Xor"), alloc); return val;
			}
		}},
		{ "%|=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_lvalue();
			auto LHS = val.get_among<ltype::integer>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer>();
			create_implicit_cast(RHS.first, LHS.first->getType());
			switch (LHS.second)
			{
			case 0: lBuilder.CreateStore(lBuilder.CreateOr(LHS.first, RHS.first, "Or"), alloc); return val;
			}
		}}
	},
	
	{
		{ "%?%:%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto local_context = static_cast<AST_local_context*>(context);
			auto then_block = AST_context::new_block("then");
			auto else_block = AST_context::new_block("else");
			auto merge_block = AST_context::new_block("endif");
			local_context->make_cond_br(syntax_node[0].code_gen(context).get_rvalue(), then_block, else_block);
			
			local_context->set_block(then_block);
			auto then_value = syntax_node[1].code_gen(context).get_rvalue();
			then_block = local_context->get_block();
			local_context->make_br(merge_block);
			
			local_context->set_block(else_block);
			auto else_value = syntax_node[2].code_gen(context).get_rvalue();
			then_block = local_context->get_block();
			local_context->make_br(merge_block);
			
			local_context->set_block(merge_block);
			auto PN = lBuilder.CreatePHI(get_binary_sync_type(then_value, else_value), 2, "PHI");
			PN->addIncoming(then_value, then_block);
			PN->addIncoming(else_value, else_block);
			return AST_result(PN, false); 
		}}
	},
	
	{
		{ "%||%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			binary_sync_cast(LHS, RHS, bool_type);
			return AST_result(lBuilder.CreateOr(LHS, RHS, "Or"), false);
		}}
	},
	
	{
		{ "%&&%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			binary_sync_cast(LHS, RHS, bool_type);
			return AST_result(lBuilder.CreateAnd(LHS, RHS, "And"), false);
		}}
	},
	
	{
		{ "%|%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_among<ltype::integer>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer>();
			binary_sync_cast(LHS.first, RHS.first);
			return AST_result(lBuilder.CreateOr(LHS.first, RHS.first, "Or"), false);
		}}
	},
	
	{
		{ "%^%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_among<ltype::integer>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer>();
			binary_sync_cast(LHS.first, RHS.first);
			return AST_result(lBuilder.CreateXor(LHS.first, RHS.first, "Xor"), false);
		}}
	},
	
	{
		{ "%&%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_among<ltype::integer>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer>();
			binary_sync_cast(LHS.first, RHS.first);
			return AST_result(lBuilder.CreateAnd(LHS.first, RHS.first, "And"), false);
		}}
	},
	
	{
		{ "%==%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_among<ltype::integer, ltype::floating_point>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer, ltype::floating_point>();
			auto key = binary_sync_cast(LHS.first, RHS.first);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateICmpEQ(LHS.first, RHS.first, "ICmpEQ"), false);
			}
			if (key == float_type)
			{
				return AST_result(lBuilder.CreateFCmpOEQ(LHS.first, RHS.first, "FCmpOEQ"), false);
			}
			throw err("unknown operator == for type: " + type_names[key]);
		}},
		{ "%!=%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateICmpNE(LHS, RHS, "ICmpNE"), false);
			}
			if (key == float_type)
			{
				return AST_result(lBuilder.CreateFCmpONE(LHS, RHS, "FCmpONE"), false);
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
				return AST_result(lBuilder.CreateICmpSGT(LHS, RHS, "ICmpSGT"), false);
			}
			if (key == float_type)
			{
				return AST_result(lBuilder.CreateFCmpOGT(LHS, RHS, "FCmpOGT"), false);
			}
			throw err("unknown operator > for type: " + type_names[key]);
		}},
		{ "%>=%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateICmpSGE(LHS, RHS, "ICmpSGE"), false);
			}
			if (key == float_type)
			{
				return AST_result(lBuilder.CreateFCmpOGE(LHS, RHS, "FCmpOGE"), false);
			}
			throw err("unknown operator >= for type: " + type_names[key]);
		}},
		{ "%<%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateICmpSLT(LHS, RHS, "ICmpSLT"), false);
			}
			if (key == float_type)
			{
				return AST_result(lBuilder.CreateFCmpOLT(LHS, RHS, "FCmpOLT"), false);
			}
			throw err("unknown operator < for type: " + type_names[key]);
		}},
		{ "%<=%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateICmpSLE(LHS, RHS, "ICmpSLE"), false);
			}
			if (key == float_type)
			{
				return AST_result(lBuilder.CreateFCmpOLE(LHS, RHS, "FCmpOLE"), false);
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
				return AST_result(lBuilder.CreateShl(LHS, RHS, "Shl"), false);
			}
			throw err("unknown operator << for type: " + type_names[key]);
		}},
		{ "%>>%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_rvalue();
			auto RHS = syntax_node[1].code_gen(context).get_rvalue();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateAShr(LHS, RHS, "AShr"), false);
			}
			throw err("unknown operator >> for type: " + type_names[key]);
		}}
	},
	
	{
		{ "%+%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_among<ltype::integer, 
				ltype::floating_point, ltype::pointer>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer,
				ltype::floating_point, ltype::pointer>();
			if (LHS.second == 2 && RHS.second == 2) throw err("unknown operator for pointer + pointer");
			if (LHS.second == 2)
			{
				if (RHS.second == 1) throw err("unknown operator for pointer + float");
				return AST_result(GetElementPtrInst::CreateInBounds(LHS.first, RHS.first, "PAdd",
					static_cast<AST_local_context*>(context)->get_block()), false);
			}
			if (RHS.second == 2)
			{
				if (LHS.second == 1) throw err("unknown operator for float + pointer");
				return AST_result(GetElementPtrInst::CreateInBounds(RHS.first, LHS.first, "PAdd",
					static_cast<AST_local_context*>(context)->get_block()), false);
			}
			auto key = binary_sync_cast(LHS.first, RHS.first);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateAdd(LHS.first, RHS.first, "Add"), false);
			}
			else if (key == float_type)
			{
				return AST_result(lBuilder.CreateFAdd(LHS.first, RHS.first, "FAdd"), false);
			}
			throw err("unknown operator + for type: " + type_names[key]);
		}},
		{ "%-%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_among<ltype::integer,
				ltype::floating_point, ltype::pointer>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer,
				ltype::floating_point, ltype::pointer>();
			if (LHS.second == 2 && RHS.second == 2)
			{
				throw err("unknown operator for pointer - pointer");
			}
			if (LHS.second == 2)
			{
				if (RHS.second == 1) throw err("unknown operator for pointer + float");
				return AST_result(GetElementPtrInst::CreateInBounds(LHS.first, lBuilder.CreateNeg(RHS.first),
					"PSub", static_cast<AST_local_context*>(context)->get_block()), false);
			}
			if (RHS.second == 2)
			{
				if (LHS.second == 1) throw err("unknown operator for float + pointer");
				return AST_result(GetElementPtrInst::CreateInBounds(RHS.first, lBuilder.CreateNeg(LHS.first),
					"PSub", static_cast<AST_local_context*>(context)->get_block()), false);
			}
			auto key = binary_sync_cast(LHS.first, RHS.first);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateSub(LHS.first, RHS.first, "Sub"), false);
			}
			else if (key == float_type)
			{
				return AST_result(lBuilder.CreateFSub(LHS.first, RHS.first, "FSub"), false);
			}
			throw err("unknown operator - for type: " + type_names[key]);
		}}
	},
	
	{
		{ "%/%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_any_among<ltype::integer, ltype::floating_point>();
			auto RHS = syntax_node[1].code_gen(context).get_any_among<ltype::integer, ltype::floating_point>();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateSDiv(LHS, RHS, "SDiv"), false);
			}
			else if (key == float_type)
			{
				return AST_result(lBuilder.CreateFDiv(LHS, RHS, "FDiv"), false);
			}
			throw err("unknown operator / for type: " + type_names[key]);
		}},
		{ "%*%", left_asl, [](gen_node& syntax_node, AST_context* context){	
			auto LHS = syntax_node[0].code_gen(context).get_any_among<ltype::integer>();
			auto RHS = syntax_node[1].code_gen(context).get_any_among<ltype::integer>();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateMul(LHS, RHS, "Mul"), false);
			}
			else if (key == float_type)
			{
				return AST_result(lBuilder.CreateFMul(LHS, RHS, "FMul"), false);
			}
			throw err("unknown operator * for type: " + type_names[key]);
		}},
		{ "%\\%%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_any_among<ltype::integer>();
			auto RHS = syntax_node[1].code_gen(context).get_any_among<ltype::integer>();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateSRem(LHS, RHS, "SRem"), false);
			}
			throw err("unknown operator % for type: " + type_names[key]);
		}}
	},
	
	{
		{ "*%", right_asl, [](gen_node& syntax_node, AST_context* context)->AST_result{
			auto data = syntax_node[0].code_gen(context).get_among<ltype::pointer, ltype::array>();
			switch (data.second)
			{
			case 0: if (static_cast<PointerType*>(data.first->getType())->getElementType()->isFunctionTy())
					return AST_result(data.first, false);
				return AST_result(data.first, true);
			case 1: vector<Value*> idx = { ConstantInt::get(int_type, 0), ConstantInt::get(int_type, 0) };
				return AST_result(GetElementPtrInst::CreateInBounds(data.first, idx, "PElem",
					static_cast<AST_local_context*>(context)->get_block()), true);
			}
		}},
		{ "&%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto data = syntax_node[0].code_gen(context);
			if (auto func = data.get<ltype::pointer>())
				if (static_cast<PointerType*>(func->getType())->getElementType()->isFunctionTy())
					return AST_result(func, false);
			return AST_result(data.get_lvalue(), false);
		}},
		{ "-%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto RHS = syntax_node[0].code_gen(context).get_rvalue();
			auto key = RHS->getType();
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateNeg(RHS, "Neg"), false);
			}
			else if (key == float_type)
			{
				return AST_result(lBuilder.CreateFNeg(RHS, "FNeg"), false);
			}
			throw err("unknown operator - for type: " + type_names[key]);
		}},
		{ "!%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto RHS = syntax_node[0].code_gen(context).get_rvalue();
			return AST_result(lBuilder.CreateNot(create_implicit_cast(RHS, bool_type), "Not"), false);
		}},
		/*{
			"~%", right_asl
		}*/
	},
	
	{
		{ "% [ %Expr ]", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto data = syntax_node[0].code_gen(context).get_among<ltype::pointer, ltype::array>();
			vector<Value*> idx;
			switch (data.second)
			{
			case 1: idx.push_back(ConstantInt::get(int_type, 0));
			case 0: idx.push_back(create_implicit_cast(syntax_node[1].code_gen(context).get_as<ltype::integer>(), int_type));
			}
			return AST_result(GetElementPtrInst::CreateInBounds(data.first, idx, "PElem",
				static_cast<AST_local_context*>(context)->get_block()), true);
		}},
		{ "% ( %$ )", left_asl, [](gen_node& syntax_node, AST_context* context){	
			auto fdata = syntax_node[0].code_gen(context).get_among<ltype::overload, ltype::function>();
			auto params = syntax_node[1].code_gen(context).get_data<std::vector<Value*>>();
			llvm::Function* function = nullptr;
			switch (fdata.second)
			{
			case 1: {
				function = static_cast<llvm::Function*>(fdata.first);
				auto function_proto = function->getFunctionType();
				if (function_proto->getNumParams() != params->size())
					throw err("function param number mismatch");
				for (unsigned i = 0; i < params->size(); ++i)
					(*params)[i] = create_implicit_cast((*params)[i], function_proto->getParamType(i));
			}
			case 0: {
				func_sig sig;
				for (auto arg: *params)
					{ sig.push_back(arg->getType()); }
				auto& fndata = (*reinterpret_cast<overload_map_type*>(fdata.first))[sig];
				if (!fndata) throw err("none of the overloaded functions matches the given param");
				switch (fndata.flag)
				{
				case function_meta::is_method: 
					if (!fndata.parent->selected) throw err("no object is selected");
					params->insert(params->begin(), fndata.parent->selected);
				case function_meta::is_function: function = fndata.ptr;
				}
			}
			}
			auto call_inst = lBuilder.CreateCall(function, *params, "Call");
			delete params;
			return AST_result(call_inst, false);
		}},
		{ "% . %Id", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto struct_inst = syntax_node[0].code_gen(context).get_as<ltype::wstruct>();
			auto struct_namespace = context->get_namespace(struct_inst);
			auto& name = static_cast<term_node&>(syntax_node[1]).data.attr->value;
			return struct_namespace->get_id(name, true);
		}},
		{ "% -> %Id", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto struct_inst = syntax_node[0].code_gen(context).get_as<ltype::pointer>();
			if (!static_cast<PointerType*>(struct_inst->getType())->getElementType()->isStructTy())
				throw err("target isnot struct pointer");
			auto struct_namespace = context->get_namespace(struct_inst);
			auto& name = static_cast<term_node&>(syntax_node[1]).data.attr->value;
			return struct_namespace->get_id(name, true);
		}},
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
	{ "CasesWithoutDefault", {
		{ "case ConstExpr : StmtEmptyBlock CasesWithoutDefault", [](gen_node& syntax_node, AST_context* context){
			auto label = static_cast<ConstantInt*>(syntax_node[0].code_gen(context).get_as<ltype::integer>());
			auto block_val = syntax_node[1].code_gen(context);
			auto block = block_val.flag ? block_val.get_data<llvm::BasicBlock>() : nullptr;
			auto switch_context = static_cast<AST_switch_context*>(context);
			if (!switch_context->default_block && block) switch_context->default_block = block;
			switch_context->cases.push_back(make_pair(label, block));
			syntax_node[2].code_gen(context);
			return AST_result();
		}},
		{ "", parser::empty }
	}},
	{ "Cases", {
		{ "case ConstExpr : StmtEmptyBlock Cases", [](gen_node& syntax_node, AST_context* context){
			auto label = static_cast<ConstantInt*>(syntax_node[0].code_gen(context).get_as<ltype::integer>());
			auto block_val = syntax_node[1].code_gen(context);
			auto block = block_val.flag ? block_val.get_data<llvm::BasicBlock>() : nullptr;
			static_cast<AST_switch_context*>(context)->cases.push_back(make_pair(label, block));
			syntax_node[2].code_gen(context);
			return AST_result();
		}},
		{ "default : StmtEmptyBlock CasesWithoutDefault", [](gen_node& syntax_node, AST_context* context){
			auto block_val = syntax_node[0].code_gen(context);
			static_cast<AST_switch_context*>(context)->default_block = block_val.flag ?
				block_val.get_data<llvm::BasicBlock>() : nullptr;
			syntax_node[1].code_gen(context);
			return AST_result();
		}},
		{ "", parser::empty }
	}},
	{ "Stmt", {
		{ "switch ( Expr ) { Cases }", [](gen_node& syntax_node, AST_context* context){
			AST_switch_context switch_context(static_cast<AST_local_context*>(context));
			auto value = syntax_node[0].code_gen(context).get_as<ltype::integer>();
			switch_context.make_br(switch_context.switch_block);
			syntax_node[1].code_gen(&switch_context);
			
			switch_context.set_block(switch_context.switch_block);
			if (!switch_context.default_block) switch_context.default_block = switch_context.switch_end;
			auto sch = SwitchInst::Create(value, switch_context.default_block, switch_context.cases.size(),
				switch_context.get_block());
			for (auto itr = switch_context.cases.begin(); itr != switch_context.cases.end(); ++itr)
			{
				for (auto sitr = itr + 1; sitr != switch_context.cases.end(); ++sitr)
				{
					if (itr->first->getValue() == sitr->first->getValue())
					throw err("case value redeclared");
				}
			}
			for (auto itr = switch_context.cases.rbegin(); itr != switch_context.cases.rend(); ++itr)
			{
				if (itr->second == nullptr) itr->second = itr == switch_context.cases.rbegin() ?
					switch_context.switch_end : (itr - 1)->second;
			}
			for (auto c: switch_context.cases)
			{
				sch->addCase(c.first, c.second);
			}
			switch_context.set_block(switch_context.switch_end);
			return AST_result();
		}},
		{ "while ( Expr ) Stmt finally Stmt", [](gen_node& syntax_node, AST_context* context){
			AST_while_loop_context while_context(static_cast<AST_local_context*>(context));
			auto loop_finally = context->new_block("loop_finally");
			
			while_context.make_br(while_context.loop_next);
			while_context.set_block(while_context.loop_next);
			while_context.make_cond_br(syntax_node[0].code_gen(&while_context).get_rvalue(),
				while_context.while_body, loop_finally);
			
			while_context.set_block(while_context.while_body);
			syntax_node[1].code_gen(&while_context);
			while_context.make_br(while_context.loop_next);
			
			while_context.set_block(loop_finally);
			syntax_node[2].code_gen(context);
			while_context.make_br(while_context.loop_end);
			
			while_context.set_block(while_context.loop_end);
			return AST_result();
		}},
		{ "while ( Expr ) Stmt", [](gen_node& syntax_node, AST_context* context){
			AST_while_loop_context while_context(static_cast<AST_local_context*>(context));
			while_context.make_br(while_context.loop_next);
			while_context.set_block(while_context.loop_next);
			while_context.make_cond_br(syntax_node[0].code_gen(&while_context).get_rvalue(),
				while_context.while_body, while_context.loop_end);
			
			while_context.set_block(while_context.while_body);
			syntax_node[1].code_gen(&while_context);
			while_context.make_br(while_context.loop_next);
			
			while_context.set_block(while_context.loop_end);
			return AST_result();
		}},
		{ "do Stmt while ( Expr ) finally Stmt", [](gen_node&syntax_node, AST_context* context){
			AST_while_loop_context while_context(static_cast<AST_local_context*>(context));
			auto loop_finally = context->new_block("loop_finally");
			
			while_context.make_br(while_context.loop_next);
			while_context.set_block(while_context.loop_next);
			syntax_node[0].code_gen(&while_context);
			while_context.make_br(while_context.while_body);
			
			while_context.set_block(while_context.while_body);
			while_context.make_cond_br(syntax_node[1].code_gen(&while_context).get_rvalue(),
				while_context.loop_next, loop_finally);
				
			while_context.set_block(loop_finally);
			syntax_node[2].code_gen(context);
			while_context.make_br(while_context.loop_end);
			
			while_context.set_block(while_context.loop_end);
			return AST_result();
		}},
		{ "do Stmt while ( Expr )", [](gen_node&syntax_node, AST_context* context){
			AST_while_loop_context while_context(static_cast<AST_local_context*>(context));
			while_context.make_br(while_context.loop_next);
			while_context.set_block(while_context.loop_next);
			syntax_node[0].code_gen(&while_context);
			while_context.make_br(while_context.while_body);
			
			while_context.set_block(while_context.while_body);
			while_context.make_cond_br(syntax_node[1].code_gen(&while_context).get_rvalue(),
				while_context.loop_next, while_context.loop_end);
			
			while_context.set_block(while_context.loop_end);
			return AST_result();
		}},
		{ "if ( Expr ) Stmt else Stmt", [](gen_node& syntax_node, AST_context* context){	
			auto local_context = static_cast<AST_local_context*>(context);
			auto then_block = AST_context::new_block("then");
			auto else_block = AST_context::new_block("else");
			auto merge_block = AST_context::new_block("endif");
			local_context->make_cond_br(syntax_node[0].code_gen(context).get_rvalue(), then_block, else_block);
			
			local_context->set_block(then_block);
			syntax_node[1].code_gen(context);
			local_context->make_br(merge_block);
			
			local_context->set_block(else_block);
			syntax_node[2].code_gen(context);
			local_context->make_br(merge_block);
			
			local_context->set_block(merge_block);
			return AST_result();
		}},
		{ "if ( Expr ) Stmt", [](gen_node& syntax_node, AST_context* context){
			auto local_context = static_cast<AST_local_context*>(context);
			auto then_block = AST_context::new_block("then");
			auto merge_block = AST_context::new_block("endif");
			local_context->make_cond_br(syntax_node[0].code_gen(context).get_rvalue(), then_block, merge_block);
			
			local_context->set_block(then_block);
			syntax_node[1].code_gen(context);
			local_context->make_br(merge_block);
			
			local_context->set_block(merge_block);
			return AST_result();
		}},
		{ "LocalVarDefine ;", parser::forward },
		{ "TypeDefine ;", parser::forward },
		{ "Expr ;", parser::forward },
		{ "StmtBlock", parser::forward },
		{ "return expr ;", [](gen_node& syntax_node, AST_context* context){
			static_cast<AST_local_context*>(context)->make_return(syntax_node[1].code_gen(context).get_rvalue());
			return AST_result();
		}},
		{ "return ;", parser::forward },
		{ "break ;", parser::forward },
		{ "continue ;", parser::forward },
		{ ";", parser::empty }
	}},
	{ "StmtEmptyBlock", {
		{ "StmtBlock", parser::forward },
		{ "", parser::empty }
	}},
	{ "StmtBlock", {
		{ "{ Block }", [](gen_node& syntax_node, AST_context* context){
			AST_local_context new_context(static_cast<AST_local_context*>(context));
			auto block = new_context.get_block();
			syntax_node[0].code_gen(&new_context);
			return AST_result(reinterpret_cast<void*>(block));
		}}
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
	{ "Type", {
		{ "pointer", [](gen_node& syntax_node, AST_context* context){
			return AST_result(void_ptr_type);
		}},
		{ "ptr Type", [](gen_node& syntax_node, AST_context* context){
			auto base_type = syntax_node[0].code_gen(context).get_type();
			auto ret = PointerType::getUnqual(base_type);
			type_names[ret] = type_names[base_type] + " ptr";
			return AST_result(ret);
		}},
		{ "fn ( FunctionParams ) FunctionRetType", [](gen_node& syntax_node, AST_context* context){
			auto base_type = syntax_node[1].code_gen(context).get_type();
			if (base_type->isArrayTy())
				throw err("function cannot return an array");
			if (base_type->isFunctionTy())
				throw err("function cannot return a function");
			
			auto params = syntax_node[0].code_gen(context).get_data<function_params>();
			auto ret = FunctionType::get(base_type, *params, false);	// cannot return an array
			type_names[ret] = type_names[base_type] + "(";
			if (params->size())
			{
				type_names[ret] += type_names[(*params)[0]];
				for (auto itr = params->begin() + 1; itr != params->end(); ++itr)
					type_names[ret] += ", " + type_names[*itr];
			}
			type_names[ret] += ")";
			delete params;
			return AST_result(ret);
		}},
		{ "arr ConstExpr Type", [](gen_node& syntax_node, AST_context* context){
			auto base_type = syntax_node[1].code_gen(context).get_type();
			if (base_type->isFunctionTy())
				throw err("cannot create array of function");
				
			auto size = static_cast<ConstantInt*>(create_implicit_cast(
				syntax_node[0].code_gen(context).get_rvalue(), int_type));
			if (size->isNegative()) throw err("negative array size");
			//auto elem_type = context->current_type;
			auto ret = ArrayType::get(base_type, size->getZExtValue());
			// record typename
			type_names[ret] = type_names[base_type] + "[" + [](int x)->string
			{
				char s[20];	sprintf(s, "%d", x); return s;
			}(size->getZExtValue()) + "]";
			return AST_result(ret);
		}},
		{ "TypeElem", parser::forward }		// dummy
	}},
	{ "FunctionRetType", {
		{ "-> Type", parser::forward },
		{ "", [](gen_node&, AST_context*){ return AST_result(void_type); } }
	}},
	{ "TypeElem", {
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
		{ "Type Id", [](gen_node& syntax_node, AST_context* context){
			auto bak_collect = context->collect_param_name;
			context->collect_param_name = false;
			
			auto type = syntax_node[0].code_gen(context).get_type();	
			if (type->isFunctionTy() || type->isVoidTy()) throw err("type " + type_names[type] + " is invalid argument type");
			if (context->collect_param_name = bak_collect)
				context->function_param_name.push_back(static_cast<term_node&>(syntax_node[1]).data.attr->value);
			return AST_result(type);
		}},
		{ "Type", [](gen_node& syntax_node, AST_context* context){
			auto bak_collect = context->collect_param_name;
			context->collect_param_name = false;
			
			auto type = syntax_node[0].code_gen(context).get_type();	
			if (type->isFunctionTy() || type->isVoidTy()) throw err("type " + type_names[type] + " is invalid argument type");
			if (context->collect_param_name = bak_collect)
				context->function_param_name.push_back("");
			return AST_result(type);
		}}
	}},
	{ "Function", {
		{ "Type Id { Block }", [](gen_node& syntax_node, AST_context* context){
			context->collect_param_name = true;
			context->function_param_name.resize(0);
			auto type = syntax_node[0].code_gen(context).get_type();
			auto name = static_cast<term_node&>(syntax_node[1]).data.attr->value;
			if (!type->isFunctionTy()) throw err("not function type");
			Function* F = Function::Create(static_cast<FunctionType*>(type), Function::ExternalLinkage, name, lModule);
			AST_function_context new_context(context, F, name);
			new_context.register_args();
			syntax_node[2].code_gen(&new_context);
			return AST_result();
		}}
	}},
	
	// Defination
	{ "TypeDefine", {
		{ "typedef Id = Type", [](gen_node& syntax_node, AST_context* context){
			auto type = syntax_node[1].code_gen(context).get_type();
			context->add_type(type, static_cast<term_node&>(syntax_node[0]).data.attr->value);
			return AST_result();
		}}
	}},
	{ "GlobalVarDefine", {
		{ "GlobalVarDefine, Id GlobalInitExpr", [](gen_node& syntax_node, AST_context* context){
			Type* type = syntax_node[0].code_gen(context).get_type();
			auto init_expr = syntax_node[2].code_gen(context);
			Constant* init = static_cast<Constant*>(init_expr.flag != AST_result::is_none ?
				init_expr.get_rvalue() : nullptr);		// init this value implicitly by default
			context->alloc_var(type, static_cast<term_node&>(syntax_node[1]).data.attr->value, init);
			return AST_result(type);
		}},
		{ "Type Id GlobalInitExpr", [](gen_node& syntax_node, AST_context* context){
			Type* type = syntax_node[0].code_gen(context).get_type();
			auto init_expr = syntax_node[2].code_gen(context);
			Constant* init = static_cast<Constant*>(init_expr.flag != AST_result::is_none ?
				init_expr.get_rvalue() : nullptr);		// init this value implicitly by default
			context->alloc_var(type, static_cast<term_node&>(syntax_node[1]).data.attr->value, init);
			return AST_result(type);
		}}
	}},
	{ "GlobalInitExpr", {
		{ "= ConstExpr", parser::forward },
		{ "", parser::empty }
	}},
	{ "LocalVarDefine", {
		{ "LocalVarDefine, Id InitExpr", [](gen_node& syntax_node, AST_context* context){
			Type* type = syntax_node[0].code_gen(context).get_type();
			auto init_expr = syntax_node[2].code_gen(context);
			auto init = init_expr.flag != AST_result::is_none ? init_expr.get_rvalue() : nullptr;		// no init
			context->alloc_var(type, static_cast<term_node&>(syntax_node[1]).data.attr->value, init);
			return AST_result(type);
		}},
		{ "Type Id InitExpr", [](gen_node& syntax_node, AST_context* context){
			Type* type = syntax_node[0].code_gen(context).get_type();
			auto init_expr = syntax_node[2].code_gen(context);
			auto init = init_expr.flag != AST_result::is_none ? init_expr.get_rvalue() : nullptr;		// no init
			context->alloc_var(type, static_cast<term_node&>(syntax_node[1]).data.attr->value, init);
			return AST_result(type);
		}}
	}},
	{ "InitExpr", {
		{ "= Expr", parser::forward },
		{ "", parser::empty }
	}},
	
	// Struct
	{ "StructDefine", {
		{ "class Id { StructInterface }", [](gen_node& syntax_node, AST_context* context){
			auto& class_name =  static_cast<term_node&>(syntax_node[0]).data.attr->value;
			AST_struct_context* struct_context = new AST_struct_context(context);
			syntax_node[1].code_gen(struct_context);
			struct_context->finish_struct(class_name);
			syntax_node[1].code_gen(struct_context);
			return AST_result();
		}},
	}},
	{ "StructInterface", {
		{ "StructInterfaceItem StructInterface", parser::expand },
		{ "", parser::empty }
	}},
	{ "StructInterfaceItem", {
		{ "Type Id ;", [](gen_node& syntax_node, AST_context* context){
			auto struct_context = static_cast<AST_struct_context*>(context);
			if (!struct_context->type)
			{
				auto type = syntax_node[0].code_gen(context).get_type();
				auto name = static_cast<term_node&>(syntax_node[1]).data.attr->value;
				struct_context->alloc_var(type, name, nullptr);
			}
			return AST_result();
		}},
		{ "Method", parser::forward }
	}},
	{ "Method", {
		{ "Type Id MethodAttr { Block }", [](gen_node& syntax_node, AST_context* context){
			auto struct_context = static_cast<AST_struct_context*>(context);
			if (struct_context->type)
			{
				auto fnattr = syntax_node[2].code_gen(context).get_data<function_attr>();
				fnattr->insert(is_method);
				context->collect_param_name = true;
				context->function_param_name.resize(0);
				auto type = syntax_node[0].code_gen(context).get_type();
				auto name = static_cast<term_node&>(syntax_node[1]).data.attr->value;
				if (!type->isFunctionTy()) throw err("not function type");
				AST_method_context new_context(struct_context, static_cast<FunctionType*>(type), name, fnattr);
				new_context.register_args();
				syntax_node[3].code_gen(&new_context);
				delete fnattr;
			}
			return AST_result();
		}}
	}},
	{ "MethodAttr", {
		{ "MethodAttr MethodAttrElem", [](gen_node& syntax_node, AST_context* context){
			auto fnattr = syntax_node[0].code_gen(context).get_data<function_attr>();
			auto attr = syntax_node[1].code_gen(context).get_attr();
			if (fnattr->count(attr)) throw err("function attribute redeclared");
			fnattr->insert(attr);
			return AST_result(fnattr);
		}},
		{ "", [](gen_node& syntax_node, AST_context* context){ return AST_result(new function_attr); } }
	}},
	{ "MethodAttrElem", {
		{ "virtual", parser::attribute<is_virtual> }
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
