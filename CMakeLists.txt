cmake_minimum_required(VERSION 3.19)
set(PROJECT_ASSEMBLER assembler CACHE INTERNAL "")
set(LOCAL_PROJECT ${PROJECT_ASSEMBLER})


find_package(Python3 REQUIRED)
execute_process(
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/.github/release.py --version
    OUTPUT_VARIABLE PROJECT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE RELEASE_RESULT
)
if(NOT RELEASE_RESULT EQUAL 0 OR PROJECT_VERSION STREQUAL "")
    set(PROJECT_VERSION "0.0.0")
endif()
project(${LOCAL_PROJECT} VERSION ${PROJECT_VERSION} LANGUAGES CXX)

include(FetchContent)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(AS_ENABLE_WGSL "Enable WGSL support" ON)

set(CMAKE_CXX_EXTENSIONS OFF )
set(CXX_EXTENSIONS OFF )
set(CMAKE_CXX_STANDARD 23)
option(AS_PARADIGM_EXTERNAL "Enable this if you are including this as a sub-project of paradigm" OFF)

# -------------------------------------------------------------------------------------------------
# Paradigm Engine setup
# -------------------------------------------------------------------------------------------------

if(NOT AS_PARADIGM_EXTERNAL)
	# required call to get the paradigm engine core here, and setting up the compile defines
	set(BUILD_SHARED_LIBS OFF)
	set(PE_TESTS OFF CACHE INTERNAL "" FORCE)
	set(PE_MAKE_EXE OFF)
	add_subdirectory(paradigm)
	
	get_directory_property(PE_COMPILE_OPTIONS DIRECTORY paradigm DEFINITION PE_COMPILE_OPTIONS)
	get_directory_property(PE_COMPILE_OPTIONS_EXE DIRECTORY paradigm DEFINITION PE_COMPILE_OPTIONS_EXE)
	get_directory_property(PE_COMPILER_FEATURES DIRECTORY paradigm DEFINITION PE_COMPILER_FEATURES)
	get_directory_property(PE_DEFINES DIRECTORY paradigm DEFINITION PE_DEFINES)
endif()
add_subdirectory(extern)

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
install(TARGETS ${LOCAL_PROJECT} RUNTIME DESTINATION bin)

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
)

message(STATUS "Building ${LOCAL_PROJECT} version ${PROJECT_VERSION} (${GIT_SHA})")

target_compile_features(${LOCAL_PROJECT} PUBLIC ${PE_COMPILER_FEATURES})
target_compile_options(${LOCAL_PROJECT} PUBLIC ${PE_COMPILE_OPTIONS} ${PE_COMPILE_OPTIONS_EXE})
