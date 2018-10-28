#pragma once
#include "cli_pack.h"
#include "meta.h"
#include "bash_terminal.h"
#include "bash_utils.h"
#include "meta/texture.h"
#include "meta/shader.h"

namespace assembler::generators
{
	class meta
	{
	public:
		meta()
		{
			m_Library << std::move(cli::value<psl::string>{}
			.name(("library directory"))
				.command(("directory"))
				.short_command(('d'))
				.hint(("location of the library"))
				.required(true))
				<< std::move(cli::value<psl::string>{}
			.name(("name"))
				.command(("name"))
				.short_command(('n'))
				.hint(("the output name of the library file"))
				.default_value(("resources.metalib")))
				<< std::move(cli::value<psl::string>{}
			.name(("resource directory"))
				.command(("res"))
				.short_command(('r'))
				.hint(("data folder that is the root to generate the library from."))
				.required(true))
				<< std::move(cli::value<bool>{}
			.name(("cleanup"))
				.command(("clean"))
				.hint(("cleanup dangling .meta files that no longer have a corresponding file, and dangling library entries"))
				.default_value(false))
				<< std::move(cli::value<bool>{}
			.name(("absolute"))
				.command(("absolute"))
				.hint(("should the entries be stored as absolute paths instead of relative ones?"))
				.default_value(false));

			m_File << std::move(cli::value<psl::string>{}
			.name(("target"))
				.command(("source"))
				.short_command(('s'))
				.hint(("target file or folder to generate meta for"))
				.required(true))
			<< std::move(cli::value<psl::string>{}
			.name(("type"))
				.command(("type"))
				.short_command(('t'))
				.hint(("type of meta data to generate"))
				.default_value(("META")))
				<< std::move(cli::value<bool>{}
			.name(("recursive"))
				.command(("recursive"))
				.short_command(('r'))
				.hint(("when true, and the source path is a directory, it will recursively go through it")))
				<< std::move(cli::value<bool>{}
			.name(("force"))
				.command(("force"))
				.short_command(('f'))
				.hint(("remove existing and regenerate them from scratch"))) 
				<< std::move(cli::value<bool>{}
			.name(("update"))
				.command(("update"))
				.short_command(('u'))
				.hint(("update is a specialization of force, where everything will be regenerated except the UID")));

			m_Library.callback(std::bind(&assembler::generators::meta::on_library_generate, this, m_Library));
			m_File.callback(std::bind(&assembler::generators::meta::on_meta_generate, this, m_File));
			m_Pack << std::move(cli::value<cli::parameter_pack>(m_Library).name(("library")).command(("library")).short_command(('l')).hint(("library generator")));
			m_Pack << std::move(cli::value<cli::parameter_pack>(m_File).name(("meta")).command(("meta")).short_command(('m')).hint(("meta generator")));
		}

		cli::parameter_pack& pack()
		{
			return m_Pack;
		}

	private:

		void on_library_generate(cli::parameter_pack& pack)
		{
			// -g -l -l -d "d:/Projects\Paradigm\library" -r "d:/Projects\Paradigm\data"
			tools::bash_terminal bt;
			auto lib_dir = pack.get_as_a<psl::string>(("library directory"))->get();
			auto lib_name = pack.get_as_a<psl::string>(("name"))->get();
			auto res_dir = pack.get_as_a<psl::string>(("resource directory"))->get();
			auto cleanup = pack.get_as_a<bool>(("cleanup"))->get();
			auto absolute = pack.get_as_a<bool>(("absolute"))->get();

			size_t relative_position = 0u;

			lib_dir = utility::platform::directory::to_unix(lib_dir);
			if(lib_dir[lib_dir.size() -1] != '/')
				lib_dir += '/';
			res_dir = utility::platform::directory::to_unix(res_dir);
			if(res_dir[res_dir.size() - 1] != '/')
				res_dir += '/';
			const auto max_index = std::min(lib_dir.find_last_of(('/')), res_dir.find_last_of(('/')));
			for(auto i = max_index; i > 0; --i)
			{
				if(psl::string_view(lib_dir.data(), i) == psl::string_view(res_dir.data(), i))
				{
					relative_position = (max_index == i)?i:i-1;
					break;
				}
			}

			if(!utility::platform::directory::exists(res_dir))
			{
				utility::terminal::set_color(utility::terminal::color::RED);
				std::cerr << "resource directory does not exist, so nothing to generate. This was likely unintended?" << std::endl;
				utility::terminal::set_color(utility::terminal::color::WHITE);
				return;
			}

			if(!utility::platform::file::exists(lib_dir + lib_name))
			{
				utility::platform::file::write(lib_dir + lib_name, "");
				utility::bash::echo(bt, ("file not found, created one."));
			}

			//psl::string content{(!regenerate) ? utility::bash::file::read(bt, lib_dir, lib_name).value_or(_T("")) : _T("")};

			auto all_files = utility::platform::directory::all_files(res_dir, true);
			// remove all files that don't end in .meta
			psl::string meta_ext = "." + ::meta::META_EXTENSION;
			all_files.erase(std::remove_if(std::begin(all_files), std::end(all_files), [&bt, &meta_ext](const psl::string_view& file)
				{
					return (file.size() >= meta_ext.size())? file.substr(file.size() - meta_ext.size()) != meta_ext:true;
				}), std::end(all_files));
			
			//auto all_files = utility::bash::directory::all_items(bt, res_dir, ("*.") + psl::from_string8_t(::meta::META_EXTENSION));

			psl::cout << "checking for dangling meta files..." << std::endl;
			auto it = std::remove_if(std::begin(all_files), std::end(all_files), [&bt](const psl::string_view& file)
						   {
							   return !utility::platform::file::exists(file.substr(0, file.find_last_of(('.'))));
									 });
			std::vector<psl::string> dangling_files{it, std::end(all_files)};
			
			for(const auto& file : dangling_files)
			{
				if(!utility::platform::file::erase(file))
				{
					utility::terminal::set_color(utility::terminal::color::RED);
					psl::cerr << psl::to_platform_string("could not delete '" + file + "'\n");
					utility::terminal::set_color(utility::terminal::color::WHITE);
				}
			}
			psl::cout << "deleted " << utility::to_string(dangling_files.size()).c_str() << " dangling files." << std::endl;

			all_files.erase(it, std::end(all_files));
			psl::string content;
			serialization::serializer s;
			for(const auto& file : all_files)
			{
				if(auto file_content = utility::platform::file::read(file); file_content)
				{
					format::container cont{psl::to_string8_t(file_content.value())};
					psl::string UID;

					::meta::file* metaPtr = nullptr;
					try
					{
					
					if(!s.deserialize<serialization::decode_from_format>(metaPtr, cont) || !metaPtr)
					{
						psl::cout << "error: could not decode the meta file at: " << file.c_str() << std::endl;					
						continue;
					}
					}
					catch(...)
					{
						debug_break();
					}

					UID = metaPtr->ID().to_string();
					delete(metaPtr);
					auto file_unix = utility::platform::directory::to_unix(file);

					psl::string final_filepath{file_unix.substr(0, file_unix.find_last_of(('.')))};
					psl::string final_metapath{file_unix};
					if(!absolute)
					{
						final_filepath = utility::platform::directory::to_generic(std::filesystem::relative(final_filepath, lib_dir).string());
						final_metapath = utility::platform::directory::to_generic(std::filesystem::relative(final_metapath, lib_dir).string());
					}

					bt.exec(("date -r \"") + file_unix.substr(0, file_unix.find_last_of(('.'))) + ("\" +%s"));
					psl::string time = bt.read(false);
					bt.exec(("date -r \"") + file_unix + ("\" +%s"));
					psl::string metatime = bt.read(false);
					content += ("[UID=") + UID + ("][PATH=") + final_filepath + ("][METAPATH=") + final_metapath + ("][TIME=") + time.substr(0, time.size() - 1) + ("][METATIME=") + metatime.substr(0, metatime.size() - 1) + ("]\n");
				}
			}

			utility::platform::file::write(lib_dir + lib_name, psl::string_view(content.data(), content.size() -1));
			psl::cout << "wrote out a new meta library at: '" << psl::to_platform_string(lib_dir + lib_name) << ("'\n");
			
		}

		void on_meta_generate(cli::parameter_pack& pack)
		{
			tools::bash_terminal bt;
			// -g -l -m -s "c:\\Projects\Paradigm\data\should_see_this\New Text Document.txt" -t "TEXTURE_META"
			// -g -l -m -s D:\Projects\Paradigm\data\textures -r -t "TEXTURE_META"
			auto source_path = pack.get_as_a<psl::string>(("target"))->get();
			auto meta_type = pack.get_as_a<psl::string>(("type"))->get();
			auto recursive_search = pack.get_as_a<bool>(("recursive"))->get();
			auto force_regenerate = pack.get_as_a<bool>(("force"))->get();
			auto update = pack.get_as_a<bool>(("update"))->get();

			if(update)
				force_regenerate = update;

			if(source_path.size() == 0)
			{
				utility::terminal::set_color(utility::terminal::color::RED);
				psl::cerr << "error: the input source path did not contain any characters." << std::endl;
				utility::terminal::set_color(utility::terminal::color::WHITE);
				return;
			}

			source_path = utility::platform::file::to_generic(source_path);
			std::vector<psl::string> all_files;

			if(source_path[source_path.size() -1] == '*')
			{
				source_path = source_path.substr(0, source_path.find_last_not_of('*'));
			}

			const psl::string meta_extension = "." + ::meta::META_EXTENSION;

			if(utility::platform::directory::is_directory(source_path))
			{
				all_files = utility::platform::directory::all_files(source_path, recursive_search);
				// remove all those with meta extensions
				all_files.erase(std::remove_if(std::begin(all_files), std::end(all_files), [meta_extension](const psl::string& file)
						{ 
							return (file.size() < meta_extension.size())
								? false: file.substr(file.size() - meta_extension.size()) == meta_extension;
						}), std::end(all_files));
			}
			else
			{
				all_files.emplace_back(source_path);
			}

			if(!force_regenerate)
			{
				// remove all those with existing meta files
				all_files.erase(std::remove_if(std::begin(all_files), std::end(all_files), [meta_extension](const psl::string& file)
											   {
												   return utility::platform::file::exists(file + meta_extension);
											   }), std::end(all_files));
			}

			for(const auto& file : all_files)
			{
				auto id = utility::crc64(psl::to_string8_t(meta_type));
				if(auto it = serialization::accessor::polymorphic_data().find(id); it != serialization::accessor::polymorphic_data().end())
				{
					::meta::file* target = (::meta::file*)((*it->second->factory)());

					UID uid = UID::generate();
					serialization::serializer s;

					if(utility::platform::file::exists(file + meta_extension) && update)
					{
						::meta::file* original = nullptr;
						s.deserialize<serialization::decode_from_format>(original, file + meta_extension);
						uid = original->ID();
					}
					
					format::container cont;
					s.serialize<serialization::encode_to_format>(target, cont);

					auto metaNode = cont.find("META");
					auto node = cont.find(metaNode.get(), "UID");
					cont.remove(node.get());					
					cont.add_value(metaNode.get(), "UID", utility::to_string(uid));

					auto unix_filepath = utility::platform::file::to_unix(file);
					auto unix_dir = unix_filepath.substr(0, unix_filepath.find_last_of(('/')));
					auto filename = unix_filepath.substr(unix_filepath.find_last_of(('/'))+1);

					utility::platform::file::write(unix_dir + "/" + filename + (".") + psl::from_string8_t(::meta::META_EXTENSION), psl::from_string8_t(cont.to_string()));
				}
				else
				{
					utility::terminal::set_color(utility::terminal::color::RED);
					psl::cerr << psl::to_platform_string("error: could not deduce the polymorphic type from the given key '" + meta_type +"'") << std::endl;
					psl::cerr << ("  either the given key was incorrect, or the type was not registered to the assembler.") << std::endl;
					utility::terminal::set_color(utility::terminal::color::WHITE);
					continue;
				}
			}
		}
		cli::parameter_pack m_Pack;
		cli::parameter_pack m_Library;
		cli::parameter_pack m_File;
	};
}
//const uint64_t core::meta::texture::polymorphic_identity{serialization::register_polymorphic<core::meta::texture>()};
//const uint64_t core::meta::shader::polymorphic_identity{serialization::register_polymorphic<core::meta::shader>()};
