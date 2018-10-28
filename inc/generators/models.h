#pragma once
#include "cli_pack.h"

namespace assembler::generators
{
	class models
	{
	public:
		models();
		~models() = default;

		models(const models&) = delete;
		models(models&&) = delete;
		models& operator=(const models&) = delete;
		models& operator=(models&&) = delete;


		cli::parameter_pack& pack()
		{
			return m_Pack;
		}
	private:
		void on_invoke(cli::parameter_pack& pack);

		cli::parameter_pack m_Pack;
	};
}
