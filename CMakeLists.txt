cmake_minimum_required(VERSION 3.11)
set(PROJECT_ASSEMBLER assembler CACHE INTERNAL "")
set(LOCAL_PROJECT ${PROJECT_ASSEMBLER})
project(${LOCAL_PROJECT} VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)

if(MSVC) # we currently make no distinction for CLang (todo)
	add_compile_options(/MP)
endif()
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
get_directory_property(PE_DL_LIBS DIRECTORY paradigm DEFINITION PE_DL_LIBS)
get_directory_property(PROJECT_PSL DIRECTORY paradigm DEFINITION PROJECT_PSL)
get_directory_property(PROJECT_CORE DIRECTORY paradigm DEFINITION PROJECT_CORE)
get_directory_property(PE_DEFINES DIRECTORY paradigm DEFINITION PE_DEFINES)

set_property(TARGET ${PROJECT_PSL} PROPERTY FOLDER "paradigm")
set_property(TARGET ${PROJECT_CORE} PROPERTY FOLDER "paradigm")

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

add_definitions(${PE_DEFINES})
add_executable(${LOCAL_PROJECT} ${INC} ${SRC} ${GENERATOR_INFORMATION} ${NATVIS})

set_property(TARGET ${LOCAL_PROJECT} PROPERTY FOLDER "tools")
add_dependencies(${LOCAL_PROJECT} glslang)
target_link_libraries(${LOCAL_PROJECT} PUBLIC 
	${PE_DL_LIBS} psl core assimp nlohmann_json)

target_include_directories(${LOCAL_PROJECT}
	PUBLIC 
		${CMAKE_CURRENT_SOURCE_DIR}/inc
		${assimp_SOURCE_DIR}/include
		${assimp_BINARY_DIR}/include
)

target_compile_features(${LOCAL_PROJECT} PUBLIC ${PE_COMPILER_FEATURES})
target_compile_options(${LOCAL_PROJECT} PRIVATE ${PE_COMPILE_OPTIONS} ${PE_COMPILE_OPTIONS_EXE})