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
		KT::Content::AssetReferenceState state, std::optional<KT::Content::AssetId> id, std::optional<std::uint32_t> submesh = {},
		std::string_view materialSlot = {})
	{
		if (state == KT::Content::AssetReferenceState::Missing || state == KT::Content::AssetReferenceState::TypeMismatch)
		{
			result.diagnostics.push_back({entity, field,
				state == KT::Content::AssetReferenceState::Missing ? RenderDiagnosticCode::MissingAsset :
																	 RenderDiagnosticCode::AssetTypeMismatch,
				id, submesh, std::string(materialSlot)});
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

		// 同じ完成入力から主光源をコピーする。設定はWorld座標で扱う。
		bool foundLight = false;
		for (std::size_t index = 0; index < inputs.size(); ++index)
		{
			if (!state.entities[index].active.value || !inputs[index].light || !inputs[index].light->enabled)
			{
				continue;
			}
			if (foundLight)
			{
				throw std::invalid_argument("activeでenabledなDirectionalLightはSceneに一つだけ指定してください。");
			}
			foundLight = true;
			const auto& source = *inputs[index].light;
			source.Validate();
			auto& light = result.lighting;
			light.constants.direction = KT::Core::Math::Normalized(source.direction);
			light.constants.color = source.color;
			light.constants.ambient = source.ambient;
			light.constants.intensity = source.intensity;
			light.constants.depthBias = source.depthBias;
			light.constants.normalBias = source.normalBias;
			light.constants.shadowStrength = source.shadowStrength;
			light.constants.shadowViewProjection = KT::Renderer::DirectionalShadowMatrix(
				source.direction, source.shadowCenter, source.shadowExtent, source.shadowDepth, source.shadowResolution);
			light.shadowResolution = source.shadowResolution;
			light.shadows = source.shadows && source.intensity > 0 && source.shadowStrength > 0;
			light.constants.shadowEnabled = light.shadows ? 1.0f : 0.0f;
		}

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
			ReportReference(result, entity.entity, RenderDiagnosticField::Mesh, mesh.state,
				renderer->mesh ? std::optional(renderer->mesh->Value()) : std::nullopt);
			if (!mesh.asset)
			{
				continue;
			}
			// 再取込で消えたslotの割当を保持し、誤った別Materialへの付け替えを避ける。
			for (const auto& [slot, reference] : renderer->materials)
			{
				const auto ranges = mesh.asset->Submeshes();
				if (std::none_of(ranges.begin(), ranges.end(), [&](const auto& submesh) { return submesh.materialSlot == slot; }))
				{
					result.diagnostics.push_back({entity.entity, RenderDiagnosticField::MaterialSlot,
						RenderDiagnosticCode::UnknownMaterialSlot, reference ? std::optional(reference->Value()) : std::nullopt, {}, slot});
				}
			}

			// Material欠落は当該Submeshだけを除外し、他のslotを描画候補として残す。
			for (std::size_t submeshIndex = 0; submeshIndex < mesh.asset->Submeshes().size(); ++submeshIndex)
			{
				const auto& submesh = mesh.asset->Submeshes()[submeshIndex];
				const auto overrideMaterial = renderer->materials.find(submesh.materialSlot);
				const auto materialId = overrideMaterial == renderer->materials.end() ? renderer->material : overrideMaterial->second;
				const auto material = assets.Resolve(materialId);
				ReportReference(result, entity.entity, RenderDiagnosticField::Material, material.state,
					materialId ? std::optional(materialId->Value()) : std::nullopt, static_cast<std::uint32_t>(submeshIndex),
					submesh.materialSlot);
				if (material.asset)
				{
					if (const auto textureId = material.asset->Settings().baseColorTexture)
					{
						const auto texture = assets.Resolve(textureId);
						ReportReference(result, entity.entity, RenderDiagnosticField::BaseColorTexture, texture.state, textureId->Value(),
							static_cast<std::uint32_t>(submeshIndex), submesh.materialSlot);
						if (!texture.asset)
						{
							continue;
						}
						if (mesh.asset->Texcoords().empty())
						{
							result.diagnostics.push_back(
								{entity.entity, RenderDiagnosticField::Texcoords, RenderDiagnosticCode::MissingTexcoords,
									renderer->mesh->Value(), static_cast<std::uint32_t>(submeshIndex), submesh.materialSlot});
							continue;
						}
					}
					if (material.asset->Model() == KT::Content::MaterialModel::Toon)
					{
						if (mesh.asset->Normals().empty())
						{
							result.diagnostics.push_back(
								{entity.entity, RenderDiagnosticField::Normals, RenderDiagnosticCode::MissingNormals,
									renderer->mesh->Value(), static_cast<std::uint32_t>(submeshIndex), submesh.materialSlot});
							continue;
						}
						try
						{
							(void)KT::Core::Math::Inverse(entity.transform.matrix);
						}
						catch (const std::exception&)
						{
							result.diagnostics.push_back(
								{entity.entity, RenderDiagnosticField::NormalTransform, RenderDiagnosticCode::SingularToonTransform, {},
									static_cast<std::uint32_t>(submeshIndex), submesh.materialSlot});
							continue;
						}
					}
					const auto bounds = TransformBounds(mesh.asset->SubmeshBounds()[submeshIndex], entity.transform.matrix);
					if (!bounds)
					{
						result.diagnostics.push_back({entity.entity, RenderDiagnosticField::WorldBounds,
							RenderDiagnosticCode::InvalidWorldBounds, {}, static_cast<std::uint32_t>(submeshIndex), submesh.materialSlot});
						continue;
					}
					result.candidates.objects.push_back(
						{*renderer->mesh, *materialId, entity.transform.matrix, *bounds, static_cast<std::uint32_t>(submeshIndex)});
					result.sources.push_back(entity.entity);
				}
			}
		}
		return result;
	}
}
