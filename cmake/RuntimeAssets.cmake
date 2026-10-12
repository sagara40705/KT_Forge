# Engine標準素材とゲーム素材をexe横の独立rootへ置く。別ゲームも同じ関数を使える。
function(kt_forge_runtime_assets target project_assets)
    set(engine_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
    add_custom_target(${target}_Assets
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/EngineAssets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${project_assets}" "$<TARGET_FILE_DIR:${target}>/ProjectAssets"
        VERBATIM)
    add_dependencies(${target} ${target}_Assets)
    if(EXISTS "${engine_root}/Assets")
        add_custom_command(TARGET ${target}_Assets POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${engine_root}/Assets" "$<TARGET_FILE_DIR:${target}>/EngineAssets"
            VERBATIM)
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} "-Ddlls=$<TARGET_RUNTIME_DLLS:${target}>"
            "-Ddestination=$<TARGET_FILE_DIR:${target}>" -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/CopyRuntimeDlls.cmake"
        VERBATIM)
endfunction()
