#include <Content/MeshGeometry.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>

namespace KT::Content
{
	namespace
	{
		// 属性生成中の積・差のoverflowを避けるためdoubleで計算する。
		struct Direction
		{
			double x = 0, y = 0, z = 0;

			Direction operator+(Direction other) const
			{
				return {x + other.x, y + other.y, z + other.z};
			}

			Direction operator-(Direction other) const
			{
				return {x - other.x, y - other.y, z - other.z};
			}

			Direction operator*(double value) const
			{
				return {x * value, y * value, z * value};
			}
		};

		Direction Wide(KT::Core::Math::Vector3 value)
		{
			return {value.x, value.y, value.z};
		}

		double Dot(Direction left, Direction right)
		{
			return left.x * right.x + left.y * right.y + left.z * right.z;
		}

		Direction Cross(Direction left, Direction right)
		{
			return {left.y * right.z - left.z * right.y, left.z * right.x - left.x * right.z, left.x * right.y - left.y * right.x};
		}

		Direction Unit(Direction value)
		{
			const double length = std::sqrt(Dot(value, value));
			if (!std::isfinite(length) || length == 0)
			{
				throw std::invalid_argument("Meshの退化面・未参照頂点・相殺した属性から単位方向を生成できません。");
			}
			return value * (1 / length);
		}

		KT::Core::Math::Vector3 Narrow(Direction value)
		{
			return {static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z)};
		}
	}

	MeshAsset MeshGeometry::Generate(const MeshAsset& source, MeshNormalMode mode, bool generateTangents)
	{
		source.Validate();
		if (mode != MeshNormalMode::Preserve && mode != MeshNormalMode::Flat && mode != MeshNormalMode::Smooth)
		{
			throw std::invalid_argument("Meshの法線生成方針が未対応です。");
		}
		std::vector<KT::Core::Math::Vector3> positions(source.Positions().begin(), source.Positions().end());
		std::vector<KT::Core::Math::Vector2> texcoords(source.Texcoords().begin(), source.Texcoords().end());
		std::vector<std::uint32_t> indices(source.Indices().begin(), source.Indices().end());
		std::vector<KT::Core::Math::Vector3> normals;
		std::vector<MeshSubmesh> submeshes(source.Submeshes().begin(), source.Submeshes().end());
		std::vector<SkinInfluence> influences(source.Influences().begin(), source.Influences().end());

		// Flatは面ごとに頂点を分離し、既存のSubmesh範囲を保つ。
		if (mode == MeshNormalMode::Flat)
		{
			positions.clear();
			texcoords.clear();
			influences.clear();
			for (std::size_t first = 0; first < indices.size(); first += 3)
			{
				const auto direction = Unit(Cross(Wide(source.Positions()[indices[first + 1]]) - Wide(source.Positions()[indices[first]]),
					Wide(source.Positions()[indices[first + 2]]) - Wide(source.Positions()[indices[first]])));
				for (std::size_t corner = 0; corner < 3; ++corner)
				{
					if (positions.size() >= (std::numeric_limits<std::uint32_t>::max)() / 48)
					{
						throw std::overflow_error("Flat法線生成後の頂点byte数がuint32範囲を超えます。");
					}
					const auto original = indices[first + corner];
					indices[first + corner] = static_cast<std::uint32_t>(positions.size());
					positions.push_back(source.Positions()[original]);
					if (source.Skin())
					{
						influences.push_back(source.Influences()[original]);
					}
					normals.push_back(Narrow(direction));
					if (!source.Texcoords().empty())
					{
						texcoords.push_back(source.Texcoords()[original]);
					}
				}
			}
		}
		else if (mode == MeshNormalMode::Preserve && !source.Normals().empty())
		{
			normals.assign(source.Normals().begin(), source.Normals().end());
		}
		else
		{
			// 各cornerの角度で面法線を重み付けする。UVや硬い稜線の共有は取込側が決める。
			std::vector<Direction> sums(positions.size());
			for (std::size_t first = 0; first < indices.size(); first += 3)
			{
				const auto faceNormal = Unit(Cross(Wide(positions[indices[first + 1]]) - Wide(positions[indices[first]]),
					Wide(positions[indices[first + 2]]) - Wide(positions[indices[first]])));
				for (std::size_t corner = 0; corner < 3; ++corner)
				{
					const auto current = indices[first + corner];
					const auto edge0 = Unit(Wide(positions[indices[first + (corner + 1) % 3]]) - Wide(positions[current]));
					const auto edge1 = Unit(Wide(positions[indices[first + (corner + 2) % 3]]) - Wide(positions[current]));
					const double weight = std::acos(std::clamp(Dot(edge0, edge1), -1.0, 1.0));
					sums[current] = sums[current] + faceNormal * weight;
				}
			}
			for (const auto sum : sums)
			{
				normals.push_back(Narrow(Unit(sum)));
			}
		}

		std::vector<std::array<float, 4>> tangents;
		if (generateTangents && !texcoords.empty())
		{
			// 鏡像UVの符号ごとに頂点を分離し、異なるTBNを一つの接線へ混ぜない。
			std::map<std::pair<std::uint32_t, int>, std::uint32_t> vertices;
			std::vector<KT::Core::Math::Vector3> splitPositions, splitNormals;
			std::vector<KT::Core::Math::Vector2> splitTexcoords;
			std::vector<SkinInfluence> splitInfluences;
			std::vector<Direction> tangentSums, bitangentSums;
			for (std::size_t first = 0; first < indices.size(); first += 3)
			{
				const auto i0 = indices[first], i1 = indices[first + 1], i2 = indices[first + 2];
				const auto edge0 = Wide(positions[i1]) - Wide(positions[i0]);
				const auto edge1 = Wide(positions[i2]) - Wide(positions[i0]);
				(void)Unit(Cross(edge0, edge1));
				const double du0 = double(texcoords[i1].x) - texcoords[i0].x, dv0 = double(texcoords[i1].y) - texcoords[i0].y;
				const double du1 = double(texcoords[i2].x) - texcoords[i0].x, dv1 = double(texcoords[i2].y) - texcoords[i0].y;
				const double determinant = du0 * dv1 - du1 * dv0;
				if (determinant == 0 || !std::isfinite(determinant))
				{
					throw std::invalid_argument("Meshの退化UVから接線を生成できません。UV0を修正するか接線生成を無効にしてください。");
				}
				const auto tangent = Unit((edge0 * dv1 - edge1 * dv0) * (1 / determinant));
				const auto bitangent = Unit((edge1 * du0 - edge0 * du1) * (1 / determinant));
				// 同じ三角形内でTBN符号が異なる法線を拒否する。
				const int sign = Dot(Cross(Wide(normals[i0]), tangent), bitangent) < 0 ? -1 : 1;
				for (std::size_t corner = 0; corner < 3; ++corner)
				{
					const auto original = indices[first + corner];
					const int cornerSign = Dot(Cross(Wide(normals[original]), tangent), bitangent) < 0 ? -1 : 1;
					if (cornerSign != sign)
					{
						throw std::invalid_argument("三角形内の法線から整合する接線符号を生成できません。");
					}
					const auto key = std::pair{original, sign};
					auto found = vertices.find(key);
					if (found == vertices.end())
					{
						if (splitPositions.size() >= (std::numeric_limits<std::uint32_t>::max)() / 48)
						{
							throw std::overflow_error("接線分割後の頂点byte数がuint32範囲を超えます。");
						}
						found = vertices.emplace(key, static_cast<std::uint32_t>(splitPositions.size())).first;
						splitPositions.push_back(positions[original]);
						splitNormals.push_back(normals[original]);
						splitTexcoords.push_back(texcoords[original]);
						if (source.Skin())
						{
							splitInfluences.push_back(influences[original]);
						}
						tangentSums.push_back({});
						bitangentSums.push_back({});
					}
					const auto current = found->second;
					tangentSums[current] = tangentSums[current] + tangent;
					bitangentSums[current] = bitangentSums[current] + bitangent;
					indices[first + corner] = current;
				}
			}
			for (std::size_t index = 0; index < splitPositions.size(); ++index)
			{
				const auto normal = Wide(splitNormals[index]);
				const auto tangent = Unit(tangentSums[index] - normal * Dot(normal, tangentSums[index]));
				const float sign = Dot(Cross(normal, tangent), bitangentSums[index]) < 0 ? -1.0f : 1.0f;
				tangents.push_back({static_cast<float>(tangent.x), static_cast<float>(tangent.y), static_cast<float>(tangent.z), sign});
			}
			positions.swap(splitPositions);
			normals.swap(splitNormals);
			texcoords.swap(splitTexcoords);
			influences.swap(splitInfluences);
		}
		auto recipe = source.ImportRecipe();
		if (recipe)
		{
			if (mode != MeshNormalMode::Preserve)
			{
				recipe->normals = mode;
			}
			recipe->generateTangents = generateTangents;
		}
		return MeshAsset(std::move(positions), std::move(indices), std::move(texcoords), std::move(normals), std::move(tangents),
			std::move(submeshes), std::move(recipe), source.Skin(), std::move(influences));
	}
}
