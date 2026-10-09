# 表示グループはディレクトリ単位のため、各ターゲットを作成したディレクトリで設定する。
function(kt_forge_shader_groups target)
    get_target_property(_headers ${target} KT_FORGE_SHADER_HEADERS)
    if(NOT _headers)
        return()
    endif()
    foreach(_config IN LISTS CMAKE_CONFIGURATION_TYPES)
        foreach(_header IN LISTS _headers)
            # CMake 4.2でも使えるよう、ビルド構成ごとのパスを明示的に展開する。
            string(REPLACE "$<CONFIG>" "${_config}" _configured_header "${_header}")
            source_group("Generated Shaders/${_config}" FILES "${_configured_header}")
        endforeach()
    endforeach()
endfunction()

# 1回の呼び出しで、埋め込み用のシェーダーを1つ登録する。
# ソース・明示指定したinclude依存・コンパイラ本体の変更時に再生成する。
# 生成物はビルド構成とビルド先ディレクトリごとに分ける。
# 生成規則と共通の生成ターゲットを同じディレクトリに置くため、
# 同じ製品のシェーダー登録は1つのCMakeディレクトリに集約する。
function(kt_forge_shader target source symbol profile entry)
    if(NOT symbol MATCHES "^[A-Za-z_][A-Za-z0-9_]*$" OR
       NOT entry MATCHES "^[A-Za-z_][A-Za-z0-9_]*$" OR
       NOT profile MATCHES "^(vs|ps)_6_0$")
        message(FATAL_ERROR "Unsupported built-in shader symbol/profile/entry")
    endif()
    get_filename_component(_source "${source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT EXISTS "${_source}")
        message(FATAL_ERROR "Shader source missing: ${_source}")
    endif()
    set_source_files_properties("${_source}" TARGET_DIRECTORY ${target} PROPERTIES HEADER_FILE_ONLY TRUE)
    get_filename_component(_sdk_root
        "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]" ABSOLUTE)
    find_program(KT_FORGE_DXC_EXECUTABLE NAMES dxc
        HINTS "${_sdk_root}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64"
            "$ENV{WindowsSdkVerBinPath}/x64" REQUIRED)
    get_filename_component(_dxc_dir "${KT_FORGE_DXC_EXECUTABLE}" DIRECTORY)
    set(_compiler_inputs "${KT_FORGE_DXC_EXECUTABLE}")
    foreach(_dll dxcompiler.dll dxil.dll)
        if(EXISTS "${_dxc_dir}/${_dll}")
            list(APPEND _compiler_inputs "${_dxc_dir}/${_dll}")
        endif()
    endforeach()
    set(_dir "${CMAKE_CURRENT_BINARY_DIR}/Shaders/${symbol}/$<CONFIG>")
    set(_header "${_dir}/${symbol}.h")
    add_custom_command(OUTPUT "${_header}"
        BYPRODUCTS "${_dir}/${symbol}.dxil"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_dir}"
        COMMAND "${KT_FORGE_DXC_EXECUTABLE}" -T "${profile}" -E "${entry}"
            -HV 2021 -Ges -WX -Zpr "$<IF:$<CONFIG:Debug>,-Od,-O3>"
            "$<$<CONFIG:Debug>:-Zi>" "$<$<CONFIG:Debug>:-Qembed_debug>"
            -Fh "${_header}" -Fo "${_dir}/${symbol}.dxil" -Vn "${symbol}" "${_source}"
        DEPENDS "${_source}" ${ARGN} ${_compiler_inputs}
        COMMAND_EXPAND_LISTS VERBATIM)
    # 全シェーダーの生成規則を1つのターゲットにまとめ、C++の依存関係スキャンより先に実行する。
    # 生成ヘッダーをソースとして登録し、各シェーダーの差分ビルドを維持する。
    set(_shaders "${target}_Shaders")
    if(NOT TARGET ${_shaders})
        add_custom_target(${_shaders})
        add_dependencies(${target} ${_shaders})
    else()
        get_target_property(_shader_source_dir ${_shaders} SOURCE_DIR)
        if(NOT "${_shader_source_dir}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
            message(FATAL_ERROR "Register all ${target} shaders in ${_shader_source_dir}")
        endif()
    endif()
    target_sources(${_shaders} PRIVATE "${_header}")
    target_sources(${target} PRIVATE "${_header}")
    set_property(TARGET ${_shaders} ${target} APPEND PROPERTY KT_FORGE_SHADER_HEADERS "${_header}")
    kt_forge_shader_groups(${_shaders})
    target_include_directories(${target} PRIVATE "${_dir}")
endfunction()
