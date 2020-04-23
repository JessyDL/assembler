#include "glslang_utils.h"
#include "nlohmann/json.hpp"
#include "psl/meta.h"
#include "psl/platform_utils.h"
#include "stdafx.h"
#include <iostream>
#include <stdlib.h>

#include "gfx/types.h"
#include "psl/array.h"
#include "psl/array_view.h"
#include "psl/library.h"
#include "psl/serialization.h"
#include "psl/ustring.h"

#include "meta/shader.h"

struct typeinfo
{
	uint32_t count;			  // how many elements
	uint32_t stride;		  // how big ever element is
	core::gfx::format format; // the expected format;
};

std::unordered_map<psl::string, typeinfo> m_TypeToInfo{
	{"vec4", {1, 16, core::gfx::format::r32g32b32a32_sfloat}},
	{"vec3", {1, 12, core::gfx::format::r32g32b32_sfloat}},
	{"vec2", {1, 8, core::gfx::format::r32g32_sfloat}},
	{"float", {1, 4, core::gfx::format::r32_sfloat}},
	{"mat4", {4, 16, core::gfx::format::r32g32b32a32_sfloat}},
	{"mat4x4", {4, 16, core::gfx::format::r32g32b32a32_sfloat}},
	{"mat4x3", {3, 16, core::gfx::format::r32g32b32a32_sfloat}},
	{"mat4x2", {2, 16, core::gfx::format::r32g32b32a32_sfloat}},
	{"mat4x1", {1, 16, core::gfx::format::r32g32b32a32_sfloat}},
	{"mat3", {3, 12, core::gfx::format::r32g32b32_sfloat}},
	{"mat3x4", {4, 12, core::gfx::format::r32g32b32_sfloat}},
	{"mat3x3", {3, 12, core::gfx::format::r32g32b32_sfloat}},
	{"mat3x2", {2, 12, core::gfx::format::r32g32b32_sfloat}},
	{"mat3x1", {1, 12, core::gfx::format::r32g32b32_sfloat}},
	{"mat2", {2, 8, core::gfx::format::r32g32_sfloat}},
	{"mat2x4", {4, 8, core::gfx::format::r32g32_sfloat}},
	{"mat2x3", {3, 8, core::gfx::format::r32g32_sfloat}},
	{"mat2x2", {2, 8, core::gfx::format::r32g32_sfloat}},
	{"mat2x1", {1, 8, core::gfx::format::r32g32_sfloat}},

	{"bool", {1, 1, core::gfx::format::undefined}},
	{"int", {1, 4, core::gfx::format::r32_sint}},
	{"uint", {1, 4, core::gfx::format::r32_uint}},
	{"double", {1, 8, core::gfx::format::undefined}},

	{"mat1x4", {1, 4, core::gfx::format::r32_sfloat}},
	{"mat1x3", {1, 4, core::gfx::format::r32_sfloat}},
	{"mat1x2", {1, 4, core::gfx::format::r32_sfloat}},
	{"mat1x1", {1, 4, core::gfx::format::r32_sfloat}},
	{"mat1", {1, 4, core::gfx::format::r32_sfloat}},
};

using namespace tools;

bool glslang::compile(psl::string_view compiler_location, psl::string_view source, psl::string_view outputfile,
					  type type, bool optimize, std::optional<size_t> gles_version)
{
	if(!utility::platform::directory::exists(compiler_location)) return false;
	psl::string compiler{compiler_location};
	auto input = compiler + "tempfile";
	if(!utility::platform::file::write(input, source))
	{
		assembler::log->error("ERROR: could not write the temporary shader file.");
		return false;
	}
	while(!utility::platform::file::read(input))
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(150));
	}

	auto full_path = compiler + "glslangValidator.exe";

	if(!utility::platform::file::exists(full_path))
	{
		assembler::log->error("ERROR: missing 'glslangValidator'");
		utility::platform::file::erase(input);
		return false;
	}
	psl::string ofile   = utility::platform::file::to_platform(outputfile) + "-" + type_str[(uint8_t)type];
	auto ofile_spirv	= ofile + ".spv";
	psl::string command = "cd \"" + compiler + "\" &&" + " glslangValidator.exe -S " + type_str[(uint8_t)type] +
						  " -V -t -s \"" + input + "\" -o \"" + ofile_spirv + "\"";
	if(std::system(command.c_str()) != 0)
	{
		command = "cd \"" + compiler + "\" &&" + " glslangValidator.exe -S " + type_str[(uint8_t)type] +
				  " -V -t \"" + input + "\" -o \"" + ofile_spirv + "\"";
		std::system(command.c_str());
		utility::platform::file::erase(input);
		return false;
	}
	utility::platform::file::erase(input);

	while(!utility::platform::file::read(ofile_spirv))
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(150));
	}

	if(optimize)
	{
		command = ("spirv-opt.exe -O \"") + ofile_spirv + ("\" -o \"") + ofile_spirv + ("\"");
		if(std::system(command.c_str()) != 0) return false;
	}

	if(gles_version)
	{
		auto ofile_gles = ofile + ".gles";
		command = "cd \"" + compiler + "\" &&" + " spirv-cross.exe --version " + std::to_string(gles_version.value()) +
				  " --es \"" + ofile_spirv + "\" --output \"" + ofile_gles + "\"";

		if(std::system(command.c_str()) != 0)
		{
			return false;
		}
		while(!utility::platform::file::read(ofile_gles))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(150));
		}
	}

	auto ofile_refl = ofile_spirv + ".temp";
	// reflect
	command =
		"cd \"" + compiler + "\" && spirv-cross.exe \"" + ofile_spirv + "\" --reflect --output \"" + ofile_refl + "\"";
	if(std::system(command.c_str()) != 0)
	{
		if(utility::platform::file::exists(ofile_refl)) utility::platform::file::erase(ofile_refl);
		return false;
	}
	std::optional<psl::string> reflect_opt = utility::platform::file::read(ofile_refl);
	while(!reflect_opt)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(150));
		reflect_opt = utility::platform::file::read(ofile_refl);
	}
	auto reflection{std::move(reflect_opt.value())};
	utility::platform::file::erase(ofile_refl);

	// CompilerReflection::emit_resources() : spirv_reflect.cpp
	auto json			= nlohmann::json::parse(reflection);
	auto inputs			= json["inputs"];
	auto outputs		= json["outputs"];
	auto push_constants = json["push_constants"];

	// bindings
	auto ubos			   = json["ubos"];
	auto ssbos			   = json["ssbos"];
	auto sampled		   = json["textures"];
	auto separate_images   = json["separate_images"];
	auto separate_samplers = json["separate_samplers"];
	auto images			   = json["images"];

	using namespace psl;
	using namespace core::meta;
	auto ofile_meta = ofile + "." + meta::META_EXTENSION;
	UID meta_UID	= UID::generate();
	if(utility::platform::file::exists(ofile_meta))
	{
		meta::file* original = nullptr;
		serialization::serializer temp_s;
		temp_s.deserialize<serialization::decode_from_format>(original, ofile_meta);
		meta_UID = original->ID();
	}
	shader shaderMeta{meta_UID};

	psl::array<shader::attribute> attributes;
	auto parse_attributed = [](const auto& source, auto& attributes) {
		for(const auto& value : source)
		{
			auto& attribute = attributes.emplace_back();
			attribute.name(value["name"]);
			attribute.location(value["location"]);
			const auto& info = m_TypeToInfo[psl::string(value["type"])];
			attribute.count(info.count);
			attribute.format(info.format);
			attribute.stride(info.stride);
		}

		std::sort(std::begin(attributes), std::end(attributes),
				  [](const auto& l, const auto& r) { return l.location() < r.location(); });
	};
	parse_attributed(inputs, attributes);
	shaderMeta.inputs(attributes);
	attributes.clear();
	parse_attributed(outputs, attributes);
	shaderMeta.outputs(attributes);

	auto parse_type = [](const auto& type_name, const auto& json, psl::array<core::meta::shader::member>& members, auto& parse_type) mutable -> void
	{
		const auto& type = json["types"].find(type_name).value();
		for (const auto& member_json : type["members"])
		{
			auto& member{ members.emplace_back() };
			member.name(member_json["name"]);
			if (auto it = m_TypeToInfo.find(member_json["type"]); it != std::end(m_TypeToInfo))
			{
				member.count(it->second.count);
				member.stride(it->second.stride);
				member.size(it->second.count * 4);
				//member.format(it->second.format);
				member.offset(member_json["offset"]);
			}
			else
			{
				// array (count)
				// array_size_is_literal T/F
				// array_stride
				if (member_json.contains("array"))
				{
					psl::array<core::meta::shader::member> sub_members;
					parse_type(member_json["type"], json, sub_members, parse_type);
					assert(member_json["array"].size() == 1 || "we only support one dimensional arrays as of now");
					member.count(member_json["array"][0]);
					member.stride(member_json["array_stride"]);
					member.size(member.stride() * member.count());
					member.members(sub_members);
				}
				else
					throw std::runtime_error("unhandled type");
			}
		}
	};

	psl::array<shader::descriptor> descriptors;
	for(const auto& value : ubos)
	{
		auto& descr = descriptors.emplace_back();
		descr.set(value["set"]);
		descr.binding(value["binding"]);
		descr.name(value["name"]);
		descr.qualifier(shader::descriptor::dependency::in);
		descr.type(core::gfx::binding_type::uniform_buffer);

		psl::array<core::meta::shader::member> members;
		parse_type(value["type"], json, members, parse_type);
		descr.members(members);
	}
	for(const auto& value : sampled)
	{
		auto& descr = descriptors.emplace_back();
		descr.set(value["set"]);
		descr.binding(value["binding"]);
		descr.name(value["name"]);
		descr.qualifier(shader::descriptor::dependency::in);
		descr.type(core::gfx::binding_type::combined_image_sampler);
	}
	for(const auto& value : separate_images)
	{
		auto& descr = descriptors.emplace_back();
		descr.set(value["set"]);
		descr.binding(value["binding"]);
		descr.name(value["name"]);
		descr.qualifier(shader::descriptor::dependency::in);
		descr.type(core::gfx::binding_type::sampled_image);
	}
	for(const auto& value : separate_samplers)
	{
		auto& descr = descriptors.emplace_back();
		descr.set(value["set"]);
		descr.binding(value["binding"]);
		descr.name(value["name"]);
		descr.qualifier(shader::descriptor::dependency::in);
		descr.type(core::gfx::binding_type::sampler);
	}

	// figure out image and buffer bindings if they are input and/or outputs
	for(const auto& image : images)
	{
		psl::string name = image["name"];

		bool in = false, out = false;

		for(auto loc : utility::string::locations(source, "imageStore"))
		{
			loc += sizeof("imageStore") - 1;
			out |= psl::string_view{source.data() + loc, source.find(",", loc)}.find(name) != psl::string::npos;
		}
		for(auto loc : utility::string::locations(source, "imageLoad"))
		{
			loc += sizeof("imageLoad") - 1;
			in |= psl::string_view{source.data() + loc, source.find(",", loc)}.find(name) != psl::string::npos;
		}

		auto& descr = descriptors.emplace_back();
		descr.set(image["set"]);
		descr.binding(image["binding"]);
		descr.name(name);
		descr.qualifier((in && out) ? shader::descriptor::dependency::inout
									: (in) ? shader::descriptor::dependency::in : shader::descriptor::dependency::out);
		descr.type(core::gfx::binding_type::storage_image);
	}

	for(const auto& buffer : ssbos)
	{
		psl::string name = buffer["name"];

		bool in = false, out = false;

		for(auto loc : utility::string::locations(source, name))
		{
			auto begin = source.rfind('\n', loc);
			auto end   = std::min(source.find('\n', loc), source.size());
			auto equal = source.find("=", begin, end - begin);
			in |= equal > loc;
			out |= equal < loc;
		}

		auto& descr = descriptors.emplace_back();
		descr.set(buffer["set"]);
		descr.binding(buffer["binding"]);
		descr.name(name);
		descr.qualifier((in && out) ? shader::descriptor::dependency::inout
									: (in) ? shader::descriptor::dependency::in : shader::descriptor::dependency::out);
		descr.type(core::gfx::binding_type::storage_buffer);

		psl::array<core::meta::shader::member> members;
		parse_type(buffer["type"], json, members, parse_type);
		descr.members(members);
	}

	shaderMeta.descriptors(std::move(descriptors));

	switch(type)
	{
	case tools::glslang::type::vert:
	{
		shaderMeta.stage(core::gfx::shader_stage::vertex);
	}
	break;
	case tools::glslang::type::frag:
	{
		shaderMeta.stage(core::gfx::shader_stage::fragment);
	}
	break;
	case tools::glslang::type::geom:
	{
		shaderMeta.stage(core::gfx::shader_stage::geometry);
	}
	break;
	case tools::glslang::type::tesc:
	{
		shaderMeta.stage(core::gfx::shader_stage::tesselation_control);
	}
	break;
	case tools::glslang::type::tese:
	{
		shaderMeta.stage(core::gfx::shader_stage::tesselation_evaluation);
	}
	break;
	case tools::glslang::type::comp:
	{
		shaderMeta.stage(core::gfx::shader_stage::compute);
	}
	break;
	}

	serialization::serializer s;
	format::container container;
	s.serialize<serialization::encode_to_format>(&shaderMeta, container);
	utility::platform::file::write(ofile_meta, psl::from_string8_t(container.to_string()));

	return true;
}
