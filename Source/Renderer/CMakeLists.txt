target_sources(Engine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/StaticSceneRenderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/StaticSceneRenderer.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/MeshRenderCandidate.h"
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
    "${CMAKE_CURRENT_SOURCE_DIR}/Graph/GraphCompiledPlan.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Graph/GraphCompiler.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Graph/GraphCompiler.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Graph/GraphPass.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Graph/GraphResourceHandle.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Graph/GraphResourceHandle.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Graph/GraphResourceUse.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Graph/GraphStorage.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Graph/GraphTextureRange.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Graph/GraphUseResolver.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Graph/GraphUseResolver.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Graph/GraphValidator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Graph/GraphValidator.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Graph/GraphView.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Graph/GraphViewHandle.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Graph/GraphViewHandle.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Passes/DepthClearPass.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Passes/DepthClearPass.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Passes/OpaquePass.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Passes/OpaquePass.h"
)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Unlit3D.hlsl" TARGET_DIRECTORY Engine PROPERTIES HEADER_FILE_ONLY TRUE)
kt_forge_shader(Engine "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Unlit3D.hlsl" Unlit3DVS vs_6_0 VSMain)
kt_forge_shader(Engine "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Unlit3D.hlsl" Unlit3DSkinnedVS vs_6_0 SkinnedVSMain)
kt_forge_shader(Engine "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Unlit3D.hlsl" Unlit3DPS ps_6_0 PSMain)

kt_forge_shader(Engine "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Unlit3D.hlsl" Unlit3DTexturedPS ps_6_0 TexturedPSMain)

target_sources(Engine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Lighting/SceneLighting.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Lighting/SceneLighting.cpp"
)

target_sources(Engine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Lighting/ToonPipeline.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Lighting/ToonPipeline.cpp"
)
kt_forge_shader(Engine "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Toon3D.hlsl" Toon3DVS vs_6_0 VSMain)
kt_forge_shader(Engine "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Toon3D.hlsl" Toon3DSkinnedVS vs_6_0 SkinnedVSMain)
kt_forge_shader(Engine "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Toon3D.hlsl" Toon3DPS ps_6_0 PSMain)
kt_forge_shader(Engine "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Unlit3D.hlsl" Shadow3DPS ps_6_0 ShadowPSMain)
kt_forge_shader(Engine "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Unlit3D.hlsl" Shadow3DTexturedPS ps_6_0 ShadowTexturedPSMain)

target_sources(Engine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Passes/ShadowPass.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Passes/ShadowPass.cpp"
)

target_sources(Engine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Effects/EffectPlayer.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Effects/EffectPlayer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Passes/EffectPass.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Passes/EffectPass.cpp"
)

target_sources(Engine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/UI/UiRenderer.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UI/UiRenderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Passes/UiPass.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Passes/UiPass.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Ui2D.hlsl"
)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Ui2D.hlsl" TARGET_DIRECTORY Engine PROPERTIES HEADER_FILE_ONLY TRUE)
kt_forge_shader(Engine "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Ui2D.hlsl" Ui2DVS vs_6_0 VSMain)
kt_forge_shader(Engine "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Ui2D.hlsl" Ui2DPS ps_6_0 PSMain)
