// assembler.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include "string_utils.h"
#include "ustring.h"
#include <array>
#include <windows.h>
#include <stack>

#include "bash_terminal.h"
#include "cli_pack.h"
#include "generators/header_generator.h"
#include "generators/meta.h"
#include "generators/shader.h"
#include "generators/models.h"

#include <io.h>
#include <fcntl.h>

#include "terminal_utils.h"

BOOL CtrlHandler(DWORD fdwCtrlType)
{
	switch(fdwCtrlType)
	{
	// Handle the CTRL-C signal.
	case CTRL_C_EVENT:
		// entry.pop();
		psl::cout << "^C" << std::endl;
		return (TRUE);

	// CTRL-CLOSE: confirm that the user wants to exit.
	case CTRL_CLOSE_EVENT:
		psl::cout << "exiting..." << std::endl;
		return (TRUE);

	// Pass other signals to the next handler.
	case CTRL_BREAK_EVENT: return FALSE;

	case CTRL_LOGOFF_EVENT: return FALSE;

	case CTRL_SHUTDOWN_EVENT: return FALSE;

	default: return FALSE;
	}
}

static psl::string_view get_input()
{
	static psl::platform_string input(4096, ('\0'));
	std::memset(input.data(), ('\0'), sizeof(psl::platform_char_t) * input.size());
	psl::cin.getline(input.data(), input.size() - 1);

#if WIN32
	static psl::string override;
	override = psl::from_platform_string(input);
	return {override.data(), psl::strlen(override.data())};
#else
	return {input.data(), psl::strlen(input.data())};
#endif
}

int main(int argc, char* argv[])
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);

	SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);

	std::wstring res = L"Стоял";
	cli::parameter_pack pack;
	cli::parameter_pack generator_pack;

	assembler::generators::header header_pack;
	assembler::generators::meta meta_pack;
	assembler::generators::shader shader_pack;
	assembler::generators::models models_pack;
	generator_pack << std::move(cli::value<cli::parameter_pack>{header_pack.pack()}
									.name(("header"))
									.command(("header"))
									.short_command(('h'))
									.hint(("generates a header file")))
				   /* << std::move(cli::value<cli::parameter_pack>{material_pack}
									 .name(_T("material"))
									 .command(_T("material"))
									 .short_command(_T('m'))
									 .hint(_T("generates a material file")))*/
					<< std::move(cli::value<cli::parameter_pack>{shader_pack.pack()}
									 .name(("shader"))
									 .command(("shader"))
									 .short_command(('s'))
									.hint(("generates a shader file")))
				   << std::move(cli::value<cli::parameter_pack>{meta_pack.pack()}
									.name(("meta library"))
									.command(("meta"))
									.short_command(('l'))
									.hint("generates a meta library file"))
				   << std::move(cli::value<cli::parameter_pack>{models_pack.pack()}
									.name("geometry generator")
									.command(("geometry"))
									.short_command(('g'))
									.hint(("generates a pgf file from the inputted model file")));
	bool bHelp = false;
	bool bExit = false;


	CONSOLE_FONT_INFOEX cfi;
	cfi.cbSize		 = sizeof cfi;
	cfi.nFont		 = 0;
	cfi.dwFontSize.X = 0;
	cfi.dwFontSize.Y = 14;
	cfi.FontFamily   = FF_MODERN;
	cfi.FontWeight   = FW_NORMAL;
	wcscpy(cfi.FaceName, L"Consolas");
	SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &cfi);

	// psl::string x2 = _T("Árvíztűrő tükörfúrógép");
	// psl::string x3 = _T("кошка 日本国 أَبْجَدِيَّة عَرَبِيَّة‎中文");
	// psl::string x4 = _T("你爱我");
	_setmode(_fileno(stdout), _O_U16TEXT);


	// psl::cout << x2 << _T(" ") << x3 << " " << x4 << std::endl;

	pack << std::move(cli::value<cli::parameter_pack>{generator_pack}
						  .name(("generate"))
						  .command(("generate"))
						  .short_command(('g'))
						  .hint(("generates a file")))
		 << std::move(cli::value<bool>{}
						  .name(("quit"))
						  .command(("quit"))
						  .short_command(('q'))
						  .hint(("quits the application"))
						  .default_value(false)
						  .bind(bExit))
		 << std::move(cli::value<bool>{}
						  .name(("help"))
						  .command(("help"))
						  .short_command(('h'))
						  .hint(("prints the help, and usage information"))
						  .default_value(false)
						  .bind(bHelp));

	{
		psl::cout << _T("welcome to assembler, use -h or --help to get information on the commands.") << std::endl;
		psl::string arg;


		for(int i = 1; i < argc; ++i)
		{
			arg += psl::string(psl::from_string8_t(psl::string8_t(argv[i]))) + (' ');
		}

		if(argc > 1) psl::cout << psl::to_platform_string("received the command: "+ arg) << std::endl;
		std::vector<psl::string_view> commands = utility::string::split(arg, ("|"));

		for(const auto& c : commands)
		{
			if(c.empty()) continue;
			psl::cout << psl::to_platform_string("executing: "  + c ) << std::endl;
			pack.parse(c);
		}
	}

	while(!bExit)
	{
		std::vector<psl::string_view> commands = utility::string::split(get_input(), ("|"));
		for(const auto c : commands)
		{
			if(!pack.parse(c) || bExit) break;
			if(bHelp)
			{
				utility::terminal::set_color(utility::terminal::color::GREEN);
				psl::cout << psl::to_platform_string(pack.help());
				utility::terminal::set_color(utility::terminal::color::WHITE);
			}
		}
	}


	return 0;
}
