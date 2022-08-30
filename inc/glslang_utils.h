#pragma once
#include "psl/ustring.hpp"
#include <optional>

// similarly to the bash terminal, this one invokes glslangvalidator
// glslangvalidator needs to be present in the nearby folder structure however so it can be invoked.
namespace tools
{
	class glslang
	{
	private:

	public:
	  static constexpr psl::string_view type_str[]{"vert", ("tesc"), ("tese"), ("geom"), ("frag"), ("comp")};
		enum class type : uint8_t
		{
			unknown = 100,
			vert = 0,
			tesc = 1,
			tese = 2,
			geom = 3,
			frag = 4,
			comp = 5
		};
		glslang() = delete;
		~glslang() = delete;
		glslang(const glslang&) = delete;
		glslang(glslang&&) = delete;
		glslang& operator=(const glslang&) = delete;
		glslang& operator=(glslang&&) = delete;

		static bool compile(psl::string_view compiler_location, psl::string_view source,
								  psl::string_view outputfile, type type, bool optimize = false, std::optional<size_t> gles_version = std::nullopt);
	};
}
