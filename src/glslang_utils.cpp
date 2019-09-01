#include "stdafx.h"
#include "glslang_utils.h"
#include <stdlib.h>
#include "psl/platform_utils.h"
#include <iostream>

using namespace tools;

bool glslang::compile(psl::string_view compiler_location, psl::string_view source, psl::string_view outputfile,
					  type type, bool optimize, std::optional<size_t> gles_version)
{
	if(!utility::platform::directory::exists(compiler_location)) return false;
	psl::string compiler{compiler_location};
	auto input = compiler + "tempfile";
	if(!utility::platform::file::write(input, source))
	{
		std::cerr << "ERROR: could not write the temporary shader file.\n";
		return false;
	}
	while(!utility::platform::file::read(input))
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(150));
	}

	auto full_path = compiler + "glslangValidator.exe";

	if(!utility::platform::file::exists(full_path))
	{
		std::cerr << "ERROR: missing 'glslangValidator'\n";
		utility::platform::file::erase(input);
		return false;
	}
	psl::string ofile   = utility::platform::file::to_platform(outputfile);
	auto ofile_spirv	= ofile + "-" + type_str[(uint8_t)type] + ".spv";
	psl::string command = "cd \"" + compiler + "\" &&" + " glslangValidator.exe -S " + type_str[(uint8_t)type] +
						  " -V \"" + input + "\" -o \"" + ofile_spirv + "\"";
	if(std::system(command.c_str()) != 0)
	{
		utility::platform::file::erase(input);
		return false;
	}
	std::cout << psl::to_string8_t("outputted the spv binary file at '" + ofile_spirv + "'\n");
	utility::platform::file::erase(input);

	while(!utility::platform::file::read(ofile_spirv))
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(150));
	}

	if(optimize)
	{
		command = ("spirv-opt.exe -O \"") + ofile_spirv + ("\" -o \"") + ofile_spirv + ("\"");
		if(std::system(command.c_str()) != 0) return false;
		std::cout << psl::to_string8_t("optimized the spv binary file at '" + ofile_spirv + "'\n");
	}

	if(gles_version)
	{
		auto ofile_gles = ofile + "-" + type_str[(uint8_t)type] + ".gles";
		command = "cd \"" + compiler + "\" &&" + " spirv-cross.exe --version " + std::to_string(gles_version.value()) +
				  " --es \"" + ofile_spirv + "\" --output \"" + ofile_gles + "\"";

		if(std::system(command.c_str()) != 0)
		{
			return false;
		}
		while(!utility::platform::file::read(ofile_gles))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(150));
		}

		std::cout << psl::to_string8_t("outputted the gles shader file at '" + ofile_gles + "'\n");
	}
	return true;
}
