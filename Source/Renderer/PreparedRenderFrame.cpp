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
#include <Graphics/Textures/DepthTargetView.h>
#include <Graphics/Commands/IndexedDrawPacket.h>
#include <Core/Math/Matrix4.h>
#include <array>
#include <algorithm>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <map>

namespace KT::Renderer
{
	PreparedRenderFrame PreparedRenderFrame::Prepare(const RenderWorld& world, const RenderView& view, const MeshStore& meshes,
		const MaterialStore& materials, KT::Graphics::ConstantBufferArena& constants, const SceneLighting& lighting)
	{
		if (view.width == 0 || view.height == 0 || view.width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
			view.height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || !KT::Core::Math::IsFinite(view.viewProjection))
		{
			throw std::invalid_argument("描画Cameraの寸法またはViewProjection行列が不正です。");
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
				throw std::invalid_argument("描画ObjectのWorld行列には有限のAffine行列が必要です。");
			}
			result.draws_.push_back({&meshes.Get(object.meshId), &materials.Get(object.materialId), {}, {}});
			auto& draw = result.draws_.back();
			if (!draw.mesh->IsReady())
			{
				throw std::logic_error("PreparedRenderFrameのMesh転送が完了していません。");
			}
			const auto& submesh = draw.mesh->Submesh(object.submesh);
			draw.range = {submesh.firstIndex, submesh.indexCount};
			const auto skinCount = draw.mesh->SkinBindingCount();
			if ((skinCount != 0) != bool(object.palette) || (object.palette && object.palette->size() != skinCount))
			{
				throw std::invalid_argument("描画Meshと個体別Skin paletteの要素数が一致しません。");
			}
			(void)draw.material->GetSurfacePipeline(skinCount != 0);
			if (draw.material->IsToon())
			{
				if (!draw.mesh->HasNormals())
				{
					throw std::invalid_argument("Toon描画のMeshには法線が必要です。");
				}
				(void)KT::Core::Math::Inverse(object.world);
				draw.texturePair = materials.GetToonTextures(object.materialId);
				if (!draw.texturePair)
				{
					throw std::logic_error("Toon描画のMaterialStoreへ影画像を接続してください。");
				}
			}
			if (object.palette)
			{
				for (const auto& matrix : *object.palette)
				{
					if (!KT::Core::Math::IsAffine(matrix))
					{
						throw std::invalid_argument("Skin paletteには有限Affine行列が必要です。");
					}
				}
			}
		}
		if (std::any_of(result.draws_.begin(), result.draws_.end(), [](const auto& draw) { return draw.material->IsToon(); }))
		{
			lighting.Validate();
			result.lightConstants_ = constants.Write(std::as_bytes(std::span{&lighting.constants, 1}));
		}
		const ViewConstants vc{view.viewProjection};
		result.viewConstants_ = constants.Write(std::as_bytes(std::span{&vc, 1}));
		// 同じ個体のSubmesh間で一つの定数sliceを共有する。
		std::map<const void*, KT::Graphics::ConstantSlice> palettes;
		for (std::size_t i = 0; i < result.draws_.size(); ++i)
		{
			const ObjectConstants oc{world.objects[i].world,
				result.draws_[i].material->IsToon() ? KT::Core::Math::Transpose(KT::Core::Math::Inverse(world.objects[i].world)) :
													  KT::Core::Math::Matrix4{}};
			const auto& mc = result.draws_[i].material->GetConstants();
			result.draws_[i].objectConstants = constants.Write(std::as_bytes(std::span{&oc, 1}));
			result.draws_[i].materialConstants = constants.Write(std::as_bytes(std::span{&mc, 1}));
			if (const auto& palette = world.objects[i].palette)
			{
				auto found = palettes.find(palette.get());
				if (found == palettes.end())
				{
					std::array<KT::Core::Math::Matrix4, KT::Content::SkinDefinition::MaxJoints> matrices;
					std::copy(palette->begin(), palette->end(), matrices.begin());
					found = palettes.emplace(palette.get(), constants.Write(std::as_bytes(std::span(matrices)))).first;
				}
				result.draws_[i].skinConstants = found->second;
			}
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
			if (draw.material->IsToon())
			{
				throw std::logic_error("Toon描画は影のRead宣言を含むGraph passから記録してください。");
			}
			if (target.GetWidth() != width_ || target.GetHeight() != height_)
			{
				throw std::invalid_argument("準備済み描画の寸法が描画先と一致しません。");
			}
			std::vector<KT::Graphics::RootConstantBinding> bindings{
				{0, viewConstants_}, {1, draw.objectConstants}, {2, draw.materialConstants}};
			const bool skinned = draw.mesh->SkinBindingCount() != 0;
			if (skinned)
			{
				bindings.push_back({draw.material->GetTexture() ? 5u : 3u, draw.skinConstants});
			}
			const KT::Graphics::IndexedDrawPacket packet{&draw.material->GetPipeline(skinned).GetPipeline(), &draw.mesh->GetVertices(),
				&draw.mesh->GetIndices(), bindings, width_, height_, draw.material->GetTexture(), draw.material->GetSampler(), 3, 4,
				draw.range};
			context.DrawIndexed(packet, target, KT::Graphics::DepthTargetView(depth), constants);
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
