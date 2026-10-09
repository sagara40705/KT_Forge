#include <Renderer/PreparedRenderFrame.h>
#include <Renderer/RenderConstants.h>
#include <Renderer/RenderData.h>
#include <Renderer/MeshStore.h>
#include <Renderer/MaterialStore.h>
#include <Renderer/Mesh.h>
#include <Renderer/UnlitMaterial.h>
#include <Graphics/Commands/CommandContext.h>
#include <Graphics/Textures/ColorTargetView.h>
#include <Graphics/Textures/DepthBuffer.h>
#include <Core/Math/Matrix4.h>
#include <array>
#include <cstddef>
#include <span>
#include <stdexcept>

namespace KT::Renderer
{
	PreparedRenderFrame PreparedRenderFrame::Prepare(const RenderWorld& world, const RenderView& view, const MeshStore& meshes,
		const MaterialStore& materials, KT::Graphics::ConstantBufferArena& constants)
	{
		if (view.width == 0 || view.height == 0 || view.width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
			view.height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || !KT::Core::Math::IsFinite(view.viewProjection))
		{
			throw std::invalid_argument("Prepared view is invalid.");
		}
		PreparedRenderFrame result;
		result.width_ = view.width;
		result.height_ = view.height;
		result.draws_.reserve(world.objects.size());
		// ID/行列を先に全検証。部分的なPreparedFrameは公開しない。
		for (const auto& object : world.objects)
		{
			if (!KT::Core::Math::IsAffine(object.world))
			{
				throw std::invalid_argument("Object world matrix must be finite affine.");
			}
			result.draws_.push_back({&meshes.Get(object.meshId), &materials.Get(object.materialId), {}, {}});
		}
		const ViewConstants vc{view.viewProjection};
		result.viewConstants_ = constants.Write(std::as_bytes(std::span{&vc, 1}));
		for (std::size_t i = 0; i < result.draws_.size(); ++i)
		{
			const ObjectConstants oc{world.objects[i].world};
			const MaterialConstants mc{result.draws_[i].material->GetColor()};
			result.draws_[i].objectConstants = constants.Write(std::as_bytes(std::span{&oc, 1}));
			result.draws_[i].materialConstants = constants.Write(std::as_bytes(std::span{&mc, 1}));
		}
		return result;
	}

	void PreparedRenderFrame::RecordDraw(KT::Graphics::CommandContext& context, std::size_t index,
		const KT::Graphics::ColorTargetView& target, const KT::Graphics::DepthBuffer& depth,
		const KT::Graphics::ConstantBufferArena& constants) const
	{
		try
		{
			const auto& draw = draws_.at(index);
			if (target.GetWidth() != width_ || target.GetHeight() != height_)
			{
				throw std::invalid_argument("Prepared viewport/target mismatch.");
			}
			const std::array<KT::Graphics::RootConstantBinding, 3> bindings{
				{{0, viewConstants_}, {1, draw.objectConstants}, {2, draw.materialConstants}}};
			context.DrawIndexed(draw.material->GetPipeline().GetPipeline(), draw.mesh->GetVertices(), draw.mesh->GetIndices(), target,
				depth, constants, bindings);
		}
		catch (...)
		{
			context.Invalidate();
			throw;
		}
	}

	// Prepareで保持した値をOpaquePassへ渡す。
	KT::Graphics::ConstantSlice PreparedRenderFrame::GetViewConstants() const noexcept
	{
		return viewConstants_;
	}

	// Prepareで保持した値をOpaquePassへ渡す。
	UINT PreparedRenderFrame::GetWidth() const noexcept
	{
		return width_;
	}

	// Prepareで保持した値をOpaquePassへ渡す。
	UINT PreparedRenderFrame::GetHeight() const noexcept
	{
		return height_;
	}

}
