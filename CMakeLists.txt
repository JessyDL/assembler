cmake_minimum_required(VERSION 3.11)
set(PROJECT_ASSEMBLER assembler CACHE INTERNAL "")
set(LOCAL_PROJECT ${PROJECT_ASSEMBLER})
project(${LOCAL_PROJECT} VERSION 1.0.0 LANGUAGES CXX)

# -------------------------------------------------------------------------------------------------
# Assimp setup
# -------------------------------------------------------------------------------------------------

set(ASSIMP_PATH "" CACHE PATH "set the root directory of where assimp is")
if(ASSIMP_PATH STREQUAL  "")
	message(FATAL_ERROR "the assimp library needs to be installed, if it is, please check your installation as we couldn't find it.")
else()
	set(ASSIMP_INC_PATH "${ASSIMP_PATH}/include")
	set(ASSIMP_BIN_PATH "${ASSIMP_PATH}/bin/x64")
	set(ASSIMP_LIB "${ASSIMP_PATH}/lib/x64/assimp-vc140-mt.lib")
	message("assimp directory set at: ${ASSIMP_PATH}")
endif()

# -------------------------------------------------------------------------------------------------
# Paradigm Engine setup
# -------------------------------------------------------------------------------------------------

# we don't care to deploy on non-static platforms, as we are just building a toolchain, not
# the actual binary.
set(VK_STATIC true)
# required call to get the paradigm engine core here, and setting up the compile defines
include(paradigm.txt)


# -------------------------------------------------------------------------------------------------
# Build binary
# -------------------------------------------------------------------------------------------------

include(inc.txt)
include(src.txt)
include(generators.txt)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/inc" PREFIX "inc" FILES ${INC}) 
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "src" FILES ${SRC}) 
source_group(TREE "${paradigm_SOURCE_DIR}/${PARADIGM_ROOT}/core/src" PREFIX "src" FILES ${GENERATOR_INFORMATION}) 


include_directories(AFTER ${ASSIMP_INC_PATH})
include_directories(AFTER ${paradigm_SOURCE_DIR}/core/inc)
include_directories(${paradigm_SOURCE_DIR}/externals/inc/glm)
include_directories(${paradigm_SOURCE_DIR}/externals/inc/gli)
include_directories(${VULKAN_INC_PATH})

link_directories(${ASSIMP_BIN_PATH})

if(VS_ADD_NATIVE_VISUALIZERS)	
	file(GLOB_RECURSE NATVIS nvs/*.natvis)
	source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/nvs" PREFIX "natvis" FILES ${NATVIS}) 
endif()

add_executable(${LOCAL_PROJECT} ${INC} ${SRC} ${GENERATOR_INFORMATION} ${NATVIS})

if(PE_PCH AND MSVC)
	set_target_properties(${LOCAL_PROJECT} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "inc/stdafx.h")
	cotire(${LOCAL_PROJECT})
endif()

set_property(TARGET ${LOCAL_PROJECT} PROPERTY FOLDER "tools")


target_link_libraries(${LOCAL_PROJECT} PUBLIC 
	${VULKAN_LIB_NAME}  ${PROJECT_PSL} 
	${ASSIMP_LIB} ${PE_DL_LIBS} )


target_include_directories(${LOCAL_PROJECT}
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_compile_features(${LOCAL_PROJECT} PUBLIC ${PROJECT_COMPILER_FEATURES} PRIVATE ${PROJECT_COMPILER_FEATURES_PRIVATE})
target_compile_options(${LOCAL_PROJECT} PRIVATE ${COMPILE_OPTIONS} ${COMPILE_OPTIONS_EXE})