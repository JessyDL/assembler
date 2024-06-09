#pragma once
#include "importers/importer.hpp"

namespace assembler::importer {
class model_t : public importer_base_t {
  public:
	struct options_t {
		enum class axis_t {
			xyz = 1,
			xzy = 2,
			yxz = 3,
			yzx = 4,
			zxy = 5,
			zyx = 6,
		};
		bool tangents {true};
		bool normals {true};
		bool snormals {true};
		bool uvs {true};
		bool optimize {true};
		bool left_handed {true};
		bool flip_uvs {false};
		bool flip_winding {false};
		bool flatten {false};
		bool binary_output {false};
		axis_t axis {axis_t::xzy};
	};

	model_t(options_t options = {}) : m_Options(options), importer_base_t() {}
	auto import(std::filesystem::path const& file) -> importer_result_t override;

  private:
	options_t m_Options;
};
}	 // namespace assembler::importer