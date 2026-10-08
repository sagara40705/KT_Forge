#pragma once
#include <Renderer/Mesh.h>
#include <memory>

namespace KT::BuiltinAssets
{
	// 原点中心、各辺1meter、8位置/36index。同じMesh実体を複数Objectで共有する。
	std::unique_ptr<KT::Renderer::Mesh> CreateCubeMesh(KT::Graphics::GraphicsDevice& device);
}
