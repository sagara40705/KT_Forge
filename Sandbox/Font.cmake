# OFL 1.1の日本語フォントを固定版・hashで取得し、製品のUI素材と同じ場所へ配置する。

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
