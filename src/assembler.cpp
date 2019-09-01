// assembler.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include "psl/string_utils.h"
#include "psl/ustring.h"
#include <array>

#include <windows.h>
#include <stack>

#include <io.h>
#include <fcntl.h>
#include "cli/value.h"
#include "generators/shader.h"
#include "generators/models.h"
#include "generators/meta.h"

using psl::cli::pack;
using psl::cli::value;

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

static psl::string_view get_input()
{
	static psl::string8_t input(4096, ('\0'));
	std::memset(input.data(), ('\0'), sizeof(psl::platform::char_t) * input.size());
	std::cin.getline(input.data(), input.size() - 1);

#if WIN32
	static psl::string override;
	override = psl::to_string8_t(input);
	return {override.data(), psl::strlen(override.data())};
#else
	return {input.data(), psl::strlen(input.data())};
#endif
}


template <typename T>
void advance_impl(std::reference_wrapper<T>& target, std::intptr_t count)
{
	using type = std::remove_const_t<T>;
	// const_cast<std::reference_wrapper<type>&>(target) = *((type*)(&target.get()) + count);
	target = *((type*)&target.get() + count);
}

template <typename... Ts>
void advance_tuple(std::tuple<Ts...>& target, std::uintptr_t count)
{
	(advance_impl(std::get<Ts>(target), count), ...);
}


int main(int argc, char* argv[])
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);

	std::tuple<std::vector<int>, std::vector<float>> ta1 = {{10, -5}, {5.0f, -9.6f}};

	std::tuple<std::reference_wrapper<int>, std::reference_wrapper<float>> t3{std::get<0>(ta1)[0], std::get<1>(ta1)[0]};
	//advance_tuple(t3, 1);
	// t3	 = std::tuple<int&, float&>(advance(std::get<0>(t3), 1), advance(std::get<1>(t3), 1));
	//auto x = std::get<0>(t3);

	std::tuple<int*, float*> t5{&std::get<0>(ta1)[0], &std::get<1>(ta1)[0]};

	std::tuple<int&, float&>& t4 = *reinterpret_cast<std::tuple<int&, float&>*>(&t5);
	std::get<0>(t4) += 5;
	//return x;


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

	// psl::string x2 = _T("Árvíztűrő tükörfúrógép");
	// psl::string x3 = _T("кошка 日本国 أَبْجَدِيَّة عَرَبِيَّة‎中文");
	// psl::string x4 = _T("你爱我");
	//_setmode(_fileno(stdout), _O_U16TEXT);


	// psl::cout << x2 << _T(" ") << x3 << " " << x4 << std::endl;

	std::cout << _T("welcome to assembler, use -h or --help to get information on the commands.\nyou can also pass ")
				 _T("the specific command (or its chain) after --help to get more information of that specific ")
				 _T("command, such as '--help generate shader'.")
			  << std::endl;

	assembler::generators::shader shader_gen{};
	assembler::generators::models model_gen{};
	assembler::generators::meta meta_gen{};

	psl::cli::pack generator_pack{
		value<pack>{"shader", "glsl to spir-v compiler", {"shader", "s"}, std::move(shader_gen.pack())},
		value<pack>{"model", "model importer", {"models", "m"}, std::move(model_gen.pack())},
		value<pack>{"library", "meta library and file generator", {"meta"}, std::move(meta_gen.pack())}};

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
