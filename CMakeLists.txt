cmake_minimum_required(VERSION 4.2)

project(KT_Forge LANGUAGES C CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "KT_Forge requires Windows and MSVC.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "KT_Forge requires a 64-bit build. Use the vs2026-x64 preset.")
endif()

include(FetchContent)

# GLFW 3.5.1の公式リリースに対応するコミットを固定して使用する。
# FetchContentで取得する依存ソースと生成物はbuild/_depsに置く。
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

# Microsoft DirectX-Headers v1.614.1。d3dx12の付随ヘッダーも同じ版で取得する。
set(DXHEADERS_BUILD_TEST OFF CACHE BOOL "Build DirectX-Headers tests" FORCE)
set(DXHEADERS_BUILD_GOOGLE_TEST OFF CACHE BOOL "Build DirectX-Headers Google tests" FORCE)
set(DXHEADERS_INSTALL OFF CACHE BOOL "Generate DirectX-Headers install rules" FORCE)
FetchContent_Declare(directx_headers
    GIT_REPOSITORY https://github.com/microsoft/DirectX-Headers.git
    GIT_TAG 48a762973271c5a75869946bf1fdbc489a628a5c
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(directx_headers)

# SceneのJSON入出力に使用するnlohmann/json 3.12.0のコミットを固定する。
set(JSON_BuildTests OFF CACHE BOOL "JSON tests" FORCE)
set(JSON_Install OFF CACHE BOOL "JSON install" FORCE)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG 55f93686c01528224f448c19128836e7df245f72
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(nlohmann_json)

# 製品ターゲットは1つとし、各モジュールのディレクトリではソースを明示的に登録する。
add_executable(KT_Forge)
include(cmake/ShaderBuild.cmake)
add_subdirectory(Source)
target_compile_features(KT_Forge PRIVATE cxx_std_20)
target_include_directories(KT_Forge PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
# SDK版d3d12.hより先に、d3dx12.hと同じ版のD3D12ヘッダーを解決する。
target_include_directories(KT_Forge BEFORE PRIVATE "${directx_headers_SOURCE_DIR}/include/directx")
target_compile_options(KT_Forge PRIVATE /W4 /utf-8)
target_compile_definitions(KT_Forge PRIVATE GLFW_INCLUDE_NONE)
target_link_libraries(KT_Forge PRIVATE glfw Microsoft::DirectX-Headers d3d12 dxgi)
target_link_libraries(KT_Forge PRIVATE nlohmann_json::nlohmann_json)

set_target_properties(KT_Forge PROPERTIES
    CXX_EXTENSIONS OFF
    # 既存のビルド先ディレクトリ内で、DebugとReleaseの出力を分ける。
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
)

get_target_property(_forge_sources KT_Forge SOURCES)
list(FILTER _forge_sources INCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/Source/")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" PREFIX "Source" FILES ${_forge_sources})
kt_forge_shader_groups(KT_Forge)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT KT_Forge)
