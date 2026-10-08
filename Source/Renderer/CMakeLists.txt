target_sources(KT_Forge PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Graph/GraphExecutionContext.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Graph/GraphExecutionContext.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Graph/GraphImportedTexture.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Graph/RenderGraph.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Graph/RenderGraph.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Graph/RenderGraphTypes.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/MaterialStore.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/MaterialStore.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Mesh.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Mesh.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/MeshStore.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/MeshStore.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Passes/ClearPass.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Passes/ClearPass.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/PreparedRenderFrame.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PreparedRenderFrame.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/RenderConstants.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/RenderData.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Unlit3D.hlsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/UnlitMaterial.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/UnlitMaterial.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UnlitPipeline.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/UnlitPipeline.h"
)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Unlit3D.hlsl" TARGET_DIRECTORY KT_Forge PROPERTIES HEADER_FILE_ONLY TRUE)
kt_forge_shader(KT_Forge "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Unlit3D.hlsl" Unlit3DVS vs_6_0 VSMain)
kt_forge_shader(KT_Forge "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Unlit3D.hlsl" Unlit3DPS ps_6_0 PSMain)
