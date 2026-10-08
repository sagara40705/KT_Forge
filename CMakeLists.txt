cmake_minimum_required(VERSION 4.2)

project(KT_Forge LANGUAGES C CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "KT_Forge requires Windows and MSVC.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "KT_Forge requires a 64-bit build. Use the vs2026-x64 preset.")
endif()

include(FetchContent)

# GLFW 3.5.1: official release pinned to its immutable commit.
# FetchContent keeps dependency sources and outputs in build/_deps.
set(GLFW_LIBRARY_TYPE STATIC CACHE STRING "GLFW library type" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build GLFW examples" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build GLFW tests" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build GLFW documentation" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "Generate GLFW install rules" FORCE)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG d9d6f0f1f967807ffade6598ea9a631ebaf37a56
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(glfw)

# One product target; module directories only register explicit sources.
add_executable(KT_Forge)
include(cmake/ShaderBuild.cmake)
add_subdirectory(Source)
target_compile_features(KT_Forge PRIVATE cxx_std_20)
target_include_directories(KT_Forge PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_options(KT_Forge PRIVATE /W4 /utf-8)
target_compile_definitions(KT_Forge PRIVATE GLFW_INCLUDE_NONE)
target_link_libraries(KT_Forge PRIVATE glfw d3d12 dxgi)

set_target_properties(KT_Forge PROPERTIES
    CXX_EXTENSIONS OFF
    # Keep Debug and Release outputs separate inside the existing build tree.
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
)

get_target_property(_forge_sources KT_Forge SOURCES)
list(FILTER _forge_sources INCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/Source/")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" PREFIX "Source" FILES ${_forge_sources})
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT KT_Forge)
