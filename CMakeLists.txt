cmake_minimum_required(VERSION 3.11)
set(PROJECT_ASSEMBLER assembler CACHE INTERNAL "")
set(LOCAL_PROJECT ${PROJECT_ASSEMBLER})
project(${LOCAL_PROJECT} VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)
# -------------------------------------------------------------------------------------------------
# Paradigm Engine setup
# -------------------------------------------------------------------------------------------------

# required call to get the paradigm engine core here, and setting up the compile defines
SET (BUILD_SHARED_LIBS OFF)
add_subdirectory(paradigm)

get_directory_property(PE_COMPILE_OPTIONS DIRECTORY paradigm DEFINITION PE_COMPILE_OPTIONS)
get_directory_property(PE_COMPILE_OPTIONS_EXE DIRECTORY paradigm DEFINITION PE_COMPILE_OPTIONS_EXE)
get_directory_property(PE_COMPILER_FEATURES DIRECTORY paradigm DEFINITION PE_COMPILER_FEATURES)
get_directory_property(PE_DL_LIBS DIRECTORY paradigm DEFINITION PE_DL_LIBS)
get_directory_property(PROJECT_PSL DIRECTORY paradigm DEFINITION PROJECT_PSL)
get_directory_property(PROJECT_CORE DIRECTORY paradigm DEFINITION PROJECT_CORE)
get_directory_property(PE_DEFINES DIRECTORY paradigm DEFINITION PE_DEFINES)

set_property(TARGET ${PROJECT_PSL} PROPERTY FOLDER "paradigm")
set_property(TARGET ${PROJECT_CORE} PROPERTY FOLDER "paradigm")

# -------------------------------------------------------------------------------------------------
# Assimp setup
# -------------------------------------------------------------------------------------------------

FetchContent_Declare(assimp GIT_REPOSITORY https://github.com/assimp/assimp GIT_TAG v4.1.0 GIT_SHALLOW TRUE GIT_PROGRESS TRUE)
FetchContent_GetProperties(assimp)
if(NOT assimp_POPULATED)
	message(STATUS "fetching assimp/assimp...")
	FetchContent_Populate(assimp)
	add_subdirectory(${assimp_SOURCE_DIR} ${assimp_BINARY_DIR})
	message(STATUS "assimp/assimp fetched")
endif()

# -------------------------------------------------------------------------------------------------
# glslang setup
# -------------------------------------------------------------------------------------------------

FetchContent_Declare(glslang GIT_REPOSITORY https://github.com/KhronosGroup/glslang GIT_SHALLOW TRUE GIT_PROGRESS TRUE)
FetchContent_GetProperties(glslang)
if(NOT glslang_POPULATED)
	message(STATUS "fetching KhronosGroup/glslang...")
	FetchContent_Populate(glslang)
	#EXECUTE_PROCESS(COMMAND ${PYTHON_EXECUTABLE} update_glslang_sources.py WORKING_DIRECTORY ${glslang_SOURCE_DIR})
	set(BUILD_SHARED_LIBS OFF)
	set(ENABLE_GLSLANG_BINARIES ON)
	set(ENABLE_HLSL OFF)
	add_subdirectory(${glslang_SOURCE_DIR} ${glslang_BINARY_DIR})
	message(STATUS "KhronosGroup/glslang fetched")
endif()

# -------------------------------------------------------------------------------------------------
# SPIRV-Cross setup
# -------------------------------------------------------------------------------------------------

FetchContent_Declare(spirvcross GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross GIT_SHALLOW TRUE GIT_PROGRESS TRUE)
FetchContent_GetProperties(spirvcross)
if(NOT spirvcross_POPULATED)
	message(STATUS "fetching KhronosGroup/SPIRV-Cross ...")
	FetchContent_Populate(spirvcross)
	add_subdirectory(${spirvcross_SOURCE_DIR} ${spirvcross_BINARY_DIR})
	message(STATUS "KhronosGroup/SPIRV-Cross  fetched")
endif()

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

add_definitions(${PE_DEFINES})
add_executable(${LOCAL_PROJECT} ${INC} ${SRC} ${GENERATOR_INFORMATION} ${NATVIS})

set_property(TARGET ${LOCAL_PROJECT} PROPERTY FOLDER "tools")
add_dependencies(${LOCAL_PROJECT} glslang)
target_link_libraries(${LOCAL_PROJECT} PUBLIC 
	${PROJECT_PSL} ${PE_DL_LIBS} ${PROJECT_CORE} assimp)
	

target_include_directories(${LOCAL_PROJECT}
	PUBLIC 
		${CMAKE_CURRENT_SOURCE_DIR}/inc
		${assimp_SOURCE_DIR}/include
		${assimp_BINARY_DIR}/include
)

target_compile_features(${LOCAL_PROJECT} PUBLIC ${PE_COMPILER_FEATURES})
target_compile_options(${LOCAL_PROJECT} PRIVATE ${PE_COMPILE_OPTIONS} ${PE_COMPILE_OPTIONS_EXE})