cmake_minimum_required(VERSION 4.2)

project(KT_Forge LANGUAGES C CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "KT_Forge requires Windows and MSVC.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "KT_Forge requires a 64-bit build. Use the vs2026-x64 preset.")
endif()

include(FetchContent)

# GLFW 3.5.1: official release pinned to its immutable commit.
# FetchContent keeps dependency sources and outputs in build/_deps.
set(GLFW_LIBRARY_TYPE STATIC CACHE STRING "GLFW library type" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build GLFW examples" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build GLFW tests" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build GLFW documentation" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "Generate GLFW install rules" FORCE)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG d9d6f0f1f967807ffade6598ea9a631ebaf37a56
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(glfw)

# Add new .cpp and .h files here so the build inputs stay explicit.
set(KT_FORGE_SOURCES
    Source/main.cpp
    Source/Application/Application.h
    Source/Application/Application.cpp
    Source/Core/Utility/NonCopyable.h
    Source/Core/Log.h
    Source/Core/Log.cpp
    Source/Core/Assert.h
    Source/Core/Assert.cpp
    Source/Core/Timer.h
    Source/Core/File.h
    Source/Core/File.cpp
    Source/Graphics/GraphicsDevice.h
    Source/Graphics/GraphicsDevice.cpp
    Source/Graphics/CommandQueue.h
    Source/Graphics/CommandQueue.cpp
    Source/Graphics/CommandContext.h
    Source/Graphics/CommandContext.cpp
    Source/Graphics/Swapchain.h
    Source/Graphics/Swapchain.cpp
    Source/Graphics/TriangleVertex.h
    Source/Graphics/VertexBuffer.h
    Source/Graphics/VertexBuffer.cpp
    Source/Graphics/TriangleRenderer.h
    Source/Graphics/TriangleRenderer.cpp
    Source/Graphics/Shaders/Triangle.hlsl
    Source/Platform/GlfwContext.h
    Source/Platform/GlfwContext.cpp
    Source/Platform/Window.h
    Source/Platform/Window.cpp
    Source/Renderer/Graph/RenderGraph.cpp
    Source/Renderer/Graph/RenderGraph.h
    Source/Renderer/Graph/RenderGraphTypes.h
    Source/Renderer/Graph/GraphImportedTexture.h
    Source/Renderer/Graph/GraphExecutionContext.h
    Source/Renderer/Graph/GraphExecutionContext.cpp
    Source/Renderer/Passes/ClearPass.h
    Source/Renderer/Passes/ClearPass.cpp
    Source/World/Entity.h
    Source/World/World.h
    Source/World/World.cpp
    Source/Core/Math/SceneMath.h
    Source/Core/Math/Constants.h
    Source/Core/Math/Scalar.h
    Source/Core/Math/Angle.h
    Source/Core/Math/Vector2.h
    Source/Core/Math/Vector3.h
    Source/Core/Math/Vector4.h
    Source/Core/Math/Quaternion.h
    Source/Core/Math/Matrix4.h
    Source/Core/Math/AABB.h
    Source/Core/Math/Plane.h
    Source/Core/Math/Ray.h
    Source/Core/Math/Frustum.h
    Source/Core/Math/Scalar.cpp
    Source/Core/Math/Angle.cpp
    Source/Core/Math/Vector2.cpp
    Source/Core/Math/Vector3.cpp
    Source/Core/Math/Vector4.cpp
    Source/Core/Math/Quaternion.cpp
    Source/Core/Math/Matrix4.cpp
    Source/Core/Math/AABB.cpp
    Source/Core/Math/Plane.cpp
    Source/Core/Math/Ray.cpp
    Source/Core/Math/Frustum.cpp
    Source/World/SceneComponents.h
    Source/World/Hierarchy.h
    Source/World/Hierarchy.cpp
    Source/Renderer/RenderData.h
    Source/World/SceneData.h
    Source/World/Systems/SceneUpdateContext.h
    Source/World/Systems/SceneUpdateContext.cpp
    Source/World/Systems/HierarchySystem.h
    Source/World/Systems/HierarchySystem.cpp
    Source/World/Systems/ActivationSystem.h
    Source/World/Systems/ActivationSystem.cpp
    Source/World/Systems/TransformSystem.h
    Source/World/Systems/TransformSystem.cpp
    Source/World/Systems/CameraSystem.h
    Source/World/Systems/CameraSystem.cpp
    Source/RuntimeIntegration/RenderFrame.h
    Source/RuntimeIntegration/RenderExtractionSystem.h
    Source/RuntimeIntegration/RenderExtractionSystem.cpp
    Source/RuntimeIntegration/SceneUpdater.h
    Source/RuntimeIntegration/SceneUpdater.cpp
)

# Visual Studio must not run its default FXC rule (entry point "main"). DXC is below.
set_source_files_properties(Source/Graphics/Shaders/Triangle.hlsl PROPERTIES HEADER_FILE_ONLY TRUE)
add_executable(KT_Forge ${KT_FORGE_SOURCES})
include(cmake/TriangleShaders.cmake)
kt_forge_triangle_shaders(KT_Forge)

target_compile_features(KT_Forge PRIVATE cxx_std_20)
target_include_directories(KT_Forge PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_options(KT_Forge PRIVATE /W4 /utf-8)
target_compile_definitions(KT_Forge PRIVATE GLFW_INCLUDE_NONE)
target_link_libraries(KT_Forge PRIVATE glfw d3d12 dxgi)

set_target_properties(KT_Forge PROPERTIES
    CXX_EXTENSIONS OFF
    # Keep Debug and Release outputs separate inside the existing build tree.
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" PREFIX "Source"
    FILES ${KT_FORGE_SOURCES}
)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT KT_Forge)
