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
target_link_libraries(KT_Forge PRIVATE RmlUi::Core)

# OFL 1.1の日本語フォントを固定版・hashで取得し、製品のUI素材と同じ場所へ配置する。
set(KT_FORGE_UI_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(KT_FORGE_UI_FONT "${CMAKE_BINARY_DIR}/_deps/noto-font/NotoSansCJKjp-Regular.otf" CACHE FILEPATH "Pinned Japanese UI font")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_deps/noto-font")
if(NOT EXISTS "${KT_FORGE_UI_FONT}")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/notofonts/noto-cjk/523d033d6cb47f4a80c58a35753646f5c3608a78/Sans/OTF/Japanese/NotoSansCJKjp-Regular.otf"
        "${KT_FORGE_UI_FONT}"
        EXPECTED_HASH SHA256=68a3fc98800b2a27b371f2fb79991daf3633bd89309d4ffaa6946fd587f375b5
        TLS_VERIFY ON
    )
endif()
file(SHA256 "${KT_FORGE_UI_FONT}" ui_font_hash)
if(NOT ui_font_hash STREQUAL "68a3fc98800b2a27b371f2fb79991daf3633bd89309d4ffaa6946fd587f375b5")
    message(FATAL_ERROR "日本語UIフォントのSHA256が固定版と一致しません。")
endif()
# RML/RCSSだけの編集でも、再linkを待たずに次の起動用素材を更新する。
add_custom_target(KT_Forge_UIAssets
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${KT_FORGE_UI_ROOT}/Assets/UI" "$<TARGET_FILE_DIR:KT_Forge>/UI"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${KT_FORGE_UI_FONT}" "$<TARGET_FILE_DIR:KT_Forge>/UI/NotoSansCJKjp-Regular.otf"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${KT_FORGE_UI_ROOT}/ThirdParty/RmlUi" "$<TARGET_FILE_DIR:KT_Forge>/UI/Licenses"
    VERBATIM
)
add_dependencies(KT_Forge KT_Forge_UIAssets)
