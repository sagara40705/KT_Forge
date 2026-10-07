cmake_minimum_required(VERSION 4.2)

project(KT_Forge LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "KT_Forge requires Windows and MSVC.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "KT_Forge requires a 64-bit build. Use the vs2026-x64 preset.")
endif()

# Add new .cpp and .h files here so the build inputs stay explicit.
set(KT_FORGE_SOURCES
    Source/main.cpp
    Source/Core/Utility/NonCopyable.h
    Source/Core/Log.h
    Source/Core/Log.cpp
    Source/Core/Assert.h
    Source/Core/Assert.cpp
    Source/Core/Timer.h
    Source/Core/File.h
    Source/Core/File.cpp
    Source/Renderer/Graph/RenderGraph.cpp
    Source/Renderer/Graph/RenderGraph.h
    Source/Renderer/Graph/RenderGraphTypes.h
)

add_executable(KT_Forge ${KT_FORGE_SOURCES})

target_compile_features(KT_Forge PRIVATE cxx_std_20)
target_include_directories(KT_Forge PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_options(KT_Forge PRIVATE /W4 /utf-8)

set_target_properties(KT_Forge PROPERTIES
    CXX_EXTENSIONS OFF
    # Keep Debug and Release outputs separate inside the existing build tree.
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" PREFIX "Source"
    FILES ${KT_FORGE_SOURCES}
)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT KT_Forge)
