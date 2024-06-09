#pragma once
#include "cli/value.h"
#include "psl/ustring.hpp"

#include "psl/serialization/property.hpp"
#include "psl/serialization/serializer.hpp"

namespace assembler {
class pathstring;
}	 // namespace assembler

namespace assembler::generators {
class project {
	template <typename T>
	using cli_value = psl::cli::value<T>;

  public:
	project() = default;

	auto pack() -> psl::cli::pack {
	  return psl::cli::pack {std::bind(&project::on_generate, this, std::placeholders::_1),
		  cli_value<psl::string> {"input", "The project file to use", {"input", "i"}, "", false},
		  cli_value<bool> {"models", "Explicitly set the importer to import models, disables other importers by default", {"models"}, false, true},
		  cli_value<bool> {"shaders", "Explicitly set the importer to import shaders, disables other importers by default", {"shaders"}, false, true},
		};
	}

  private:
	void on_generate(psl::cli::pack& pack);
	psl::string m_ProjectFile {};
};
}	 // namespace assembler::generators
