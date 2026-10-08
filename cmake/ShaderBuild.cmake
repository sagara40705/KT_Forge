# One embedded shader per explicit call. Source/include dependencies and compiler
# binaries trigger regeneration; each configuration and binary tree is separate.
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
    # The product target is created in the root directory. An explicit producer
    # target carries this child-directory rule and runs before C++ module scans.
    add_custom_target(${target}_${symbol} DEPENDS "${_header}")
    add_dependencies(${target} ${target}_${symbol})
    target_sources(${target} PRIVATE "${_header}")
    target_include_directories(${target} PRIVATE "${_dir}")
endfunction()
