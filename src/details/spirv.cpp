#include "details/spirv.hpp"
#include "stdafx.h"

#include "psl/array.hpp"
#include "psl/array_view.hpp"
#include "psl/ustring.hpp"
#include "psl/string_utils.hpp"

#include "glslang/Public/ResourceLimits.h"
#include <SPIRV/GlslangToSpv.h>
#include <spirv_reflect.hpp>
#include <spirv_glsl.hpp>
tools::_internal::glslang_manager_t tools::_internal::glslang_manager {};

namespace tools {
namespace _internal {
	glslang_manager_t::glslang_manager_t() {
		glslang::InitializeProcess();
	}
	glslang_manager_t::~glslang_manager_t() {
		glslang::FinalizeProcess();
	}
}	 // namespace _internal

constexpr EShLanguage get_stage(shader_stage_t stage) {
	switch(stage) {
	case shader_stage_t::comp:
		return EShLangCompute;
	case shader_stage_t::vert:
		return EShLangVertex;
	case shader_stage_t::tesc:
		return EShLangTessControl;
	case shader_stage_t::tese:
		return EShLangTessEvaluation;
	case shader_stage_t::geom:
		return EShLangGeometry;
	case shader_stage_t::frag:
		return EShLangFragment;
	default:
		return EShLangCount;
	}
}

bool reflect_spirv(std::vector<uint32_t> const& spirv, glsl_compile_result_t& result) {
	spirv_cross::CompilerReflection module(spirv);
	auto const& resources = module.get_shader_resources();

	auto parse_attributes = [&module,
							 &result](auto const& source) mutable -> psl::array<core::meta::shader::attribute> {
		if(!result) {
			return {};
		}
		psl::array<core::meta::shader::attribute> attributes {};
		for(spirv_cross::Resource const& value : source) {
			auto& attribute = attributes.emplace_back();
			attribute.name(value.name);
			auto const& type = module.get_type(value.type_id);
			attribute.stride(type.columns * type.width / 8);
			attribute.count(type.vecsize);
			attribute.location(module.get_decoration(value.id, spv::DecorationLocation));

			using base_type = spirv_cross::SPIRType::BaseType;
			switch(type.basetype) {
			case base_type::Float:
				attribute.format(
				  core::gfx::format_t(std::to_underlying(core::gfx::format_t::r32_sfloat) + 3 * (type.vecsize - 1)));
				break;
			case base_type::Boolean:
			case base_type::Double:
				attribute.format(core::gfx::format_t::undefined);
				break;
			case base_type::Int:
				attribute.format(core::gfx::format_t::r32_sint);
				break;
			case base_type::UInt:
				attribute.format(core::gfx::format_t::r32_uint);
				break;
			default:
				result.success = false;
				result.shader  = {};
				result.messages.emplace_back(
				  fmt::format("reflection error, could not decode the attributes type {}", std::to_underlying(type.basetype)), true);
				return {};
			}
		}

		std::sort(std::begin(attributes), std::end(attributes), [](const auto& l, const auto& r) {
			return l.location() < r.location();
		});
		return attributes;
	};
	result.shader.inputs  = parse_attributes(resources.stage_inputs);
	result.shader.outputs = parse_attributes(resources.stage_outputs);

	auto parse_descriptors = [&module,
							  &result](auto const& source) mutable -> psl::array<core::meta::shader::descriptor> {
		if(!result) {
			return {};
		}
		constexpr auto qualifier = [](auto const& module,
									  auto id) noexcept -> core::meta::shader::descriptor::dependency {
			if(module.get_decoration(id, spv::DecorationNonReadable)) {
				return (module.get_decoration(id, spv::DecorationNonWritable))
						 ? core::meta::shader::descriptor::dependency::inout
						 : core::meta::shader::descriptor::dependency::out;
			} else {
				return (module.get_decoration(id, spv::DecorationNonWritable))
						 ? core::meta::shader::descriptor::dependency::out
						 : core::meta::shader::descriptor::dependency::inout;
			}
		};
		psl::array<core::meta::shader::descriptor> descriptors {};
		for(spirv_cross::Resource const& value : source) {
			auto& descriptor = descriptors.emplace_back();
			descriptor.name(value.name);
			descriptor.set(module.get_decoration(value.id, spv::DecorationDescriptorSet));
			descriptor.binding(module.get_decoration(value.id, spv::DecorationBinding));
			descriptor.qualifier(qualifier(module, value.id));
			auto const& type = module.get_type(value.type_id);
			using base_type	 = spirv_cross::SPIRType::BaseType;

			switch(type.basetype) {
			case base_type::Struct: {
				auto storage =
				  (type.storage == spv::StorageClassUniform && module.get_decoration(type.self, spv::DecorationBlock))
					? spv::StorageClassUniform
					: spv::StorageClassStorageBuffer;

				switch(storage) {
				case spv::StorageClassUniform:
					descriptor.type(psl::utility::string::contains(value.name, "_DYNAMIC_")
									  ? core::gfx::binding_type::uniform_buffer_dynamic
									  : core::gfx::binding_type::uniform_buffer);
					break;
				case spv::StorageClassStorageBuffer:
					descriptor.type(psl::utility::string::contains(value.name, "_DYNAMIC_")
									  ? core::gfx::binding_type::storage_buffer_dynamic
									  : core::gfx::binding_type::storage_buffer);
					break;
				default:
					throw std::runtime_error("unhandled type");
				}
			} break;
			case base_type::SampledImage:
				assert(type.storage == spv::StorageClassUniformConstant);
				descriptor.type(core::gfx::binding_type::combined_image_sampler);
				break;
			case base_type::Sampler:
				assert(type.storage == spv::StorageClassUniformConstant);
				descriptor.type(core::gfx::binding_type::sampler);
				break;
			case base_type::Image:
				// image
				if(type.storage == spv::StorageClassUniformConstant && type.image.sampled == 2) {
					descriptor.type(core::gfx::binding_type::storage_image);
				}
				// separate image
				else if(type.storage == spv::StorageClassUniformConstant && type.image.sampled == 1) {
					descriptor.type(core::gfx::binding_type::sampled_image);
				} else {
					result.shader  = {};
					result.success = false;
					result.messages.emplace_back(
					  fmt::format("reflection error, unknown storage {} for image type", std::to_underlying(type.storage)), true);
					return {};
				}
				break;
			default:
				result.shader  = {};
				result.success = false;
				result.messages.emplace_back(fmt::format("reflection error, unknown descriptor type {}", std::to_underlying(type.basetype)),
											 true);
				return {};
			}
		}
		return descriptors;
	};

	auto descriptors = parse_descriptors(resources.uniform_buffers);
	descriptors.append_range(parse_descriptors(resources.storage_buffers));
	descriptors.append_range(parse_descriptors(resources.sampled_images));
	descriptors.append_range(parse_descriptors(resources.separate_images));
	descriptors.append_range(parse_descriptors(resources.separate_samplers));
	descriptors.append_range(parse_descriptors(resources.storage_images));
	result.shader.descriptors = std::move(descriptors);

	auto const& entries = module.get_entry_points_and_stages();
	if(entries.size() != 1) {
		result.shader  = glsl_compile_result_t::shader_t();
		result.success = false;
		return result;
	}

	switch(entries[0].execution_model) {
	case spv::ExecutionModelVertex:
		result.shader.stage = core::gfx::shader_stage::vertex;
		break;
	case spv::ExecutionModelFragment:
		result.shader.stage = core::gfx::shader_stage::fragment;
		break;
	case spv::ExecutionModelGeometry:
		result.shader.stage = core::gfx::shader_stage::geometry;
		break;
	case spv::ExecutionModelTessellationControl:
		result.shader.stage = core::gfx::shader_stage::tesselation_control;
		break;
	case spv::ExecutionModelTessellationEvaluation:
		result.shader.stage = core::gfx::shader_stage::tesselation_evaluation;
		break;
	case spv::ExecutionModelGLCompute:
		result.shader.stage = core::gfx::shader_stage::compute;
		break;
	default:
		result.shader  = glsl_compile_result_t::shader_t();
		result.success = false;
		return result;
	}
	return result;
}

glsl_compile_result_t
glsl_compile(psl::string_view source, shader_stage_t type, bool optimize, std::optional<size_t> gles_version) {
	auto const stage = get_stage(type);
	glsl_compile_result_t result {true};
	if(stage == EShLangCount) {
		result.messages.emplace_back(fmt::format("unknown shader_stage_t value '{}'", std::to_underlying(type)), true);
		result.success = false;
		return result;
	}
	sizeof(glsl_compile_result_t);
	sizeof(spirv_cross::CompilerReflection);
	glslang::TShader shader(stage);
	TBuiltInResource resources = *GetDefaultResources();

	EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules | EShMsgDefault | EShMsgEnhanced);
#if !defined(NDEBUG)
	messages = (EShMessages)(messages | EShMsgDebugInfo);
#endif
	// sadly we need to add a temporary due to the API requesting an array of `char const *`
	auto source_ptr = source.data();
	shader.setStrings(&source_ptr, 1);
	shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 110);
	shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
	shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);

	int const glslVersion = glslang::EShTargetOpenGL_450;

	if(!shader.parse(&resources, glslVersion, true, messages)) {
		result.messages.emplace_back(
		  fmt::format("compilation failure: {}\n{}", shader.getInfoLog(), shader.getInfoDebugLog()), true);
		result.success = false;
		return result;
	}

	glslang::SpvOptions spvOptions;
#if !defined(NDEBUG)
	spvOptions.generateDebugInfo = true;
	spvOptions.disableOptimizer	 = true;
	spvOptions.optimizeSize		 = false;
	spvOptions.stripDebugInfo	 = false;
#else
	spvOptions.generateDebugInfo = false;
	spvOptions.disableOptimizer	 = !optimize;
	spvOptions.optimizeSize		 = optimize;
	spvOptions.stripDebugInfo	 = true;
#endif
	spvOptions.validate = true;
	spv::SpvBuildLogger logger;
	std::vector<uint32_t> spirv;

	glslang::GlslangToSpv(*shader.getIntermediate(), spirv, &logger, &spvOptions);
	if(spirv.empty()) {
		result.messages.emplace_back(fmt::format("spir-v failure: {}", logger.getAllMessages()), true);
		result.success = false;
		return result;
	}
	if(!logger.getAllMessages().empty()) {
		result.messages.emplace_back(fmt::format("spir-v output: {}", logger.getAllMessages()), false);
	}
	result.spirv =
	  psl::string_view {reinterpret_cast<char*>(spirv.data()), spirv.size() * (sizeof(uint32_t) / sizeof(char))};

	if(gles_version) {
		spirv_cross::CompilerGLSL gles_compiler(spirv);

		spirv_cross::CompilerGLSL::Options options;
		options.version = gles_version.value();
		options.es		= true;
		gles_compiler.set_common_options(options);

		result.gles = gles_compiler.compile();
	}

	if(!reflect_spirv(spirv, result)) {
		result.success = false;
		result.shader  = {};
		return result;
	}
	return result;
}
}	 // namespace tools
