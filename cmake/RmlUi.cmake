# RmlUi 6.3 / MIT、FreeType 2.14.1 / FTL。基本UIと通常フォントだけを静的リンクする。
function(kt_forge_rmlui)
    set(BUILD_SHARED_LIBS OFF)
    foreach(dependency ZLIB BZIP2 PNG HARFBUZZ BROTLI)
        set(FT_DISABLE_${dependency} ON CACHE BOOL "FreeType optional dependency" FORCE)
    endforeach()
    set(SKIP_INSTALL_ALL ON)
    FetchContent_Declare(freetype
        GIT_REPOSITORY https://github.com/freetype/freetype.git
        GIT_TAG 526ec5c47b9ebccc4754c85ac0c0cdf7c85a5e9b
        EXCLUDE_FROM_ALL SYSTEM
    )
    FetchContent_MakeAvailable(freetype)
    if(MSVC)
        target_compile_options(freetype PRIVATE /utf-8)
    endif()
    if(NOT TARGET Freetype::Freetype)
        add_library(Freetype::Freetype ALIAS freetype)
    endif()
    foreach(option RMLUI_SAMPLES RMLUI_TESTS RMLUI_LUA_BINDINGS RMLUI_LOTTIE_PLUGIN RMLUI_SVG_PLUGIN
            RMLUI_HARFBUZZ_SAMPLE RMLUI_TRACY_PROFILING)
        set(${option} OFF CACHE BOOL "RmlUi unused module" FORCE)
    endforeach()
    set(RMLUI_FONT_ENGINE freetype CACHE STRING "RmlUi font engine" FORCE)
    FetchContent_Declare(rmlui
        GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
        GIT_TAG ba95ffe8bfb6370efb2cdcca927eaad4710c5413
        EXCLUDE_FROM_ALL SYSTEM
    )
    FetchContent_MakeAvailable(rmlui)
endfunction()
kt_forge_rmlui()
target_link_libraries(Engine PRIVATE RmlUi::Core)
