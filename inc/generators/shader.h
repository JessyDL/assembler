#pragma once
#include "cli_pack.h"
#include "bash_terminal.h"
#include "bash_utils.h"
#include "terminal_utils.h"
#include "glslang_utils.h"
#include "../../meta/inc/meta.h"
#include "meta/shader.h"
#include <set>
#include "timer.h"

namespace assembler::generators
{
	class shader
	{
		enum class block_type { unknown_t = 0, struct_t, attribute_t, descriptor_t, inlined_t, interface_t };

		struct vertex_attribute
		{
			size_t format;
			size_t offset;
		};

		struct parsed_element
		{
			psl::string name;
			psl::string type;
			size_t size;
			std::optional<uint64_t> location;
			std::optional<psl::string> inlined_scope;

			std::optional<psl::string> backing_type;
			std::optional<psl::string> default_value;
			std::optional<psl::string> in_qualifier;
			std::optional<psl::string> out_qualifier;
			std::optional<psl::string> bind_qualifier;
			std::optional<psl::string> buffer;
			std::optional<size_t> rate;
			std::vector<vertex_attribute> vAttribute;
		};

		struct parsed_scope
		{
			psl::string unique_name;
			psl::string name;
			size_t size;
			shader::block_type type;
			psl::string scope;
			std::optional<psl::string> parent{std::nullopt};
			std::vector<parsed_element> elements;
		};
	public:
		struct file_data
		{
			uint64_t last_modified;
			psl::string content;
			psl::string filename;
			std::vector<std::pair<psl::string, size_t>> includes;
			std::vector<parsed_scope> scopes;
			tools::glslang::type type;

			psl::string using_in_type;
			psl::string using_in_name;
			psl::string using_out_type;
			psl::string using_out_name;
			psl::string using_descr_type;
			psl::string using_descr_name;
		};
	private:
		struct result_shader
		{
			psl::string content;
			psl::string filename;
			parsed_scope out;
			parsed_scope in;
			parsed_scope descriptor;
		};

		std::unordered_map<psl::string, size_t> m_KnownTypes
		{
			{("bool"), 1},
			{("int"), 4},
			{("uint"), 4},
			{("float"), 4},
			{("double"), 8},
			{("sampler"), 0},
			{("sampler2D"), 0},
			{("sampler3D"), 0},
			{("samplerCube"), 0}
		};

		std::unordered_map<psl::string, size_t> m_TypeToFormat
		{
			{("vec4"), 109},
		{("mat4"), 109},
		{("vec3"), 106},
		{("mat3"), 106},
		{("vec2"), 103},
		{("mat2"), 103}
		};

		std::unordered_map<psl::string, size_t> m_DescriptorType
		{
			{("ubo"), 6},
			{("ssbo"), 7},
			{("combined_sampler"), 1}
		};
	public:
		shader()
		{
			m_Pack << std::move(cli::value<psl::string>{}
			.name(("input"))
				.command(("input"))
				.short_command(('i'))
				.hint(("location of the input file"))
				.required(true))
				<< std::move(cli::value<psl::string>{}
			.name(("output"))
				.command(("output"))
				.short_command(('o'))
				.hint(("location where to place the file"))
				.required(true))
				<< std::move(cli::value<bool>{}
			.name(("overwrite"))
				.command(("overwrite"))
				.short_command(('f'))
				.hint(("should the output file be overwritten when it already exists?"))
				.default_value(true))
				<< std::move(cli::value<bool>{}
			.name(("optimize"))
				.command(("optimize"))
				.short_command(('O'))
				.hint(("should we optimize the output?"))
				.default_value(false))
				<< std::move(cli::value<bool>{}
			.name(("compiled_glsl"))
				.command(("glsl"))
				.hint(("should we instead print the complete constructed GLSL (with includes etc..)?"))
				.default_value(false))
				<< std::move(cli::value<bool>{}
			.name(("verbose"))
				.command(("verbose"))
				.short_command(('v'))
				.hint(("should verbose information be printed about the internal process?"))
				.default_value(false));

			m_Pack.callback(std::bind(&assembler::generators::shader::on_generate, this, m_Pack));

			std::unordered_map<psl::string, size_t> vectoral_types
			{
				{("b"), 1},
				{("i"), 4},
				{("u"), 4},
				{(""), 4},
				{("d"), 8}
			};
			for(size_t i = 2; i <= 4; ++i)
			{
				for(const auto& vType : vectoral_types)
				{
					psl::string fulltype = vType.first + ("vec") + psl::from_string8_t(utility::to_string(i));
					size_t size = vType.second * i;
					m_KnownTypes[fulltype] = size;
				}
			}

			vectoral_types = 
				std::unordered_map<psl::string, size_t>{
				{(""), 4},
				{("d"), 8}
			};
			for(size_t i = 2; i <= 4; ++i)
			{
				for(const auto& vType : vectoral_types)
				{
					psl::string fulltype = vType.first + ("mat") + psl::from_string8_t(utility::to_string(i));
					size_t size = vType.second * i * i;
					m_KnownTypes[fulltype] = size;
				}
			}

			for(size_t n = 2; n <= 4; ++n)
			{
				for(size_t m = 2; m <= 4; ++m)
				{
					for(const auto& vType : vectoral_types)
					{
						psl::string fulltype = vType.first + ("mat") + psl::from_string8_t(utility::to_string(n)) + ("x") + psl::from_string8_t(utility::to_string(m));
						size_t size = vType.second * n * m;
						m_KnownTypes[fulltype] = size;
					}
				}
			}
		}

		cli::parameter_pack& pack()
		{
			return m_Pack;
		}
	private:
		bool all_scopes(file_data& data);
		bool parse_includes(tools::bash_terminal& bt, file_data& data);


		bool parse_using(file_data& data, psl::string name, psl::string* type_out, psl::string* name_out);

		bool parse_using(file_data& data);

		bool parse(tools::bash_terminal& bt, file_data& data);
		bool cache_file(tools::bash_terminal& bt, const psl::string& file);

		psl::string construct(const file_data& fdata, std::set<psl::string_view>& includes) const;

		bool find(const file_data& fdata, const psl::string& name, block_type type, parsed_scope* out) const;

		bool construct(const file_data& fdata, result_shader& out, psl::string_view entry = ("main")) const;
		void on_generate(cli::parameter_pack& pack);

		cli::parameter_pack m_Pack;
		std::unordered_map<psl::string, file_data> m_Cache; // Caches the files read. Note that the filepath 
															// will always be Unix style regardless of input
		bool m_Verbose;
	};
}
