#include "generators/project.hpp"
#include "utils.h"
#include <filesystem>

#include "psl/format.hpp"
#include "psl/platform_utils.hpp"
#include "psl/serialization/serializer.hpp"

#include "data/project.hpp"

#include "importers/importer.hpp"
#include "importers/meta.hpp"
#include "importers/model.hpp"
#include "importers/shader.hpp"

#include <future>

namespace assembler::generators {
void project::on_generate(psl::cli::pack& pack) {
	auto projectFile	= pathstring {pack["input"]->as<psl::string>().get()}.platform();
	auto only_models	= pack["models"]->as<bool>().get();
	auto only_shaders	= pack["shaders"]->as<bool>().get();
	auto import_models	= !only_shaders || only_models;
	auto import_shaders = !only_models || only_shaders;
	auto import_default = !(only_models || only_shaders);

	using assembler::data::project_t;

	project_t project {};


	psl::string projectDir		= {};
	psl::string projectFilename = {};
	psl::string projectExt		= {};

	// Deconstruct, or default initialize the path parts for the project file.
	if(psl::utility::platform::directory::is_directory(projectFile)) {
		projectDir = projectFile.substr(
		  0,
		  psl::utility::string::rfind_first_of(projectFile,
											   psl::string(psl::utility::platform::directory::seperator) +
												 psl::utility::platform::directory::seperator_platform));
		projectFilename = project_t::DEFAULT_NAME;
		projectExt		= project_t::DEFAULT_EXTENSION;
	} else {
		auto lastSlash =
		  psl::utility::string::rfind_first_of(projectFile,
											   psl::string(psl::utility::platform::directory::seperator) +
												 psl::utility::platform::directory::seperator_platform);
		projectDir		   = projectFile.substr(0, lastSlash);
		auto has_extension = projectFile.find_last_of('.');
		if(has_extension != psl::string::npos) {
			projectExt		= projectFile.substr(has_extension + 1);
			projectFilename = projectFile.substr(lastSlash + 1, has_extension - lastSlash - 1);
		} else {
			projectFilename =
			  (lastSlash + 1 == projectFile.length()) ? project_t::DEFAULT_NAME : projectFile.substr(lastSlash + 1);
			projectExt = project_t::DEFAULT_EXTENSION;
		}
	}

	// Reconstruct the project file path using the just deconstructed parts.
	projectFile = projectDir + psl::utility::platform::directory::seperator + projectFilename + "." + projectExt;

	if(!psl::utility::platform::file::exists(projectFile)) {
		assembler::log->info("The project file '{}' does not exist. Generating one now..", projectFile);
		psl::serialization::serializer s;
		psl::format::container cont {};
		s.serialize<psl::serialization::encode_to_format>(project, cont);
		assembler::log->info("Generated project file '{}'", projectFile);
		if(psl::utility::platform::file::write(projectFile, cont.to_string()) == false) {
			assembler::log->error("Failed to write the project file '{}'", projectFile);
			return;
		}
	} else {
		psl::serialization::serializer s;
		s.deserialize<psl::serialization::decode_from_format>(project, projectFile);
	}

	project.project_directory(projectDir);

	assembler::log->info("Project file '{}' loaded", projectFile);

	std::filesystem::path projectPath = projectDir;

	std::filesystem::path sourceDir = (projectPath / project.source_directory()).lexically_normal();
	std::filesystem::path buildDir	= (projectPath / project.build_directory()).lexically_normal();

	if(!std::filesystem::exists(sourceDir)) {
		assembler::log->error("The source directory '{}' does not exist", sourceDir.string());
		return;
	}

	if(!std::filesystem::exists(buildDir)) {
		assembler::log->info("The build directory '{}' does not exist, creating it now", buildDir.string());
		std::filesystem::create_directories(buildDir);
	}

	auto files = psl::utility::platform::directory::all_files(sourceDir.string(), true);

	auto shaderFileTypes = project.meta_mapping().mapping("SHADER_META");
	std::transform(std::begin(shaderFileTypes),
				   std::end(shaderFileTypes),
				   std::begin(shaderFileTypes),
				   [](auto const& str) { return "." + str; });

	auto run_importer = [&shaderFileTypes, &sourceDir, &buildDir, import_models, import_shaders, import_default](
						  assembler::data::project_t project, psl::array_view<psl::string> files) {
		importer::importer_t importer {project};
		importer.ignore_extension(".meta");
		if(import_shaders) {
			auto shader_importer = importer.register_importer<importer::shader_t>();
			importer.map_extension(shaderFileTypes, shader_importer);
		}
		if(import_models) {
			auto model_importer = importer.register_importer<importer::model_t>();
			importer.map_extension(psl::array<psl::string> {".dae", ".gltf", ".fbx", ".md5mesh"}, model_importer);
		}
		if(import_default) {
			auto meta_importer = importer.register_importer<importer::meta_t>();
			importer.default_importer(meta_importer);
		}

		for(auto const& file : files) {
			auto ipath = std::filesystem::path(file);
			auto rel   = std::filesystem::relative(ipath, sourceDir);
			auto opath = buildDir / rel;

			if(!importer.import(ipath)) {
				assembler::log->error("Failed to import file '{}', inspect log for more details", file);
			}
		}
	};

	run_importer(project, files);
	assembler::log->info("Project generation complete");
}
}	 // namespace assembler::generators
