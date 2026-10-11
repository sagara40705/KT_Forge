#include <Content/ModelImporter.h>
#include <Content/MeshGeometry.h>
#include <Core/Math/Matrix4.h>
#include <assimp/Importer.hpp>
#include <assimp/DefaultIOSystem.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/GltfMaterial.h>
#include <DirectXTex.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <set>

// Win32のA/W別名を、Assimpの同名virtual関数へ展開させない。
#undef CreateDirectory
#undef DeleteFile

namespace KT::Content
{
	namespace
	{
		std::string Utf8(const std::filesystem::path& path)
		{
			const auto text = path.generic_u8string();
			return {text.begin(), text.end()};
		}

		std::filesystem::path Path(std::string_view text)
		{
			return std::filesystem::path(std::u8string(text.begin(), text.end()));
		}

		// Unicodeの標準I/Oを使い、依存読込を元モデルのdirectory内と容量上限に限定する。
		class ModelFiles final : public Assimp::IOSystem
		{
		public:
			explicit ModelFiles(std::filesystem::path root)
				: root_(std::move(root))
			{
			}

			std::filesystem::path Resolve(std::string_view name) const
			{
				auto path = Path(name);
				if (path.is_relative())
				{
					path = root_ / path;
				}
				path = std::filesystem::canonical(path);
				const auto relative = path.lexically_relative(root_);
				if (relative.empty() || relative.has_root_path())
				{
					throw std::invalid_argument("モデル依存の元directoryとの相対配置を解決できません。");
				}
				for (const auto& part : relative)
				{
					if (part == ".." || part.wstring().find(L':') != std::wstring::npos)
					{
						throw std::invalid_argument("モデルの依存ファイルは元モデルのdirectory内へ配置してください。");
					}
				}
				return path;
			}

			bool Exists(const char* name) const override
			{
				try
				{
					return std::filesystem::is_regular_file(Resolve(name));
				}
				catch (const std::exception&)
				{
					return false;
				}
			}

			char getOsSeparator() const override
			{
				return native_.getOsSeparator();
			}

			void Close(Assimp::IOStream* stream) override
			{
				native_.Close(stream);
			}

			bool ComparePaths(const char* first, const char* second) const override
			{
				try
				{
					return std::filesystem::equivalent(Resolve(first), Resolve(second));
				}
				catch (const std::exception&)
				{
					return false;
				}
			}

			// 取込用I/Oからdirectory変更やファイル削除へ進ませない。
			bool CreateDirectory(const std::string&) override
			{
				return false;
			}

			bool ChangeDirectory(const std::string&) override
			{
				return false;
			}

			bool DeleteFile(const std::string&) override
			{
				return false;
			}

			void CheckSize(const std::filesystem::path& path)
			{
				const auto bytes = std::filesystem::file_size(path);
				if (bytes == 0 || bytes > 64 * 1024 * 1024)
				{
					throw std::invalid_argument("モデル依存ファイルが空、または64MiBを超えています: " + Utf8(path));
				}
				if (!visited_.contains(path))
				{
					if (visited_.size() >= 10000 || bytes > 512 * 1024 * 1024 - totalBytes_)
					{
						throw std::invalid_argument("モデル依存の件数または合計512MiB上限を超えています。");
					}
					visited_.insert(path);
					totalBytes_ += bytes;
				}
			}

			Assimp::IOStream* Open(const char* name, const char* mode = "rb") override
			{
				try
				{
					if (std::string_view(mode).find_first_of("wa+") != std::string_view::npos)
					{
						throw std::invalid_argument("モデル取込中のファイル書込は許可していません。");
					}
					const auto path = Resolve(name);
					CheckSize(path);
					auto* stream = native_.Open(Utf8(path).c_str(), mode);
					if (!stream)
					{
						failure = "モデル依存ファイルを開けません: " + Utf8(path);
					}
					return stream;
				}
				catch (const std::exception& error)
				{
					failure = error.what();
					return nullptr;
				}
			}

			std::vector<std::uint8_t> ReadImage(std::string_view name)
			{
				const auto path = Resolve(name);
				CheckSize(path);
				std::ifstream input(path, std::ios::binary | std::ios::ate);
				const auto bytes = input.tellg();
				if (!input || bytes <= 0 || bytes > 64 * 1024 * 1024)
				{
					throw std::runtime_error("モデル画像を開けないか、読込中にサイズが変化しました。");
				}
				std::vector<std::uint8_t> result(static_cast<std::size_t>(bytes));
				input.seekg(0);
				if (!input.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size())) ||
					input.peek() != std::char_traits<char>::eof())
				{
					throw std::runtime_error("モデル画像を最後まで読み込めません。");
				}
				return result;
			}

			std::string failure;

		private:
			// 標準実装のUnicode読込/解放へ委譲し、所有するstreamの種類を揃える。
			Assimp::DefaultIOSystem native_;
			// canonicalで確定した入力directoryと、重複を除いた読込容量。
			std::filesystem::path root_;
			std::set<std::filesystem::path> visited_;
			std::uintmax_t totalBytes_ = 0;
		};

		// 浮動小数点の縮小前に検査し、座標Scaleのoverflowを拒否する。
		float Number(double value)
		{
			if (!std::isfinite(value) || std::abs(value) > (std::numeric_limits<float>::max)())
			{
				throw std::invalid_argument("モデルの属性または変換値を有限floatで保持できません。");
			}
			return static_cast<float>(value);
		}

		std::vector<std::uint8_t> RawImage(const aiTexture& texture)
		{
			if (!texture.pcData || texture.mWidth == 0 || texture.mHeight == 0 || texture.mWidth > 16384 || texture.mHeight > 16384 ||
				std::uint64_t(texture.mWidth) * texture.mHeight > TextureAsset::MaxDecodedBytes / 4)
			{
				throw std::invalid_argument("モデルの埋込RGBA画像の寸法またはbyte上限が不正です。");
			}
			DirectX::ScratchImage image;
			if (FAILED(image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, texture.mWidth, texture.mHeight, 1, 1)))
			{
				throw std::runtime_error("モデル埋込画像のRGBA配置生成に失敗しました。");
			}
			const auto* output = image.GetImage(0, 0, 0);
			for (std::size_t y = 0; y < texture.mHeight; ++y)
			{
				for (std::size_t x = 0; x < texture.mWidth; ++x)
				{
					const auto pixel = texture.pcData[y * texture.mWidth + x];
					auto* destination = output->pixels + y * output->rowPitch + x * 4;
					destination[0] = pixel.r;
					destination[1] = pixel.g;
					destination[2] = pixel.b;
					destination[3] = pixel.a;
				}
			}
			DirectX::Blob bytes;
			if (FAILED(DirectX::SaveToDDSMemory(
					image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::DDS_FLAGS_NONE, bytes)))
			{
				throw std::runtime_error("モデル埋込画像のDDS変換に失敗しました。");
			}
			const auto* first = static_cast<const std::uint8_t*>(bytes.GetBufferPointer());
			return {first, first + bytes.GetBufferSize()};
		}
	}

	ImportedModel ModelImporter::Load(
		const std::filesystem::path& source, ModelImportOptions options, const std::map<std::string, AssetId>& previousIds)
	{
		if (!std::isfinite(options.scale) || options.scale <= 0)
		{
			throw std::invalid_argument("モデルのScaleには有限の正数が必要です。");
		}
		if (options.normals != MeshNormalMode::Preserve && options.normals != MeshNormalMode::Flat &&
			options.normals != MeshNormalMode::Smooth)
		{
			throw std::invalid_argument("モデルの法線生成方針が未対応です。");
		}
		const auto path = std::filesystem::canonical(source);
		Assimp::Importer importer;
		auto* files = new ModelFiles(path.parent_path());
		importer.SetIOHandler(files);
		unsigned flags = aiProcess_Triangulate | aiProcess_ValidateDataStructure;
		if (options.flipZ)
		{
			flags |= aiProcess_MakeLeftHanded | aiProcess_FlipWindingOrder;
		}
		if (options.flipV)
		{
			flags |= aiProcess_FlipUVs;
		}
		const auto* scene = importer.ReadFile(Utf8(path), flags);
		if (!scene || !scene->mRootNode || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE))
		{
			throw std::invalid_argument(
				"Assimpでモデルを読み込めません: " + std::string(importer.GetErrorString()) + " / " + files->failure);
		}
		if (!files->failure.empty())
		{
			throw std::invalid_argument("モデル依存の読込失敗を黙って補完できません: " + files->failure);
		}
		if (scene->mNumMeshes == 0 || scene->mNumMeshes > 100000 || scene->mNumMaterials > 100000)
		{
			throw std::invalid_argument("モデルのMeshが空、またはMesh/Material数が10万を超えています。");
		}
		if (scene->HasAnimations())
		{
			throw std::invalid_argument("モデルのAnimationは静的Sceneへ保存できません。静的モデルとして出力してください。");
		}
		ImportedModel result;
		if (scene->HasLights() || scene->HasCameras())
		{
			result.diagnostics.push_back("モデルのCamera/Lightは取込対象外です。Scene側で追加してください。");
		}
		const auto idFor = [&](const std::string& key)
		{
			const auto found = previousIds.find(key);
			return found == previousIds.end() ? AssetId::Generate() : found->second;
		};
		std::vector<MaterialAssetId> materialIds;
		std::vector<UnlitSettings> materialSettings;
		std::vector<std::string> slots;
		std::map<std::string, std::size_t> materialNames;
		std::map<std::string, TextureAssetId> textureIds;
		std::size_t imageBytes = 0;

		// 全MaterialをUnlitの色/画像/UV/Sampler/Maskへ変換する。未知の描画方式は診断する。
		for (unsigned index = 0; index < scene->mNumMaterials; ++index)
		{
			const auto& material = *scene->mMaterials[index];
			aiString name;
			material.Get(AI_MATKEY_NAME, name);
			const std::string label = name.length ? name.C_Str() : "Material";
			const auto occurrence = materialNames[label]++;
			const auto slot = label + "#" + std::to_string(occurrence);
			if (slot.size() > 256)
			{
				throw std::invalid_argument("モデルのMaterial slot名が256byteを超えています。");
			}
			const auto key = "material:" + slot;
			const MaterialAssetId id(idFor(key));
			aiColor4D color(1, 1, 1, 1);
			if (material.Get(AI_MATKEY_BASE_COLOR, color) != AI_SUCCESS)
			{
				material.Get(AI_MATKEY_COLOR_DIFFUSE, color);
			}
			UnlitSettings settings;
			int twoSided = 0;
			material.Get(AI_MATKEY_TWOSIDED, twoSided);
			if (twoSided)
			{
				result.diagnostics.push_back("モデルの両面設定は現在の片面Unlit描画へ接続しません: " + slot);
			}
			aiString alpha;
			material.Get(AI_MATKEY_GLTF_ALPHAMODE, alpha);
			if (std::string_view(alpha.C_Str()) == "BLEND")
			{
				throw std::invalid_argument("モデルの透過Blend Materialは未対応です: " + slot);
			}
			if (std::string_view(alpha.C_Str()) == "MASK")
			{
				settings.alphaMode = AlphaMode::Mask;
				material.Get(AI_MATKEY_GLTF_ALPHACUTOFF, settings.alphaCutoff);
			}
			float opacity = 1;
			material.Get(AI_MATKEY_OPACITY, opacity);
			if (opacity < 1 && settings.alphaMode != AlphaMode::Mask && alpha.length == 0)
			{
				throw std::invalid_argument("モデルの半透明MaterialはUnlitOpaqueへ変換できません: " + slot);
			}
			// glTF alphaはbaseColorを正本とし、汎用opacityとの二重乗算を避ける。
			if (alpha.length == 0)
			{
				color.a *= opacity;
			}
			const auto textureType = material.GetTextureCount(aiTextureType_BASE_COLOR) ? aiTextureType_BASE_COLOR : aiTextureType_DIFFUSE;
			if (material.GetTextureCount(textureType) > 1)
			{
				throw std::invalid_argument("BaseColorの多重Texture合成は未対応です: " + slot);
			}
			if (material.GetTextureCount(textureType))
			{
				aiString texturePath;
				aiTextureMapping mapping = aiTextureMapping_UV;
				unsigned uvIndex = 0;
				ai_real blend = 1;
				aiTextureOp operation = aiTextureOp_Multiply;
				aiTextureMapMode address[3]{aiTextureMapMode_Wrap, aiTextureMapMode_Wrap, aiTextureMapMode_Wrap};
				if (material.GetTexture(textureType, 0, &texturePath, &mapping, &uvIndex, &blend, &operation, address) != AI_SUCCESS ||
					mapping != aiTextureMapping_UV || uvIndex != 0 || blend != 1 || operation != aiTextureOp_Multiply)
				{
					throw std::invalid_argument("モデルのTexture mapping/UV channel/合成設定は未対応です: " + slot);
				}
				const auto addressMode = [](aiTextureMapMode value)
				{
					if (value != aiTextureMapMode_Wrap && value != aiTextureMapMode_Clamp)
					{
						throw std::invalid_argument("モデルTextureのSamplerはWrap/Clampだけに対応しています。");
					}
					return value == aiTextureMapMode_Wrap ? TextureAddress::Wrap : TextureAddress::Clamp;
				};
				settings.addressU = addressMode(address[0]);
				settings.addressV = addressMode(address[1]);
				aiUVTransform uv;
				if (material.Get(AI_MATKEY_UVTRANSFORM(textureType, 0), uv) == AI_SUCCESS)
				{
					if (uv.mRotation != 0)
					{
						throw std::invalid_argument("モデルTextureのUV回転は現在のMaterialでは保持できません。");
					}
					settings.uvScale = {uv.mScaling.x, uv.mScaling.y};
					settings.uvOffset = {uv.mTranslation.x, uv.mTranslation.y};
				}
				int filter = 0;
				material.Get(AI_MATKEY_GLTF_MAPPINGFILTER_MAG(textureType, 0), filter);
				settings.filter = filter == 9728 ? TextureFilter::Point : TextureFilter::Linear;
				const std::string imageKey = texturePath.C_Str();
				const auto existing = textureIds.find(imageKey);
				if (existing != textureIds.end())
				{
					settings.baseColorTexture = existing->second;
				}
				else
				{
					std::vector<std::uint8_t> bytes;
					std::string extension;
					if (const auto* embedded = scene->GetEmbeddedTexture(texturePath.C_Str()))
					{
						if (embedded->mHeight)
						{
							bytes = RawImage(*embedded);
							extension = ".dds";
						}
						else
						{
							if (!embedded->pcData || embedded->mWidth == 0 || embedded->mWidth > 64 * 1024 * 1024)
							{
								throw std::invalid_argument("モデルの埋込画像byte数が不正です。");
							}
							const auto* first = reinterpret_cast<const std::uint8_t*>(embedded->pcData);
							bytes.assign(first, first + embedded->mWidth);
							extension = "." + std::string(embedded->achFormatHint);
							if (bytes.size() >= 4 && bytes[0] == 137 && bytes[1] == 80 && bytes[2] == 78 && bytes[3] == 71)
							{
								extension = ".png";
							}
							else if (bytes.size() >= 2 && bytes[0] == 255 && bytes[1] == 216)
							{
								extension = ".jpg";
							}
						}
					}
					else
					{
						bytes = files->ReadImage(imageKey);
						extension = Utf8(Path(imageKey).extension());
					}
					std::transform(extension.begin(), extension.end(), extension.begin(),
						[](char value) { return value >= 'A' && value <= 'Z' ? char(value + 32) : value; });
					if (extension != ".png" && extension != ".jpg" && extension != ".jpeg" && extension != ".bmp" && extension != ".dds" &&
						extension != ".tga")
					{
						throw std::invalid_argument("モデル画像はPNG/JPEG/BMP/TGA/DDSへ変換してから取り込んでください。");
					}
					const auto textureKey = "texture:" + imageKey;
					if (bytes.size() > 512 * 1024 * 1024 - imageBytes)
					{
						throw std::invalid_argument("モデル画像の保存byte数が合計512MiBを超えています。");
					}
					imageBytes += bytes.size();
					const TextureAssetId textureId(idFor(textureKey));
					const auto output = "images/" + textureId.ToString() + extension;
					result.images.emplace(output, std::move(bytes));
					result.assets.push_back({textureId.Value(), imageKey, TextureAsset(output)});
					result.assetKeys.push_back(textureKey);
					textureIds.emplace(imageKey, textureId);
					settings.baseColorTexture = textureId;
				}
			}
			materialIds.push_back(id);
			materialSettings.push_back(settings);
			slots.push_back(slot);
			result.assets.push_back(
				{id.Value(), label, MaterialAsset({color.r, color.g, color.b, color.a}, MaterialModel::Unlit, settings)});
			result.assetKeys.push_back(key);
		}
		result.diagnostics.push_back("MaterialはBaseColorのUnlitへ変換します。照明/PBR・NormalMap・影の設定は描画へ接続しません。");

		// Assimp meshごとに属性を完成させ、その後ノードのMesh集合をSubmeshへ結合する。
		std::vector<MeshAsset> meshes;
		for (unsigned meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
		{
			const auto& mesh = *scene->mMeshes[meshIndex];
			if (mesh.HasBones() || mesh.mNumAnimMeshes)
			{
				throw std::invalid_argument("モデルのSkinning/Morphは静的Meshへ保存できません。");
			}
			if (mesh.mNumVertices == 0 || mesh.mNumVertices > (std::numeric_limits<std::uint32_t>::max)() / 48 ||
				mesh.mNumFaces > (std::numeric_limits<std::uint32_t>::max)() / 12 || mesh.mMaterialIndex >= materialIds.size())
			{
				throw std::invalid_argument("モデルMeshの要素数またはMaterial参照が不正です。");
			}
			std::vector<KT::Core::Math::Vector3> positions, normals;
			std::vector<KT::Core::Math::Vector2> texcoords;
			std::vector<std::uint32_t> indices;
			for (unsigned index = 0; index < mesh.mNumVertices; ++index)
			{
				const auto value = mesh.mVertices[index];
				positions.push_back({Number(double(value.x) * options.scale), Number(double(value.y) * options.scale),
					Number(double(value.z) * options.scale)});
				if (mesh.HasNormals())
				{
					const auto direction = mesh.mNormals[index];
					const double length = std::hypot(double(direction.x), double(direction.y), double(direction.z));
					if (!std::isfinite(length) || length == 0)
					{
						throw std::invalid_argument("モデル法線が全ゼロまたは非有限値です。");
					}
					normals.push_back({Number(direction.x / length), Number(direction.y / length), Number(direction.z / length)});
				}
				if (mesh.HasTextureCoords(0))
				{
					if (mesh.mNumUVComponents[0] > 2)
					{
						throw std::invalid_argument("モデルUV0の3次元座標は未対応です。");
					}
					texcoords.push_back({mesh.mTextureCoords[0][index].x, mesh.mTextureCoords[0][index].y});
				}
			}
			for (unsigned index = 0; index < mesh.mNumFaces; ++index)
			{
				const auto& face = mesh.mFaces[index];
				if (face.mNumIndices != 3)
				{
					throw std::invalid_argument("モデルの点/線/三角形化できない面は静的Triangle Listへ変換できません。");
				}
				indices.insert(indices.end(), face.mIndices, face.mIndices + 3);
			}
			const auto& material = materialSettings[mesh.mMaterialIndex];
			if (material.baseColorTexture && texcoords.empty())
			{
				throw std::invalid_argument("Texture付きモデルMeshにUV0がありません。");
			}
			meshes.push_back(
				MeshGeometry::Generate(MeshAsset(std::move(positions), std::move(indices), std::move(texcoords), std::move(normals)),
					options.normals, options.generateTangents));
			if (mesh.HasVertexColors(0) || mesh.HasTextureCoords(1))
			{
				result.diagnostics.push_back(
					"モデルの頂点色/追加UV channelは現在の静的Meshへ保存しません: " + std::string(mesh.mName.C_Str()));
			}
		}

		// 親先行で列挙する。深い階層は再帰せず、Scene上限に合わせて拒否する。
		struct NodeWork
		{
			const aiNode* node;
			std::optional<std::size_t> parent;
			std::string key;
			unsigned depth;
		};

		std::vector<NodeWork> work{{scene->mRootNode, {}, "root", 0}};
		std::map<std::vector<unsigned>, std::size_t> groups;
		std::vector<ImportedModelNode> meshChildren;
		for (std::size_t nodeIndex = 0; nodeIndex < work.size(); ++nodeIndex)
		{
			const auto current = work[nodeIndex];
			if (current.depth > 128 || work.size() > 100000)
			{
				throw std::invalid_argument("モデルのノード深さ128またはノード数10万上限を超えています。");
			}
			const auto& sourceNode = *current.node;
			ImportedModelNode node;
			node.key = current.key;
			node.name = sourceNode.mName.C_Str();
			node.parent = current.parent;
			aiVector3D scaling, position;
			aiQuaternion rotation;
			sourceNode.mTransformation.Decompose(scaling, rotation, position);
			node.position = {Number(double(position.x) * options.scale), Number(double(position.y) * options.scale),
				Number(double(position.z) * options.scale)};
			node.rotation = KT::Core::Math::Normalize({rotation.x, rotation.y, rotation.z, rotation.w});
			node.scale = {scaling.x, scaling.y, scaling.z};
			const auto reconstructed = KT::Core::Math::LocalMatrix(node.position, node.rotation, node.scale);
			for (unsigned row = 0; row < 4; ++row)
			{
				for (unsigned column = 0; column < 4; ++column)
				{
					// Assimpはcolumn-vector、エンジンはrow-vector。移動だけScaleを適用する。
					const double expected = sourceNode.mTransformation[column][row] * (row == 3 && column < 3 ? options.scale : 1.0);
					if (!std::isfinite(expected) ||
						std::abs(reconstructed(row, column) - expected) > 0.0001 * (std::max)(1.0, std::abs(expected)))
					{
						throw std::invalid_argument("モデルノードのShear/特異変換をLocalTransformへ保持できません: " + node.name);
					}
				}
			}
			if (sourceNode.mNumMeshes)
			{
				std::vector<unsigned> group(sourceNode.mMeshes, sourceNode.mMeshes + sourceNode.mNumMeshes);
				const auto assignMesh = [&](const std::vector<unsigned>& group, ImportedModelNode& target)
				{
					const auto existing = groups.find(group);
					if (existing != groups.end())
					{
						target.mesh = existing->second;
					}
					else
					{
						std::vector<KT::Core::Math::Vector3> positions, normals;
						std::vector<KT::Core::Math::Vector2> texcoords;
						std::vector<std::array<float, 4>> tangents;
						std::vector<std::uint32_t> indices;
						std::vector<MeshSubmesh> submeshes;
						std::map<std::string, std::optional<MaterialAssetId>> assignments;
						const bool hasUv = !meshes.at(group.front()).Texcoords().empty();
						const bool hasTangents = !meshes.at(group.front()).Tangents().empty();
						for (const auto meshIndex : group)
						{
							const auto& mesh = meshes.at(meshIndex);
							if (hasUv != !mesh.Texcoords().empty() || hasTangents != !mesh.Tangents().empty())
							{
								throw std::invalid_argument("同じノードの複数MeshにはUV0/接線の有無を揃えてください。");
							}
							if (mesh.Positions().size() > (std::numeric_limits<std::uint32_t>::max)() / 48 - positions.size() ||
								mesh.Indices().size() > (std::numeric_limits<std::uint32_t>::max)() / 4 - indices.size())
							{
								throw std::invalid_argument("モデル結合後の頂点/Index byte数が32bit上限を超えています。");
							}
							const auto vertexOffset = static_cast<std::uint32_t>(positions.size());
							const auto materialIndex = scene->mMeshes[meshIndex]->mMaterialIndex;
							submeshes.push_back({static_cast<std::uint32_t>(indices.size()),
								static_cast<std::uint32_t>(mesh.Indices().size()), slots[materialIndex]});
							assignments.emplace(slots[materialIndex], materialIds[materialIndex]);
							positions.insert(positions.end(), mesh.Positions().begin(), mesh.Positions().end());
							normals.insert(normals.end(), mesh.Normals().begin(), mesh.Normals().end());
							texcoords.insert(texcoords.end(), mesh.Texcoords().begin(), mesh.Texcoords().end());
							tangents.insert(tangents.end(), mesh.Tangents().begin(), mesh.Tangents().end());
							for (const auto value : mesh.Indices())
							{
								indices.push_back(vertexOffset + value);
							}
						}
						const auto key = "mesh:" + target.key;
						const MeshAssetId id(idFor(key));
						target.mesh = result.meshes.size();
						groups.emplace(group, *target.mesh);
						result.meshes.push_back(id);
						result.materials.push_back(std::move(assignments));
						result.assets.push_back({id.Value(), target.name,
							MeshAsset(std::move(positions), std::move(indices), std::move(texcoords), std::move(normals),
								std::move(tangents), std::move(submeshes))});
						result.assetKeys.push_back(key);
					}
				};
				const bool hasUv = !meshes.at(group.front()).Texcoords().empty();
				const bool uniform =
					std::all_of(group.begin(), group.end(), [&](unsigned index) { return hasUv == !meshes.at(index).Texcoords().empty(); });
				if (uniform)
				{
					assignMesh(group, node);
				}
				else
				{
					// 異なる属性のMeshはidentity子ノードへ分け、未指定UVを捏造しない。
					for (std::size_t index = 0; index < group.size(); ++index)
					{
						ImportedModelNode child;
						child.parent = nodeIndex;
						child.name = "Mesh " + std::to_string(index);
						child.key = node.key + "/mesh:" + std::to_string(index);
						assignMesh({group[index]}, child);
						meshChildren.push_back(std::move(child));
					}
				}
			}
			result.nodes.push_back(std::move(node));
			std::map<std::string, std::size_t> names;
			for (unsigned child = 0; child < sourceNode.mNumChildren; ++child)
			{
				const auto* next = sourceNode.mChildren[child];
				const std::string name = next->mName.C_Str();
				// 長さを含め、名前中のseparatorと同名兄弟を区別する。
				const auto key = current.key + "/" + std::to_string(name.size()) + ":" + name + "#" + std::to_string(names[name]++);
				work.push_back({next, nodeIndex, key, current.depth + 1});
			}
		}
		for (auto& child : meshChildren)
		{
			result.nodes.push_back(std::move(child));
		}
		if (result.nodes.size() > 100000)
		{
			throw std::invalid_argument("分割Meshを含むモデルノード数が10万上限を超えています。");
		}
		return result;
	}
}
