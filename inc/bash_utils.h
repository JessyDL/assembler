#pragma once
#include "bash_terminal.h"
#include "ustring.h"
#include <algorithm>
#include "platform_utils.h"

namespace utility
{
	namespace bash
	{
		static void echo(tools::bash_terminal& bt, psl::string_view text)
		{
			bt.exec("echo \"" + text + "\"");
			bt.read();
		}

		namespace directory
		{
			static bool exists(tools::bash_terminal& bt, psl::string_view directory)
			{
				if(bt.exec("test -d \"" + utility::platform::directory::to_unix(directory) + "\" && echo \"found\" || echo \"not\" ");
				bt.read(false).substr(0, 3) == "not")
				{
					return false;
				}
				return true;
			}

			static bool cd(tools::bash_terminal& bt, psl::string_view directory)
			{
				auto unix_dir = utility::platform::directory::to_unix(directory);
				if(!exists(bt, unix_dir))
					return false;


				bt.exec("cd \"" + unix_dir + "\"");
				return true;
			}

			static void create(tools::bash_terminal& bt, psl::string_view directory, bool recursive = true)
			{

			}

			static std::vector<psl::string_view> all_items(tools::bash_terminal& bt, psl::string_view directory, std::optional<::psl::string> filter = std::nullopt, bool recursive = true, bool files = true, bool folders = false)
			{
				auto unix_dir = utility::platform::directory::to_unix(directory);
				if(!exists(bt, unix_dir))
					return {};

				::psl::string type_params;
				if(files && !folders)
					type_params = " -type f";
				if(!files && folders)
					type_params = " -type d";

				filter = ((filter) ? " -name '" + filter.value() + "'" : filter);

				//psl::string depth = (!recursive)?_T(" \\( -type d ! -name \"") + unix_dir + _T("\" -prune \\) -o") :_T("");
				::psl::string depth = (!recursive) ? " ! -path \"" + unix_dir + "\" -prune" : "";
				bt.exec("find \"" + unix_dir + "\"" + depth + type_params + filter.value_or("")+ " -print");
				auto res = bt.read(false);
				return utility::string::split(res, "\n");
			}
		}

		namespace file
		{
			static bool exists(tools::bash_terminal& bt, psl::string_view file)
			{
				if(bt.exec("test -f \"" + utility::platform::file::to_unix(file) + "\" && echo \"found\" || echo \"not\" ");
				bt.read(false).substr(0, 3) == "not")
				{
					return false;
				}
				return true;
			}

			// returns the last time this filme has been modified in Unix Epoch/POSIX time
			static std::optional<uint64_t> last_modification(tools::bash_terminal& bt, psl::string_view file)
			{
				if(!exists(bt, file))
					return std::nullopt;

				bt.exec("date -r \"" + file + "\" +%s");
				psl::string time = bt.read(false);
				return utility::from_string<uint64_t>(psl::to_string8_t(time));
			}

			static bool exists(tools::bash_terminal& bt, psl::string_view folder, psl::string_view file)
			{
				auto loc = utility::platform::file::to_unix(folder);
				if(loc[loc.size() - 1] != '/')
					loc += '/';
				loc += file;

				if(bt.exec("test -f \"" + loc + "\" && echo \"found\" || echo \"not\" ");
				bt.read(false).substr(0, 3) == "not")
				{
					return false;
				}
				return true;
			}

			static bool write(tools::bash_terminal& bt, psl::string_view folder, psl::string_view file, psl::string_view content)
			{
				bool success = false;
				bt.exec("temp_buffer_directory = $(pwd)");
				if(utility::bash::directory::cd(bt, folder))
				{
					success = true;
					bt.exec("IFS='' read -r -d '' filecontent << _EOF_\n" + content + "\n_EOF_");
					bt.exec("echo -n \"${filecontent::-1}\" > \"" + file + "\"");
				}
				bt.exec("cd \"$temp_buffer_directory\"");
				return success;
			}

			static bool write(tools::bash_terminal& bt, psl::string_view file, psl::string_view content)
			{
				auto loc = ::psl::string(file);
				auto find = loc.find("\\");
				while(find < loc.size())
				{
					loc.replace(find, 1, "/");
					find = loc.find("\\");
				}

				find = loc.find_last_of("/");
				if(find >= loc.size())
				{
					return write(bt, ".", loc, content);
				}
				return write(bt, loc.substr(0, find), loc.substr(find + 1), content);
			}
			static std::optional<::psl::string> read(tools::bash_terminal& bt, psl::string_view folder, psl::string_view file)
			{
				auto loc = utility::platform::file::to_unix(folder);
				if(loc[loc.size() - 1] != '/')
					loc += '/';
				loc += file;
				if(!exists(bt, loc))
					return std::nullopt;

				bt.exec("IFS='' read -rd '' f <\"" + loc + "\"; echo -n \"${f}\"");
				return bt.read(false);
			}
			static std::optional<::psl::string> read(tools::bash_terminal& bt, psl::string_view file)
			{
				auto loc = ::psl::string(file);
				auto find = loc.find("\\");
				while(find < loc.size())
				{
					loc.replace(find, 1, "/");
					find = loc.find("\\");
				}

				find = loc.find_last_of("/");
				if(find >= loc.size())
				{
					return read(bt, ".", loc);
				}
				return read(bt, loc.substr(0, find), loc.substr(find + 1));
			}
			static bool erase(tools::bash_terminal& bt, psl::string_view file)
			{
				if(!exists(bt, file))
					return false;

				bt.exec("rm -f \"" + file + "\"");
				return true;
			}
		}
	}
}
