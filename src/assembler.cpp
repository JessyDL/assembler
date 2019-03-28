// assembler.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include "string_utils.h"
#include "ustring.h"
#include <array>
#include <windows.h>
#include <stack>

#include <io.h>
#include <fcntl.h>
#include "cli/value.h"

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

int main(int argc, char* argv[])
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);

	SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);

	std::wstring res = L"Стоял";
	

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
	
	{
		psl::cout << _T("welcome to assembler, use -h or --help to get information on the commands. ") << res << std::endl;
		psl::string arg;
	}

	psl::cli::value<int> int_v{"int value", { "i", "integer" }, 5};
	auto x = int_v.get_shared();
	*x = 10;
	auto y = int_v.get();
	if(int_v.contains_command("i"))
	{
		return 0;
	}
	return 1;
}
