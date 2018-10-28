#include "stdafx.h"
#include "glslang_utils.h"
#include "bash_terminal.h"
#include "bash_utils.h"
using namespace tools;

static const psl::string type_str[]
{
	("vert"),
	("tesc"),
	("tese"),
	("geom"),
	("frag"),
	("comp")
};
bool glslang::compile(psl::string_view compiler_location, psl::string_view source, psl::string_view outputfile, type type, bool optimize)
{
	static bash_terminal bt;
	if(!utility::platform::directory::exists(compiler_location))
		return false;

	psl::string ofile = utility::platform::file::to_platform(outputfile);
	utility::bash::directory::cd(bt, compiler_location);
	bt.exec(("glslangValidator.exe --stdin ") + type_str[(uint8_t)type] + ("  -V  -o \"") + ofile + ("\" <<EOT_IMPOSSIBLY_LONG_TO_ACCIDENTALLY_GUESS_IF_YOU_WROTE_THIS_YOU_DID_IT_ON_PURPOSE"));
	bt.exec(source);
	bt.exec(("EOT_IMPOSSIBLY_LONG_TO_ACCIDENTALLY_GUESS_IF_YOU_WROTE_THIS_YOU_DID_IT_ON_PURPOSE"));
	auto res = bt.read(false);
	if(psl::string_view(res.data(), 5) == ("stdin") && res.size() <= 7)
	{
		if(optimize)
		{
			bt.exec(("spirv-opt.exe -O \"") + ofile + ("\" -o \"") + ofile+ ("\""));
			res = bt.read(false);
			if(res.size() < 2)
				return true;

			psl::cerr << psl::to_platform_string(res);
			return false;
		}
		return true;
	}
	psl::cerr << psl::to_platform_string(res);
	return false;
}
