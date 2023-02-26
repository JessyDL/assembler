cmake_minimum_required(VERSION 3.19)
set(PROJECT_ASSEMBLER assembler CACHE INTERNAL "")
set(LOCAL_PROJECT ${PROJECT_ASSEMBLER})
project(${LOCAL_PROJECT} VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# -------------------------------------------------------------------------------------------------
# Paradigm Engine setup
# -------------------------------------------------------------------------------------------------

# required call to get the paradigm engine core here, and setting up the compile defines
set(BUILD_SHARED_LIBS OFF)
set(PE_TESTS OFF CACHE INTERNAL "" FORCE)
set(PE_MAKE_EXE OFF)
add_subdirectory(paradigm)

get_directory_property(PE_COMPILE_OPTIONS DIRECTORY paradigm DEFINITION PE_COMPILE_OPTIONS)
get_directory_property(PE_COMPILE_OPTIONS_EXE DIRECTORY paradigm DEFINITION PE_COMPILE_OPTIONS_EXE)
get_directory_property(PE_COMPILER_FEATURES DIRECTORY paradigm DEFINITION PE_COMPILER_FEATURES)
get_directory_property(PE_DEFINES DIRECTORY paradigm DEFINITION PE_DEFINES)

add_subdirectory(extern)

# -------------------------------------------------------------------------------------------------
# Build binary
# -------------------------------------------------------------------------------------------------

include(inc.txt)
include(src.txt)
include(generators.txt)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/inc" PREFIX "inc" FILES ${INC}) 
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "src" FILES ${SRC}) 
source_group(TREE "${paradigm_SOURCE_DIR}/${PARADIGM_ROOT}/core/src" PREFIX "src" FILES ${GENERATOR_INFORMATION}) 

if(VS_ADD_NATIVE_VISUALIZERS)	
	file(GLOB_RECURSE NATVIS nvs/*.natvis)
	source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/nvs" PREFIX "natvis" FILES ${NATVIS}) 
endif()

add_executable(${LOCAL_PROJECT} ${INC} ${SRC} ${GENERATOR_INFORMATION} ${NATVIS})

set_property(TARGET ${LOCAL_PROJECT} PROPERTY FOLDER "tools")
target_link_libraries(${LOCAL_PROJECT} PUBLIC
	paradigm::psl
	paradigm::core
	assimp
	spirv-cross-reflect					# reflect spirv
	spirv-cross-glsl					# spirv to gles
	SPIRV								# glsl to spirv
	glslang-default-resource-limits		# default resources required to feed to glsl compiler
)

target_include_directories(${LOCAL_PROJECT}
	PUBLIC 
		${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_compile_features(${LOCAL_PROJECT} PUBLIC ${PE_COMPILER_FEATURES})
target_compile_options(${LOCAL_PROJECT} PRIVATE ${PE_COMPILE_OPTIONS} ${PE_COMPILE_OPTIONS_EXE})