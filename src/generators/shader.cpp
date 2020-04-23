#include "generators/shader.h"
#include "gfx/types.h"
#include "glslang_utils.h"
#include "psl/application_utils.h"
#include "psl/platform_utils.h"
#include "psl/serialization.h"
#include "psl/meta.h"
#include "psl/library.h"
#include "stdafx.h"
#include "utf8.h"
#include "utils.h"
#include <filesystem>
#include <iostream>
// todo we should figure out the bindings dynamically instead of having them written into the files, and then
// potentially safeguard the user from accidentally merging shaders together that do not work together

using namespace assembler::generators;
using namespace psl;


bool shader::parse(file_data& data)
{
	auto inc_n = data.content.find(("#include"));
	while(inc_n != psl::string_view::npos)
	{
		auto endline_n	= data.content.find(("\n"), inc_n);
		auto file_begin_n = data.content.find(("\""), inc_n);
		if(file_begin_n > endline_n) file_begin_n = data.content.find(("\'"), inc_n);
		if(file_begin_n > endline_n)
		{
			psl::string_view current_view{data.content.data(), endline_n};
			auto line_number = utility::string::count(current_view, ("\n"));
			utility::terminal::set_color(utility::terminal::color::RED);
			assembler::log->error("ERROR: could not deduce the input file's path.");
			assembler::log->error("\tthe include's opening directive was not found, are you missing a \" or ' ?");
			assembler::log->error("\tat line '" + utility::to_string(line_number) + "' of '" + data.filename);
			utility::terminal::set_color(utility::terminal::color::WHITE);
			return false;
		}

		file_begin_n += 1;
		auto file_end_n = data.content.find(("\""), file_begin_n);
		if(file_end_n > endline_n) file_end_n = data.content.find(("\'"), file_begin_n);

		if(file_end_n > endline_n)
		{
			psl::string_view current_view{data.content.data(), endline_n};
			auto line_number = utility::string::count(current_view, ("\n"));
			utility::terminal::set_color(utility::terminal::color::RED);
			assembler::log->error("ERROR: could not deduce the input file's path.");
			assembler::log->error(
				"\tthe include directive was opened, but not closed, please add a \" or ' at the end of the "
				"line.");
			assembler::log->error("\tat line '" + utility::to_string(line_number) + "' of '" + data.filename);
			utility::terminal::set_color(utility::terminal::color::WHITE);
			return false;
		}
		psl::string_view include{&data.content[file_begin_n], file_end_n - file_begin_n};
		psl::string_view include_def{&data.content[file_begin_n], file_end_n - file_begin_n};
		psl::string include_path = data.filename.substr(0, data.filename.find_last_of(("/")));

		while(include.substr(0, 3) == ("../"))
		{
			include_path = include_path.substr(0, include_path.find_last_of(("/")));
			include		 = include.substr(3);
		}
		include_path = include_path + ('/') + include;
		include_path = utility::platform::file::to_platform(include_path);
		if(!std::filesystem::exists(include_path))
		{
			psl::string_view current_view{data.content.data(), endline_n};
			auto line_number = utility::string::count(current_view, ("\n"));
			utility::terminal::set_color(utility::terminal::color::RED);
			assembler::log->error("ERROR: include file not found.\n");
			assembler::log->error("\tit was defined as '" + include_def + "' and transformed into '" + include_path +
								  "'");
			assembler::log->error("\tat line '" + utility::to_string(line_number) + "' of '" + data.filename);
			utility::terminal::set_color(utility::terminal::color::WHITE);
			return false;
		}

		if(!cache_file(include_path)) return false;

		data.includes.emplace_back(std::make_pair(include_path, inc_n));
		data.content.erase(std::begin(data.content) + inc_n, std::begin(data.content) + endline_n);

		inc_n = data.content.find(("#include"), inc_n);
	}

	return true;
}

bool shader::cache_file(const psl::string& file)
{
	auto it = m_Cache.find(file);
	if(it != m_Cache.end() &&
	   std::filesystem::last_write_time(file).time_since_epoch().count() == it->second.last_modified)
	{
		if(m_Verbose)
		{
			assembler::log->info("{0} was found in the cache", file);
			assembler::log->info("\tchecking its dependencies..\n");
		}
		for(const auto& include : it->second.includes)
		{
			cache_file(include.first);
		}
		return true;
	}
	else
	{
		if(auto res = utility::platform::file::read(file); !res)
		{
			utility::terminal::set_color(utility::terminal::color::RED);
			assembler::log->error("ERROR: missing file, no file was found at the location: " + file);
			utility::terminal::set_color(utility::terminal::color::WHITE);
			return false;
		}
		else if(!utf8::is_valid(res.value().begin(), res.value().end()))
		{
			utility::terminal::set_color(utility::terminal::color::RED);
			assembler::log->error("ERROR: the encoding was not valid UTF-8 for the file: " + file);
			utility::terminal::set_color(utility::terminal::color::WHITE);
			return false;
		}
		else
		{
			if(m_Verbose) assembler::log->info(file + " is being loaded into the cache\n");
			file_data fdata;
			fdata.content		= std::move(res.value());
			fdata.last_modified = std::filesystem::last_write_time(file).time_since_epoch().count();
			fdata.filename		= file;
			auto& data = m_Cache[file] = fdata;
			parse(data);
		}
	}
	return true;
}

psl::string shader::construct(const file_data& fdata, std::set<psl::string_view>& includes) const
{
	psl::string res = fdata.content;

	size_t offset = 0u;
	for(const auto& inc : fdata.includes)
	{
		if(includes.find(inc.first) == std::end(includes))
		{
			includes.insert(inc.first);
			auto it = m_Cache.find(inc.first);
			if(it == std::end(m_Cache))
			{
				assembler::log->error("the include" + inc.first + " is not present in the cache.");
				continue;
			}
			auto sub_res{construct(it->second, includes)};
			res.insert(inc.second + offset, sub_res);
			offset += sub_res.size();
		}
	}
	return res;
}

bool shader::generate(assembler::pathstring ifile, assembler::pathstring ofile, bool compiled_glsl, bool optimize,
					   psl::array<psl::string> types)
{
	auto directory = utility::platform::file::to_platform(ofile->substr(0, ofile->find_last_of("/")));
	if(!utility::platform::directory::exists(directory)) utility::platform::directory::create(directory, true);

	auto index				  = ifile->find_last_of(("."));
	auto extension			  = ifile->substr(index + 1);
	tools::glslang::type type = tools::glslang::type::unknown;
	if(extension == ("vert"))
		type = tools::glslang::type::vert;
	else if(extension == ("frag"))
		type = tools::glslang::type::frag;
	else if(extension == ("tesc"))
		type = tools::glslang::type::tesc;
	else if(extension == ("geom"))
		type = tools::glslang::type::geom;
	else if(extension == ("comp"))
		type = tools::glslang::type::comp;
	else if(extension == ("tese"))
		type = tools::glslang::type::tese;

	if(type == tools::glslang::type::unknown) return false;

	auto ofile_meta = ofile.platform() + "-" + tools::glslang::type_str[(uint8_t)type] + "." + ::meta::META_EXTENSION;

	psl::timer timer;

	try
	{
		if(!cache_file(ifile.platform()))
		{
			assembler::log->error(
				"something went wrong when loading the file in the cache, please consult the output to see why");
			return false;
		}
	}
	catch(std::exception e)
	{
		assembler::log->error("exception happened during caching! \n" + psl::string8_t(e.what()));
	}

	const auto& data = m_Cache[ifile.platform()];

	// emplace include data
	psl::string content = data.content;
	std::set<psl::string_view> includes;
	size_t offset = 0u;
	for(const auto& inc : data.includes)
	{
		if(includes.find(inc.first) == std::end(includes))
		{
			includes.insert(inc.first);
			auto it = m_Cache.find(inc.first);
			if(it == std::end(m_Cache))
			{
				assembler::log->error("the include" + inc.first + " is not present in the cache.");
				continue;
			}
			auto sub_res{construct(it->second, includes)};
			content.insert(std::begin(content) + inc.second + offset, std::begin(sub_res), std::end(sub_res));
			offset += sub_res.size();
		}
	}

	std::optional<size_t> gles_version;
	if(std::find(std::begin(types), std::end(types), "gles") != std::end(types))
		gles_version = (type == tools::glslang::type::comp) ? 310 : 300;

	if(!tools::glslang::compile(utility::application::path::get_path(), content, ofile.platform(), type, optimize,
								gles_version))
		return false;

	return true;
}

bool valid(const assembler::pathstring& path)
{
	const auto extension = path->substr(path->find_last_of('.'));
	return (extension == (".vert") || extension == (".frag") || extension == (".tesc") || extension == (".geom") ||
			extension == (".comp") || extension == (".tese"));
}

void shader::on_generate(psl::cli::pack& pack)
{
	//-g -s -i "C:\Projects\github\example_data\source\shaders/*" -o "C:\Projects\github\example_data\data\shaders/" -O
	// "C:\Projects\data_old\Shaders\test_output\default-glsl.vert" --glsl -v

	auto ifile		   = assembler::pathstring{pack["input"]->as<psl::string>().get()};
	auto ofile		   = assembler::pathstring{pack["output"]->as<psl::string>().get()};
	auto overwrite	 = pack["overwrite"]->as<bool>().get();
	auto compiled_glsl = pack["compiled glsl"]->as<bool>().get();
	auto optimize	  = pack["optimize"]->as<bool>().get();
	m_Verbose		   = pack["verbose"]->as<bool>().get();
	auto types		   = pack["types"]->as<std::vector<psl::string>>().get();

	auto files = assembler::get_files(ifile, ofile);

	size_t success = 0;
	for(const auto& file : files)
	{
		if(!valid(file.first))
		{
			assembler::log->info("skipping {0}", file.first.platform());
			continue;
		}
		psl::string_view output = file.second;
		if(ofile->empty() || ifile->at(ifile->size() - 1) == '*')
		{
			if(size_t last_dot = output.find_last_of('.'); last_dot != std::string::npos)
			{
				output = psl::string_view{file.second.data().data(), last_dot};
			}
		}

		if(generate(file.first, output, compiled_glsl, optimize, types))
		{
			assembler::log->info("generated shaders for {0}", file.first.platform());
			++success;
		}
		else
			assembler::log->error("failed to generate shaders for " + file.first.platform());
	}

	assembler::log->info("generated {0} shaders\n", success);
}
