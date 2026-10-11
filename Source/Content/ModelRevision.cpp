#include <Content/ModelRevision.h>
#include <Content/AssetJson.h>
#include <Windows.h>
#include <bcrypt.h>
#include <set>
#include <algorithm>
#include <limits>

namespace KT::Content
{
	namespace
	{
		// Windows標準のSHA-256を操作内だけ所有する。失敗時もhandleを閉じる。
		class Digest
		{
		public:
			Digest()
			{
				if (BCryptOpenAlgorithmProvider(&algorithm_, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
				{
					throw std::runtime_error("Model照合用SHA-256を開けません。");
				}
				if (BCryptCreateHash(algorithm_, &hash_, nullptr, 0, nullptr, 0, 0) < 0)
				{
					BCryptCloseAlgorithmProvider(algorithm_, 0);
					throw std::runtime_error("Model照合用SHA-256を生成できません。");
				}
			}

			~Digest()
			{
				BCryptDestroyHash(hash_);
				BCryptCloseAlgorithmProvider(algorithm_, 0);
			}

			Digest(const Digest&) = delete;
			Digest& operator=(const Digest&) = delete;

			void Add(std::span<const std::uint8_t> bytes)
			{
				// 区切り長を固定のlittle-endianで含め、連結の曖昧さをなくす。
				std::array<std::uint8_t, 8> length{};
				const auto size = static_cast<std::uint64_t>(bytes.size());
				for (std::size_t index = 0; index < length.size(); ++index)
				{
					length[index] = static_cast<std::uint8_t>(size >> (index * 8));
				}
				Feed(length);
				Feed(bytes);
			}

			void Add(std::string_view text)
			{
				Add({reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
			}

			std::array<std::uint8_t, 32> Finish()
			{
				std::array<std::uint8_t, 32> result{};
				if (BCryptFinishHash(hash_, result.data(), static_cast<ULONG>(result.size()), 0) < 0)
				{
					throw std::runtime_error("Model照合用SHA-256を確定できません。");
				}
				return result;
			}

		private:
			void Feed(std::span<const std::uint8_t> bytes)
			{
				while (!bytes.empty())
				{
					const auto count = static_cast<ULONG>((std::min)(bytes.size(), std::size_t((std::numeric_limits<ULONG>::max)())));
					if (BCryptHashData(hash_, const_cast<PUCHAR>(bytes.data()), count, 0) < 0)
					{
						throw std::runtime_error("Model照合用SHA-256へ内容を追加できません。");
					}
					bytes = bytes.subspan(count);
				}
			}

			BCRYPT_ALG_HANDLE algorithm_ = nullptr;
			BCRYPT_HASH_HANDLE hash_ = nullptr;
		};
	}

	ModelNodeId LegacyModelNodeId(AssetId model, std::string_view key)
	{
		Digest digest;
		digest.Add("KT_Forge.ModelNode.v1");
		digest.Add(model.ToString());
		digest.Add(key);
		const auto bytes = digest.Finish();
		AssetId id;
		std::copy_n(bytes.begin(), id.bytes.size(), id.bytes.begin());
		return ModelNodeId(id);
	}

	std::string ModelRevision(const AssetCatalog& catalog, ModelAssetId model)
	{
		const auto lookup = catalog.Resolve(std::optional(model));
		if (lookup.state != AssetReferenceState::Ready)
		{
			throw std::invalid_argument("Revisionを計算するModelが欠落または種類違いです。");
		}
		lookup.asset->ValidateReferences(catalog);
		std::set<AssetId> dependencies{model.Value()};
		for (const auto& node : lookup.asset->Nodes())
		{
			if (node.mesh)
			{
				dependencies.insert(node.mesh->Value());
			}
			if (node.material)
			{
				dependencies.insert(node.material->Value());
			}
			for (const auto& [slot, material] : node.materials)
			{
				if (material)
				{
					dependencies.insert(material->Value());
				}
			}
		}
		// Material参照の画像も含め、同じUUIDの形状・画素更新を識別する。
		auto closure = dependencies;
		for (const auto id : dependencies)
		{
			const auto& document = catalog.Document(id);
			if (const auto* material = std::get_if<MaterialAsset>(&document.data); material && material->Settings().baseColorTexture)
			{
				closure.insert(material->Settings().baseColorTexture->Value());
			}
		}
		Digest digest;
		digest.Add("KT_Forge.ModelRevision.v1");
		for (const auto id : closure)
		{
			const auto& document = catalog.Document(id);
			digest.Add(AssetJson::Stringify(document));
			if (const auto* texture = std::get_if<TextureAsset>(&document.data))
			{
				for (const auto& mip : texture->Mips())
				{
					digest.Add(std::to_string(mip.width) + ":" + std::to_string(mip.height));
					digest.Add(mip.pixels);
				}
			}
		}
		std::string result;
		constexpr char hex[] = "0123456789abcdef";
		for (const auto byte : digest.Finish())
		{
			result.push_back(hex[byte >> 4]);
			result.push_back(hex[byte & 15]);
		}
		return result;
	}
}
