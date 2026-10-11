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
			if (!root.at("version").is_number_integer() || (root.at("version") != 1 && root.at("version") != 2))
			{
				throw std::invalid_argument("素材JSONのversionは1と2に対応しています。");
			}
			const bool extended = root.at("version") == 2;
			const auto id = AssetId::Parse(root.at("assetId").get<std::string>());
			const auto name = root.at("name").get<std::string>();
			const auto type = root.at("type").get<std::string>();
			const auto& data = root.at("data");
			if (type == "kt.Mesh")
			{
				if (extended)
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
				return {id, name, MeshAsset(std::move(positions), std::move(indices), std::move(texcoords))};
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
