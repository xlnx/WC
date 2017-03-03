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
		{ "void", "void", {word} },
		// key words
		{ "while", "while", {word, no_attr} },
		{ "if", "if", {word, no_attr} },
		{ "else", "else", {word, no_attr} },
		{ "finally", "finally", {word, no_attr} },
		{ "for", "for", {word, no_attr} },
		{ "do", "do", {word, no_attr} },
		{ "ref", "ref", {word, no_attr} },
		{ "private", "private", {word, no_attr} },
		{ "protected", "protected", {word, no_attr} },
		{ "public", "public", {word, no_attr} },
		{ "fn", "fn", {word, no_attr} },
		{ "let", "let", {word, no_attr} },
		{ "as", "as", {word, no_attr} },
		{ "virtual", "virtual", {word, no_attr} },
		{ "override", "override", {word, no_attr} },
		{ "switch", "switch", {word, no_attr} },
		{ "case", "case", {word, no_attr} },
		{ "default", "default", {word, no_attr} },
		{ "true", "true", {word} },
		{ "false", "false", {word} },
		//
		{ "type", "type", {word, no_attr} },
		{ "class", "class", {word, no_attr} },
		//
		{ "return", "return", {word} },
		{ "break", "break", {word} },
		{ "continue", "continue", {word} },
		// identifier
		{ "Id", "[a-zA-Z_]\\w*", {word} },
		{ "IdType", "", {word} },
		{ "IdTemplateFunc", "", {word} },
		{ "IdTemplateClass", "", {word} },
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
			auto LHS = val.get_as<ltype::lvalue>();
			auto RHS = syntax_node[1].code_gen(context).get_as<ltype::rvalue>();
			RHS = create_implicit_cast(RHS, static_cast<AllocaInst*>(LHS)->getAllocatedType());
			lBuilder.CreateStore(RHS, LHS);
			return val;
		}},
		{ "%/=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_as<ltype::lvalue>();
			auto LHS = val.get_among<ltype::integer, ltype::floating_point>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer, ltype::floating_point>();
			RHS.first = create_implicit_cast(RHS.first, LHS.first->getType());
			switch (LHS.second)
			{
			case 0: lBuilder.CreateStore(lBuilder.CreateSDiv(LHS.first, RHS.first, "SDiv"), alloc); return val;
			case 1: lBuilder.CreateStore(lBuilder.CreateFDiv(LHS.first, RHS.first, "FDiv"), alloc); return val;
			}
		}},
		{ "%*=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_as<ltype::lvalue>();
			auto LHS = val.get_among<ltype::integer, ltype::floating_point>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer, ltype::floating_point>();
			RHS.first = create_implicit_cast(RHS.first, LHS.first->getType());
			switch (LHS.second)
			{
			case 0: lBuilder.CreateStore(lBuilder.CreateMul(LHS.first, RHS.first, "Mul"), alloc); return val;
			case 1: lBuilder.CreateStore(lBuilder.CreateFMul(LHS.first, RHS.first, "FMul"), alloc); return val;
			}
		}},
		{ "%\\%=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_as<ltype::lvalue>();
			auto LHS = val.get_among<ltype::integer>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer>();
			RHS.first = create_implicit_cast(RHS.first, LHS.first->getType());
			switch (LHS.second)
			{
			case 0: lBuilder.CreateStore(lBuilder.CreateSRem(LHS.first, RHS.first, "SRem"), alloc); return val;
			}
		}},
		{ "%+=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_as<ltype::lvalue>();
			auto LHS = val.get_among<ltype::integer, ltype::floating_point>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer, ltype::floating_point>();
			RHS.first = create_implicit_cast(RHS.first, LHS.first->getType());
			switch (LHS.second)
			{
			case 0: lBuilder.CreateStore(lBuilder.CreateAdd(LHS.first, RHS.first, "Add"), alloc); return val;
			case 1: lBuilder.CreateStore(lBuilder.CreateFAdd(LHS.first, RHS.first, "FAdd"), alloc); return val;
			}
		}},
		{ "%-=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_as<ltype::lvalue>();
			auto LHS = val.get_among<ltype::integer, ltype::floating_point>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer, ltype::floating_point>();
			RHS.first = create_implicit_cast(RHS.first, LHS.first->getType());
			switch (LHS.second)
			{
			case 0: lBuilder.CreateStore(lBuilder.CreateSub(LHS.first, RHS.first, "Sub"), alloc); return val;
			case 1: lBuilder.CreateStore(lBuilder.CreateFSub(LHS.first, RHS.first, "FSub"), alloc); return val;
			}
		}},
		{ "%<<=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_as<ltype::lvalue>();
			auto LHS = val.get_among<ltype::integer>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer>();
			RHS.first = create_implicit_cast(RHS.first, LHS.first->getType());
			switch (LHS.second)
			{
			case 0: lBuilder.CreateStore(lBuilder.CreateShl(LHS.first, RHS.first, "Shl"), alloc); return val;
			}
		}},
		{ "%>>=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_as<ltype::lvalue>();
			auto LHS = val.get_among<ltype::integer>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer>();
			RHS.first = create_implicit_cast(RHS.first, LHS.first->getType());
			switch (LHS.second)
			{
			case 0: lBuilder.CreateStore(lBuilder.CreateAShr(LHS.first, RHS.first, "AShr"), alloc); return val;
			}
		}},
		{ "%&=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_as<ltype::lvalue>();
			auto LHS = val.get_among<ltype::integer>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer>();
			RHS.first = create_implicit_cast(RHS.first, LHS.first->getType());
			switch (LHS.second)
			{
			case 0: lBuilder.CreateStore(lBuilder.CreateAnd(LHS.first, RHS.first, "And"), alloc); return val;
			}
		}},
		{ "%^=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_as<ltype::lvalue>();
			auto LHS = val.get_among<ltype::integer>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer>();
			RHS.first = create_implicit_cast(RHS.first, LHS.first->getType());
			switch (LHS.second)
			{
			case 0: lBuilder.CreateStore(lBuilder.CreateXor(LHS.first, RHS.first, "Xor"), alloc); return val;
			}
		}},
		{ "%|=%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto val = syntax_node[0].code_gen(context);
			auto alloc = val.get_as<ltype::lvalue>();
			auto LHS = val.get_among<ltype::integer>();
			auto RHS = syntax_node[1].code_gen(context).get_among<ltype::integer>();
			RHS.first = create_implicit_cast(RHS.first, LHS.first->getType());
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
			local_context->make_cond_br(syntax_node[0].code_gen(context).get_as<ltype::rvalue>(), then_block, else_block);

			local_context->set_block(then_block);
			auto then_value = syntax_node[1].code_gen(context).get_as<ltype::rvalue>();
			then_block = local_context->get_block();
			local_context->make_br(merge_block);

			local_context->set_block(else_block);
			auto else_value = syntax_node[2].code_gen(context).get_as<ltype::rvalue>();
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
		{ "% as %Type", left_asl, [](gen_node& syntax_node, AST_context* context){
			return AST_result(create_cast(syntax_node[0].code_gen(context).get_as<ltype::rvalue>(),
				syntax_node[1].code_gen(context).get_type()), false);
		}}
	},

	{
		{ "%||%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_as<ltype::rvalue>();
			auto RHS = syntax_node[1].code_gen(context).get_as<ltype::rvalue>();
			binary_sync_cast(LHS, RHS, bool_type);
			return AST_result(lBuilder.CreateOr(LHS, RHS, "Or"), false);
		}}
	},

	{
		{ "%&&%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_as<ltype::rvalue>();
			auto RHS = syntax_node[1].code_gen(context).get_as<ltype::rvalue>();
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
			auto LHS = syntax_node[0].code_gen(context).get_as<ltype::rvalue>();
			auto RHS = syntax_node[1].code_gen(context).get_as<ltype::rvalue>();
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
			auto LHS = syntax_node[0].code_gen(context).get_as<ltype::rvalue>();
			auto RHS = syntax_node[1].code_gen(context).get_as<ltype::rvalue>();
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
			auto LHS = syntax_node[0].code_gen(context).get_as<ltype::rvalue>();
			auto RHS = syntax_node[1].code_gen(context).get_as<ltype::rvalue>();
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
			auto LHS = syntax_node[0].code_gen(context).get_as<ltype::rvalue>();
			auto RHS = syntax_node[1].code_gen(context).get_as<ltype::rvalue>();
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
			auto LHS = syntax_node[0].code_gen(context).get_as<ltype::rvalue>();
			auto RHS = syntax_node[1].code_gen(context).get_as<ltype::rvalue>();
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
			auto LHS = syntax_node[0].code_gen(context).get_as<ltype::rvalue>();
			auto RHS = syntax_node[1].code_gen(context).get_as<ltype::rvalue>();
			auto key = binary_sync_cast(LHS, RHS);
			if (key == int_type || key == bool_type || key == char_type)
			{
				return AST_result(lBuilder.CreateShl(LHS, RHS, "Shl"), false);
			}
			throw err("unknown operator << for type: " + type_names[key]);
		}},
		{ "%>>%", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto LHS = syntax_node[0].code_gen(context).get_as<ltype::rvalue>();
			auto RHS = syntax_node[1].code_gen(context).get_as<ltype::rvalue>();
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
			return AST_result(data.get_as<ltype::lvalue>(), false);
		}},
		{ "-%", right_asl, [](gen_node& syntax_node, AST_context* context){
			auto RHS = syntax_node[0].code_gen(context).get_as<ltype::rvalue>();
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
			auto RHS = syntax_node[0].code_gen(context).get_as<ltype::rvalue>();
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
			auto params = syntax_node[1].code_gen(context).get_data<std::vector<Value*>>();
			auto fdata = syntax_node[0].code_gen(context).get_among<ltype::overload, ltype::function>();
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
				break;
			}
			case 0: {
				func_sig sig;
				for (auto arg: *params) sig.push_back(arg->getType());
				auto map = reinterpret_cast<overload_map_type*>(fdata.first);
				auto& fndata = (*map)[sig];
				if (!fndata) throw err("none of the overloaded functions matches the given param");
				switch (fndata.flag)
				{
				case function_meta::is_method:
					if (!fndata.object) throw err("no object is selected");
					params->insert(params->begin(), fndata.object);
					fndata.object = nullptr;
				case function_meta::is_function: function = fndata.ptr;
				}
				for (auto& f: *map) f.second.object = nullptr;
				break;
			}
			}

			auto call_inst = function->getReturnType() != void_type ? lBuilder.CreateCall(function, *params, "Call")
				: lBuilder.CreateCall(function, *params);
			delete params;
			if (function->getReturnType() != void_type) return AST_result(call_inst, false);
				else return AST_result();
		}},
		{ "% . %Id", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto struct_inst = syntax_node[0].code_gen(context).get_as<ltype::wstruct>();
			auto struct_namespace = context->get_namespace(struct_inst);
			auto& name = static_cast<term_node&>(syntax_node[1]).data.attr->value;
			auto ret = struct_namespace->get_id(name, true);
			struct_namespace->selected.pop();
			return ret;
		}},
		{ "% -> %Id ( %$ )", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto params = syntax_node[2].code_gen(context).get_data<std::vector<Value*>>();
			llvm::Function* function = nullptr;
			auto struct_inst = syntax_node[0].code_gen(context).get_as<ltype::pointer>();

			if (!static_cast<PointerType*>(struct_inst->getType())->getElementType()->isStructTy())
				throw err("target isnot struct pointer");
			auto struct_namespace = context->get_namespace(struct_inst);
			auto& name = static_cast<term_node&>(syntax_node[1]).data.attr->value;
			auto map = reinterpret_cast<overload_map_type*>(
				struct_namespace->get_id(name, true).get_as<ltype::overload>());
			struct_namespace->selected.pop();
			func_sig sig;
			for (auto arg: *params) sig.push_back(arg->getType());
			auto& fndata = (*map)[sig];
			if (!fndata) throw err("none of the overloaded functions matches the given param");

			params->insert(params->begin(), fndata.object);
			if (fndata.vtable_id)
				function = struct_namespace->get_virtual_function(fndata.object, fndata.vtable_id - 1);
			else
				function = fndata.ptr;
			for (auto& f: *map) f.second.object = nullptr;

			auto call_inst = function->getReturnType() != void_type ? lBuilder.CreateCall(function, *params, "Call")
				: lBuilder.CreateCall(function, *params);
			delete params;
			if (function->getReturnType() != void_type) return AST_result(call_inst, false);
				else return AST_result();
		}},
		{ "% -> %Id", left_asl, [](gen_node& syntax_node, AST_context* context){
			auto struct_inst = syntax_node[0].code_gen(context).get_as<ltype::pointer>();
			if (!static_cast<PointerType*>(struct_inst->getType())->getElementType()->isStructTy())
				throw err("target isnot struct pointer");
			auto struct_namespace = context->get_namespace(struct_inst);
			auto& name = static_cast<term_node&>(syntax_node[1]).data.attr->value;
			auto ret = struct_namespace->get_id(name, true);
			struct_namespace->selected.pop();
			return ret;
		}},
	},
};

parser::init_rules mparse_rules =
{	// Basic
	{ "S", {
		{ "S GlobalItem", parser::expand },
		{ "", parser::empty }
	}},
	{ "GlobalItem", {
		{ "Template", parser::forward },
		{ "Function", parser::forward },
		{ "Class;", parser::forward },
		{ "TypeDefine;", parser::forward },
		{ "GlobalVarDefine;", parser::forward }
	}},
	{ "InitList", {
		{ "InitList , InitItem", [](gen_node& syntax_node, AST_context* context){
			auto vec = reinterpret_cast<init_vec*>(syntax_node[0].code_gen(context).get_as<ltype::init_list>());
			auto data = reinterpret_cast<init_vec*>(syntax_node[1].code_gen(context).get_as<ltype::init_list>());
			for (auto& elem: *data) vec->push_back(elem);
			return AST_result(vec);
		}},
		{ "InitItem", [](gen_node& syntax_node, AST_context* context){
			auto vec = new init_vec;
			auto data = reinterpret_cast<init_vec*>(syntax_node[0].code_gen(context).get_as<ltype::init_list>());
			for (auto& elem: *data) vec->push_back(elem);
			return AST_result(vec);
		}}
	}},
	{ "InitItem", {
		{ "ConstExpr", [](gen_node& syntax_node, AST_context* context){
			auto vec = new init_vec;
			auto data = syntax_node[0].code_gen(context).get_among<ltype::rvalue, ltype::init_list>();
			switch (data.second)
			{
			case 0: vec->push_back({ static_cast<Constant*>(data.first), init_item::is_constant}); break;
			case 1: vec->push_back({ data.first, init_item::is_init_list}); break;
			}
			return AST_result(vec);
		}},
		{ "ConstExpr : ConstExpr", [](gen_node& syntax_node, AST_context* context){
			auto vec = new init_vec;
			auto data = syntax_node[0].code_gen(context).get_among<ltype::rvalue, ltype::init_list>();
			auto size = static_cast<ConstantInt*>(syntax_node[1].code_gen(context).get_as<ltype::integer>());
			if (size->isNegative()) throw err("negative init list size");
			//auto elem_type = context->current_type;
			auto t = size->getZExtValue();
			if (t == 0) throw err("zero init list size");
			switch (data.second)
			{
			case 0:
				while (t--)
					vec->push_back({ static_cast<Constant*>(data.first), init_item::is_constant});
				break;
			case 1: vec->push_back({ data.first, init_item::is_init_list});
				while (--t)
					vec->push_back({ new init_vec(*reinterpret_cast<init_vec*>(data.first)), init_item::is_init_list});
				break;
			}
			return AST_result(vec);
		}},
	}},
	{ "CasesWithoutDefault", {
		{ "case ConstExpr : Stmts CasesWithoutDefault", [](gen_node& syntax_node, AST_context* context){
			auto label = static_cast<ConstantInt*>(syntax_node[0].code_gen(context).get_as<ltype::integer>());
			auto switch_context = static_cast<AST_switch_context*>(context);
			auto case_block = AST_context::new_block("CaseBlock");
			if (!switch_context->cases.empty())
				switch_context->make_br(case_block);
			static_cast<AST_local_context*>(context)->set_block(case_block);
			syntax_node[1].code_gen(context);
			//if (!switch_context->default_block) switch_context->default_block = case_block;
			switch_context->cases.push_back(make_pair(label, case_block));
			syntax_node[2].code_gen(context);
			return AST_result();
		}},
		{ "", parser::empty }
	}},
	{ "Cases", {
		{ "case ConstExpr : Stmts Cases", [](gen_node& syntax_node, AST_context* context){
			auto label = static_cast<ConstantInt*>(syntax_node[0].code_gen(context).get_as<ltype::integer>());
			auto switch_context = static_cast<AST_switch_context*>(context);
			auto case_block = AST_context::new_block("CaseBlock");
			if (!switch_context->cases.empty())
				switch_context->make_br(case_block);
			static_cast<AST_local_context*>(context)->set_block(case_block);
			syntax_node[1].code_gen(context);
			switch_context->cases.push_back(make_pair(label, case_block));
			syntax_node[2].code_gen(context);
			return AST_result();
		}},
		{ "default : Stmts CasesWithoutDefault", [](gen_node& syntax_node, AST_context* context){
			auto switch_context = static_cast<AST_switch_context*>(context);
			auto case_block = AST_context::new_block("CaseBlock");
			if (!switch_context->cases.empty())
				switch_context->make_br(case_block);
			static_cast<AST_local_context*>(context)->set_block(case_block);
			syntax_node[0].code_gen(context);
			static_cast<AST_switch_context*>(context)->default_block = case_block;
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
			if (!switch_context.cases.empty())
				switch_context.make_br(switch_context.switch_end);

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
		},
		{	//$ parser callback
			{ 5, parser::enter_block },
			{ 7, parser::leave_block }
		}},
		{ "while ( Expr ) Stmt finally Stmt", [](gen_node& syntax_node, AST_context* context){
			AST_while_loop_context while_context(static_cast<AST_local_context*>(context));
			auto loop_finally = context->new_block("loop_finally");

			while_context.make_br(while_context.loop_next);
			while_context.set_block(while_context.loop_next);
			while_context.make_cond_br(syntax_node[0].code_gen(&while_context).get_as<ltype::rvalue>(),
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
			while_context.make_cond_br(syntax_node[0].code_gen(&while_context).get_as<ltype::rvalue>(),
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
			while_context.make_cond_br(syntax_node[1].code_gen(&while_context).get_as<ltype::rvalue>(),
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
			while_context.make_cond_br(syntax_node[1].code_gen(&while_context).get_as<ltype::rvalue>(),
				while_context.loop_next, while_context.loop_end);

			while_context.set_block(while_context.loop_end);
			return AST_result();
		}},
		{ "if ( Expr ) Stmt else Stmt", [](gen_node& syntax_node, AST_context* context){
			auto local_context = static_cast<AST_local_context*>(context);
			auto then_block = AST_context::new_block("then");
			auto else_block = AST_context::new_block("else");
			auto merge_block = AST_context::new_block("endif");
			local_context->make_cond_br(syntax_node[0].code_gen(context).get_as<ltype::rvalue>(), then_block, else_block);

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
			local_context->make_cond_br(syntax_node[0].code_gen(context).get_as<ltype::rvalue>(), then_block, merge_block);

			local_context->set_block(then_block);
			syntax_node[1].code_gen(context);
			local_context->make_br(merge_block);

			local_context->set_block(merge_block);
			return AST_result();
		}},
		{ "LocalVarDefine ;", parser::forward },
		{ "LocalRefDefine ;", parser::forward },
		{ "TypeDefine ;", parser::forward },
		{ "Expr ;", parser::forward },
		{ "StmtBlock", parser::forward },
		{ "return expr ;", [](gen_node& syntax_node, AST_context* context){
			static_cast<AST_local_context*>(context)->make_return(syntax_node[1].code_gen(context).get_as<ltype::rvalue>());
			return AST_result();
		}},
		{ "return ;", parser::forward },
		{ "break ;", parser::forward },
		{ "continue ;", parser::forward },
		{ ";", parser::empty }
	}},
	{ "Stmts", {
		{ "Stmts Stmt", parser::expand },
		{ "", parser::empty }
	}},
	{ "StmtBlock", {
		{ "{ Block }", [](gen_node& syntax_node, AST_context* context){
			AST_local_context new_context(static_cast<AST_local_context*>(context));
			auto block = new_context.get_block();
			syntax_node[0].code_gen(&new_context);
			return AST_result(reinterpret_cast<void*>(block));
		},
		{	//$ parser callback
			{ 1, parser::enter_block },
			{ 3, parser::leave_block }
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
		{ "expr", [](gen_node& syntax_node, AST_context* context){
			auto data = syntax_node[0].code_gen(context);
			if (auto ptr = reinterpret_cast<overload_map_type*>(data.get<ltype::overload>()))
			{
				for (auto& elem: *ptr)
				{
					if (elem.second.flag == function_meta::is_method)
						elem.second.object = nullptr;
				}
			}
			return data;
		}}
	}},
	{ "ConstExpr", {
		{ "ConstExpr, constexpr", [](gen_node& syntax_node, AST_context* context){
			syntax_node[0].code_gen(context);
			return syntax_node[1].code_gen(context);
		}},
		{ "constexpr", parser::forward }
	}},
	{ "Type", {
		{ "TypeElem TypeExpr", [](gen_node& syntax_node, AST_context* context){
			auto type = context->cur_type;
			context->cur_type = syntax_node[0].code_gen(context).get_type();
			syntax_node[1].code_gen(context);
			auto ret = context->cur_type;
			if (ret == void_type) throw err("cannot declare variable of void type");
			context->cur_type = type;
			return AST_result(ret);
		}}
	}},
	{ "TypeExpr_m", {
		{ "* TypeExpr", [](gen_node& syntax_node, AST_context* context){
			auto base_type = context->cur_type;
			context->cur_type = base_type == void_type ? 
				void_ptr_type : 
				PointerType::getUnqual(base_type);
			type_names[context->cur_type] = "ptr " + type_names[base_type];
			return syntax_node[0].code_gen(context);
		}},
		{ "TypeExpr1", parser::forward }
	}},
	{ "TypeExpr", {
		{ "* TypeExpr", [](gen_node& syntax_node, AST_context* context){
			auto base_type = context->cur_type;
			context->cur_type = base_type == void_type ? 
				void_ptr_type : 
				PointerType::getUnqual(base_type);
			type_names[context->cur_type] = "ptr " + type_names[base_type];
			return syntax_node[0].code_gen(context);
		}},
		{ "TypeExpr1", parser::forward },
		{ "", parser::forward }
	}},
	{ "TypeExpr1", {
		{ "TypeExpr1 ( FunctionParams )", [](gen_node& syntax_node, AST_context* context){
			auto base_type = context->cur_type;			//syntax_node[0].code_gen(context).get_type();
			if (base_type->isArrayTy())
				throw err("function cannot return an array");
			if (base_type->isFunctionTy())
				throw err("function cannot return a function");
			
			auto params = syntax_node[1].code_gen(context).get_data<function_params>();
			context->cur_type = FunctionType::get(base_type, *params, false);	// cannot return an array
			type_names[context->cur_type] = type_names[base_type] + "(";
			if (params->size())
			{
				type_names[context->cur_type] += type_names[(*params)[0]];
				for (auto itr = params->begin() + 1; itr != params->end(); ++itr)
					type_names[context->cur_type] += ", " + type_names[*itr];
			}
			type_names[context->cur_type] += ")";
			delete params;
			return syntax_node[0].code_gen(context);
		}},
		{ "TypeExpr1 [ ConstExpr ]", [](gen_node& syntax_node, AST_context* context){
			auto base_type = context->cur_type;
			if (base_type->isFunctionTy())
				throw err("cannot create array of function");
			if (base_type->isVoidTy())
				throw err("cannot create array of void type");

			auto size = static_cast<ConstantInt*>(create_implicit_cast(
				syntax_node[1].code_gen(context).get_as<ltype::rvalue>(), int_type));
			if (size->isNegative()) throw err("negative array size");
			//auto elem_type = context->current_type;
			context->cur_type = ArrayType::get(base_type, size->getZExtValue());
			// record typename
			type_names[context->cur_type] = type_names[base_type] + "[" + [](int x)->string
			{
				char s[20];	sprintf(s, "%d", x); return s;
			}(size->getZExtValue()) + "]";
			return syntax_node[0].code_gen(context);
		}},
		{ "TypeExpr2", parser::forward }
	}},
	{ "TypeExpr2", {
		{ "( TypeExpr_m )", parser::forward },
		{ "( FunctionParams )", [](gen_node& syntax_node, AST_context* context){
			auto base_type = context->cur_type;			//syntax_node[0].code_gen(context).get_type();
			if (base_type->isArrayTy())
				throw err("function cannot return an array");
			if (base_type->isFunctionTy())
				throw err("function cannot return a function");
			
			auto params = syntax_node[0].code_gen(context).get_data<function_params>();
			context->cur_type = FunctionType::get(base_type, *params, false);	// cannot return an array
			type_names[context->cur_type] = type_names[base_type] + "(";
			if (params->size())
			{
				type_names[context->cur_type] += type_names[(*params)[0]];
				for (auto itr = params->begin() + 1; itr != params->end(); ++itr)
					type_names[context->cur_type] += ", " + type_names[*itr];
			}
			type_names[context->cur_type] += ")";
			delete params;
			return AST_result();
		}},
		{ "[ ConstExpr ]", [](gen_node& syntax_node, AST_context* context){
			auto base_type = context->cur_type;
			if (base_type->isFunctionTy())
				throw err("cannot create array of function");
			if (base_type->isVoidTy())
				throw err("cannot create array of void type");

			auto size = static_cast<ConstantInt*>(create_implicit_cast(
				syntax_node[0].code_gen(context).get_as<ltype::rvalue>(), int_type));
			if (size->isNegative()) throw err("negative array size");
			//auto elem_type = context->current_type;
			context->cur_type = ArrayType::get(base_type, size->getZExtValue());
			// record typename
			type_names[context->cur_type] = type_names[base_type] + "[" + [](int x)->string
			{
				char s[20];	sprintf(s, "%d", x); return s;
			}(size->getZExtValue()) + "]";
			return AST_result();
		}},
		//{ "", parser::empty }
	}},
	{ "FunctionRetType", {
		{ "-> Type", parser::forward },
		{ "", [](gen_node&, AST_context*){ return AST_result(void_type); } }
	}},
	{ "LambdaType", {
		{ "( FunctionParams ) FunctionRetType", [](gen_node& syntax_node, AST_context* context){
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
	}},
	{ "TemplateArgumentPack", {
		{ "TemplateArgumentPack , TemplateArgument", [](gen_node& syntax_node, AST_context* context){
			auto vec = syntax_node[0].code_gen(context).get_data<template_params>();
			auto data = syntax_node[1].code_gen(context).get_data<template_param_item>();
			vec->push_back(std::move(*data));
			delete data;
			return AST_result(vec);
		}},
		{ "TemplateArgument", [](gen_node& syntax_node, AST_context* context){
			auto vec = new template_params;
			auto data = syntax_node[0].code_gen(context).get_data<template_param_item>();
			vec->push_back(std::move(*data));
			delete data;
			return AST_result(vec);
		}}
	}},
	{ "TemplateArgument", {
		{ "Type", [](gen_node& syntax_node, AST_context* context){
			return AST_result(new template_param_item(syntax_node[0].code_gen(context).get_type()));
		}},
		{ "constexpr", [](gen_node& syntax_node, AST_context* context){
			return AST_result(new template_param_item(static_cast<Constant*>(
				syntax_node[0].code_gen(context).get_as<ltype::rvalue>()
			)));
		}}
	}},
	{ "TypeElem", {
		{ "int", parser::forward },
		{ "float", parser::forward },
		{ "char", parser::forward },
		{ "bool", parser::forward },
		{ "void", parser::forward },
		{ "IdType", [](gen_node& syntax_node, AST_context* context){
			return context->get_type(static_cast<term_node&>(syntax_node[0]).data.attr->value);
		}},
		/*{ "fn ( FunctionParams ) FunctionRetType", [](gen_node& syntax_node, AST_context* context){
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
		}},*/
		{ "IdTemplateClass < TemplateArgumentPack >", [](gen_node& syntax_node, AST_context* context){
			auto vec = syntax_node[1].code_gen(context).get_data<template_params>();
			auto ty = reinterpret_cast<template_class_meta*>(
					syntax_node[0].code_gen(context).get_as<ltype::template_class>()
				)->generate_class(*vec, context);
			delete vec;
			return AST_result(static_cast<Type*>(ty));
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
			if (reinterpret_cast<unsigned long long&>(type) > 512)
				if (type->isFunctionTy() || type->isVoidTy())
					throw err("type " + type_names[type] + " is invalid argument type");
			if (context->collect_param_name = bak_collect)
				context->function_param_name.push_back(static_cast<term_node&>(syntax_node[1]).data.attr->value);
			return AST_result(type);
		}},
		{ "Type", [](gen_node& syntax_node, AST_context* context){
			auto bak_collect = context->collect_param_name;
			context->collect_param_name = false;

			auto type = syntax_node[0].code_gen(context).get_type();
			if (reinterpret_cast<unsigned long long&>(type) > 512)
				if (type->isFunctionTy() || type->isVoidTy())
					throw err("type " + type_names[type] + " is invalid argument type");
			if (context->collect_param_name = bak_collect)
				context->function_param_name.push_back("");
			return AST_result(type);
		}}
	}},
	{ "Function", {
		{ "Type Id ( FunctionParams ) { Block }", [](gen_node& syntax_node, AST_context* context){
			context->collect_param_name = true;
			context->function_param_name.resize(0);
			auto name = static_cast<term_node&>(syntax_node[1]).data.attr->value;
			auto base_type = syntax_node[0].code_gen(context).get_type();			//syntax_node[0].code_gen(context).get_type();
			if (base_type->isArrayTy())
				throw err("function cannot return an array");
			if (base_type->isFunctionTy())
				throw err("function cannot return a function");
			
			auto params = syntax_node[2].code_gen(context).get_data<function_params>();
			auto type = FunctionType::get(base_type, *params, false);	// cannot return an array
			type_names[type] = type_names[base_type] + "(";
			if (params->size())
			{
				type_names[type] += type_names[(*params)[0]];
				for (auto itr = params->begin() + 1; itr != params->end(); ++itr)
					type_names[type] += ", " + type_names[*itr];
			}
			type_names[type] += ")";
			delete params;

			Function* F = Function::Create(type, Function::ExternalLinkage, name, lModule);
			AST_function_context new_context(context, F, name);
			new_context.register_args();
			syntax_node[3].code_gen(&new_context);
			return AST_result();
		},
		{	//$ parser callback
			{ 3, parser::enter_block },
			{ 5, parser::leave_block }
		}}
	}},

	// Template
	{ "TemplateFunction", {
		{ "Type Id ( FunctionParams ) { Block }", [](gen_node& syntax_node, AST_context* context){
			context->collect_param_name = true;
			context->function_param_name.resize(0);
			auto name = static_cast<term_node&>(syntax_node[1]).data.attr->value;
			auto base_type = syntax_node[0].code_gen(context).get_type();
			if (base_type->isArrayTy())
				throw err("function cannot return an array");
			if (base_type->isFunctionTy())
				throw err("function cannot return a function");

			auto params = syntax_node[2].code_gen(context).get_data<function_params>();
			auto type = FunctionType::get(base_type, *params, false);	// cannot return an array
			type_names[type] = type_names[base_type] + "(";
			if (params->size())
			{
				type_names[type] += type_names[(*params)[0]];
				for (auto itr = params->begin() + 1; itr != params->end(); ++itr)
					type_names[type] += ", " + type_names[*itr];
			}
			type_names[type] += ")";
			delete params;

			Function* F = Function::Create(type, Function::ExternalLinkage, name, lModule);
			AST_function_context new_context(context, F);
			*static_cast<AST_template_context*>(context)->func_ptr = new_context.function;
			new_context.register_args();
			syntax_node[3].code_gen(&new_context);
			return AST_result(reinterpret_cast<void*>(new_context.function));
		},
		{	//$ parser callback
			{ 3, parser::register_template_func },
			{ 4, parser::enter_block },
			{ 6, parser::leave_block }
		}}
	}},
	{ "TemplateClass", {
		{ "class Id ClassBase { ClassInterface }", [](gen_node& syntax_node, AST_context* context){
			Type* base = nullptr;
			unsigned hwnd = is_public;
			if (auto base_val = syntax_node[1].code_gen(context))
			{
				auto dat = base_val.get_data<pair<Type*, unsigned>>();
				base = dat->first;
				hwnd = dat->second;
				delete dat;
			}
			AST_template_class_context* struct_context = new AST_template_class_context(context, !base ? nullptr :
				context->get_namespace(static_cast<StructType*>(base)));
			struct_context->visibility_hwnd = hwnd;

			syntax_node[2].code_gen(struct_context);	// vtable
			struct_context->verify_vmethod();

			syntax_node[2].code_gen(struct_context);
			struct_context->finish_struct(static_cast<term_node&>(syntax_node[0]).data.attr->value);

			syntax_node[2].code_gen(struct_context);
			struct_context->verify();

			return AST_result(reinterpret_cast<void*>(struct_context->type));
		},
		{	//$ parser callback
			{ 2, parser::register_template_class },
			{ 4, parser::enter_block },
			{ 6, parser::leave_block }
		}},
	}},

	{ "Template", {
		{ "TemplateItem", parser::forward, {
			{ 0, parser::enter_block },
			{ 1, parser::leave_block }
		}}
	}},
	{ "TemplateItem", {
		{ "ClassTemplate", parser::forward },
		{ "FunctionTemplate", parser::forward }
	}},
	{ "ClassTemplate", {
		{ "< TemplateParams > TemplateClass ;", [](gen_node& syntax_node, AST_context*context){
			auto ta = syntax_node[0].code_gen(context).get_data<template_args_type>();
			context->add_template_class(ta, static_cast<term_node&>(
					static_cast<gen_node&>(syntax_node[1])[0]
				).data.attr->value, syntax_node[1]);
			delete ta;
			return AST_result();
		}}
	}},
	{ "FunctionTemplate", {
		{ "< TemplateParams > TemplateFunction", [](gen_node& syntax_node, AST_context* context){
			auto ta = syntax_node[0].code_gen(context).get_data<template_args_type>();
			AST_template_context template_context(context);
			for (unsigned i = 0; i != ta->size(); ++i)
				if (!(*ta)[i].first)		// if this template argument is typename
			{
				unsigned long long idx = i;
				template_context.add_type(reinterpret_cast<Type*&>(idx), (*ta)[i].second);
			}
			//else throw err("non-typename template argument not supported");
			auto params = static_cast<gen_node&>(
				static_cast<gen_node&>(
					syntax_node[1]
				)[0]
			)[0].code_gen(&template_context).get_data<function_params>();
			context->add_template_func(ta, params, static_cast<term_node&>(
					static_cast<gen_node&>(syntax_node[1])[1]
				).data.attr->value, syntax_node[1]);
			delete ta;
			delete params;
			return AST_result();
		}}
	}},
	{ "TemplateParams", {
		{ "TemplateParams , TemplateParamItem", [](gen_node& syntax_node, AST_context* context){
			auto vec = syntax_node[0].code_gen(context).get_data<template_args_type>();
			auto data = syntax_node[1].code_gen(context).get_data<pair<Type*, std::string>>();
			vec->push_back(*data);
			delete data;
			return AST_result(vec);
		}},
		{ "TemplateParamItem", [](gen_node& syntax_node, AST_context* context){
			auto vec = new template_args_type;
			auto data = syntax_node[0].code_gen(context).get_data<pair<Type*, std::string>>();
			vec->push_back(*data);
			delete data;
			return AST_result(vec);
		}},
	}},
	{ "TemplateParamItem", {
		{ "type Id", [](gen_node& syntax_node, AST_context* context){
			return AST_result(new pair<Type*, std::string>(nullptr,
					static_cast<term_node&>(syntax_node[0]).data.attr->value));
		},
		{	//$ parser callback
			{ 2, parser::register_type }
		}},
		{ "Type Id", [](gen_node& syntax_node, AST_context* context){
			return AST_result(new pair<Type*, std::string>(
					syntax_node[0].code_gen(context).get_type(),
					static_cast<term_node&>(syntax_node[1]).data.attr->value));
		}},
	}},
	{ "TemplateFunctionCall", {					// TODO remove this
		{ "IdTemplateFunc < TemplateArgumentPack > ( exprpk )", [](gen_node& syntax_node, AST_context* context){
			auto vec = syntax_node[1].code_gen(context).get_data<template_params>();
			auto params = syntax_node[2].code_gen(context).get_data<std::vector<Value*>>();
			auto fdata = syntax_node[0].code_gen(context).get_as<ltype::template_func>();
			llvm::Function* function = reinterpret_cast<template_func_meta*>(fdata)->get_function(*params, context, vec);
			auto call_inst = function->getReturnType() != void_type ? lBuilder.CreateCall(function, *params, "Call")
				: lBuilder.CreateCall(function, *params);
			delete params;
			delete vec;
			if (function->getReturnType() != void_type) return AST_result(call_inst, false);
				else return AST_result();
		}},
		{ "IdTemplateFunc ( exprpk )", [](gen_node& syntax_node, AST_context* context){
			auto params = syntax_node[1].code_gen(context).get_data<std::vector<Value*>>();
			auto fdata = syntax_node[0].code_gen(context).get_as<ltype::template_func>();
			llvm::Function* function = reinterpret_cast<template_func_meta*>(fdata)->get_function(*params, context);
			auto call_inst = function->getReturnType() != void_type ? lBuilder.CreateCall(function, *params, "Call")
				: lBuilder.CreateCall(function, *params);
			delete params;
			if (function->getReturnType() != void_type) return AST_result(call_inst, false);
				else return AST_result();
		}}
	}},

	// Defination
	{ "TypeDefine", {
		{ "TypeDefine, Id = Type", [](gen_node& syntax_node, AST_context* context){
			syntax_node[0].code_gen(context);
			auto type = syntax_node[2].code_gen(context).get_type();
			context->add_type(type, static_cast<term_node&>(syntax_node[1]).data.attr->value);
			return AST_result();
		},
		{	//$ parser callback
			{ 3, parser::register_type }
		}},
		{ "type Id = Type", [](gen_node& syntax_node, AST_context* context){
			auto type = syntax_node[1].code_gen(context).get_type();
			context->add_type(type, static_cast<term_node&>(syntax_node[0]).data.attr->value);
			return AST_result();
		},
		{	//$ parser callback
			{ 2, parser::register_type }
		}}
	}},
	{ "GlobalVarDefine", {
		{ "GlobalVarDefine, Id GlobalInitExpr", [](gen_node& syntax_node, AST_context* context){
			Type* type = syntax_node[0].code_gen(context).get_type();
			llvm::Value* init = nullptr;
			if (auto init_expr = syntax_node[2].code_gen(context))
			{
				auto data = init_expr.get_among<ltype::init_list, ltype::rvalue>();		// no init
				switch (data.second)
				{
				case 0: init = create_initializer_list(type, reinterpret_cast<init_vec*>(data.first)); break;
				case 1: init = data.first; break;
				}
			}
			if (!type && !init) throw err("let clause must be initialized");
			context->alloc_var(type ? type : init->getType(), static_cast<term_node&>(syntax_node[1]).data.attr->value, init);
			return AST_result(type);
		}},
		{ "Type Id GlobalInitExpr", [](gen_node& syntax_node, AST_context* context){
			Type* type = syntax_node[0].code_gen(context).get_type();
			llvm::Value* init = nullptr;
			if (auto init_expr = syntax_node[2].code_gen(context))
			{
				auto data = init_expr.get_among<ltype::init_list, ltype::rvalue>();		// no init
				switch (data.second)
				{
				case 0: init = create_initializer_list(type, reinterpret_cast<init_vec*>(data.first)); break;
				case 1: init = data.first; break;
				}
			}
			context->alloc_var(type, static_cast<term_node&>(syntax_node[1]).data.attr->value, init);
			return AST_result(type);
		}},
		{ "let Id GlobalInitExpr", [](gen_node& syntax_node, AST_context* context){
			llvm::Value* init = nullptr;
			auto data = syntax_node[1].code_gen(context).get_among<ltype::init_list, ltype::rvalue>();
			switch (data.second)
			{
			case 0: init = create_initializer_list(nullptr, reinterpret_cast<init_vec*>(data.first)); break;
			case 1: init = data.first; break;
			}
			context->alloc_var(init->getType(), static_cast<term_node&>(syntax_node[0]).data.attr->value, init);
			return AST_result((Type*)nullptr);
		}}
	}},
	{ "GlobalInitExpr", {
		{ "= ConstExpr", parser::forward },
		{ "", parser::empty }
	}},
	{ "LocalVarDefine", {
		{ "LocalVarDefine, Id InitExpr", [](gen_node& syntax_node, AST_context* context){
			Type* type = syntax_node[0].code_gen(context).get_type();
			llvm::Value* init = nullptr;
			if (auto init_expr = syntax_node[2].code_gen(context))
			{
				auto data = init_expr.get_among<ltype::init_list, ltype::rvalue>();		// no init
				switch (data.second)
				{
				case 0: init = create_initializer_list(type, reinterpret_cast<init_vec*>(data.first)); break;
				case 1: init = data.first; break;
				}
			}
			if (!type && !init) throw err("let clause must be initialized");
			context->alloc_var(type ? type : init->getType(), static_cast<term_node&>(syntax_node[1]).data.attr->value, init);
			return AST_result(type);
		}},
		{ "Type Id InitExpr", [](gen_node& syntax_node, AST_context* context){
			Type* type = syntax_node[0].code_gen(context).get_type();
			llvm::Value* init = nullptr;
			if (auto init_expr = syntax_node[2].code_gen(context))
			{
				auto data = init_expr.get_among<ltype::init_list, ltype::rvalue>();		// no init
				switch (data.second)
				{
				case 0: init = create_initializer_list(type, reinterpret_cast<init_vec*>(data.first)); break;
				case 1: init = data.first; break;
				}
			}
			context->alloc_var(type, static_cast<term_node&>(syntax_node[1]).data.attr->value, init);
			return AST_result(type);
		}},
		{ "let Id InitExpr", [](gen_node& syntax_node, AST_context* context){
			llvm::Value* init = nullptr;
			auto data = syntax_node[1].code_gen(context).get_among<ltype::init_list, ltype::rvalue>();
			switch (data.second)
			{
			case 0: init = create_initializer_list(nullptr, reinterpret_cast<init_vec*>(data.first)); break;
			case 1: init = data.first; break;
			}
			context->alloc_var(init->getType(), static_cast<term_node&>(syntax_node[0]).data.attr->value, init);
			return AST_result((Type*)nullptr);
		}}
	}},
	{ "LocalRefDefine", {
		{ "LocalRefDefine, Id InitExpr", [](gen_node& syntax_node, AST_context* context){
			syntax_node[0].code_gen(context);
			auto ptr = syntax_node[2].code_gen(context).get_as<ltype::lvalue>();
			context->add_ref(ptr, static_cast<term_node&>(syntax_node[1]).data.attr->value);
			return AST_result();
		}},
		{ "ref Id InitExpr", [](gen_node& syntax_node, AST_context* context){
			auto ptr = syntax_node[1].code_gen(context).get_as<ltype::lvalue>();
			context->add_ref(ptr, static_cast<term_node&>(syntax_node[0]).data.attr->value);
			return AST_result();
		}}
	}},
	{ "InitExpr", {
		{ "= Expr", parser::forward },
		{ "", parser::empty }
	}},

	// Class
	{ "Class", {
		{ "class Id ClassBase { ClassInterface }", [](gen_node& syntax_node, AST_context* context){
			auto& class_name =  static_cast<term_node&>(syntax_node[0]).data.attr->value;
			Type* base = nullptr;
			unsigned hwnd = is_public;
			if (auto base_val = syntax_node[1].code_gen(context))
			{
				auto dat = base_val.get_data<pair<Type*, unsigned>>();
				base = dat->first;
				hwnd = dat->second;
				delete dat;
			}
			AST_struct_context* struct_context = new AST_struct_context(context, !base ? nullptr :
				context->get_namespace(static_cast<StructType*>(base)));
			struct_context->visibility_hwnd = hwnd;

			syntax_node[2].code_gen(struct_context);	// vtable
			struct_context->verify_vmethod();

			syntax_node[2].code_gen(struct_context);
			struct_context->finish_struct(class_name);

			syntax_node[2].code_gen(struct_context);
			struct_context->verify();

			return AST_result();
		},
		{	//$ parser callback
			{ 4, parser::enter_block },
			{ 6, parser::leave_block }
		}},
	}},
	{ "ClassBase", {
		{ ": VisitAttr Id", [](gen_node& syntax_node, AST_context* context){
			auto pir = new pair<Type*, unsigned>;
			auto base = syntax_node[1].code_gen(context).get_type();
			if (!base->isStructTy()) throw err("class base declared non-class type");
			pir->first = base;
			pir->second = syntax_node[0].code_gen(context).get_attr();
			return AST_result(pir);
		}},
		{ "", parser::empty }
	}},
	{ "ClassInterface", {
		{ "ClassInterfaceItem ClassInterface", parser::expand },
		{ "", parser::empty }
	}},
	{ "ClassInterfaceItem", {
		{ "VisitAttr Type Id ;", [](gen_node& syntax_node, AST_context* context){
			auto struct_context = static_cast<AST_struct_context*>(context);
			if (struct_context->chk_vptr && !struct_context->type)
			{
				auto type = syntax_node[1].code_gen(context).get_type();
				auto name = static_cast<term_node&>(syntax_node[2]).data.attr->value;
				struct_context->alloc_var(type, name, nullptr);
				struct_context->set_name_visibility(name, syntax_node[0].code_gen(context).get_attr());
			}
			return AST_result();
		}},
		{ "Method", parser::forward }
	}},
	{ "Method", {
		{ "VisitAttr Type Id ( FunctionParams ) MethodAttr { Block }", [](gen_node& syntax_node, AST_context* context){
			auto struct_context = static_cast<AST_struct_context*>(context);
			if (!struct_context->chk_vptr)
			{
				auto fnattr = syntax_node[3].code_gen(context).get_data<function_attr>();
				if (fnattr->count(is_virtual)) struct_context->is_vclass = true;
				delete fnattr;
			}
			else if (struct_context->type)
			{
				auto fnattr = syntax_node[4].code_gen(context).get_data<function_attr>();
				context->collect_param_name = true;
				context->function_param_name.resize(0);
				auto name = static_cast<term_node&>(syntax_node[2]).data.attr->value;
				auto base_type = syntax_node[1].code_gen(context).get_type();
				if (base_type->isArrayTy())
					throw err("function cannot return an array");
				if (base_type->isFunctionTy())
					throw err("function cannot return a function");
				
				auto params = syntax_node[2].code_gen(context).get_data<function_params>();
				auto type = FunctionType::get(base_type, *params, false);	// cannot return an array
				type_names[type] = type_names[base_type] + "(";
				if (params->size())
				{
					type_names[type] += type_names[(*params)[0]];
					for (auto itr = params->begin() + 1; itr != params->end(); ++itr)
						type_names[type] += ", " + type_names[*itr];
				}
				type_names[type] += ")";
				delete params;

				AST_method_context new_context(struct_context, type, name, fnattr);
				struct_context->set_name_visibility(name, syntax_node[0].code_gen(context).get_attr());
				new_context.register_args();
				syntax_node[4].code_gen(&new_context);
				delete fnattr;
			}
			return AST_result();
		},
		{	//$ parser callback
			{ 5, parser::enter_block },
			{ 7, parser::leave_block }
		}}
	}},
	{ "MethodAttr", {
		{ "MethodAttr MethodAttrElem", [](gen_node& syntax_node, AST_context* context){
			auto fnattr = syntax_node[0].code_gen(context).get_data<function_attr>();
			auto attr = syntax_node[1].code_gen(context).get_attr();
			if (fnattr->count(attr)) throw err("function attribute redeclared");
			fnattr->insert(attr);
			if (attr == is_override) fnattr->insert(is_virtual);
			return AST_result(fnattr);
		}},
		{ "", [](gen_node& syntax_node, AST_context* context){
			auto fnattr = new function_attr;
			fnattr->insert(is_method);
			return AST_result(fnattr);
		}}
	}},
	{ "MethodAttrElem", {
		{ "virtual", parser::attribute<is_virtual> },
		{ "override", parser::attribute<is_override> },
	}},
	{ "VisitAttr", {
		{ "private", parser::attribute<is_private> },
		{ "protected", parser::attribute<is_protected> },
		{ "public", parser::attribute<is_public> },
	}},


	{ "Lambda", {
		{ "LambdaType { Block }", [](gen_node& syntax_node, AST_context* T){
			auto context = T->get_global_context();
			context->collect_param_name = true;
			context->function_param_name.resize(0);
			auto type = syntax_node[0].code_gen(context).get_type();
			Function* F = Function::Create(static_cast<FunctionType*>(type), Function::ExternalLinkage, ".lambda", lModule);
			AST_function_context new_context(context, F);
			new_context.register_args();
			syntax_node[1].code_gen(&new_context);
			return AST_result(F, false);
		},
		{	//$ parser callback
			{ 3, parser::enter_block },
			{ 5, parser::leave_block }
		}},
	}},
	// utils
	{ "exprelem", {
		{ "( Expr )", parser::forward },
		{ "TemplateFunctionCall", parser::forward },
		{ "[ InitList ]", parser::forward },
		{ "Lambda", parser::forward },
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
		{ "[ InitList ]", parser::forward },
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

parser::reinterpret_list rep_list = 
{
	{ "Id", {
		{ parser::is_type_symbol, "IdType" },
		{ parser::is_template_func_symbol, "IdTemplateFunc" },
		{ parser::is_template_class_symbol, "IdTemplateClass" }
	}}
};

#endif
