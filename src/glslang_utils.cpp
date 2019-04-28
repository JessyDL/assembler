#include "stdafx.h"
#include "glslang_utils.h"
#include <stdlib.h>
using namespace tools;

static const psl::string type_str[]{("vert"), ("tesc"), ("tese"), ("geom"), ("frag"), ("comp")};
bool glslang::compile(psl::string_view compiler_location, psl::string_view source, psl::string_view outputfile,
					  type type, bool optimize)
{
	if(!utility::platform::directory::exists(compiler_location)) return false;
	psl::string compiler{compiler_location};
	auto input = compiler + "tempfile";
	if(!utility::platform::file::write(input, source))
	{
		psl::cerr << "ERROR: could not write the temporary shader file.\n";
		return false;
	}
	while(!utility::platform::file::read(input))
	{
		Sleep(150);
	}

	compiler.append("glslangValidator.exe");
	if(!utility::platform::file::exists(compiler))
	{
		psl::cerr << "ERROR: missing 'glslangValidator'\n";
		utility::platform::file::erase(input);
		return false;
	}
	psl::string ofile   = utility::platform::file::to_platform(outputfile);
	psl::string command = compiler + " -S " + type_str[(uint8_t)type] + " -V " + input + " -o \"" + ofile;
	if(std::system(command.c_str()) != 0)
	{
		utility::platform::file::erase(input);
		return false;
	}
	utility::platform::file::erase(input);

	while(!utility::platform::file::read(ofile))
	{
		Sleep(150);
	}

	if(optimize)
	{
		command = ("spirv-opt.exe -O \"") + ofile + ("\" -o \"") + ofile + ("\"");
		return std::system(command.c_str()) == 0;
	}
	return true;
}
