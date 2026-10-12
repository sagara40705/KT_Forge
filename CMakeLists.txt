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

# Microsoft DirectXTex May 2026 / MIT。画像処理だけを組み込み、付属toolとDX11/12 helperは使用しない。
function(kt_forge_directxtex)
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TOOLS OFF)
    set(BUILD_SAMPLE OFF)
    set(BUILD_DX11 OFF)
    set(BUILD_DX12 OFF)
    set(BC_USE_OPENMP OFF)
    FetchContent_Declare(directxtex
        GIT_REPOSITORY https://github.com/microsoft/DirectXTex.git
        GIT_TAG 4feb3e11a020f35b796fc769a74216a555d4f5ef
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(directxtex)
endfunction()
kt_forge_directxtex()

# Assimp 6.0.5 / BSD-3-Clause。取込だけを静的リンクし、viewer・exporter・testは組み込まない。
function(kt_forge_assimp)
    set(BUILD_SHARED_LIBS OFF)
    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "Assimp tests" FORCE)
    set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "Assimp tools" FORCE)
    set(ASSIMP_INSTALL OFF CACHE BOOL "Assimp install" FORCE)
    set(ASSIMP_NO_EXPORT ON CACHE BOOL "Assimp import only" FORCE)
    set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "Assimp upstream warnings" FORCE)
    set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "Assimp bundled zlib" FORCE)
    FetchContent_Declare(assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG 392a658f9c271be965271f45e7521a1b80ea4392
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(assimp)
endfunction()
kt_forge_assimp()

# meshoptimizer 1.3 / MIT。静的Mesh最適化だけを使用し、付属toolとinstallは生成しない。
function(kt_forge_meshoptimizer)
    set(MESHOPT_BUILD_DEMO OFF)
    set(MESHOPT_BUILD_GLTFPACK OFF)
    set(MESHOPT_BUILD_SHARED_LIBS OFF)
    set(MESHOPT_INSTALL OFF)
    FetchContent_Declare(meshoptimizer
        GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
        GIT_TAG 9e1f07b159d3cb777f1c67ed31fc11fd117986f4
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(meshoptimizer)
endfunction()
kt_forge_meshoptimizer()

# Engineの公開ヘッダーはSourceを基準にする。別ゲームからも同じAPIを利用する。
add_library(Engine STATIC)
add_library(KT::Engine ALIAS Engine)
include(cmake/OzzAnimation.cmake)
include(cmake/Effekseer.cmake)
include(cmake/RmlUi.cmake)
include(cmake/JoltPhysics.cmake)
include(cmake/ShaderBuild.cmake)
include(cmake/RuntimeAssets.cmake)
add_subdirectory(Source)
target_compile_features(Engine PUBLIC cxx_std_20)
target_include_directories(Engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")
# 公開GraphicsヘッダーでもSDKより先に同じDirectX-Headersを探索する。
target_include_directories(Engine BEFORE PUBLIC "${directx_headers_SOURCE_DIR}/include/directx")
target_compile_options(Engine PRIVATE /W4 /utf-8)
target_compile_definitions(Engine PUBLIC GLFW_INCLUDE_NONE)
target_link_libraries(Engine PUBLIC Microsoft::DirectX-Headers)
target_link_libraries(Engine PRIVATE glfw d3d12 dxgi nlohmann_json::nlohmann_json
    DirectXTex windowscodecs ole32 assimp::assimp meshoptimizer bcrypt)
set_target_properties(Engine PROPERTIES CXX_EXTENSIONS OFF)

get_target_property(_engine_sources Engine SOURCES)
list(FILTER _engine_sources INCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/Source/")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" PREFIX "Source" FILES ${_engine_sources})
kt_forge_shader_groups(Engine)

# 別ゲームから取り込む場合はEngineだけを作り、必要なときだけSandboxを選ぶ。
option(KT_FORGE_BUILD_SANDBOX "Build the first Engine application" ${PROJECT_IS_TOP_LEVEL})
if(KT_FORGE_BUILD_SANDBOX)
    add_subdirectory(Tools)
    add_subdirectory(Sandbox)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT Sandbox)
endif()