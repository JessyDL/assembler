// assembler.cpp : Defines the entry point for the console application.
//

#include "psl/string_utils.hpp"
#include "psl/ustring.hpp"
#include "stdafx.h"
#include <array>
#include <iostream>

#ifdef WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

#include "cli/value.h"
#include "generators/meta.h"
#include "generators/models.h"
#include "generators/shader.h"

#include "resource/cache.hpp"

#include "psl/collections/spmc.hpp"


using psl::cli::pack;
using psl::cli::value;

using namespace core::resource;
using namespace core::gfx;

#ifdef WIN32
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	switch(fdwCtrlType)
	{
	// Handle the CTRL-C signal.
	case CTRL_C_EVENT:
		// entry.pop();
		std::cout << "^C" << std::endl;
		return (TRUE);

	// CTRL-CLOSE: confirm that the user wants to exit.
	case CTRL_CLOSE_EVENT: std::cout << "exiting..." << std::endl; return (TRUE);

	// Pass other signals to the next handler.
	case CTRL_BREAK_EVENT: return FALSE;

	case CTRL_LOGOFF_EVENT: return FALSE;

	case CTRL_SHUTDOWN_EVENT: return FALSE;

	default: return FALSE;
	}
}
#endif

static psl::string_view get_input()
{
	static psl::pstring_t input(4096, ('\0'));
	std::memset(input.data(), ('\0'), sizeof(psl::platform::char_t) * input.size());
#if defined(WIN32) && defined(UNICODE)
	static psl::string storage(8192, ('\0'));
	std::wcin.getline(input.data(), input.size() - 1);
	auto intermediate = psl::to_string8_t(psl::platform::view(input.data(), std::wcslen(input.data())));
	std::memcpy(storage.data(), intermediate.data(), intermediate.size());
	return {storage.data(), intermediate.size()};
#else
	std::cin.getline(input.data(), input.size() - 1);
	return {input.data(), psl::strlen(input.data())};
#endif
}

#include "paradigm.hpp"
#include "psl/application_utils.hpp"
#include "psl/literals.hpp"

#include "data/buffer.hpp"
#include "data/material.hpp"
#include "data/window.hpp"
#include "os/surface.hpp"
#include "os/context.hpp"

#include "gfx/buffer.hpp"
#include "gfx/computepass.hpp"
#include "gfx/context.hpp"
#include "gfx/drawpass.hpp"
#include "gfx/material.hpp"
#include "gfx/render_graph.hpp"
#include "gfx/swapchain.hpp"


#include "logging.hpp"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/dist_sink.h"
#ifdef _MSC_VER
#include "spdlog/sinks/msvc_sink.h"
#endif

void setup_loggers()
{
	psl::string sub_path = "logs/";
	if(!utility::platform::file::exists(utility::application::path::get_path() + sub_path + "main.log"))
		utility::platform::file::write(utility::application::path::get_path() + sub_path + "main.log", "");
	std::vector<spdlog::sink_ptr> sinks;

	auto mainlogger = std::make_shared<spdlog::sinks::dist_sink_mt>();
	mainlogger->add_sink(std::make_shared<spdlog::sinks::basic_file_sink_mt>(
		utility::application::path::get_path() + sub_path + "main.log", true));
	mainlogger->add_sink(std::make_shared<spdlog::sinks::basic_file_sink_mt>(
		utility::application::path::get_path() + "logs/latest.log", true));
#ifdef _MSC_VER
	mainlogger->add_sink(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#else
	auto outlogger = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	outlogger->set_level(spdlog::level::level_enum::warn);
	mainlogger->add_sink(outlogger);
#endif

	auto ivklogger = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
		utility::application::path::get_path() + sub_path + "ivk.log", true);

	auto igleslogger = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
		utility::application::path::get_path() + sub_path + "igles.log", true);

	auto gfxlogger = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
		utility::application::path::get_path() + sub_path + "gfx.log", true);

	auto systemslogger = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
		utility::application::path::get_path() + sub_path + "systems.log", true);

	auto oslogger = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
		utility::application::path::get_path() + sub_path + "os.log", true);

	auto datalogger = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
		utility::application::path::get_path() + sub_path + "data.log", true);

	auto corelogger = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
		utility::application::path::get_path() + sub_path + "core.log", true);

	sinks.push_back(mainlogger);
	sinks.push_back(corelogger);

	auto logger = std::make_shared<spdlog::logger>("main", begin(sinks), end(sinks));
	spdlog::register_logger(logger);
	core::log = logger;


	sinks.clear();
	sinks.push_back(mainlogger);
	sinks.push_back(systemslogger);

	auto system_logger = std::make_shared<spdlog::logger>("systems", begin(sinks), end(sinks));
	spdlog::register_logger(system_logger);
	core::systems::log = system_logger;

	sinks.clear();
	sinks.push_back(mainlogger);
	sinks.push_back(oslogger);

	auto os_logger = std::make_shared<spdlog::logger>("os", begin(sinks), end(sinks));
	spdlog::register_logger(os_logger);
	core::os::log = os_logger;

	sinks.clear();
	sinks.push_back(mainlogger);
	sinks.push_back(datalogger);

	auto data_logger = std::make_shared<spdlog::logger>("data", begin(sinks), end(sinks));
	spdlog::register_logger(data_logger);
	core::data::log = data_logger;

	sinks.clear();
	sinks.push_back(mainlogger);
	sinks.push_back(gfxlogger);

	auto gfx_logger = std::make_shared<spdlog::logger>("gfx", begin(sinks), end(sinks));
	spdlog::register_logger(gfx_logger);
	core::gfx::log = gfx_logger;

#ifdef PE_VULKAN
	sinks.clear();
	sinks.push_back(mainlogger);
	sinks.push_back(ivklogger);

	auto ivk_logger = std::make_shared<spdlog::logger>("ivk", begin(sinks), end(sinks));
	spdlog::register_logger(ivk_logger);
	core::ivk::log = ivk_logger;
#endif
#ifdef PE_GLES
	sinks.clear();
	sinks.push_back(mainlogger);
	sinks.push_back(igleslogger);

	auto igles_logger = std::make_shared<spdlog::logger>("igles", begin(sinks), end(sinks));
	spdlog::register_logger(igles_logger);
	core::igles::log = igles_logger;
#endif
	spdlog::set_pattern("%8T.%6f [%=8n] [%=8l] %^%v%$ %@", spdlog::pattern_time_type::utc);
}

#include <atomic>

std::atomic<graphics_backend> gBackend{graphics_backend::undefined};

using namespace core;

#include "data/sampler.hpp"
#include "gfx/sampler.hpp"
#include "gfx/texture.hpp"

void load_texture(resource::cache_t& cache, handle<core::gfx::context> context_handle, const psl::UID& texture)
{
	if(!cache.contains(texture))
	{
		auto textureHandle = cache.instantiate<gfx::texture_t>(texture, context_handle);
		assert(textureHandle);
	}
}

handle<core::data::material_t> setup_gfx_material_data(resource::cache_t& cache,
													   handle<core::gfx::context> context_handle,
													 psl::UID vert, psl::UID frag, const psl::UID& texture)
{
	auto vertShaderMeta = cache.library().get<core::meta::shader>(vert).value();
	auto fragShaderMeta = cache.library().get<core::meta::shader>(frag).value();

	load_texture(cache, context_handle, texture);

	// create the sampler
	auto samplerData   = cache.create<data::sampler_t>();
	auto samplerHandle = cache.create<gfx::sampler_t>(context_handle, samplerData);

	// load the example material
	auto matData = cache.create<data::material_t>();

	matData->from_shaders(cache.library(), {vertShaderMeta, fragShaderMeta});

	auto stages = matData->stages();
	for(auto& stage : stages)
	{
		if(stage.shader_stage() != core::gfx::shader_stage::fragment) continue;

		auto bindings = stage.bindings();
		for(auto& binding : bindings)
		{
			if(binding.descriptor() != core::gfx::binding_type::combined_image_sampler) continue;
			binding.texture(texture);
			binding.sampler(samplerHandle);
		}
		stage.bindings(bindings);
		// binding.texture()
	}
	matData->stages(stages);
	matData->blend_states({core::data::material_t::blendstate(0)});
	return matData;
}


handle<core::gfx::material_t> setup_gfx_material(resource::cache_t& cache, handle<core::gfx::context> context_handle,
											   handle<core::gfx::pipeline_cache> pipeline_cache,
												 handle<core::gfx::buffer_t> matBuffer, psl::UID vert, psl::UID frag,
											   const psl::UID& texture)
{
	auto matData  = setup_gfx_material_data(cache, context_handle, vert, frag, texture);
	auto material = cache.create<core::gfx::material_t>(context_handle, matData, pipeline_cache, matBuffer);

	return material;
}

void ui_icon() {}

#include "ecs/systems/fly.hpp"
#include "ecs/systems/geometry_instance.hpp"

#include "ecs/components/camera.hpp"
#include "ecs/components/input_tag.hpp"

volatile bool should_exit = false;
void launch_gassembler(graphics_backend backend)
{
	using namespace core;
	psl::string libraryPath{utility::application::path::library + "resources.metalib"};
	memory::region resource_region{20_mb, 4u, new memory::default_allocator()};

	psl::string8_t environment = "";
	switch(backend)
	{
	case graphics_backend::gles: environment = "gles"; break;
	case graphics_backend::vulkan: environment = "vulkan"; break;
	}

	cache_t cache{psl::meta::library{psl::to_string8_t(libraryPath), {{environment}}}};

	auto window_data = cache.instantiate<data::window>("cd61ad53-5ac8-41e9-a8a2-1d20b43376d9"_uid);
	auto window_name = APPLICATION_FULL_NAME + " { " + environment + " }";
	window_data->name(window_name);

	auto surface_handle = cache.create<core::os::surface>(window_data);
	if(!surface_handle)
	{
		core::log->critical("Could not create a OS surface to draw on.");
		return;
	}

	auto context_handle = cache.create<core::gfx::context>(backend, psl::string8_t{APPLICATION_NAME});

	// this exists due to Android support.
	/// \todo ideally this is passed into the entry function
	auto os_context		  = core::os::context{};
	auto swapchain_handle = cache.create<core::gfx::swapchain>(surface_handle, context_handle, os_context);

	auto storage_buffer_align = context_handle->limits().storage.alignment;
	auto uniform_buffer_align = context_handle->limits().uniform.alignment;
	auto mapped_buffer_align  = context_handle->limits().memorymap.alignment;

	// create a staging buffer, this is allows for more advantagous resource access for the GPU
	core::resource::handle<gfx::buffer_t> stagingBuffer{};
	if(backend == graphics_backend::vulkan)
	{
		auto stagingBufferData = cache.create<data::buffer_t>(
			core::gfx::memory_usage::transfer_source,
			core::gfx::memory_property::host_visible | core::gfx::memory_property::host_coherent,
			memory::region{(size_t)128_mb, 4, new memory::default_allocator(false)});
		stagingBuffer = cache.create<gfx::buffer_t>(context_handle, stagingBufferData);
	}

	// create the buffers to store the model in
	// - memory region which we'll use to track the allocations, this is supposed to be virtual as we don't care to
	//   have a copy on the CPU
	// - then we create the vulkan buffer resource to interface with the GPU
	auto vertexBufferData = cache.create<data::buffer_t>(
		core::gfx::memory_usage::vertex_buffer | core::gfx::memory_usage::transfer_destination,
		core::gfx::memory_property::device_local, memory::region{256_mb, 4, new memory::default_allocator(false)});
	auto vertexBuffer = cache.create<gfx::buffer_t>(context_handle, vertexBufferData, stagingBuffer);

	auto indexBufferData = cache.create<data::buffer_t>(
		core::gfx::memory_usage::index_buffer | core::gfx::memory_usage::transfer_destination,
		core::gfx::memory_property::device_local, memory::region{128_mb, 4, new memory::default_allocator(false)});
	auto indexBuffer = cache.create<gfx::buffer_t>(context_handle, indexBufferData, stagingBuffer);

	auto dynamicInstanceBufferData = cache.create<data::buffer_t>(
		core::gfx::memory_usage::vertex_buffer,
								   core::gfx::memory_property::host_visible | core::gfx::memory_property::host_coherent,
								   memory::region{128_mb, 4, new memory::default_allocator(false)});

	// instance buffer for vertex data, these are unique per streamed instance of a geometry in a shader
	auto instanceBufferData = cache.create<data::buffer_t>(
		core::gfx::memory_usage::vertex_buffer | core::gfx::memory_usage::transfer_destination,
		core::gfx::memory_property::device_local, memory::region{128_mb, 4, new memory::default_allocator(false)});
	auto instanceBuffer = cache.create<gfx::buffer_t>(context_handle, instanceBufferData, stagingBuffer);

	// instance buffer for material data, these are shared over all instances of a given material bind (over all
	// instances in the invocation)
	auto instanceMaterialBufferData = cache.create<data::buffer_t>(
		core::gfx::memory_usage::uniform_buffer | core::gfx::memory_usage::transfer_destination,
		core::gfx::memory_property::device_local,
		memory::region{8_mb, uniform_buffer_align, new memory::default_allocator(false)});
	auto instanceMaterialBuffer =
		cache.create<gfx::buffer_t>(context_handle, instanceMaterialBufferData, stagingBuffer);
	auto intanceMaterialBinding = cache.create<gfx::shader_buffer_binding>(instanceMaterialBuffer, 8_mb);
	cache.library().set(intanceMaterialBinding.uid(), core::data::material_t::MATERIAL_DATA);


	render_graph renderGraph{};
	auto swapchain_pass = renderGraph.create_drawpass(context_handle, swapchain_handle);

	// using psl::ecs::state;
	using namespace core::ecs::components;
	using namespace core::ecs::systems;
	using namespace psl::ecs;
	psl::ecs::state_t ECSState{};
	geometry_instancing geometry_instancing_system{ECSState};
	fly fly_system{ECSState, surface_handle->input()};

	/* create editor camera */
	auto eCam = ECSState.create(
		1,
		[](transform& value) {
			value		   = transform{};
			value.position = {40, 15, 150};
			value.rotation = psl::math::look_at_q(value.position, psl::vec3::zero, psl::vec3::up);
		},
		empty<camera>{}, empty<input_tag>{});

	size_t frame{0};
	std::chrono::duration<float> elapsed{};
	auto last_tick = std::chrono::high_resolution_clock::now();
	while(!should_exit && surface_handle->tick())
	{
		std::cout << frame << std::endl;
		ECSState.tick(elapsed);
		renderGraph.present();

		auto current_time = std::chrono::high_resolution_clock::now();
		elapsed			  = std::chrono::duration_cast<std::chrono::duration<float>>(current_time - last_tick);
		last_tick		  = current_time;

		++frame;
	}

	std::cout << "[gassembler] closing graphical assembler.." << std::endl;
	gBackend = graphics_backend::undefined;
}

template <typename T>
struct option
{
	T value{};
	std::vector<T> options{};
};

core::gfx::graphics_backend parse(const std::string& value)
{
	if(value == "") return core::gfx::graphics_backend::undefined;
	if(value == "vulkan") return core::gfx::graphics_backend::vulkan;
	if(value == "gles") return core::gfx::graphics_backend::gles;
	return core::gfx::graphics_backend::undefined;
}

int main(int argc, char* argv[])
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);

	setup_loggers();

	assembler::log = std::make_shared<spdlog::logger>("", std::make_shared<spdlog::sinks::stdout_color_sink_st>());
	assembler::log->set_pattern("%v");
#ifdef WIN32
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);

	// std::wstring res = L"Стоял";


	CONSOLE_FONT_INFOEX cfi;
	cfi.cbSize		 = sizeof cfi;
	cfi.nFont		 = 0;
	cfi.dwFontSize.X = 0;
	cfi.dwFontSize.Y = 14;
	cfi.FontFamily	 = FF_MODERN;
	cfi.FontWeight	 = FW_NORMAL;
	wcscpy_s(cfi.FaceName, L"Consolas");
	SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &cfi);

	/*psl::string x2 = _T("Árvíztűrő tükörfúrógép");
	psl::string x3 = _T("кошка 日本国 أَبْجَدِيَّة عَرَبِيَّة‎中文");
	psl::string x4 = _T("你爱我");*/
	//_setmode(_fileno(stdout), _O_U16TEXT);
	//_setmode(_fileno(stdout), _O_U8TEXT);
#endif

	assembler::log->info(
		"welcome to assembler, use -h or --help to get information on the commands.\nyou can also pass "
		"the specific command (or its chain) after --help to get more information of that specific "
		"command, such as '--help generate shader'.\n");

	assembler::generators::shader shader_gen{};
	assembler::generators::models model_gen{};
	assembler::generators::meta meta_gen{};

	psl::cli::pack generator_pack{
		value<pack>{"shader", "glsl to spir-v compiler", {"shader", "s"}, std::move(shader_gen.pack())},
		value<pack>{"model", "model importer", {"models", "g"}, std::move(model_gen.pack())},
		value<pack>{"library", "meta library generator", {"library", "l"}, meta_gen.library_pack()},
		value<pack>{"meta", "meta file generator", {"meta", "m"}, meta_gen.meta_pack()}};

	psl::cli::pack root{value<bool>{"exit", "quits the application", {"exit", "quit", "q"}, false},
						value<std::string>{"graphical assembler",
										   "launches the graphical editor (only one can be created)",
										   {"geditor", "gassembler"},
										   "",
										   true,
										   {{"vulkan", "gles"}}},
						value<pack>{"generator", "generator for various data files", {"generate", "g"}, generator_pack}

	};

	psl::array<psl::string_view> commands;
	for(auto i = 1; i < argc; ++i) commands.emplace_back(argv[i]);
	if(!commands.empty()) root.parse(commands);


	std::thread geditor_thread;


	while(!root["exit"]->as<bool>().get())
	{
		if(root["graphical assembler"]->as<std::string>().get() != "")
		{
			if(gBackend == graphics_backend::undefined)
			{
				gBackend = parse(root["graphical assembler"]->as<std::string>().get());
				if(geditor_thread.joinable()) geditor_thread.join();
				geditor_thread = std::thread{launch_gassembler, gBackend.load()};
			}
			else
			{
				assembler::log->error("ERROR: a graphical editor is already running");
			}
		}
		psl::array<psl::string_view> commands = utility::string::split(get_input(), ("|"));
		root.parse(commands);
	}
	should_exit = true;
	if(geditor_thread.joinable()) geditor_thread.join();

	return 0;
}
