#pragma once
#include "ustring.h"
#include "stdafx_psl.h"
#include <vector>
#include <chrono>
namespace tools
{
	class bash_terminal
	{
	public:
		bash_terminal(uint64_t bufferSize = 4096u, std::optional<psl::string> params = std::nullopt);
		~bash_terminal();

		void exec(psl::string_view command);

		psl::string read(bool bOutputImmediately = true, std::chrono::duration<int64_t, std::micro> timeout = std::chrono::duration<int64_t, std::micro>::max(), std::chrono::duration<int64_t, std::micro> interval = std::chrono::duration<int64_t, std::micro>{10});
		psl::string read_error(bool bOutputImmediately = true, std::chrono::duration<int64_t, std::micro> timeout = std::chrono::duration<int64_t, std::micro>::max(), std::chrono::duration<int64_t, std::micro> interval = std::chrono::duration<int64_t, std::micro>{10});
		psl::string async_read();

	private:
	#ifdef PLATFORM_WINDOWS
		struct pipe
		{
			void close();
			HANDLE write;
			HANDLE read;
		};
		pipe m_In, m_Out, m_Error;
		//HANDLE pipin_w, pipin_r, pipout_w, pipout_r, piperror_w, piperror_r;
		PROCESS_INFORMATION pi;
	#endif
		std::vector<psl::string8::char_t> m_Buffer;
	};
}
