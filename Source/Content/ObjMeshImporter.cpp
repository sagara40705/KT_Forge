#include <Content/ObjMeshImporter.h>
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace KT::Content
{
	static double ReadNumber(std::string_view text)
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

	static float NarrowPosition(double number)
	{
		if (!std::isfinite(number) || std::abs(number) > (std::numeric_limits<float>::max)())
		{
			throw std::invalid_argument("OBJの変換後座標がfloatの有限範囲を超えています。");
		}
		const auto result = static_cast<float>(number);
		if (number != 0 && result == 0)
		{
			throw std::invalid_argument("OBJの座標がfloatへの変換で0に消失します。");
		}
		return result;
	}

	// OBJの1始まりと負の相対Indexを、定義済み要素内の0始まりへ変換する。
	static std::uint32_t ReadIndex(std::string_view text, std::size_t count)
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
			throw std::invalid_argument("OBJの面Indexが不正、または定義済み頂点の範囲外です。");
		}
		return static_cast<std::uint32_t>(number < 0 ? limit + number : number - 1);
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
		if (text.starts_with("\xEF\xBB\xBF"))
		{
			text.remove_prefix(3);
		}
		std::vector<KT::Core::Math::Vector3> positions;
		std::vector<std::uint32_t> indices;
		std::size_t texcoords = 0;
		std::size_t normals = 0;
		bool materials = false;
		bool groups = false;
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
					materials |= command == "usemtl" || command == "mtllib";
					groups |= command == "o" || command == "g" || command == "s";
					continue;
				}

				// 許可した幾何命令の要素数を制限し、余剰の色や多角形を黙って捨てない。
				std::vector<std::string> values;
				std::string value;
				while (row >> value)
				{
					values.push_back(value);
					if (values.size() > 4)
					{
						throw std::invalid_argument("OBJの頂点・面の要素数が初版の対応範囲を超えています。");
					}
				}
				if (command == "v")
				{
					if ((values.size() != 3 && values.size() != 4) || (values.size() == 4 && ReadNumber(values[3]) != 1))
					{
						throw std::invalid_argument("OBJの位置頂点はxyzと、省略可能なw=1だけに対応しています。");
					}
					positions.push_back(
						{NarrowPosition(ReadNumber(values[0]) * options.scale), NarrowPosition(ReadNumber(values[1]) * options.scale),
							NarrowPosition(ReadNumber(values[2]) * options.scale * (options.flipZ ? -1 : 1))});
				}
				else if (command == "vt" || command == "vn")
				{
					if (values.empty() || values.size() > 3 || (command == "vn" && values.size() != 3))
					{
						throw std::invalid_argument("OBJのUVまたは法線の要素数が不正です。");
					}
					for (const auto& number : values)
					{
						(void)ReadNumber(number);
					}
					if (command == "vt")
					{
						++texcoords;
					}
					else
					{
						++normals;
					}
				}
				else if (command == "f")
				{
					if (values.size() != 3)
					{
						throw std::invalid_argument("OBJは三角形の面だけに対応しています。出力元で三角形化してください。");
					}
					std::array<std::uint32_t, 3> face;
					for (std::size_t corner = 0; corner < face.size(); ++corner)
					{
						const std::string_view token = values[corner];
						const auto slash = token.find('/');
						face[corner] = ReadIndex(token.substr(0, slash), positions.size());
						if (slash != std::string_view::npos)
						{
							const auto tail = token.substr(slash + 1);
							const auto secondSlash = tail.find('/');
							if (secondSlash == std::string_view::npos)
							{
								(void)ReadIndex(tail, texcoords);
							}
							else
							{
								if (secondSlash != 0)
								{
									(void)ReadIndex(tail.substr(0, secondSlash), texcoords);
								}
								(void)ReadIndex(tail.substr(secondSlash + 1), normals);
							}
						}
					}
					if (options.flipZ)
					{
						std::swap(face[1], face[2]);
					}
					indices.insert(indices.end(), face.begin(), face.end());
				}
				else
				{
					throw std::invalid_argument("OBJの命令が未対応です: " + command);
				}
			}
			catch (const std::invalid_argument& error)
			{
				throw std::invalid_argument("OBJ " + std::to_string(lineNumber) + "行: " + error.what());
			}
		}
		return {MeshAsset(std::move(positions), std::move(indices)), texcoords != 0, normals != 0, materials, groups};
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
			throw std::invalid_argument("OBJの入力と素材の保存先に同じファイルは指定できません。");
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

		// 再取込では出力側の永続IDと表示名を保持し、Scene参照を失効させない。
		const bool replaced = std::filesystem::exists(destination);
		AssetId id;
		const auto filename = source.stem().u8string();
		std::string name(filename.begin(), filename.end());
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
		else
		{
			id = AssetId::Generate();
		}
		AssetJson::Save(destination, {id, std::move(name), std::move(result.mesh)});
		return {MeshAssetId(id), replaced, result.omittedTexcoords, result.omittedNormals, result.omittedMaterials, result.mergedGroups};
	}
}
