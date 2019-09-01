#include "stdafx.h"
#include "generators/shader.h"
#include <filesystem>
#include "psl/platform_utils.h"
#include "psl/application_utils.h"
#include "glslang_utils.h"
#include "gfx/types.h"
#include <iostream>
#include "utf8.h"
// todo we should figure out the bindings dynamically instead of having them written into the files, and then
// potentially safeguard the user from accidentally merging shaders together that do not work together

using namespace assembler::generators;
using namespace psl;

using MShader	  = core::meta::shader;
using MSVertex	 = MShader::vertex;
using MSVAttribute = MSVertex::attribute;
using MSVBinding   = MSVertex::binding;
using MSInstance   = MShader::instance;
using MSIElement   = MSInstance::element;
using MSDescriptor = MShader::descriptor;

bool write_error(const shader::file_data& data, size_t current_offset, psl::string_view message)
{
	psl::string_view current_view{data.content.data(), current_offset};
	auto line_number = utility::string::count(current_view, "\n");
	utility::terminal::set_color(utility::terminal::color::RED);
	std::cerr << psl::to_string8_t("ERROR: " + message + "\n");
	std::cerr << psl::to_string8_t("\tat line '" + psl::from_string8_t(utility::to_string(line_number)) + "' of '" +
								 data.filename + "'\n");
	utility::terminal::set_color(utility::terminal::color::WHITE);
	return false;
}

bool shader::all_scopes(file_data& data)
{
	psl::string& source						  = data.content;
	std::vector<shader::parsed_scope>& result = data.scopes;
	auto scope_start						  = source.find(("{"));
	while(scope_start != psl::string_view::npos)
	{
		auto scope_name_n2 = utility::string::rfind_first_not_of(source, (" \t\n\r"), scope_start);
		if(scope_name_n2 == psl::string_view::npos)
		{
			return write_error(data, scope_start, ("could not find the end of the scope's name"));
		}
		scope_name_n2 -= 1;
		if(source[scope_name_n2] == (')'))
		{
			// we've entered a function scope, let's exit asap
			auto function_end = source.find((";"), scope_start);
			if(function_end == psl::string_view::npos)
			{
				return write_error(data, scope_start, ("could not find the end of the function scope's name"));
			}
			function_end += 1;
			int64_t difference = (int64_t)utility::string::count(
									 psl::string_view{&source[scope_start], function_end - scope_start}, ("{")) -
								 ((int64_t)utility::string::count(
									 psl::string_view{&source[scope_start], function_end - scope_start}, ("}")));
			bool sub_scope = difference > 0;
			while(function_end != psl::string_view::npos && difference != 0)
			{
				auto next_end_n = source.find((";"), function_end);
				if(next_end_n == psl::string::npos) break;
				next_end_n += 1;
				difference += (int64_t)utility::string::count(
								  psl::string_view{&source[function_end], next_end_n - function_end}, ("{")) -
							  ((int64_t)utility::string::count(
								  psl::string_view{&source[function_end], next_end_n - function_end}, ("}")));
				function_end = next_end_n;
			}
			scope_start = source.find(("{"), function_end);
			continue;
		}
		scope_name_n2 += 1;
		auto scope_name_n1 = utility::string::rfind_first_of(source, (" \t\n\r"), scope_name_n2);
		if(scope_name_n1 == psl::string_view::npos)
		{
			return write_error(data, scope_name_n2, ("an unknown error happened"));
		}
		scope_name_n1 += 1;

		auto name = psl::string_view(&source[scope_name_n1], scope_name_n2 - scope_name_n1);

		auto scope_type_n2 = utility::string::rfind_first_not_of(source, (" \t\n\r"), (scope_name_n1 - 1));
		if(scope_type_n2 == psl::string_view::npos)
		{
			return write_error(data, scope_name_n1, ("could not figure out the type of the scope '") + name + ("'"));
		}

		auto scope_type_n1 = utility::string::rfind_first_of(source, (" \t\n\r"), scope_type_n2);
		if(scope_type_n1 == psl::string_view::npos)
		{
			return write_error(data, scope_type_n2, ("could not figure out the type of the scope '") + name + ("'"));
		}
		scope_type_n1 += 1;

		auto type = psl::string_view(&source[scope_type_n1], scope_type_n2 - scope_type_n1);

		auto scope_end = source.find((";"), scope_start);
		if(scope_end == psl::string_view::npos)
		{
			return write_error(data, scope_start, ("could not figure out the end of the scope '") + name + ("'"));
		}
		scope_end += 1;
		int64_t difference =
			(int64_t)utility::string::count(psl::string_view{&source[scope_start], scope_end - scope_start}, ("{")) -
			((int64_t)utility::string::count(psl::string_view{&source[scope_start], scope_end - scope_start}, ("}")));
		bool sub_scope = difference > 0;
		while(scope_end != psl::string_view::npos && difference != 0)
		{
			auto next_end_n = source.find((";"), scope_end);
			if(next_end_n == psl::string::npos)
			{
				return write_error(data, scope_end,
								   ("could not figure out the end of the scope '") + name +
									   ("' got stuck in a sub-scope (nested scope)"));
			}
			next_end_n += 1;
			difference +=
				(int64_t)utility::string::count(psl::string_view{&source[scope_end], next_end_n - scope_end}, ("{")) -
				((int64_t)utility::string::count(psl::string_view{&source[scope_end], next_end_n - scope_end}, ("}")));
			scope_end = next_end_n;
		}

		auto& local_scope = result.emplace_back();
		local_scope.name  = name;
		local_scope.type  = block_type::unknown_t;
		if(type == ("struct"))
			local_scope.type = block_type::struct_t;
		else if(type == ("attribute"))
			local_scope.type = block_type::attribute_t;
		else if(type == ("descriptor"))
			local_scope.type = block_type::descriptor_t;
		else if(type == ("interface"))
			local_scope.type = block_type::interface_t;

		local_scope.unique_name = local_scope.name;

		auto true_scope_start = source.rfind(("\n"), scope_type_n1);
		if(true_scope_start == psl::string_view::npos)
			true_scope_start = 0;
		else
			true_scope_start += 1;
		local_scope.scope = psl::string_view(&source[true_scope_start], (scope_end) - (true_scope_start));


		// descriptors allow inlined scopes, they are a bit annoying to deal with.
		// type naming convention is 'parent_name::child_type' as unique type
		if(sub_scope && type == ("descriptor"))
		{
			auto sub_scope{psl::string_view(&source[scope_start + 1], (scope_end - 2) - (scope_start + 1))};

			size_t offset		= 0u;
			size_t open_bracket = 0u;
			size_t open_brace   = 0u;
			size_t close_brace  = 0u;
			do
			{
				open_bracket = sub_scope.find(("{"), offset);
				open_brace   = sub_scope.find(("("), offset);
				close_brace  = sub_scope.find((")"), offset);
				if(open_bracket == psl::string_view::npos) break;

				if(open_bracket > open_brace && open_bracket < close_brace)
				{
					offset = close_brace + 1;
					continue;
				}

				auto sub_element_end = sub_scope.find(("}"), open_bracket);
				if(sub_element_end == psl::string_view::npos)
				{
					return write_error(data, scope_start,
									   ("could not figure out the end of the sub-element in the scope '") +
										   local_scope.unique_name + ("'"));
				}
				auto sub			 = sub_scope.substr(0, sub_element_end + 2);
				auto sub_scope_start = open_bracket;
				if(sub_scope_start == psl::string_view::npos)
				{
					return write_error(data, scope_start,
									   ("could not figure out the end of the sub-element in the scope '") +
										   local_scope.unique_name + ("'"));
				}
				size_t local_name_n2 = utility::string::rfind_first_not_of(sub, (" \n\r\t"), sub_scope_start);
				if(local_name_n2 == psl::string_view::npos)
				{
					return write_error(data, sub_scope_start,
									   ("could not figure out the name of the sub-element in the scope '") +
										   local_scope.unique_name + ("'"));
				}

				if(sub[local_name_n2 - 1] == (')'))
				{
					local_name_n2 = sub.rfind(('('), local_name_n2);
					local_name_n2 = utility::string::rfind_first_not_of(sub, (" \n\r\t"), local_name_n2);
				}

				auto local_name_n1 = utility::string::rfind_first_of(sub, (" \n\r\t"), local_name_n2);
				if(local_name_n1 == psl::string_view::npos)
				{
					return write_error(data, local_name_n2,
									   ("could not figure out the name of the sub-element in the scope '") +
										   local_scope.unique_name + ("'"));
				}
				local_name_n1 += 1;

				auto local_name = psl::string_view(&sub[local_name_n1], local_name_n2 - local_name_n1);

				auto local_type_n2 = utility::string::rfind_first_not_of(sub, (" \t\n\r"), (local_name_n1));
				if(local_type_n2 == psl::string_view::npos)
				{
					return write_error(data, local_name_n1,
									   ("could not figure out the type of the sub-element '") + local_name +
										   ("' in the scope '") + local_scope.unique_name + ("'"));
				}

				auto local_type_n1 = utility::string::rfind_first_of(sub, (" \t\n\r"), local_type_n2);
				if(local_type_n1 == psl::string_view::npos)
				{
					return write_error(data, local_type_n2,
									   ("could not figure out the type of the sub-element '") + local_name +
										   ("' in the scope '") + local_scope.unique_name + ("'"));
				}
				local_type_n1 += 1;

				auto local_type = psl::string_view(&sub[local_type_n1], local_type_n2 - local_type_n1);

				auto& inlined_scope = result.emplace_back();
				inlined_scope.name  = local_name;

				inlined_scope.type = block_type::inlined_t;

				inlined_scope.parent	  = name;
				inlined_scope.unique_name = psl::string(name) + ("::") + local_type;
				auto local_scope_start	= sub.rfind(("\n"), local_type_n1);
				if(local_scope_start == psl::string_view::npos)
					local_scope_start = 0;
				else
					local_scope_start += 1;
				inlined_scope.scope = psl::string_view(&sub[local_scope_start], (sub.size()) - (local_scope_start));
				offset				= sub_element_end;

			} while(open_bracket != psl::string_view::npos);
		}

		if(local_scope.type != block_type::struct_t && local_scope.type != block_type::unknown_t)
		{
			source.erase(std::begin(source) + true_scope_start, std::begin(source) + scope_end);
			scope_start = source.find(("{"), true_scope_start);
		}
		else
		{
			scope_start = source.find(("{"), scope_end);
		}
	}

	// we do all structs first as the can introduce new types
	for(auto& scope : result)
	{
		if(scope.type != block_type::struct_t) continue;

		auto start = scope.scope.find(("{")) + 1;
		auto end   = scope.scope.find_last_of(("}"));
		auto split = utility::string::split(psl::string_view{&scope.scope[start], end - start}, (";"));
		scope.size = 0u;
		for(const auto& element : split)
		{
			auto type_start = element.find_first_not_of((" \n\r\t"));
			if(type_start == psl::string_view::npos) continue;
			auto type_end = element.find_first_of((" \n\r\t"), type_start);
			if(type_end == psl::string_view::npos) continue;
			auto type = element.substr(type_start, type_end - type_start);

			auto name_start = element.find_first_not_of((" \n\r\t"), type_end);
			auto name_end   = element.find_first_of((" \n\r\t"), name_start);
			auto name		= element.substr(name_start, name_end - name_start);

			parsed_element& e = scope.elements.emplace_back();
			e.name			  = name;
			e.type			  = type;
			const auto& it	= m_KnownTypes.find(e.type);
			if(it != std::end(m_KnownTypes))
				e.size = it->second;
			else
			{
				e.size = 0;
				return write_error(data, 0, ("could not deduce the size of the type: ") + type);
			}
			scope.size += e.size;
		}

		m_KnownTypes[scope.name] = scope.size;
	}

	// next up all inlined types
	for(auto& scope : result)
	{
		if(scope.type != block_type::inlined_t) continue;

		auto start_offset = scope.scope.find((")"));
		if(start_offset == psl::string_view::npos) start_offset = 0u;
		auto start = scope.scope.find(("{"), start_offset) + 1;
		auto end   = scope.scope.find_last_of(("}"));
		auto split = utility::string::split(psl::string_view{&scope.scope[start], end - start}, (";"));
		scope.size = 0u;
		for(const auto& element : split)
		{
			auto type_start = element.find_first_not_of((" \n\r\t"));
			if(type_start == psl::string_view::npos) continue;
			auto type_end = element.find_first_of((" \n\r\t"), type_start);
			if(type_end == psl::string_view::npos) continue;
			auto type = element.substr(type_start, type_end - type_start);

			auto name_start = element.find_first_not_of((" \n\r\t"), type_end);
			auto name_end   = element.find_first_of((" \n\r\t"), name_start);
			auto name		= element.substr(name_start, name_end - name_start);

			parsed_element& e = scope.elements.emplace_back();
			e.name			  = name;
			e.type			  = type;
			const auto& it	= m_KnownTypes.find(e.type);
			if(it != std::end(m_KnownTypes))
				e.size = it->second;
			else
			{
				e.size = 0;
				return write_error(data, 0, ("could not deduce the size of the type: ") + type);
			}
			scope.size += e.size;
		}

		m_KnownTypes[scope.unique_name] = scope.size;
	}


	for(auto& scope : result)
	{
		if(scope.type == block_type::unknown_t || scope.type == block_type::struct_t ||
		   scope.type == block_type::inlined_t)
			continue;

		std::vector<psl::string_view> split;
		// figure out all elements
		if(scope.type == block_type::descriptor_t)
		{
			auto start	 = scope.scope.find(("{")) + 1;
			auto end	   = scope.scope.find_last_of(("}"));
			size_t current = start;
			do
			{
				auto terminator_n = scope.scope.find((";"), current);
				auto bracket_n	= scope.scope.find(("{"), current);
				auto search_end   = terminator_n + 1;

				if(terminator_n == psl::string_view::npos)
				{
					return write_error(data, 0,
									   ("could not figure out the end of the scope for the descriptor") + scope.name);
				}

				if(terminator_n > bracket_n)
				{
					terminator_n = bracket_n;
					search_end   = scope.scope.find((";"), current) + 1;

					int64_t difference = (int64_t)utility::string::count(
											 psl::string_view{&scope.scope[current], search_end - current}, ("{")) -
										 ((int64_t)utility::string::count(
											 psl::string_view{&scope.scope[current], search_end - current}, ("}")));
					bool sub_scope = difference > 0;
					while(search_end != psl::string_view::npos && difference != 0)
					{
						auto next_end_n = scope.scope.find((";"), search_end);
						if(next_end_n == psl::string::npos)
						{
							return write_error(
								data, 0, ("could not figure out the end of the scope for the descriptor") + scope.name);
						}
						next_end_n += 1;
						difference += (int64_t)utility::string::count(
										  psl::string_view{&scope.scope[search_end], next_end_n - search_end}, ("{")) -
									  ((int64_t)utility::string::count(
										  psl::string_view{&scope.scope[search_end], next_end_n - search_end}, ("}")));
						search_end = next_end_n;
					}
				}
				split.emplace_back(psl::string_view{&scope.scope[current], (search_end - 1) - current});
				current = search_end;

			} while(current != psl::string_view::npos && current < end);
		}
		else
		{
			auto start = scope.scope.find(("{")) + 1;
			auto end   = scope.scope.find_last_of(("}"));
			split	  = utility::string::split(psl::string_view{&scope.scope[start], end - start}, (";"));
		}

		scope.size = 0u;
		for(const auto& element : split)
		{
			auto location_start = element.find_first_of(("0123456789"));
			if(location_start == psl::string_view::npos) continue;
			auto location_end = element.find_first_not_of(("0123456789"), location_start);
			size_t location;
			utility::from_string(psl::to_string8_t(element.substr(location_start, location_end - location_start)),
								 location);

			auto type_start = element.find_first_not_of((" \n\r\t"), location_end);
			if(type_start == psl::string_view::npos) continue;
			auto type_end = element.find_first_of((" \n\r\t"), type_start);
			if(type_end == psl::string_view::npos) continue;
			auto type = element.substr(type_start, type_end - type_start);

			auto name_start = element.find_first_not_of((" \n\r\t"), type_end);
			auto name_end   = element.find_first_of((" \n\r\t"), name_start);
			auto name		= element.substr(name_start, name_end - name_start);

			parsed_element& e = scope.elements.emplace_back();
			e.name			  = name;
			e.type			  = type;
			e.location		  = location;


			auto qualifiers_start_n = element.find(("("));
			auto qualifiers_end_n   = element.find((")"));
			if(qualifiers_start_n != psl::string_view::npos)
			{
				auto brace_list = element.substr(qualifiers_start_n, qualifiers_end_n - qualifiers_start_n);
				if(auto index = brace_list.find(("buffer:")); index != psl::string_view::npos)
				{
					auto start_n = brace_list.find_first_not_of((" \t:"), index + 7);
					auto end_n   = std::min(brace_list.find_first_of((" \t)"), start_n), brace_list.size());
					e.buffer	 = psl::string_view{&brace_list[start_n], end_n - start_n};
				}

				if(auto index = brace_list.find(("type:")); index != psl::string_view::npos)
				{
					auto start_n   = brace_list.find_first_not_of((" \t:"), index + 5);
					auto end_n	 = std::min(brace_list.find_first_of((" \t)"), start_n), brace_list.size());
					e.backing_type = psl::string_view{&brace_list[start_n], end_n - start_n};
				}

				{
					if(auto index = brace_list.find(("in:")); index != psl::string_view::npos)
					{
						auto start_stream_n = brace_list.find(('"'), index) + 1;
						auto end_stream_n   = brace_list.find(('"'), start_stream_n);
						e.in_qualifier = psl::string_view{&brace_list[start_stream_n], end_stream_n - start_stream_n};
					}
					if(auto index = brace_list.find(("out:")); index != psl::string_view::npos)
					{
						auto start_stream_n = brace_list.find(('"'), index) + 1;
						auto end_stream_n   = brace_list.find(('"'), start_stream_n);
						e.out_qualifier = psl::string_view{&brace_list[start_stream_n], end_stream_n - start_stream_n};
					}

					if(auto index = brace_list.find(("bind:")); index != psl::string_view::npos)
					{
						auto start_stream_n = brace_list.find(('"'), index) + 1;
						auto end_stream_n   = brace_list.find(('"'), start_stream_n);
						e.bind_qualifier = psl::string_view{&brace_list[start_stream_n], end_stream_n - start_stream_n};
					}
				}

				e.rate = 0;
				if(auto index = brace_list.find(("rate:")); index != psl::string_view::npos)
				{
					auto start_n = brace_list.find_first_not_of((" \t:"), index + 4);
					auto end_n   = std::min(brace_list.find_first_of((" \t)"), start_n), brace_list.size());
					auto rate	= psl::string_view{&brace_list[start_n], end_n - start_n};

					if(rate == ("instance")) e.rate = 1;
				}

				if(auto index = brace_list.find(("attribute:")); index != psl::string_view::npos)
				{
					auto offset = index;
					while(true)
					{
						auto start_stream_n = brace_list.find(('{'), offset);
						if(start_stream_n == psl::string_view::npos) break;
						start_stream_n += 1;
						auto end_stream_n = brace_list.find(('}'), start_stream_n);
						auto sub_element = psl::string_view{&brace_list[start_stream_n], end_stream_n - start_stream_n};
						auto first_num_n = brace_list.find_first_of(("0123456789"), start_stream_n);
						auto first_num_n_end = brace_list.find_first_not_of(("0123456789"), first_num_n);
						auto first_num = psl::string_view{&brace_list[first_num_n], first_num_n_end - first_num_n};

						auto second_num_n	 = brace_list.find_first_of(("0123456789"), first_num_n_end);
						auto second_num_n_end = brace_list.find_first_not_of(("0123456789"), second_num_n);
						auto second_num = psl::string_view{&brace_list[second_num_n], second_num_n_end - second_num_n};

						auto& vAttr  = e.vAttribute.emplace_back();
						vAttr.format = utility::converter<size_t>::from_string(psl::to_string8_t(first_num));
						vAttr.offset = utility::converter<size_t>::from_string(psl::to_string8_t(second_num));
						offset		 = end_stream_n + 1;
					}
				}
			}

			auto it = m_KnownTypes.find(e.type);
			if(it != std::end(m_KnownTypes))
				e.size = it->second;
			else
			{
				e.size = 0;
				it	 = m_KnownTypes.find(scope.name + ("::") + e.type);
				if(it != std::end(m_KnownTypes))
					e.size = it->second;
				else
				{
					return write_error(data, 0, ("could not deduce the size of the type: ") + type);
				}

				// when we hit this spot we know we have a local scope
				e.inlined_scope = element.substr(element.find_last_of(("{")));
			}
			scope.size += e.size;
		}
	}

	return true;
}

bool shader::parse_includes(file_data& data)
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
			std::cerr << ("ERROR: could not deduce the input file's path.\n");
			std::cerr << ("\tthe include's opening directive was not found, are you missing a \" or ' ?\n");
			std::cerr << psl::to_string8_t("\tat line '" + utility::to_string(line_number) + "' of '" + data.filename +
										 "\n");
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
			std::cerr << ("ERROR: could not deduce the input file's path.\n");
			std::cerr
				<< ("\tthe include directive was opened, but not closed, please add a \" or ' at the end of the "
					"line.\n");
			std::cerr << psl::to_string8_t("\tat line '" + utility::to_string(line_number) + "' of '" + data.filename +
										 "\n");
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
			std::cerr << ("ERROR: include file not found.\n");
			std::cerr << psl::to_string8_t("\tit was defined as '" + include_def + "' and transformed into '" +
										 include_path + "'\n");
			std::cerr << psl::to_string8_t("\tat line '" + utility::to_string(line_number) + "' of '" + data.filename +
										 "\n");
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

bool shader::parse_using(file_data& data, psl::string name, psl::string& type_out, psl::string& name_out)
{
	if(auto using_n = data.content.find(("#using ") + name); using_n != psl::string::npos)
	{
		auto endl_n = data.content.find(("\n"), using_n);

		auto divider_n = data.content.find((":"), using_n);
		if(divider_n > endl_n)
		{
			std::cerr << ("ERROR: malformed #using block.'\n");
			std::cerr << psl::to_string8_t("\tthe block for '#using " + name + "' did not define a ':'.\n");
			std::cerr << psl::to_string8_t("\tin file '" + data.filename + "\n");
			return false;
		}

		divider_n += 1;
		auto type_n		= data.content.find_first_not_of((" \t"), divider_n);
		auto type_n_end = data.content.find_first_of((" \t\r\n"), type_n);
		type_out		= psl::string{&data.content[type_n], type_n_end - type_n};

		auto name_n = data.content.find_first_not_of((" \t"), type_n_end);
		if(endl_n > name_n)
		{
			auto name_n_end = data.content.find_first_of((" \t\n\r"), name_n);
			name_out		= psl::string{&data.content[name_n], name_n_end - name_n};
		}
		else
			name_out = "";
		data.content.erase(std::begin(data.content) + using_n, std::begin(data.content) + endl_n);
		// data.content.replace(std::begin(data.content) + using_n, std::begin(data.content) + endl_n,
		// psl::string(endl_n - using_n, (' ')));
	}
	return true;
}

bool shader::parse_using(file_data& data)
{
	if(parse_using(data, ("in"), data.using_in_type, data.using_in_name) &&
	   parse_using(data, ("out"), data.using_out_type, data.using_out_name) &&
	   parse_using(data, ("descriptors"), data.using_descr_type, data.using_descr_name))
		return true;

	return false;
}
bool shader::parse(file_data& data)
{
	if(parse_includes(data) && parse_using(data) && all_scopes(data)) return true;

	return false;
}
bool shader::cache_file(const psl::string& file)
{
	auto it = m_Cache.find(file);
	if(it != m_Cache.end() &&
	   std::filesystem::last_write_time(file).time_since_epoch().count() == it->second.last_modified)
	{
		if(m_Verbose)
		{
			std::cout << psl::to_string8_t(file + " was found in the cache\n");
			std::cout << ("\tchecking its dependencies..\n");
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
			std::cerr << psl::to_string8_t("ERROR: missing file, no file was found at the location: " + file);
			utility::terminal::set_color(utility::terminal::color::WHITE);
			return false;
		}
		else if(!utf8::is_valid(res.value().begin(), res.value().end()))
		{
			utility::terminal::set_color(utility::terminal::color::RED);
			std::cerr << psl::to_string8_t("ERROR: the encoding was not valid UTF-8 for the file: " + file);
			utility::terminal::set_color(utility::terminal::color::WHITE);
			return false;
		}
		else
		{
			if(m_Verbose) std::cout << psl::to_string8_t(file + " is being loaded into the cache\n");
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
				std::cerr << psl::to_string8_t("the include" + inc.first + " is not present in the cache.\n");
				continue;
			}
			auto sub_res{construct(it->second, includes)};
			res.insert(inc.second + offset, sub_res);
			offset += sub_res.size();
		}
	}
	return res;
}

bool shader::find(const file_data& fdata, const psl::string& name, block_type type, parsed_scope* out) const
{
	/*const std::vector<description>* search_list = nullptr;
	switch(interface_type)
	{
		case interface_t::generic:
		{ search_list = &fdata.interfaces; }break;
		case interface_t::attribute:
		{ search_list = &fdata.attributes; }break;
		case interface_t::descriptor:
		{ search_list = &fdata.descriptors; }break;
		case interface_t::struct_t:
		{ search_list = &fdata.structs; }break;
	}*/
	for(const auto& scope : fdata.scopes)
	{
		if(scope.type == type && scope.unique_name == name)
		{
			*out = scope;
			return true;
		}
	}

	for(const auto& include : fdata.includes)
	{
		auto it = m_Cache.find(include.first);
		if(it == std::end(m_Cache))
		{
			std::cerr << psl::to_string8_t("the include" + include.first + " is not present in the cache.\n");
			continue;
		}
		if(find(it->second, name, type, out)) return true;
	}

	return false;
}


bool shader::construct(const file_data& fdata, result_shader& out, psl::string_view entry) const
{
	psl::string res = fdata.content;
	std::set<psl::string_view> includes;
	size_t offset = 0u;
	for(const auto& inc : fdata.includes)
	{
		if(includes.find(inc.first) == std::end(includes))
		{
			includes.insert(inc.first);
			auto it = m_Cache.find(inc.first);
			if(it == std::end(m_Cache))
			{
				std::cerr << psl::to_string8_t("the include" + inc.first + " is not present in the cache.\n");
				continue;
			}
			auto sub_res{construct(it->second, includes)};
			res.insert(std::begin(res) + inc.second + offset, std::begin(sub_res), std::end(sub_res));
			offset += sub_res.size();
		}
	}

	auto entry_n					 = res.find(entry);
	size_t interfaces_input_location = 0u;
	while(entry_n != psl::string::npos)
	{
		auto previous_n = res.rfind(("\n"), entry_n);
		auto start_n	= res.find_first_not_of((" \n\r\t"), previous_n);
		if(start_n == entry_n)
		{
			entry_n = res.find(entry, entry_n + entry.size());
			continue;
		}

		auto parantheses_n = res.find_first_not_of((" \t"), entry_n + entry.size());
		if(res[parantheses_n] != ('('))
		{
			entry_n = res.find(entry, entry_n + entry.size());
			continue;
		}
		parantheses_n = res.find_first_not_of((" \t"), parantheses_n + 1);
		if(res[parantheses_n] != (')'))
		{
			entry_n = res.find(entry, entry_n + entry.size());
			continue;
		}
		auto end_n = res.find_first_not_of((" \t\r"), parantheses_n + 1);
		if(res[end_n] == ('\n'))
		{
			interfaces_input_location = previous_n;
			break;
		}

		entry_n = res.find(entry, entry_n + entry.size());
	}

	if(entry_n == psl::string::npos)
	{
		std::cerr << psl::to_string8_t("could not find the entry method '" + entry + "'\n");
		return false;
	}

	psl::string inject = ("");
	if(!fdata.using_in_type.empty())
	{
		parsed_scope attr;
		if(!find(fdata, fdata.using_in_type, block_type::attribute_t, &attr))
		{
			if(!find(fdata, fdata.using_in_type, block_type::interface_t, &attr))
			{
				std::cerr << psl::to_string8_t("could not find the shader attribute '" + fdata.using_in_type + "'\n");
				return false;
			}
			else
			{
				out.in = attr;
			}
		}
		else
		{
			out.in = attr;
		}

		for(const auto& a : attr.elements)
		{
			inject += ("layout (location = ") + psl::from_string8_t(utility::to_string(a.location.value())) + (") ") +
					  a.in_qualifier.value_or(("")) + (" in ") + a.type + (" ") + fdata.using_in_name + a.name +
					  (";\n");
			if(!fdata.using_in_name.empty())
				utility::string::replace_all(res, fdata.using_in_name + (".") + a.name, fdata.using_in_name + a.name);
		}
	}

	if(!fdata.using_out_type.empty())
	{
		parsed_scope iface;
		if(!find(fdata, fdata.using_out_type, block_type::interface_t, &iface))
		{
			std::cerr << psl::to_string8_t("could not find the shader attribute '" + fdata.using_out_type + "'\n");
			return false;
		}
		out.out = (iface);

		for(const auto& a : iface.elements)
		{
			inject += ("layout (location = ") + psl::from_string8_t(utility::to_string(a.location.value())) + (") ") +
					  a.out_qualifier.value_or(("")) + ("out ") + a.type + (" ") + fdata.using_out_name + a.name +
					  (";\n");
			if(!fdata.using_out_name.empty())
				utility::string::replace_all(res, fdata.using_out_name + (".") + a.name, fdata.using_out_name + a.name);
		}
	}


	if(!fdata.using_descr_type.empty())
	{
		parsed_scope descr;
		if(!find(fdata, fdata.using_descr_type, block_type::descriptor_t, &descr))
		{
			std::cerr << psl::to_string8_t("could not find the shader attribute '" + fdata.using_descr_type + "'\n");
			return false;
		}
		out.descriptor = (descr);

		for(const auto& a : descr.elements)
		{
			inject += ("layout (binding = ") + psl::from_string8_t(utility::to_string(a.location.value())) + (") ") +
					  a.bind_qualifier.value_or(("")) + (" ") + a.buffer.value_or(a.type) + (" ") +
					  ((!a.inlined_scope.has_value())
						   ? a.name + (";\n")
						   : ("\n") + a.inlined_scope.value() + a.name + (";\n"));

			/*parsed_scope glsl_struct;
			if(!find(fdata, fdata.using_descr_type + ("::") + a.type, block_type::struct_t, &glsl_struct))
			{
				std::cerr << ("could not find the struct type '") << a.type << ("'\n");
				return false;
			}
*/
			// auto size = sizeof_type(fdata, glsl_struct);
		}
	}
	inject += ("\n"), res.insert(interfaces_input_location, inject);
	out.content  = res;
	out.filename = fdata.filename;

	return true;
}
void shader::generate(psl::string ifile, psl::string ofile, bool compiled_glsl, bool optimize,
					  psl::array<psl::string> types)
{
	ifile = utility::platform::file::to_platform(ifile);
	ofile = utility::platform::file::to_platform(ofile);

	auto directory = ofile.substr(0, ofile.find_last_of("/\\"));
	if(!utility::platform::directory::exists(directory)) utility::platform::directory::create(directory, true);

	auto index				  = ifile.find_last_of(("."));
	auto extension			  = ifile.substr(index + 1);
	tools::glslang::type type = tools::glslang::type::vert;
	if(extension == ("frag"))
		type = tools::glslang::type::frag;
	else if(extension == ("tesc"))
		type = tools::glslang::type::tesc;
	else if(extension == ("geom"))
		type = tools::glslang::type::geom;
	else if(extension == ("comp"))
		type = tools::glslang::type::comp;
	else if(extension == ("tese"))
		type = tools::glslang::type::tese;

	auto ofile_meta = ofile + "-" + tools::glslang::type_str[(uint8_t)type] + "." + ::meta::META_EXTENSION;


	UID meta_UID = UID::generate();
	if(utility::platform::file::exists(ofile_meta))
	{
		::meta::file* original = nullptr;
		serialization::serializer temp_s;
		temp_s.deserialize<serialization::decode_from_format>(original, ofile_meta);
		meta_UID = original->ID();
	}
	core::meta::shader shaderMeta{meta_UID};

	psl::timer timer;

	try
	{
		if(!cache_file(ifile))
		{
			std::cerr
				<< ("something went wrong when loading the file in the cache, please consult the output to see why");
			goto end;
		}
	}
	catch(std::exception e)
	{
		std::cerr << psl::to_string8_t("exception happened during caching! \n" + psl::string8_t(e.what()) + "\n");
	}


	{
		// parsing the file
		result_shader shader;
		auto& cache_data = m_Cache[ifile];
		try
		{
			if(!construct(cache_data, shader))
			{
				std::cerr
					<< ("something went wrong when loading the file in the cache, please consult the output to see "
						"why");
				goto end;
			}
		}
		catch(std::exception e)
		{
			std::cerr << psl::to_string8_t("exception happened during constructing! \n" + psl::string8_t(e.what()) +
										 "\n");
		}

		if(compiled_glsl)
		{
			utility::platform::file::write(ofile + ".generated", shader.content);
			std::cout << psl::to_string8_t("outputted the generated glsl file at '" + ofile + ".generated'\n");
			goto end;
		}


		std::optional<size_t> gles_version;
		if(std::find(std::begin(types), std::end(types), "gles") != std::end(types)) gles_version = 300;

		if(tools::glslang::compile(utility::application::path::get_path(), shader.content, ofile, type, optimize,
								   gles_version))
		{
		}
		else
			goto end;

		if(type == tools::glslang::type::vert)
		{
			std::vector<MSVBinding> bindings;
			bindings.reserve(shader.in.elements.size());
			for(const auto& element : shader.in.elements)
			{
				MSVBinding& binding = bindings.emplace_back();
				binding.binding_slot(element.location.value());
				if(element.buffer) binding.buffer(psl::to_string8_t(element.buffer.value()));
				binding.input_rate((core::gfx::vertex_input_rate)element.rate.value_or(0));
				auto attribute_location = element.location.value_or(0);
				binding.size(element.size);
				std::vector<MSVAttribute> attributes;
				for(const auto& format_offset : element.vAttribute)
				{
					MSVAttribute& attr = attributes.emplace_back();
					attr.format((core::gfx::format)format_offset.format);
					attr.offset(format_offset.offset);
					attr.location(attribute_location);
					++attribute_location;
				}
				binding.attributes(attributes);
			}
			shaderMeta.vertex_bindings(bindings);

			// binding.
			// shaderMeta.att()
		}
		else
		{
			// todo: here we could add the connection into the meta, allowing validation between shaders before we turn
			// them into a program.
		}

		std::vector<MSDescriptor> descriptors;

		for(const auto& element : shader.descriptor.elements)
		{
			MSDescriptor& descriptor = descriptors.emplace_back();
			descriptor.name(element.buffer.value_or(""));
			descriptor.binding(element.location.value());
			descriptor.size(element.size);
			auto format_it = m_DescriptorType.find(element.backing_type.value());
			descriptor.type((core::gfx::binding_type)format_it->second);
			if(element.size == 0)
			{
				std::vector<MSIElement> msi_elements;
				MSIElement& msi_element = msi_elements.emplace_back();
				msi_element.name(psl::to_string8_t(element.name));
				msi_element.format((core::gfx::format)format_it->second);
				if(element.default_value)
				{
					auto split = utility::string::split(element.default_value.value(), (","));
					std::vector<uint8_t> stream;
					stream.resize(sizeof(float) / sizeof(uint8_t) * split.size());
					std::vector<float> intermediate;
					intermediate.reserve(sizeof(float) / sizeof(uint8_t) * split.size());
					for(const auto& el : split)
					{
						intermediate.emplace_back(utility::converter<float>::from_string(psl::to_string8_t(el)));
					}
					memcpy(stream.data(), intermediate.data(), intermediate.size());
					msi_element.default_value(stream);
				}
				descriptor.sub_elements(msi_elements);
				continue;
			}

			parsed_scope glsl_struct;
			if(!find(cache_data, cache_data.using_descr_type + ("::") + element.type, block_type::inlined_t,
					 &glsl_struct))
			{
				std::cerr << psl::to_string8_t("could not find the struct type '" + element.type + "'\n");
				goto end;
			}


			{

				std::vector<MSIElement> msi_elements;
				size_t offset = 0u;
				for(const auto& sub_element : glsl_struct.elements)
				{
					auto format_it = m_TypeToFormat.find(sub_element.type);
					if(format_it == std::end(m_TypeToFormat))
					{
						parsed_scope true_struct;
						if(!find(cache_data, sub_element.type, block_type::struct_t, &true_struct))
						{
							std::cerr << psl::to_string8_t("could not find the struct type '" + element.type + "'\n");
							goto end;
						}
						for(const auto& true_sub_element : true_struct.elements)
						{
							MSIElement& msi_element = msi_elements.emplace_back();
							format_it				= m_TypeToFormat.find(true_sub_element.type);
							if(format_it == std::end(m_TypeToFormat))
							{
								std::cerr << psl::to_string8_t("could not deduce the vk::Format for the type '" +
															 sub_element.type + "'\n");
								goto end;
							}

							msi_element.name(psl::to_string8_t(true_sub_element.name));
							msi_element.offset(offset);
							if(format_it == std::end(m_TypeToFormat))
							{
								std::cerr << psl::to_string8_t("could not deduce the vk::Format for the type '" +
															 true_sub_element.type + "'\n");
								goto end;
							}
							msi_element.format((core::gfx::format)format_it->second);

							if(true_sub_element.default_value)
							{
								auto split = utility::string::split(true_sub_element.default_value.value(), (","));
								std::vector<uint8_t> stream;
								stream.resize(sizeof(float) / sizeof(uint8_t) * split.size());
								std::vector<float> intermediate;
								intermediate.reserve(sizeof(float) / sizeof(uint8_t) * split.size());
								for(const auto& el : split)
								{
									intermediate.emplace_back(
										utility::converter<float>::from_string(psl::to_string8_t(el)));
								}
								memcpy(stream.data(), intermediate.data(), intermediate.size());
								msi_element.default_value(stream);
							}
							offset += true_sub_element.size;
						}
					}
					else
					{
						MSIElement& msi_element = msi_elements.emplace_back();
						msi_element.name(psl::to_string8_t(sub_element.name));
						msi_element.offset(offset);
						if(format_it == std::end(m_TypeToFormat))
						{
							std::cerr << psl::to_string8_t("could not deduce the vk::Format for the type '" +
														 sub_element.type + "'\n");
							goto end;
						}
						msi_element.format((core::gfx::format)format_it->second);

						if(sub_element.default_value)
						{
							auto split = utility::string::split(sub_element.default_value.value(), (","));
							std::vector<uint8_t> stream;
							stream.resize(sizeof(float) / sizeof(uint8_t) * split.size());
							std::vector<float> intermediate;
							intermediate.reserve(sizeof(float) / sizeof(uint8_t) * split.size());
							for(const auto& el : split)
							{
								intermediate.emplace_back(
									utility::converter<float>::from_string(psl::to_string8_t(el)));
							}
							memcpy(stream.data(), intermediate.data(), intermediate.size());
							msi_element.default_value(stream);
						}
						offset += sub_element.size;
					}
				}
				descriptor.sub_elements(msi_elements);
			}
		}
		shaderMeta.descriptors(descriptors);

		switch(type)
		{
		case tools::glslang::type::vert:
		{
			shaderMeta.stage(core::gfx::shader_stage::vertex);
		}
		break;
		case tools::glslang::type::frag:
		{
			shaderMeta.stage(core::gfx::shader_stage::fragment);
		}
		break;
		case tools::glslang::type::geom:
		{
			shaderMeta.stage(core::gfx::shader_stage::geometry);
		}
		break;
		case tools::glslang::type::tesc:
		{
			shaderMeta.stage(core::gfx::shader_stage::tesselation_control);
		}
		break;
		case tools::glslang::type::tese:
		{
			shaderMeta.stage(core::gfx::shader_stage::tesselation_evaluation);
		}
		break;
		case tools::glslang::type::comp:
		{
			shaderMeta.stage(core::gfx::shader_stage::compute);
		}
		break;
		}

		serialization::serializer s;
		format::container container;
		s.serialize<serialization::encode_to_format>(&shaderMeta, container);
		utility::platform::file::write(ofile_meta, psl::from_string8_t(container.to_string()));
	}

end:
	if(m_Verbose) std::cout << ("generating took ") << timer.elapsed().count() << ("ns") << std::endl;
}
void shader::on_generate(psl::cli::pack& pack)
{
	// -g -s -i "C:\Projects\data_old\Shaders\Surface\default.vert" -o
	// "C:\Projects\data_old\Shaders\test_output\default.spv" -O -v -g -s -i
	// "C:\Projects\data_old\Shaders\Surface\default.vert" -o
	// "C:\Projects\data_old\Shaders\test_output\default-glsl.vert" --glsl -v

	auto ifile		   = pack["input"]->as<psl::string>().get();
	auto ofile		   = pack["output"]->as<psl::string>().get();
	auto overwrite	 = pack["overwrite"]->as<bool>().get();
	auto compiled_glsl = pack["compiled glsl"]->as<bool>().get();
	auto optimize	  = pack["optimize"]->as<bool>().get();
	m_Verbose		   = pack["verbose"]->as<bool>().get();
	auto types		   = pack["types"]->as<std::vector<psl::string>>().get();

	if(ifile[ifile.size() - 1] == '*')
	{
		auto directory = ifile.erase(ifile.size() - 1);
		if(!utility::platform::directory::is_directory(directory))
		{
			directory.erase(directory.find_last_of('/') + 1);
		}
		auto ifiles = utility::platform::directory::all_files(directory, true);
		if(ofile.empty())
		{
			ofile = directory;
		}

		if(ofile[ofile.size() - 1] == '/' || ofile[ofile.size() - 1] == '\\') ofile.erase(ofile.size() - 1);

		for(const auto& input : ifiles)
		{
			auto output = ofile + "/" + input.substr(directory.size());
			if(size_t last_dot = output.find_last_of('.'); last_dot != std::string::npos)
			{
				output.erase(last_dot);
			}
			generate(input, output, compiled_glsl, optimize, types);
		}
	}
	else
	{
		if(ofile.empty())
		{
			ofile			= ifile;
			size_t last_dot = ofile.find_last_of('.');
			ofile[last_dot] = '-';
		}
		else if(size_t last_dot = ofile.find_last_of('.'); last_dot != std::string::npos)
		{
			ofile.erase(last_dot);
		}
		generate(ifile, ofile, compiled_glsl, optimize, types);
	}
}
