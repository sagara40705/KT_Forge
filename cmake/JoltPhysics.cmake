# AI支援による接続。Jolt本体の権利表記はThirdParty/JoltPhysics/LICENSEへ保持する。
function(kt_forge_jolt)
    set(JPH_BUILD_SHARED_LIBS OFF)
    set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF)
    set(OVERRIDE_CXX_FLAGS OFF)
    set(INTERPROCEDURAL_OPTIMIZATION OFF)
    set(CPP_EXCEPTIONS_ENABLED ON)
    set(CPP_RTTI_ENABLED ON)
    set(DOUBLE_PRECISION OFF)
    set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF)
    set(ENABLE_OBJECT_STREAM OFF)
    set(ENABLE_INSTALL OFF)
    set(ENABLE_ALL_WARNINGS OFF)
    set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF)
    set(PROFILER_IN_DEBUG_AND_RELEASE OFF)
    foreach(feature USE_SSE4_1 USE_SSE4_2 USE_AVX USE_AVX2 USE_AVX512 USE_LZCNT USE_TZCNT USE_F16C USE_FMADD
        JPH_USE_DX12 JPH_USE_VK JPH_USE_MTL JPH_USE_CPU_COMPUTE
        TARGET_UNIT_TESTS TARGET_HELLO_WORLD TARGET_PERFORMANCE_TEST TARGET_SAMPLES TARGET_VIEWER)
        set(${feature} OFF)
    endforeach()
    FetchContent_Declare(jolt
        GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
        GIT_TAG e77f175595e64cb44218cc9d9d56fc365ad0e36a
        SOURCE_SUBDIR Build
    )
    FetchContent_MakeAvailable(jolt)
endfunction()
kt_forge_jolt()
target_link_libraries(KT_Forge PRIVATE Jolt)
