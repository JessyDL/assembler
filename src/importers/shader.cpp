#include "importers/shader.hpp"

#include "details/spirv.hpp"
#include <chrono>
#include <filesystem>

#include "stdafx.h"
#include <core/meta/shader.hpp>
#include <psl/platform_utils.hpp>

#if defined(AS_ENABLE_WGSL)
	#include "tint/tint.h"
#endif

class shader_cache_t {
	struct entry_key_t {
		std::filesystem::path path;

		operator std::filesystem::path() const { return path; }
		bool operator==(entry_key_t const& rhs) const { return path == rhs.path; }
	};
	struct entry_t : public entry_key_t {
		struct include_t {
			entry_key_t include;
			size_t index;

			bool operator==(include_t const& rhs) const { return include.path == rhs.include.path; }
			bool operator==(entry_key_t const& rhs) const { return include.path == rhs.path; }
		};
		std::chrono::time_point<std::chrono::system_clock> last_modified;

		psl::string content;
		psl::array<include_t> includes {};
	};

	struct hash_entry_t {
		size_t operator()(entry_key_t const& entry) const { return std::hash<std::filesystem::path> {}(entry.path); }
	};

	auto get_transformed_content(entry_t const& entry) -> std::optional<psl::string> {
		psl::array<entry_key_t> excludes {};
		return get_transformed_content(entry, excludes);
	}
	auto get_transformed_content(entry_t const& entry, psl::array<entry_key_t>& excludes)
	  -> std::optional<psl::string> {
		psl::string content = entry.content;
		size_t offset		= 0;
		for(auto const& include : entry.includes) {
			if(std::find(std::begin(excludes), std::end(excludes), include.include) != std::end(excludes)) {
				continue;
			}
			auto include_entry = get_entry(include.include);
			if(!include_entry) {
				assembler::log->error("error processing '{}': could not find the include '{}'.",
									  entry.path.string(),
									  include.include.path.string());
				return std::nullopt;
			}
			excludes.push_back(include.include);
			auto include_content = get_transformed_content(include_entry.value(), excludes);
			if(!include_content) {
				assembler::log->error("error processing '{}': could not find the include '{}'.",
									  entry.path.string(),
									  include.include.path.string());
				return std::nullopt;
			}
			content.insert(include.index + offset, include_content.value());
			offset += include_content.value().size();
		}
		return content;
	}

	auto get_entry(std::filesystem::path const& key) -> std::optional<entry_t> {
		auto absolute_path = std::filesystem::absolute(key);
		auto entry		   = entry_t {absolute_path};
		auto it			   = m_Entries.find(entry);
		if(it == m_Entries.end()) {
			auto data	  = psl::utility::platform::file::read(absolute_path.string()).value_or("");
			entry.content = data;
			entry.last_modified =
			  std::chrono::clock_cast<std::chrono::system_clock>(std::filesystem::last_write_time(absolute_path));

			if(!parse_includes(entry))
				return std::nullopt;

			m_Entries.insert(entry);
			return entry;
		}
		return *it;
	}

	auto parse_includes(entry_t& entry) -> bool {
		auto pos_include = entry.content.find("#include");

		while(pos_include != psl::string::npos) {
			auto pos_next_token = entry.content.find_first_of("\"'`\n", pos_include);
			if(pos_next_token == psl::string::npos) {
				auto line_number = std::count(entry.content.begin(), entry.content.begin() + pos_include, '\n');
				assembler::log->error(
				  "error processing '{}': could not deduce the #include path at line {}, unexpected end of file",
				  entry.path.string(),
				  line_number);
				return false;
			} else if(entry.content[pos_next_token] == '\n') {
				auto line_number = std::count(entry.content.begin(), entry.content.begin() + pos_include, '\n');
				assembler::log->error(
				  "error processing '{}': could not deduce the #include path at line {}, unexpected newline (expected "
				  "either '\"', ''', or '`')",
				  entry.path.string(),
				  line_number);
				return false;
			}

			auto include_token = entry.content[pos_next_token];

			auto include_start = pos_next_token + 1;
			// we don't allow for multi-line includes, so we include it in the search so we can break if it's not found
			auto include_end = entry.content.find(include_token, include_start);
			auto end_of_line = entry.content.find('\n', include_start);
			if(include_end == psl::string::npos) {
				auto line_number = std::count(entry.content.begin(), entry.content.begin() + pos_include, '\n');
				assembler::log->error(
				  "error processing '{}': could not deduce the #include path at line {}, unexpected end of file",
				  entry.path.string(),
				  line_number);
				return false;
			} else if(end_of_line <= include_end) {
				auto line_number = std::count(entry.content.begin(), entry.content.begin() + pos_include, '\n');
				assembler::log->error(
				  "error processing '{}': could not deduce the #include path at line {}, unexpected end of include "
				  "(expected '{}'). note that multi-line includes are not supported.",
				  entry.path.string(),
				  line_number,
				  include_token);
				return false;
			}

			auto include_path = std::filesystem::absolute(
			  entry.path.parent_path() /
			  std::filesystem::path(entry.content.substr(include_start, include_end - include_start)));

			auto include_entry = get_entry(include_path);
			if(!include_entry) {
				auto line_number = std::count(entry.content.begin(), entry.content.begin() + pos_include, '\n');
				assembler::log->error("error processing '{}': could not find the include '{}' at line {}.",
									  entry.path.string(),
									  include_path.string(),
									  line_number);
				return false;
			}
			entry.content.erase(pos_include, (include_end + 1) - pos_include);
			entry.includes.push_back({include_path, pos_include});

			pos_include = entry.content.find("#include", pos_include);
		}

		return true;
	}

  public:
	auto get(std::filesystem::path const& path) -> std::optional<psl::string> {
		auto absolute_path = std::filesystem::absolute(path);
		auto key		   = entry_t {absolute_path};
		auto it			   = m_Entries.find(key);
		if(it == m_Entries.end()) {
			auto entry	  = entry_t {absolute_path};
			auto data	  = psl::utility::platform::file::read(absolute_path.string()).value_or("");
			entry.content = data;
			entry.last_modified =
			  std::chrono::clock_cast<std::chrono::system_clock>(std::filesystem::last_write_time(absolute_path));

			if(!parse_includes(entry))
				return {};

			m_Entries.insert(entry);
			return get_transformed_content(entry);
		}
		return get_transformed_content(*it);
	}

  private:
	std::unordered_set<entry_t, hash_entry_t> m_Entries;
};

// todo: this should be part of the shader_t importer
shader_cache_t shader_cache;

tools::shader_stage_t shader_stage_from_extension(psl::string_view extension) {
	if(extension == ".vert")
		return tools::shader_stage_t::vert;
	if(extension == ".tesc")
		return tools::shader_stage_t::tesc;
	if(extension == ".tese")
		return tools::shader_stage_t::tese;
	if(extension == ".geom")
		return tools::shader_stage_t::geom;
	if(extension == ".frag")
		return tools::shader_stage_t::frag;
	if(extension == ".comp")
		return tools::shader_stage_t::comp;
	return tools::shader_stage_t::unknown;
}

namespace assembler::importer {
auto shader_t::import(std::filesystem::path const& file) -> importer_result_t {
	if(!std::filesystem::exists(file)) {
		assembler::log->error("error processing '{}': file does not exist.", file.string());
		return {false};
	}

	auto shader_stage = shader_stage_from_extension(file.extension().string());

	if(shader_stage == tools::shader_stage_t::unknown) {
		assembler::log->error("error processing '{}': could not deduce the shader type from the extension.",
							  file.string());
		return {false};
	}

	auto entry = shader_cache.get(file);
	if(!entry) {
		assembler::log->error("error processing '{}': could not read the file, or it has no content.", file.string());
		return {false};
	}

	auto backends = project().graphics_backends();

	auto is_backend_enabled = [&](psl::string_view backend) {
		return std::find(std::begin(backends), std::end(backends), backend) != std::end(backends);
	};

	// todo: this can benefit from some more refining, but for now it's good enough
	std::optional<size_t> gles_version =
	  (std::find(std::begin(project().graphics_backends()), std::end(project().graphics_backends()), "gles") !=
	   std::end(project().graphics_backends()))
		? std::optional<size_t>(m_GlesVersion)
		: std::nullopt;

	auto compiled_result = tools::glsl_compile(entry.value(), shader_stage, m_Optimize, gles_version);
	for(auto const& message : compiled_result.messages) {
		if(message.error) {
			assembler::log->error(message.message);
		} else {
			assembler::log->info(message.message);
		}
	}
	if(!compiled_result) {
		assembler::log->error("error processing '{}': could not compile the shader.", file.string());
		return {false};
	}

	auto write_output = [](std::filesystem::path const& file, auto const& srcData, psl::string_view extension) {
		auto output_file = file;
		output_file.replace_extension(output_file.extension().string() + extension);
		auto size_of_element = sizeof(decltype(srcData[0]));
		psl::array<std::byte> byte_view {(std::byte*)srcData.data(),
										 (std::byte*)srcData.data() +
										   (srcData.size() * size_of_element / sizeof(std::byte))};
		return std::make_unique<write_file_t>(output_file, byte_view);
	};

	// simple utility script to get the UID from the meta file, or returns a nullopt if it doesn't exist
	auto get_uid = [](std::filesystem::path const& file) -> std::optional<psl::UID> {
		auto meta_path = file;
		meta_path.replace_extension(file.extension().string() + ".meta");
		if(!std::filesystem::exists(meta_path))
			return std::nullopt;

		psl::meta::file* original = nullptr;
		psl::serialization::serializer temp_s;
		temp_s.deserialize<psl::serialization::decode_from_format>(original, meta_path.string());
		return original->ID();
	};

	auto shader_meta_for = [&file, &get_uid, &write_output](std::filesystem::path output_file,
															auto const& compiled_result,
															psl::string_view extension) {
		output_file.replace_extension(output_file.extension().string() + extension);
		auto uid		= get_uid(output_file).value_or(get_uid(file).value_or(psl::UID::generate()));
		auto shaderMeta = core::meta::shader {uid};
		shaderMeta.inputs(compiled_result.shader.inputs);
		shaderMeta.outputs(compiled_result.shader.outputs);
		shaderMeta.descriptors(compiled_result.shader.descriptors);
		shaderMeta.stage(compiled_result.shader.stage);
		psl::serialization::serializer s;
		psl::format::container container;
		s.serialize<psl::serialization::encode_to_format>(&shaderMeta, container);
		return write_output(output_file, container.to_string(), "." + psl::meta::META_EXTENSION);
	};

	importer_result_t result {true};
	auto output_file = rebase_to_build_dir(file);


	if(is_backend_enabled("vulkan")) {
		if(compiled_result.spirv.empty()) {
			assembler::log->error("error processing '{}': could not compile the shader to spirv.", file.string());
			return {false};
		}
		result.add(write_output(output_file, compiled_result.spirv, ".spv"));
		result.add(shader_meta_for(output_file, compiled_result, ".spv"));
	}

	if(is_backend_enabled("gles")) {
		if(compiled_result.gles.empty()) {
			assembler::log->error("error processing '{}': could not compile the shader to gles.", file.string());
			return {false};
		}
		result.add(write_output(output_file, compiled_result.gles, ".gles"));
		result.add(shader_meta_for(output_file, compiled_result, ".gles"));
	}

#if defined(AS_ENABLE_WGSL)
	if(is_backend_enabled("webgpu")) {
		if(compiled_result.spirv.empty()) {
			assembler::log->error("error processing '{}': could not compile the shader to spirv (webgpu).",
								  file.string());
			return {false};
		}

		auto spirv = std::vector<uint32_t>(compiled_result.spirv.size() / sizeof(uint32_t));
		std::memcpy(spirv.data(), compiled_result.spirv.data(), compiled_result.spirv.size());
		auto spirvReadOption = tint::spirv::reader::Options {};
		auto tintIr			 = tint::spirv::reader::Read(spirv, spirvReadOption);
		if(tintIr.Diagnostics().contains_errors()) {
			assembler::log->error("failed to convert the spirv to tint-ir: {}", tintIr.Diagnostics().str());
			return false;
		}
		auto wgslOptions = tint::wgsl::writer::Options();
		auto tintWgslRes = tint::wgsl::writer::Generate(tintIr, wgslOptions);
		if(tintWgslRes != tint::Success) {
			assembler::log->error("failed to convert the tint-ir to wgsl: {}", tintWgslRes.Failure().reason.str());
			return false;
		}
		auto wgsl = tintWgslRes.Move();
		result.add(write_output(output_file, wgsl.wgsl, ".wgsl"));
		result.add(shader_meta_for(output_file, compiled_result, ".wgsl"));
	}
#endif
	return result;
}
}	 // namespace assembler::importer