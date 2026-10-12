# ozz-animation 0.16.0 / MIT。姿勢評価と取込用builderだけを静的リンクする。
function(kt_forge_ozz_animation)
    set(BUILD_SHARED_LIBS OFF)
    set(ozz_build_tools OFF CACHE BOOL "ozz tools" FORCE)
    set(ozz_build_fbx OFF CACHE BOOL "ozz FBX SDK" FORCE)
    set(ozz_build_gltf OFF CACHE BOOL "ozz glTF tools" FORCE)
    set(ozz_build_samples OFF CACHE BOOL "ozz samples" FORCE)
    set(ozz_build_howtos OFF CACHE BOOL "ozz howtos" FORCE)
    set(ozz_build_tests OFF CACHE BOOL "ozz tests" FORCE)
    set(ozz_build_data OFF CACHE BOOL "ozz sample data" FORCE)
    set(ozz_build_msvc_rt_dll ON CACHE BOOL "Match the product MSVC runtime" FORCE)
    FetchContent_Declare(ozz_animation
        GIT_REPOSITORY https://github.com/guillaumeblanc/ozz-animation.git
        GIT_TAG 6cbdc790123aa4731d82e255df187b3a8a808256
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(ozz_animation)
endfunction()
kt_forge_ozz_animation()
target_link_libraries(Engine PRIVATE ozz_animation ozz_animation_offline)
