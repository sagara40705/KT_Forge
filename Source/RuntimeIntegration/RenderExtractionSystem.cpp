#include <RuntimeIntegration/RenderExtractionSystem.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace KT::RuntimeIntegration
{
	// 8頂点をrow-vectorのWorld行列で変換する。縮退・負scale・shearでも逆行列を要求しない。
	static std::optional<KT::Core::Math::AABB> TransformBounds(const KT::Core::Math::AABB& local, const KT::Core::Math::Matrix4& world)
	{
		if (!KT::Core::Math::IsAffine(world))
		{
			return std::nullopt;
		}
		std::array<double, 3> minimum;
		std::array<double, 3> maximum;
		minimum.fill((std::numeric_limits<double>::infinity)());
		maximum.fill(-(std::numeric_limits<double>::infinity)());
		for (unsigned int corner = 0; corner < 8; ++corner)
		{
			const double x = (corner & 1) ? local.maximum.x : local.minimum.x;
			const double y = (corner & 2) ? local.maximum.y : local.minimum.y;
			const double z = (corner & 4) ? local.maximum.z : local.minimum.z;
			for (std::size_t axis = 0; axis < 3; ++axis)
			{
				const auto value = x * world(0, axis) + y * world(1, axis) + z * world(2, axis) + world(3, axis);
				minimum[axis] = (std::min)(minimum[axis], value);
				maximum[axis] = (std::max)(maximum[axis], value);
			}
		}

		// floatへの丸めで境界を内側へ縮めない。有限AABBで表せない対象だけを除外する。
		std::array<float, 3> lower;
		std::array<float, 3> upper;
		constexpr auto limit = (std::numeric_limits<float>::max)();
		for (std::size_t axis = 0; axis < 3; ++axis)
		{
			if (!std::isfinite(minimum[axis]) || !std::isfinite(maximum[axis]) || minimum[axis] < -limit || maximum[axis] > limit)
			{
				return std::nullopt;
			}
			lower[axis] = static_cast<float>(minimum[axis]);
			upper[axis] = static_cast<float>(maximum[axis]);
			if (lower[axis] > minimum[axis])
			{
				lower[axis] = std::nextafter(lower[axis], -std::numeric_limits<float>::infinity());
			}
			if (upper[axis] < maximum[axis])
			{
				upper[axis] = std::nextafter(upper[axis], std::numeric_limits<float>::infinity());
			}
		}
		return KT::Core::Math::AABB{{lower[0], lower[1], lower[2]}, {upper[0], upper[1], upper[2]}};
	}

	// 種類別の解決結果を、入力を書き換えずに診断配列へ記録する。
	static void ReportReference(RenderFrame& result, KT::World::Entity entity, RenderDiagnosticField field,
		KT::Content::AssetReferenceState state, std::optional<KT::Content::AssetId> id)
	{
		if (state == KT::Content::AssetReferenceState::Missing || state == KT::Content::AssetReferenceState::TypeMismatch)
		{
			result.diagnostics.push_back({entity, field,
				state == KT::Content::AssetReferenceState::Missing ? RenderDiagnosticCode::MissingAsset :
																	 RenderDiagnosticCode::AssetTypeMismatch,
				id});
		}
	}

	RenderFrame RenderExtractionSystem::Extract(const KT::World::SceneUpdateContext& context, const KT::Content::AssetCatalog& assets) const
	{
		const auto& state = context.GetCpuFrame();
		if (!assets.IsFrozen())
		{
			throw std::logic_error("描画候補の生成前にAssetCatalogをFreezeしてください。");
		}
		RenderFrame result;
		result.sourceWorldId = context.SourceWorldId();
		result.updateNumber = context.UpdateNumber();
		const auto& inputs = context.Inputs();

		// Snapshotのslot順を維持し、各Entityのコピー済み設定だけから候補を作る。
		for (std::size_t index = 0; index < state.entities.size(); ++index)
		{
			const auto& renderer = inputs[index].mesh;
			const auto& entity = state.entities[index];
			if (!entity.active.value || !renderer || !renderer->visible)
			{
				continue;
			}
			const auto mesh = assets.Resolve(renderer->mesh);
			const auto material = assets.Resolve(renderer->material);
			ReportReference(result, entity.entity, RenderDiagnosticField::Mesh, mesh.state,
				renderer->mesh ? std::optional(renderer->mesh->Value()) : std::nullopt);
			ReportReference(result, entity.entity, RenderDiagnosticField::Material, material.state,
				renderer->material ? std::optional(renderer->material->Value()) : std::nullopt);
			if (!mesh.asset || !material.asset)
			{
				continue;
			}
			if (const auto textureId = material.asset->Settings().baseColorTexture)
			{
				const auto texture = assets.Resolve(textureId);
				ReportReference(result, entity.entity, RenderDiagnosticField::BaseColorTexture, texture.state, textureId->Value());
				if (!texture.asset)
				{
					continue;
				}
				if (mesh.asset->Texcoords().empty())
				{
					result.diagnostics.push_back(
						{entity.entity, RenderDiagnosticField::Texcoords, RenderDiagnosticCode::MissingTexcoords, renderer->mesh->Value()});
					continue;
				}
			}
			const auto bounds = TransformBounds(mesh.asset->LocalBounds(), entity.transform.matrix);
			if (!bounds)
			{
				result.diagnostics.push_back(
					{entity.entity, RenderDiagnosticField::WorldBounds, RenderDiagnosticCode::InvalidWorldBounds, {}});
				continue;
			}
			result.candidates.objects.push_back({*renderer->mesh, *renderer->material, entity.transform.matrix, *bounds});
			result.sources.push_back(entity.entity);
		}
		return result;
	}
}
