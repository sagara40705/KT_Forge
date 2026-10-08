# Compile and embed the two small built-in shaders. No runtime paths or DXC DLL dependency.
function(kt_forge_triangle_shaders target)
    get_filename_component(_sdk_root
        "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]" ABSOLUTE)
    find_program(KT_FORGE_DXC_EXECUTABLE NAMES dxc
        HINTS "${_sdk_root}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64"
            "$ENV{WindowsSdkVerBinPath}/x64"
        REQUIRED)
    execute_process(COMMAND "${KT_FORGE_DXC_EXECUTABLE}" --version
        RESULT_VARIABLE _dxc_result OUTPUT_VARIABLE _dxc_version OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _dxc_result EQUAL 0)
        message(FATAL_ERROR "Cannot run DXC: ${KT_FORGE_DXC_EXECUTABLE}")
    endif()
    message(STATUS "Triangle shader compiler: ${KT_FORGE_DXC_EXECUTABLE} (${_dxc_version})")

    set(_shader_source "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../Source/Graphics/Shaders/Triangle.hlsl")
    set(_shader_dir "${CMAKE_CURRENT_BINARY_DIR}/TriangleShaders/$<CONFIG>")
    get_filename_component(_dxc_dir "${KT_FORGE_DXC_EXECUTABLE}" DIRECTORY)
    set(_compiler_inputs "${KT_FORGE_DXC_EXECUTABLE}")
    foreach(_dll dxcompiler.dll dxil.dll)
        if(EXISTS "${_dxc_dir}/${_dll}")
            list(APPEND _compiler_inputs "${_dxc_dir}/${_dll}")
        endif()
    endforeach()
    foreach(_stage VS PS)
        if(_stage STREQUAL "VS")
            set(_profile vs_6_0)
        else()
            set(_profile ps_6_0)
        endif()
        set(_output "${_shader_dir}/Triangle${_stage}.h")
        add_custom_command(OUTPUT "${_output}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${_shader_dir}"
            COMMAND "${KT_FORGE_DXC_EXECUTABLE}" -T "${_profile}" -E "${_stage}Main"
                -HV 2021 -Ges -WX "$<IF:$<CONFIG:Debug>,-Od,-O3>"
                "$<$<CONFIG:Debug>:-Zi>" "$<$<CONFIG:Debug>:-Qembed_debug>"
                -Fh "${_output}" -Vn "Triangle${_stage}" "${_shader_source}"
            DEPENDS "${_shader_source}" ${_compiler_inputs}
            COMMAND_EXPAND_LISTS VERBATIM)
        target_sources(${target} PRIVATE "${_output}")
    endforeach()
    target_include_directories(${target} PRIVATE "${_shader_dir}")
endfunction()
