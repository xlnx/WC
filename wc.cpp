#include <llvm/Support/raw_ostream.h>
#include <windows.h>
#include <io.h>
#include "wc.h"
#include <fstream>
const int exe_format = 0;
const int llvm_ir_format = 1;
const int asm_format = 2;
const int object_format = 3;

int execute_command(char* cmdline)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;  
	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pi, sizeof(pi));
	si.cb = sizeof(si);
	
	if (!CreateProcess(
		nullptr,
		TEXT(cmdline),
		nullptr,
		nullptr,
		FALSE,
		0,
		nullptr,
		nullptr,//TEXT(getPath(dir)),
		&si,
		&pi
	)) throw err("error in CreateProcess: " + string(cmdline));
	WaitForSingleObject(pi.hProcess, INFINITE);
	DWORD ret;
	GetExitCodeProcess(pi.hProcess, &ret);
	
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread); 
	return ret;
}

string temp(const string& s)
{
	return "$tmp_output_" + s + "~";
}

string change_suffix(string s, const string& suffix)
{	
	auto str_begin = s.c_str();
	auto p = str_begin + s.length();
	while (p != str_begin && *p != '.') --p;
	if (*p == '.')
	{
		s.resize(p - str_begin);
	}
	return s + suffix;
}

int main(int argc, char *argv[])
{
	struct params_extractor
	{
		params_extractor(int args, char** arg):
			argc(args), argv(arg) {}
		bool empty() const { return idx == argc; }
		void next() { ++idx; }
		char* current() const { if (empty()) throw err("lack of param"); return argv[idx]; }
	private:
		int idx = 1;
		const int argc;
		char** const argv;
	} params(argc, argv);
	
	string input_file_name;
	string output_file_name;
	string opt_str;
	int dest_format = 0;
	using option_callback_type = map<string, std::function<void()>>;
	using callback = option_callback_type::value_type;
	option_callback_type option_callback =
	{
		callback("-o", [&](){ params.next(); output_file_name = params.current(); }),
		callback("-s", [&](){ dest_format = asm_format; }),
		callback("-llvm", [&](){ dest_format = llvm_ir_format; }),
		callback("-obj", [&](){ dest_format = object_format; }),
		callback("-O1", [&](){ opt_str = " -O1 "; }),
		callback("-O2", [&](){ opt_str = " -O2 "; }),
		callback("-O3", [&](){ opt_str = " -O3 "; }),
	};
	
	try
	{
		parser mparser(mlex_rules, mparse_rules, mexpr_rules);
		while (!params.empty())
		{
			auto& fcallback = option_callback[params.current()];
			if (fcallback) fcallback();
			else
			{
				if (!access(params.current(), 0))	// if we can read from this file
				{
					input_file_name = params.current();
				}
				else throw err(string("unknown compiler option: ") + params.current());
			}
			params.next();
		}
		if (input_file_name == "") throw err("no input file");
		if (output_file_name == "")
		{
			string suffix;
			switch (dest_format)
			{
			case exe_format: suffix = ".exe"; break;
			case llvm_ir_format: suffix = ".ll"; break;
			case asm_format: suffix = ".s"; break;
			case object_format: suffix = ".o"; break;
			}
			output_file_name = change_suffix(input_file_name, suffix);
		}
		
		ifstream is(input_file_name);
		string src, dest;
		getline(is, src, static_cast<char>(EOF));
		is.close();
		try
		{
			mparser.parse(src.c_str());
			raw_string_ostream los(dest);
			lModule->print(los, nullptr);
			
			string tmp_file_name = dest_format == llvm_ir_format ? output_file_name : temp(output_file_name);
			ofstream os(tmp_file_name);
			os << dest;
			os.close();
			int ret;
			switch (dest_format)
			{
			case llvm_ir_format: return 0;
			case asm_format:
				ret = execute_command(const_cast<char*>((
						"llc -o " + output_file_name + opt_str + " " + tmp_file_name
					).c_str()));
				remove(tmp_file_name.c_str());
				return ret;
			case object_format:
				ret = execute_command(const_cast<char*>((
						"llc -filetype=obj -o " + output_file_name + opt_str + " " + tmp_file_name
					).c_str()));
				remove(tmp_file_name.c_str());
				return ret;
			case exe_format:
				ret = execute_command(const_cast<char*>((
						"llc -filetype=obj -o " + temp(change_suffix(output_file_name, ".o")) + opt_str + " " + tmp_file_name
					).c_str()));
				remove(tmp_file_name.c_str());
				if (ret) return ret;
				ret = execute_command(const_cast<char*>((
						"ld " + temp(change_suffix(output_file_name, ".o")) + " -o" + output_file_name
					).c_str()));
				remove(temp(change_suffix(output_file_name, ".o")).c_str());
				return ret;
			}
		}
		catch (const err& e)
		{		// poly
			e.alert();
		}
	}
	catch (const err& e)		// poly
	{
		e.alert();
	}
}
