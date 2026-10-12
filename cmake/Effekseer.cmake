# Effekseer 1.80.7 / MIT。C++ランタイムとDX12だけを静的リンクする。
function(kt_forge_effekseer)
    set(BUILD_SHARED_LIBS OFF)
    set(USE_MSVC_RUNTIME_LIBRARY_DLL ON CACHE BOOL "Match the product runtime" FORCE)
    foreach(option BUILD_VIEWER BUILD_EDITOR BUILD_TEST BUILD_EXAMPLES BUILD_EXAMPLE BUILD_TOOLS BUILD_TOOL
            BUILD_UNITYPLUGIN BUILD_GL BUILD_DX9 BUILD_DX11 BUILD_VULKAN BUILD_METAL BUILD_WEBGPU
            BUILD_WITH_EASY_PROFILER USE_LIBPNG_LOADER USE_OPENAL USE_XAUDIO2 USE_DSOUND USE_OSM NETWORK_ENABLED)
        set(${option} OFF CACHE BOOL "Effekseer unused module" FORCE)
    endforeach()
    set(BUILD_DX12 ON CACHE BOOL "Effekseer DX12" FORCE)
    FetchContent_Declare(effekseer
        GIT_REPOSITORY https://github.com/effekseer/Effekseer.git
        GIT_TAG b87d1a2e3ff3731ba25cad34f8d8abeae382b21f
        GIT_SUBMODULES Dev/Cpp/3rdParty/LLGI
        GIT_SUBMODULES_RECURSE OFF
        EXCLUDE_FROM_ALL
        SYSTEM
    )
    FetchContent_MakeAvailable(effekseer)
endfunction()
kt_forge_effekseer()
target_link_libraries(Engine PRIVATE EffekseerRendererDX12)
