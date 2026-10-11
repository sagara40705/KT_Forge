#include <Content/AssetJson.h>
#include <nlohmann/json.hpp>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace KT::Content
{
	using Json = nlohmann::json;

	// 必須フィールドの欠落と未知フィールドを拒否し、黙った補完を行わない。
	static void RequireFields(const Json& value, std::initializer_list<std::string_view> fields)
	{
		if (!value.is_object() || value.size() != fields.size())
		{
			throw std::invalid_argument("素材JSONのobjectに不足または未定義のフィールドがあります。");
		}
		for (const auto field : fields)
		{
			if (!value.contains(field))
			{
				throw std::invalid_argument("素材JSONの必須フィールドがありません: " + std::string(field));
			}
		}
	}

	static float ReadFloat(const Json& value)
	{
		if (!value.is_number())
		{
			throw std::invalid_argument("素材JSONの座標・色には数値が必要です。");
		}
		const auto number = value.get<double>();
		if (!std::isfinite(number) || std::abs(number) > (std::numeric_limits<float>::max)())
		{
			throw std::invalid_argument("素材JSONの数値がfloatの有限範囲を超えています。");
		}
		const auto result = static_cast<float>(number);
		if (number != 0 && result == 0)
		{
			throw std::invalid_argument("素材JSONの数値がfloatへの変換で0に消失します。");
		}
		return result;
	}

	// 二択の文字列を厳密に読み、未知値を既定値へ落とさない。
	static bool ReadChoice(const Json& value, const char* first, const char* second)
	{
		if (value == first)
		{
			return false;
		}
		if (value == second)
		{
			return true;
		}
		throw std::invalid_argument("素材JSONの設定値が未対応です: " + value.dump());
	}

	static std::array<float, 2> ReadFloat2(const Json& value)
	{
		if (!value.is_array() || value.size() != 2)
		{
			throw std::invalid_argument("素材JSONのUVには2要素が必要です。");
		}
		return {ReadFloat(value[0]), ReadFloat(value[1])};
	}

	static std::uint32_t ReadUint(const Json& value)
	{
		if (!value.is_number_integer() || value < 0 || value > (std::numeric_limits<std::uint32_t>::max)())
		{
			throw std::invalid_argument("Mesh素材のIndex範囲にはuint32の整数が必要です。");
		}
		return value.get<std::uint32_t>();
	}

	AssetDocument AssetJson::Parse(std::string_view text)
	{
		if (text.size() > MaxBytes)
		{
			throw std::invalid_argument("素材JSONが64MiBの上限を超えています。");
		}
		try
		{
			// DOMで上書きされる前に、各objectのキー重複と階層の上限を検査する。
			std::vector<std::set<std::string>> keys;
			const auto root = Json::parse(text.begin(), text.end(),
				[&](int depth, Json::parse_event_t event, Json& parsed)
				{
					if (depth > MaxDepth)
					{
						throw std::invalid_argument("素材JSONの階層が16を超えています。");
					}
					if (event == Json::parse_event_t::object_start)
					{
						keys.emplace_back();
					}
					else if (event == Json::parse_event_t::key)
					{
						if (!keys.back().insert(parsed.get<std::string>()).second)
						{
							throw std::invalid_argument("素材JSONのキーが重複しています。");
						}
					}
					else if (event == Json::parse_event_t::object_end)
					{
						keys.pop_back();
					}
					return true;
				});
			RequireFields(root, {"version", "assetId", "name", "type", "data"});
			if (!root.at("version").is_number_integer() || (root.at("version") != 1 && root.at("version") != 2 && root.at("version") != 3))
			{
				throw std::invalid_argument("素材JSONのversionは1..3に対応しています。");
			}
			const bool meshExtended = root.at("version") == 3;
			const bool extended = root.at("version") != 1;
			const auto id = AssetId::Parse(root.at("assetId").get<std::string>());
			const auto name = root.at("name").get<std::string>();
			const auto type = root.at("type").get<std::string>();
			const auto& data = root.at("data");
			if (type == "kt.Model" && !extended)
			{
				RequireFields(data, {"nodes"});
				const auto& values = data.at("nodes");
				if (!values.is_array() || values.empty() || values.size() > ModelAsset::MaxNodes)
				{
					throw std::invalid_argument("Model素材のnodesには1..10万要素の配列が必要です。");
				}
				std::vector<ModelNode> nodes;
				nodes.reserve(values.size());
				for (const auto& value : values)
				{
					RequireFields(value, {"key", "name", "parent", "position", "rotation", "scale", "mesh", "material", "materials"});
					ModelNode node;
					node.key = value.at("key").get<std::string>();
					node.name = value.at("name").get<std::string>();
					if (!value.at("parent").is_null())
					{
						node.parent = ReadUint(value.at("parent"));
					}
					for (const auto field : {"position", "rotation", "scale"})
					{
						const auto& array = value.at(field);
						if (!array.is_array() || array.size() != (std::string_view(field) == "rotation" ? 4 : 3))
						{
							throw std::invalid_argument("ModelのLocal Transformの要素数が不正です。");
						}
					}
					const auto& position = value.at("position");
					const auto& rotation = value.at("rotation");
					const auto& scale = value.at("scale");
					node.position = {ReadFloat(position[0]), ReadFloat(position[1]), ReadFloat(position[2])};
					node.rotation = {ReadFloat(rotation[0]), ReadFloat(rotation[1]), ReadFloat(rotation[2]), ReadFloat(rotation[3])};
					node.scale = {ReadFloat(scale[0]), ReadFloat(scale[1]), ReadFloat(scale[2])};
					if (!value.at("mesh").is_null())
					{
						node.mesh = MeshAssetId::Parse(value.at("mesh").get<std::string>());
					}
					if (!value.at("material").is_null())
					{
						node.material = MaterialAssetId::Parse(value.at("material").get<std::string>());
					}
					if (!value.at("materials").is_object())
					{
						throw std::invalid_argument("ModelのMaterial割当にはslot名を持つobjectが必要です。");
					}
					for (const auto& [slot, material] : value.at("materials").items())
					{
						node.materials.emplace(slot, material.is_null() ? std::nullopt :
							std::optional(MaterialAssetId::Parse(material.get<std::string>())));
					}
					nodes.push_back(std::move(node));
				}
				return {id, name, ModelAsset(std::move(nodes))};
			}
			if (type == "kt.Mesh")
			{
				if (meshExtended)
				{
					RequireFields(data, {"positions", "indices", "texcoords", "normals", "tangents", "submeshes", "import"});
				}
				else if (extended)
				{
					RequireFields(data, {"positions", "indices", "texcoords"});
				}
				else
				{
					RequireFields(data, {"positions", "indices"});
				}
				const auto& sourcePositions = data.at("positions");
				const auto& sourceIndices = data.at("indices");
				if (!sourcePositions.is_array() || !sourceIndices.is_array())
				{
					throw std::invalid_argument("Mesh素材のpositionsとindicesは配列が必要です。");
				}
				constexpr auto limit = (std::numeric_limits<std::uint32_t>::max)();
				if (sourcePositions.size() > limit / sizeof(KT::Core::Math::Vector3) ||
					sourceIndices.size() > limit / sizeof(std::uint32_t))
				{
					throw std::invalid_argument("Mesh素材のbyte数がuint32範囲を超えています。");
				}
				std::vector<KT::Core::Math::Vector3> positions;
				std::vector<std::uint32_t> indices;
				positions.reserve(sourcePositions.size());
				indices.reserve(sourceIndices.size());
				for (const auto& position : sourcePositions)
				{
					if (!position.is_array() || position.size() != 3)
					{
						throw std::invalid_argument("Mesh素材の頂点にはxyzの3要素が必要です。");
					}
					positions.push_back({ReadFloat(position[0]), ReadFloat(position[1]), ReadFloat(position[2])});
				}
				for (const auto& index : sourceIndices)
				{
					if (!index.is_number_integer() || index < 0 || index > limit)
					{
						throw std::invalid_argument("Mesh素材のIndexにはuint32範囲の整数が必要です。");
					}
					indices.push_back(index.get<std::uint32_t>());
				}
				std::vector<KT::Core::Math::Vector2> texcoords;
				if (extended)
				{
					const auto& values = data.at("texcoords");
					if (!values.is_array())
					{
						throw std::invalid_argument("Mesh素材のtexcoordsは配列が必要です。");
					}
					texcoords.reserve(values.size());
					for (const auto& value : values)
					{
						const auto uv = ReadFloat2(value);
						texcoords.push_back({uv[0], uv[1]});
					}
				}
				std::vector<KT::Core::Math::Vector3> normals;
				std::vector<std::array<float, 4>> tangents;
				std::vector<MeshSubmesh> submeshes;
				std::optional<MeshImportRecipe> recipe;
				if (meshExtended)
				{
					if (!data.at("normals").is_array() || !data.at("tangents").is_array() || !data.at("submeshes").is_array() ||
						data.at("submeshes").empty())
					{
						throw std::invalid_argument("Meshの法線・接線には配列、Submeshには空でない配列が必要です。");
					}
					for (const auto& normal : data.at("normals"))
					{
						if (!normal.is_array() || normal.size() != 3)
						{
							throw std::invalid_argument("Meshの法線にはxyzの3要素が必要です。");
						}
						normals.push_back({ReadFloat(normal[0]), ReadFloat(normal[1]), ReadFloat(normal[2])});
					}
					for (const auto& tangent : data.at("tangents"))
					{
						if (!tangent.is_array() || tangent.size() != 4)
						{
							throw std::invalid_argument("Meshの接線にはxyzwの4要素が必要です。");
						}
						tangents.push_back({ReadFloat(tangent[0]), ReadFloat(tangent[1]), ReadFloat(tangent[2]), ReadFloat(tangent[3])});
					}
					for (const auto& submesh : data.at("submeshes"))
					{
						RequireFields(submesh, {"firstIndex", "indexCount", "materialSlot"});
						submeshes.push_back({ReadUint(submesh.at("firstIndex")), ReadUint(submesh.at("indexCount")),
							submesh.at("materialSlot").get<std::string>()});
					}
					if (!data.at("import").is_null())
					{
						const auto& value = data.at("import");
						RequireFields(value, {"source", "importerVersion", "scale", "flipZ", "flipV", "normals", "generateTangents"});
						MeshNormalMode mode;
						if (value.at("normals") == "Preserve")
						{
							mode = MeshNormalMode::Preserve;
						}
						else
						{
							mode = ReadChoice(value.at("normals"), "Flat", "Smooth") ? MeshNormalMode::Smooth : MeshNormalMode::Flat;
						}
						recipe = MeshImportRecipe{value.at("source").get<std::string>(), ReadUint(value.at("importerVersion")),
							ReadFloat(value.at("scale")), value.at("flipZ").get<bool>(), value.at("flipV").get<bool>(), mode,
							value.at("generateTangents").get<bool>()};
					}
				}
				return {id, name,
					MeshAsset(std::move(positions), std::move(indices), std::move(texcoords), std::move(normals), std::move(tangents),
						std::move(submeshes), std::move(recipe))};
			}
			if (meshExtended)
			{
				throw std::invalid_argument("素材JSONのversion 3はMeshだけに対応しています。");
			}
			if (type == "kt.Material")
			{
				UnlitSettings settings;
				if (extended)
				{
					RequireFields(
						data, {"model", "baseColor", "baseColorTexture", "uvScale", "uvOffset", "sampler", "alphaMode", "alphaCutoff"});
					if (!data.at("baseColorTexture").is_null())
					{
						settings.baseColorTexture = TextureAssetId::Parse(data.at("baseColorTexture").get<std::string>());
					}
					settings.uvScale = ReadFloat2(data.at("uvScale"));
					settings.uvOffset = ReadFloat2(data.at("uvOffset"));
					const auto& sampler = data.at("sampler");
					RequireFields(sampler, {"filter", "addressU", "addressV"});
					settings.filter = ReadChoice(sampler.at("filter"), "Point", "Linear") ? TextureFilter::Linear : TextureFilter::Point;
					settings.addressU = ReadChoice(sampler.at("addressU"), "Wrap", "Clamp") ? TextureAddress::Clamp : TextureAddress::Wrap;
					settings.addressV = ReadChoice(sampler.at("addressV"), "Wrap", "Clamp") ? TextureAddress::Clamp : TextureAddress::Wrap;
					settings.alphaMode = ReadChoice(data.at("alphaMode"), "Opaque", "Mask") ? AlphaMode::Mask : AlphaMode::Opaque;
					settings.alphaCutoff = ReadFloat(data.at("alphaCutoff"));
				}
				else
				{
					RequireFields(data, {"model", "baseColor"});
				}
				if (data.at("model") != (extended ? "Unlit" : "UnlitOpaque"))
				{
					throw std::invalid_argument("Material素材のmodelが未対応です。");
				}
				const auto& color = data.at("baseColor");
				if (!color.is_array() || color.size() != 4)
				{
					throw std::invalid_argument("Material素材のbaseColorにはRGBAの4要素が必要です。");
				}
				return {id, name,
					MaterialAsset({ReadFloat(color[0]), ReadFloat(color[1]), ReadFloat(color[2]), ReadFloat(color[3])},
						MaterialModel::Unlit, settings)};
			}
			if (type == "kt.Texture" && !extended)
			{
				RequireFields(data, {"source", "colorSpace", "mips"});
				const auto colorSpace =
					ReadChoice(data.at("colorSpace"), "Linear", "Srgb") ? TextureColorSpace::Srgb : TextureColorSpace::Linear;
				const auto mips = ReadChoice(data.at("mips"), "Preserve", "Generate") ? TextureMipMode::Generate : TextureMipMode::Preserve;
				return {id, name, TextureAsset(data.at("source").get<std::string>(), colorSpace, mips)};
			}
			throw std::invalid_argument("素材JSONのtypeが未対応です: " + type);
		}
		catch (const Json::exception& error)
		{
			throw std::invalid_argument("素材JSONの構文・UTF-8・データ型が不正です。JSONエラーID=" + std::to_string(error.id));
		}
	}

	std::string AssetJson::Stringify(const AssetDocument& document)
	{
		if (document.data.valueless_by_exception())
		{
			throw std::invalid_argument("素材JSONへ保存するCPU素材が失効しています。");
		}
		try
		{
			Json root{{"version", 1}, {"assetId", document.id.ToString()}, {"name", document.name}};
			if (const auto* mesh = std::get_if<MeshAsset>(&document.data))
			{
				mesh->Validate();
				root["type"] = "kt.Mesh";
				auto positions = Json::array();
				for (const auto position : mesh->Positions())
				{
					positions.push_back({position.x, position.y, position.z});
				}
				root["data"] = Json{{"positions", std::move(positions)}, {"indices", Json::array()}};
				for (const auto index : mesh->Indices())
				{
					root["data"]["indices"].push_back(index);
				}
				if (!mesh->Texcoords().empty())
				{
					root["version"] = 2;
					root["data"]["texcoords"] = Json::array();
					for (const auto uv : mesh->Texcoords())
					{
						root["data"]["texcoords"].push_back({uv.x, uv.y});
					}
				}
				const auto ranges = mesh->Submeshes();
				if (!mesh->Normals().empty() || !mesh->Tangents().empty() || mesh->ImportRecipe() || ranges.size() != 1 ||
					ranges[0].materialSlot != "default")
				{
					root["version"] = 3;
					root["data"]["texcoords"] = Json::array();
					for (const auto uv : mesh->Texcoords())
					{
						root["data"]["texcoords"].push_back({uv.x, uv.y});
					}
					root["data"]["normals"] = Json::array();
					for (const auto normal : mesh->Normals())
					{
						root["data"]["normals"].push_back({normal.x, normal.y, normal.z});
					}
					root["data"]["tangents"] = Json::array();
					for (const auto& tangent : mesh->Tangents())
					{
						root["data"]["tangents"].push_back(tangent);
					}
					root["data"]["submeshes"] = Json::array();
					for (const auto& submesh : ranges)
					{
						root["data"]["submeshes"].push_back(Json{{"firstIndex", submesh.firstIndex}, {"indexCount", submesh.indexCount},
							{"materialSlot", submesh.materialSlot}});
					}
					root["data"]["import"] = nullptr;
					if (mesh->ImportRecipe())
					{
						const auto& recipe = *mesh->ImportRecipe();
						root["data"]["import"] = Json{{"source", recipe.source}, {"importerVersion", recipe.importerVersion},
							{"scale", recipe.scale}, {"flipZ", recipe.flipZ}, {"flipV", recipe.flipV},
							{"normals",
								recipe.normals == MeshNormalMode::Preserve ? "Preserve" :
									recipe.normals == MeshNormalMode::Flat ? "Flat" :
																			 "Smooth"},
							{"generateTangents", recipe.generateTangents}};
					}
				}
			}
			else if (const auto* value = std::get_if<MaterialAsset>(&document.data))
			{
				const auto& material = *value;
				root["type"] = "kt.Material";
				root["data"] = Json{{"model", "UnlitOpaque"}, {"baseColor", material.BaseColor()}};
				const auto& settings = material.Settings();
				if (settings != UnlitSettings{})
				{
					root["version"] = 2;
					root["data"]["model"] = "Unlit";
					root["data"]["baseColorTexture"] =
						settings.baseColorTexture ? Json(settings.baseColorTexture->ToString()) : Json(nullptr);
					root["data"]["uvScale"] = settings.uvScale;
					root["data"]["uvOffset"] = settings.uvOffset;
					root["data"]["sampler"] = Json{{"filter", settings.filter == TextureFilter::Point ? "Point" : "Linear"},
						{"addressU", settings.addressU == TextureAddress::Wrap ? "Wrap" : "Clamp"},
						{"addressV", settings.addressV == TextureAddress::Wrap ? "Wrap" : "Clamp"}};
					root["data"]["alphaMode"] = settings.alphaMode == AlphaMode::Mask ? "Mask" : "Opaque";
					root["data"]["alphaCutoff"] = settings.alphaCutoff;
				}
			}
			else if (const auto* model = std::get_if<ModelAsset>(&document.data))
			{
				model->Validate();
				root["type"] = "kt.Model";
				root["data"]["nodes"] = Json::array();
				for (const auto& node : model->Nodes())
				{
					Json materials = Json::object();
					for (const auto& [slot, material] : node.materials)
					{
						materials[slot] = material ? Json(material->ToString()) : Json(nullptr);
					}
					root["data"]["nodes"].push_back(Json{{"key", node.key}, {"name", node.name},
						{"parent", node.parent ? Json(*node.parent) : Json(nullptr)},
						{"position", {node.position.x, node.position.y, node.position.z}},
						{"rotation", {node.rotation.x, node.rotation.y, node.rotation.z, node.rotation.w}},
						{"scale", {node.scale.x, node.scale.y, node.scale.z}},
						{"mesh", node.mesh ? Json(node.mesh->ToString()) : Json(nullptr)},
						{"material", node.material ? Json(node.material->ToString()) : Json(nullptr)}, {"materials", std::move(materials)}});
				}
			}
			else
			{
				const auto& texture = std::get<TextureAsset>(document.data);
				root["type"] = "kt.Texture";
				root["data"] =
					Json{{"source", texture.Source()}, {"colorSpace", texture.ColorSpace() == TextureColorSpace::Srgb ? "Srgb" : "Linear"},
						{"mips", texture.MipMode() == TextureMipMode::Generate ? "Generate" : "Preserve"}};
			}
			auto text = root.dump(2) + "\n";
			if (text.size() > MaxBytes)
			{
				throw std::invalid_argument("素材JSONの出力が64MiBの上限を超えています。");
			}
			return text;
		}
		catch (const Json::exception& error)
		{
			throw std::invalid_argument("素材JSONの書き出しに失敗しました。JSONエラーID=" + std::to_string(error.id));
		}
	}
}
