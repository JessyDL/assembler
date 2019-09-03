// assembler.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include "psl/string_utils.h"
#include "psl/ustring.h"
#include <array>

#ifdef WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

#include "cli/value.h"
#include "generators/shader.h"
#include "generators/models.h"
#include "generators/meta.h"

using psl::cli::pack;
using psl::cli::value;

#ifdef WIN32
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	switch(fdwCtrlType)
	{
	// Handle the CTRL-C signal.
	case CTRL_C_EVENT:
		// entry.pop();
		std::cout << "^C" << std::endl;
		return (TRUE);

	// CTRL-CLOSE: confirm that the user wants to exit.
	case CTRL_CLOSE_EVENT: std::cout << "exiting..." << std::endl; return (TRUE);

	// Pass other signals to the next handler.
	case CTRL_BREAK_EVENT: return FALSE;

	case CTRL_LOGOFF_EVENT: return FALSE;

	case CTRL_SHUTDOWN_EVENT: return FALSE;

	default: return FALSE;
	}
}
#endif

static psl::string_view get_input()
{
	static psl::string8_t input(4096, ('\0'));
	std::memset(input.data(), ('\0'), sizeof(psl::platform::char_t) * input.size());
	std::cin.getline(input.data(), input.size() - 1);

	return {input.data(), psl::strlen(input.data())};
#if WIN32
	static psl::string override;
	override = psl::to_string8_t(input);
	return {override.data(), psl::strlen(override.data())};
#else
	return {input.data(), psl::strlen(input.data())};
#endif
}

int main(int argc, char* argv[])
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);


#ifdef WIN32
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);

	// std::wstring res = L"Стоял";


	CONSOLE_FONT_INFOEX cfi;
	cfi.cbSize		 = sizeof cfi;
	cfi.nFont		 = 0;
	cfi.dwFontSize.X = 0;
	cfi.dwFontSize.Y = 14;
	cfi.FontFamily   = FF_MODERN;
	cfi.FontWeight   = FW_NORMAL;
	wcscpy_s(cfi.FaceName, L"Consolas");
	SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &cfi);

	/*psl::string x2 = _T("Árvíztűrő tükörfúrógép");
	psl::string x3 = _T("кошка 日本国 أَبْجَدِيَّة عَرَبِيَّة‎中文");
	psl::string x4 = _T("你爱我");*/
	//_setmode(_fileno(stdout), _O_U16TEXT);
	//_setmode(_fileno(stdout), _O_U8TEXT);
#endif

	std::cout << _T("welcome to assembler, use -h or --help to get information on the commands.\nyou can also pass ")
				 _T("the specific command (or its chain) after --help to get more information of that specific ")
				 _T("command, such as '--help generate shader'.")
			  << std::endl;

	assembler::generators::shader shader_gen{};
	assembler::generators::models model_gen{};
	assembler::generators::meta meta_gen{};

	psl::cli::pack generator_pack{
		value<pack>{"shader", "glsl to spir-v compiler", {"shader", "s"}, std::move(shader_gen.pack())},
		value<pack>{"model", "model importer", {"models", "g"}, std::move(model_gen.pack())},
		value<pack>{"library", "meta library generator", {"library", "l"}, meta_gen.library_pack()},
		value<pack>{"meta", "meta file generator", {"meta", "m"}, meta_gen.meta_pack()}};

	psl::cli::pack root{value<bool>{"exit", "quits the application", {"exit", "quit", "q"}, false},
						value<pack>{"generator", "generator for various data files", {"generate", "g"}, generator_pack}

	};


	while(!root["exit"]->as<bool>().get())
	{
		psl::array<psl::string_view> commands = utility::string::split(get_input(), ("|"));
		root.parse(commands);
	}
	return 0;
}
