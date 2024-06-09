#pragma once
#include "importers/importer.hpp"

namespace assembler::importer {
class shader_t : public importer_base_t {
  public:
	auto import(std::filesystem::path const& file) -> importer_result_t override;

	private:
		bool m_Optimize {false};
		size_t m_GlesVersion {310};
};
}	 // namespace assembler::importer