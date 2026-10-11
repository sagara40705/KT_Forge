#include <Content/ObjMeshImporter.h>
#include <Content/MeshGeometry.h>
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <algorithm>

namespace KT::Content
{
	namespace
	{
		double ReadNumber(std::string_view text)
		{
			if (text.starts_with('+'))
			{
				text.remove_prefix(1);
			}
			double number = 0;
			const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
			if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || !std::isfinite(number))
			{
				throw std::invalid_argument("OBJの座標には有限の数値が必要です。");
			}
			return number;
		}

		float Narrow(double number)
		{
			if (!std::isfinite(number) || std::abs(number) > (std::numeric_limits<float>::max)() ||
				(number != 0 && static_cast<float>(number) == 0))
			{
				throw std::invalid_argument("OBJの変換後の値を有限floatで保持できません。");
			}
			return static_cast<float>(number);
		}

		std::uint32_t ReadIndex(std::string_view text, std::size_t count)
		{
			if (text.starts_with('+'))
			{
				text.remove_prefix(1);
			}
			std::int64_t number = 0;
			const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
			const auto limit = static_cast<std::int64_t>(count);
			if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || number == 0 || number < -limit || number > limit)
			{
				throw std::invalid_argument("OBJの面Indexが定義済み要素の範囲外です。");
			}
			return static_cast<std::uint32_t>(number < 0 ? limit + number : number - 1);
		}

		// OBJの別々の属性Indexを、面cornerとして保持する。
		struct Corner
		{
			std::uint32_t position = 0;
			std::optional<std::uint32_t> uv, normal;
			std::string smoothing;
		};

		std::string Utf8(const std::filesystem::path& path)
		{
			const auto value = path.generic_u8string();
			return {value.begin(), value.end()};
		}
	}

	ObjMeshImportResult ObjMeshImporter::Parse(std::string_view text, ObjMeshImportOptions options)
	{
		if (text.size() > AssetJson::MaxBytes || text.find('\0') != std::string_view::npos)
		{
			throw std::invalid_argument("OBJの入力が64MiBを超えているか、NUL文字を含んでいます。");
		}
		if (!std::isfinite(options.scale) || options.scale <= 0)
		{
			throw std::invalid_argument("OBJの単位scaleには有限の正数が必要です。");
		}
		if (options.normals != MeshNormalMode::Preserve && options.normals != MeshNormalMode::Flat &&
			options.normals != MeshNormalMode::Smooth)
		{
			throw std::invalid_argument("OBJの法線生成方針が未対応です。");
		}
		if (text.starts_with("\xEF\xBB\xBF"))
		{
			text.remove_prefix(3);
		}
		std::vector<KT::Core::Math::Vector3> sourcePositions, sourceNormals;
		std::vector<KT::Core::Math::Vector2> sourceTexcoords;
		std::vector<Corner> corners;
		std::vector<MeshSubmesh> submeshes;
		std::optional<bool> hasTexcoords;
		std::string smoothing = "off", slot = "default";
		bool omittedMaterials = false, groups = false;
		std::istringstream input{std::string(text)};
		std::string line;
		std::size_t lineNumber = 0;
		while (std::getline(input, line))
		{
			++lineNumber;
			line.erase(line.find('#') == std::string::npos ? line.size() : line.find('#'));
			std::istringstream row(line);
			std::string command;
			if (!(row >> command))
			{
				continue;
			}
			try
			{
				if (line.find('\\') != std::string::npos)
				{
					throw std::invalid_argument("OBJの行継続は未対応です。");
				}
				if (command == "o" || command == "g" || command == "s" || command == "usemtl" || command == "mtllib")
				{
					std::string argument;
					std::getline(row >> std::ws, argument);
					const auto end = argument.find_last_not_of(" \t\r");
					argument.resize(end == std::string::npos ? 0 : end + 1);
					if (command == "usemtl")
					{
						if (argument.empty() || argument.size() > 256)
						{
							throw std::invalid_argument("OBJのusemtlには1..256byteのMaterial slot名が必要です。");
						}
						slot = argument;
					}
					else if (command == "s")
					{
						if (argument.empty())
						{
							throw std::invalid_argument("OBJのsに平滑化groupがありません。");
						}
						smoothing = argument == "0" ? "off" : argument;
					}
					omittedMaterials |= command == "mtllib";
					groups |= command == "o" || command == "g";
					continue;
				}
				std::vector<std::string> values;
				std::string value;
				while (row >> value)
				{
					values.push_back(value);
					if (values.size() > 4)
					{
						throw std::invalid_argument("OBJの頂点・面の要素数が対応範囲を超えています。");
					}
				}
				if (command == "v" || command == "vn")
				{
					if ((values.size() != 3 && !(command == "v" && values.size() == 4 && ReadNumber(values[3]) == 1)))
					{
						throw std::invalid_argument("OBJの位置・法線にはxyzの3要素が必要です。");
					}
					const double x = ReadNumber(values[0]), y = ReadNumber(values[1]), z = ReadNumber(values[2]) * (options.flipZ ? -1 : 1);
					if (command == "v")
					{
						sourcePositions.push_back({Narrow(x * options.scale), Narrow(y * options.scale), Narrow(z * options.scale)});
					}
					else
					{
						const double length = std::hypot(x, y, z);
						if (!std::isfinite(length) || length == 0)
						{
							throw std::invalid_argument("OBJの法線が全ゼロまたは正規化できない値です。");
						}
						sourceNormals.push_back(
							{static_cast<float>(x / length), static_cast<float>(y / length), static_cast<float>(z / length)});
					}
				}
				else if (command == "vt")
				{
					if (values.empty() || values.size() > 3 || (values.size() == 3 && ReadNumber(values[2]) != 0))
					{
						throw std::invalid_argument("OBJのUV0には1..2要素と省略可能な第3成分0が必要です。");
					}
					const double v = values.size() >= 2 ? ReadNumber(values[1]) : 0;
					sourceTexcoords.push_back({Narrow(ReadNumber(values[0])), Narrow(options.flipV ? 1 - v : v)});
				}
				else if (command == "f")
				{
					if (values.size() != 3)
					{
						throw std::invalid_argument("OBJは三角形だけに対応しています。出力元で三角形化してください。");
					}
					if (corners.size() > (std::numeric_limits<std::uint32_t>::max)() / 48 - 3)
					{
						throw std::overflow_error("OBJの分割後の頂点数がuint32 byte上限を超えます。");
					}
					std::array<Corner, 3> face;
					for (std::size_t cornerIndex = 0; cornerIndex < 3; ++cornerIndex)
					{
						const std::string_view token = values[cornerIndex];
						const auto slash = token.find('/');
						auto& corner = face[cornerIndex];
						corner.position = ReadIndex(token.substr(0, slash), sourcePositions.size());
						if (slash != std::string_view::npos)
						{
							const auto tail = token.substr(slash + 1);
							const auto second = tail.find('/');
							const auto uv = tail.substr(0, second);
							if (!uv.empty())
							{
								corner.uv = ReadIndex(uv, sourceTexcoords.size());
							}
							if (second != std::string_view::npos)
							{
								corner.normal = ReadIndex(tail.substr(second + 1), sourceNormals.size());
							}
							else if (uv.empty())
							{
								throw std::invalid_argument("OBJの面cornerの属性Indexが空です。");
							}
						}
						const bool hasUv = corner.uv.has_value();
						if (hasTexcoords && *hasTexcoords != hasUv)
						{
							throw std::invalid_argument("OBJのUV0あり・なしの面cornerは混在できません。");
						}
						hasTexcoords = hasUv;
						corner.smoothing = options.normals == MeshNormalMode::Smooth ?
							"smooth:all" :
							(options.normals == MeshNormalMode::Flat || smoothing == "off" ? "face:" + std::to_string(corners.size() / 3) :
																							 "smooth:" + smoothing);
					}
					if (options.flipZ)
					{
						std::swap(face[1], face[2]);
					}
					if (submeshes.empty() || submeshes.back().materialSlot != slot)
					{
						submeshes.push_back({static_cast<std::uint32_t>(corners.size()), 0, slot});
					}
					submeshes.back().indexCount += 3;
					corners.insert(corners.end(), face.begin(), face.end());
				}
				else
				{
					throw std::invalid_argument("OBJの未対応命令です: " + command);
				}
			}
			catch (const std::exception& error)
			{
				throw std::invalid_argument("OBJの行" + std::to_string(lineNumber) + ": " + error.what());
			}
		}

		// 平滑化はUVの継ぎ目をまたいで計算し、その後にUV・明示法線で頂点を分割する。
		std::map<std::pair<std::uint32_t, std::string>, std::uint32_t> smoothVertices;
		std::vector<KT::Core::Math::Vector3> smoothPositions;
		std::vector<std::uint32_t> smoothIndices;
		for (const auto& corner : corners)
		{
			const auto [found, inserted] =
				smoothVertices.emplace(std::pair{corner.position, corner.smoothing}, static_cast<std::uint32_t>(smoothPositions.size()));
			if (inserted)
			{
				smoothPositions.push_back(sourcePositions[corner.position]);
			}
			smoothIndices.push_back(found->second);
		}
		std::optional<MeshAsset> generated;
		if (options.normals != MeshNormalMode::Preserve ||
			std::any_of(corners.begin(), corners.end(), [](const Corner& corner) { return !corner.normal; }))
		{
			generated = MeshGeometry::Generate(MeshAsset(std::move(smoothPositions), smoothIndices), MeshNormalMode::Smooth, false);
		}
		std::map<std::tuple<std::uint32_t, std::optional<std::uint32_t>, std::optional<std::uint32_t>, std::string>, std::uint32_t>
			vertices;
		std::vector<KT::Core::Math::Vector3> positions, normals;
		std::vector<KT::Core::Math::Vector2> texcoords;
		std::vector<std::uint32_t> indices;
		for (std::size_t index = 0; index < corners.size(); ++index)
		{
			const auto& corner = corners[index];
			const auto normal = options.normals == MeshNormalMode::Preserve ? corner.normal : std::nullopt;
			const auto key = std::tuple{corner.position, corner.uv, normal, normal ? std::string{} : corner.smoothing};
			const auto [found, inserted] = vertices.emplace(key, static_cast<std::uint32_t>(positions.size()));
			if (inserted)
			{
				positions.push_back(sourcePositions[corner.position]);
				normals.push_back(normal ? sourceNormals[*normal] : generated->Normals()[smoothIndices[index]]);
				if (corner.uv)
				{
					texcoords.push_back(sourceTexcoords[*corner.uv]);
				}
			}
			indices.push_back(found->second);
		}
		auto mesh = MeshGeometry::Generate(
			MeshAsset(std::move(positions), std::move(indices), std::move(texcoords), std::move(normals), {}, std::move(submeshes)),
			MeshNormalMode::Preserve, options.generateTangents);
		return {std::move(mesh), false, false, omittedMaterials, groups};
	}

	ObjMeshSaveResult ObjMeshImporter::Import(
		const std::filesystem::path& source, const std::filesystem::path& destination, ObjMeshImportOptions options)
	{
		if (!destination.filename().wstring().ends_with(L".asset.json"))
		{
			throw std::invalid_argument("OBJの保存先には*.asset.jsonを指定してください。");
		}
		if (std::filesystem::exists(destination) && std::filesystem::equivalent(source, destination))
		{
			throw std::invalid_argument("OBJの入力と素材の保存先は同じファイルにできません。");
		}
		std::ifstream input(source, std::ios::binary | std::ios::ate);
		if (!input)
		{
			throw std::runtime_error("OBJファイルを開けません。");
		}
		const auto size = input.tellg();
		if (size < 0 || size > static_cast<std::streamoff>(AssetJson::MaxBytes))
		{
			throw std::invalid_argument("OBJファイルのサイズが不正、または64MiBを超えています。");
		}
		std::string text(static_cast<std::size_t>(size), '\0');
		input.seekg(0);
		input.read(text.data(), static_cast<std::streamsize>(text.size()));
		if (!input || input.peek() != std::char_traits<char>::eof())
		{
			throw std::runtime_error("OBJの読込に失敗したか、読込中にサイズが変わりました。");
		}
		auto result = Parse(text, options);
		const bool replaced = std::filesystem::exists(destination);
		AssetId id = AssetId::Generate();
		std::string name = Utf8(source.stem());
		if (replaced)
		{
			const auto previous = AssetJson::Read(destination);
			if (!std::holds_alternative<MeshAsset>(previous.data))
			{
				throw std::invalid_argument("OBJの再取込先はMesh素材である必要があります。");
			}
			id = previous.id;
			name = previous.name;
		}
		// recipeは素材位置に対する相対パスで保存し、配置ごとの絶対パスを固定しない。
		const auto relative =
			std::filesystem::relative(std::filesystem::absolute(source), std::filesystem::absolute(destination).parent_path());
		MeshImportRecipe recipe{Utf8(relative), 1, options.scale, options.flipZ, options.flipV, options.normals, options.generateTangents};
		const auto& mesh = result.mesh;
		MeshAsset saved({mesh.Positions().begin(), mesh.Positions().end()}, {mesh.Indices().begin(), mesh.Indices().end()},
			{mesh.Texcoords().begin(), mesh.Texcoords().end()}, {mesh.Normals().begin(), mesh.Normals().end()},
			{mesh.Tangents().begin(), mesh.Tangents().end()}, {mesh.Submeshes().begin(), mesh.Submeshes().end()}, std::move(recipe));
		AssetJson::Save(destination, {id, std::move(name), std::move(saved)});
		return {MeshAssetId(id), replaced, false, false, result.omittedMaterials, result.mergedGroups};
	}

	ObjMeshSaveResult ObjMeshImporter::Reimport(const std::filesystem::path& destination)
	{
		const auto previous = AssetJson::Read(destination);
		const auto* mesh = std::get_if<MeshAsset>(&previous.data);
		if (!mesh || !mesh->ImportRecipe())
		{
			throw std::invalid_argument("再取込にはOBJ取込recipe付きのMesh素材が必要です。");
		}
		const auto& recipe = *mesh->ImportRecipe();
		const std::filesystem::path source(std::u8string(recipe.source.begin(), recipe.source.end()));
		return Import(destination.parent_path() / source, destination,
			{recipe.scale, recipe.flipZ, recipe.flipV, recipe.normals, recipe.generateTangents});
	}
}
