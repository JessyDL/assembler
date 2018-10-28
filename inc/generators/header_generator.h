#pragma once
#include "cli_pack.h"
#include "bash_terminal.h"
#include "bash_utils.h"
#include "terminal_utils.h"

namespace assembler::generators
{
	class header
	{
	public:
		header()
		{
			m_Pack << std::move(cli::value<psl::string>{}
			.name("input directory")
				.command("directory")
				.short_command('d')
				.hint("root directory where to look")
				.required(true))
				<< std::move(cli::value<psl::string>{}
			.name("filename")
				.command("filename")
				.short_command('n')
				.hint("filename to select")
				.default_value("header_info"))
				<< std::move(cli::value<psl::string>{}
			.name("output directory")
				.command("output")
				.short_command(('o'))
				.hint(("output folder, defaults to the root folder if empty"))
				.bind_default_value(m_Pack.get_as_a<psl::string>(("input directory"))->get()))
				<< std::move(cli::value<bool>{}
			.name(("force"))
				.command(("force"))
				.hint(("forcibly generate the file"))
				.default_value(false))
				<< std::move(cli::value<psl::string>{}
			.name(("app name"))
				.command(("app_name"))
				.hint(("the name of the application"))
				.default_value(("Paradigm Engine")));

			m_Pack.callback(std::bind(&assembler::generators::header::on_generate_header, this, m_Pack));
		}
		cli::parameter_pack& pack()
		{
			return m_Pack;
		}


	private:
		void on_generate_header(cli::parameter_pack& pack)
		{
			tools::bash_terminal BT;

			auto folder = pack.get_as_a<psl::string>(("input directory"))->get();
			auto filename = pack.get_as_a<psl::string>(("filename"))->get();
			auto force = pack.get_as_a<bool>(("force"))->get();
			auto output = pack.get_as_a<psl::string>(("output directory"))->get();
			auto app_name = pack.get_as_a<psl::string>(("app name"))->get();

			psl::string folder_seperator = ("/");
			psl::string newline = ("\n");
			psl::string file_location = output + filename + (".h");
			psl::string git_dir = folder + (".git");

			if(!utility::bash::directory::exists(BT, folder))
			{
				utility::terminal::set_color(utility::terminal::color::RED);
				psl::cerr << psl::to_platform_string("the folder '" + folder + "' does not exist\n");
				psl::cerr << psl::to_platform_string("the folder '" + utility::platform::directory::to_unix(folder) + "' does not exist\n");
				utility::terminal::set_color(utility::terminal::color::WHITE);
				return;
			}

			utility::bash::directory::cd(BT, folder);
			BT.exec(("git tag -l --sort=-v:refname"));

			struct git_version
			{
				psl::string major;
				psl::string minor;
				psl::string patch;

				git_version(psl::string full)
				{
					auto res = utility::string::split(full, ("."));
					major = res[0];
					minor = res[1];

					auto end = res[2].find_first_of((" \r\n"));
					if(end == psl::string::npos) end = res[2].size();
					patch = res[2].substr(0, end);
				}
			};

			auto version = git_version(BT.read(false));
			BT.exec(("git rev-parse HEAD"));
			auto sha1 = BT.read(false);

			sha1.resize(sha1.find_last_not_of('\n') + 1); // get rid of newline

			if(!force)
			{
				bool upo_date = true;
				auto res = utility::bash::file::read(BT, output, filename + (".h"));
				if(res)
				{
					auto& orig_file = res.value();
					if(auto find = orig_file.find(("#define VERSION_SHA1")); find < orig_file.size())
					{
						auto sha_start = orig_file.find(("\""), find)+1;
						auto sha_end = orig_file.find(("\""), sha_start);
						psl::string_view orig_sha{&orig_file[sha_start], sha_end - sha_start};
						if(orig_sha != sha1)
							upo_date = false;
					}
					if(auto find = orig_file.find(("#define VERSION_MAJOR")); find < orig_file.size())
					{
						auto start = orig_file.find_first_of(("0123456789"), find);
						auto end = orig_file.find_last_of(("0123456789"), start) +1;
						psl::string_view major{&orig_file[start], end - start};
						if(major != version.major)
							upo_date = false;
					}
					if(auto find = orig_file.find(("#define VERSION_MINOR")); find < orig_file.size())
					{
						auto start = orig_file.find_first_of(("0123456789"), find);
						auto end = orig_file.find_last_of(("0123456789"), start)+1;
						psl::string_view v{&orig_file[start], end - start};
						if(v != version.minor)
							upo_date = false;
					}
					if(auto find = orig_file.find(("#define VERSION_PATCH")); find < orig_file.size())
					{
						auto start = orig_file.find_first_of(("0123456789"), find);
						auto end = orig_file.find_last_of(("0123456789"), start)+1;
						psl::string_view v{&orig_file[start], end - start};
						if(v != version.patch)
							upo_date = false;
					}
				}

				if(upo_date)
				{
					utility::bash::echo(BT, ("file at: '") + file_location + ("' already up to date."));
					return;
				}
			}

			// -g -h -d "/mnt/c/Projects/Paradigm/" -o "/mnt/c/Projects/Paradigm/core/inc/"
			psl::string content;
			content += ("// generated header file don't edit.") + newline + newline;
			content += ("#define VERSION_MAJOR ") + version.major + newline;
			content += ("#define VERSION_MINOR ") + version.minor + newline;
			content += ("#define VERSION_PATCH ") + version.patch + newline;
			content += ("#define VERSION_SHA1 \"") + sha1 + ("\"") + newline;
			content += ("#define VERSION \"") + version.major + (".") + version.minor + (".") + version.patch + (".") +
				sha1 + ("\"") + newline + newline;
			content += ("const static psl::string8 APPLICATION_NAME {\"") + app_name + ("\"};") + newline;
			content += ("const static psl::string8 APPLICATION_FULL_NAME {\"") + app_name + (" ") + version.major + (".") +
				version.minor + (".") + version.patch + (".") + sha1 + ("\"};");

			if(utility::bash::file::write(BT, output, filename + (".h"), content))
			{
				utility::bash::echo(BT, ("file written at: '") + file_location + ("'"));
			}
		}

		cli::parameter_pack m_Pack;
	};
}
