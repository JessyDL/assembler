#pragma once
#include "psl/ustring.hpp"

#include "psl/serialization/property.hpp"
#include "psl/serialization/serializer.hpp"

namespace assembler::data {
class project_t {
	friend class psl::serialization::accessor;

	struct meta_mapping_t {
		friend class psl::serialization::accessor;

	  public:
		struct extension {
			friend class psl::serialization::accessor;

		  public:
			template <typename S>
			void serialize(S& s) {
				s << meta << extensions;
			}

			static constexpr char const serialization_name[6] {"ENTRY"};
			psl::serialization::property<"META", psl::string> meta;
			psl::serialization::property<"EXTENSIONS", psl::array<psl::string>> extensions;
		};

		struct environment {
			friend class psl::serialization::accessor;

		  public:
			template <typename S>
			void serialize(S& s) {
				s << name << extensions;
			}

			static constexpr char const serialization_name[12] {"ENVIRONMENT"};
			psl::serialization::property<"NAME", psl::string> name {};
			psl::serialization::property<"EXTENSIONS", psl::array<psl::string>> extensions {};
		};

		template <typename S>
		void serialize(S& s) {
			s << m_Mappings << m_Environments;
		}
		static constexpr char const serialization_name[6] {"TABLE"};

		psl::array<psl::string> mapping(psl::string_view type) const noexcept {
			psl::array<psl::string> res;
			for(auto const& entry : m_Mappings.value) {
				if(entry.meta.value == type) {
					res.insert(res.end(), entry.extensions.value.begin(), entry.extensions.value.end());
				}
			}
			return res;
		}

		psl::string meta(psl::string_view extension) const noexcept {
			if(extension.starts_with('.'))
				extension = extension.substr(1);

			for(auto const& entry : m_Mappings.value) {
				for(auto const& ext : entry.extensions.value) {
					if(ext == extension)
						return entry.meta.value;
				}
			}
			return "META";
		}

		std::unordered_map<psl::string, psl::array<psl::string>> environments() const noexcept {
			std::unordered_map<psl::string, psl::array<psl::string>> res;
			for(auto const& entry : m_Environments.value) {
				for(auto const& ext : entry.extensions.value) {
					res[ext].emplace_back(entry.name.value);
				}
			}
			return res;
		}

		psl::serialization::property<"MAPPING", psl::array<extension>> m_Mappings = std::initializer_list<extension> {
		  {.meta = "TEXTURE_META", .extensions = std::initializer_list<psl::string> {"dds", "ktx"}},
		  {.meta = "SHADER_META",
		   .extensions =
			 std::initializer_list<psl::string> {"spv", "gles", "vert", "frag", "geom", "tess", "tesc", "comp"}},
		};
		psl::serialization::property<"ENVIRONMENTS", psl::array<environment>> m_Environments =
		  std::initializer_list<environment> {
			{.name = "gles", .extensions = std::initializer_list<psl::string> {"gles"}},
			{.name = "vulkan", .extensions = std::initializer_list<psl::string> {"spv"}},
		  };
	};

  public:
	static constexpr psl::string_view DEFAULT_EXTENSION = "ppf";
	static constexpr psl::string_view DEFAULT_NAME		= "project";

	auto version() const noexcept { return m_Version.value; }
	auto const& project_directory() const noexcept { return m_ProjectDirectory; }
	auto const& source_directory() const noexcept { return m_SourceDirectory.value; }
	auto const& build_directory() const noexcept { return m_BuildDirectory.value; }
	auto const& library_path() const noexcept { return m_LibraryPath.value; }
	auto const& meta_mapping() const noexcept { return m_MetaMapping.value; }
	auto const& graphics_backends() const noexcept { return m_GraphicsBackends.value; }

	void version(std::uint32_t version) noexcept { m_Version.value = version; }
	void source_directory(psl::string_view source_directory) noexcept { m_SourceDirectory.value = source_directory; }
	void build_directory(psl::string_view build_directory) noexcept { m_BuildDirectory.value = build_directory; }
	void library_path(psl::string_view library_path) noexcept { m_LibraryPath.value = library_path; }
	void meta_mapping(meta_mapping_t meta_mapping) noexcept { m_MetaMapping.value = meta_mapping; }
	void project_directory(psl::string_view project_directory) noexcept { m_ProjectDirectory = project_directory; }

  private:
	/// \brief serialization method to be used by the serializer when writing this container to the disk.
	/// \param[in] serializer the serialization object, consult the serialization namespace for more information.
	template <typename S>
	void serialize(S& serializer) {
		serializer << m_Version << m_SourceDirectory << m_BuildDirectory << m_LibraryPath << m_MetaMapping
				   << m_GraphicsBackends;
	};

	/// \brief serialization name to be used by the serializer when writing and reading this container to and from
	/// disk.
	static constexpr psl::string8::view serialization_name {"PROJECT"};

	psl::serialization::property<"VERSION", std::uint32_t> m_Version {1};
	psl::serialization::property<"SOURCE_DIRECTORY", psl::string> m_SourceDirectory {"./source/"};
	psl::serialization::property<"BUILD_DIRECTORY", psl::string> m_BuildDirectory {"./data/"};
	psl::serialization::property<"META_LIBRARY", psl::string> m_LibraryPath {"./library/resources.metalib"};
	psl::serialization::property<"META_MAPPING", meta_mapping_t> m_MetaMapping {};
	psl::serialization::property<"GRAPHICS_BACKENDS", psl::array<psl::string>> m_GraphicsBackends =
	  psl::array<psl::string> {{"vulkan", "gles"}};

	psl::string m_ProjectDirectory;
};

}	 // namespace assembler::data