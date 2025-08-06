cmake_minimum_required(VERSION 3.19)
set(PROJECT_ASSEMBLER assembler CACHE INTERNAL "")
set(LOCAL_PROJECT ${PROJECT_ASSEMBLER})


find_package(Python3 REQUIRED)
execute_process(
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/.github/release.py --version
    OUTPUT_VARIABLE PROJECT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE RELEASE_RESULT
)
if(NOT RELEASE_RESULT EQUAL 0 OR PROJECT_VERSION STREQUAL "")
    set(PROJECT_VERSION "0.0.0")
endif()
project(${LOCAL_PROJECT} VERSION ${PROJECT_VERSION} LANGUAGES CXX)

include(FetchContent)
include(CMakeDependentOption)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(AS_ENABLE_WGSL "Enable WGSL support" OFF)

set(CMAKE_CXX_EXTENSIONS OFF )
set(CXX_EXTENSIONS OFF )
set(CMAKE_CXX_STANDARD 23)
option(AS_DEVMODE "Enable this for slightly easier dev experience where the app doesn't auto-shut by default" OFF)

if(TARGET paradigm::core)
  set(PARADIGM_AVAILABLE TRUE)
else()
  set(PARADIGM_AVAILABLE FALSE)
endif()

cmake_dependent_option(AS_PARADIGM_EXTERNAL "Enable this if you are including this as a sub-project of paradigm" ON "PARADIGM_AVAILABLE" OFF)
cmake_dependent_option(AS_ENABLE_INSTALL "Enable installation "
    ON
    "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR"
    OFF
)

add_subdirectory(extern)

# -------------------------------------------------------------------------------------------------
# Paradigm Engine setup
# -------------------------------------------------------------------------------------------------

if(NOT AS_PARADIGM_EXTERNAL)
	set(PE_TESTS OFF CACHE INTERNAL "" FORCE)
	set(PE_MAKE_EXE OFF)
	set(PE_WEBGPU OFF)
	set(PE_WEBGPU_NATIVE OFF)
	set(PE_ASSEMBLER OFF) # circular dependency, do not enable
	set(PE_INCLUDED_BY_ASSEMBLER ON) # triggers the right dawn setup, which paradigm will provide

	FetchContent_Declare(paradigm
		GIT_REPOSITORY https://github.com/JessyDL/paradigm.git
		GIT_TAG 7ad796242c0dc7264802356b99fe456ac5fbbb49
		GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
		GIT_SUBMODULES_RECURSE TRUE
	)
	FetchContent_MakeAvailable(paradigm)
endif()

# -------------------------------------------------------------------------------------------------
# Build binary
# -------------------------------------------------------------------------------------------------

include(inc.txt)
include(src.txt)
include(generators.txt)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/inc" PREFIX "inc" FILES ${INC})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "src" FILES ${SRC})
if(NOT AS_PARADIGM_EXTERNAL)
	source_group(TREE "${paradigm_SOURCE_DIR}/${PARADIGM_ROOT}/core/src" PREFIX "src" FILES ${GENERATOR_INFORMATION})
endif()

if(VS_ADD_NATIVE_VISUALIZERS)	
	file(GLOB_RECURSE NATVIS nvs/*.natvis)
	source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/nvs" PREFIX "natvis" FILES ${NATVIS}) 
endif()

add_executable(${LOCAL_PROJECT} ${INC} ${SRC} ${GENERATOR_INFORMATION} ${NATVIS})

if(${AS_ENABLE_INSTALL})
	install(TARGETS ${LOCAL_PROJECT} RUNTIME DESTINATION bin)
endif()

set_property(TARGET ${LOCAL_PROJECT} PROPERTY FOLDER "tools")
target_link_libraries(${LOCAL_PROJECT} PUBLIC
	paradigm::psl
	paradigm::core
	assimp
	spirv-cross-reflect					# reflect spirv
	spirv-cross-glsl					# spirv to gles
	SPIRV								# glsl to spirv
	glslang                             # glsl to spirv
	glslang-default-resource-limits		# default resources required to feed to glsl compiler
	$<$<BOOL:${AS_ENABLE_WGSL}>:tint_lang_wgsl_writer tint_lang_spirv_reader>
)

target_include_directories(${LOCAL_PROJECT}
	PUBLIC 
		${CMAKE_CURRENT_SOURCE_DIR}/inc
)

execute_process(
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/.github/release.py --sha1
	OUTPUT_VARIABLE GIT_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

target_compile_definitions(${LOCAL_PROJECT}
	PUBLIC
		$<$<BOOL:${AS_ENABLE_WGSL}>:AS_ENABLE_WGSL>
	PRIVATE
		AS_VERSION=\"${PROJECT_VERSION}\"
		AS_VERSION_SHA=\"${PROJECT_VERSION}-${GIT_SHA}\"
		$<$<BOOL:${AS_DEVMODE}>:AS_DEVMODE>
)

message(STATUS "Building ${LOCAL_PROJECT} version ${PROJECT_VERSION} (${GIT_SHA})")
