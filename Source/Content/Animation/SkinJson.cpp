#include <Content/Animation/SkinJson.h>
#include <nlohmann/json.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace KT::Content
{
	namespace
	{
		using Json = nlohmann::json;

		void Fields(const Json& value, std::initializer_list<const char*> fields)
		{
			if (!value.is_object() || value.size() != fields.size())
			{
				throw std::invalid_argument("Skin JSONの必須または未知のフィールドが不正です。");
			}
			for (const auto name : fields)
			{
				if (!value.contains(name))
				{
					throw std::invalid_argument("Skin JSONの必須フィールドがありません: " + std::string(name));
				}
			}
		}

		float Number(const Json& value)
		{
			if (!value.is_number())
			{
				throw std::invalid_argument("Skin JSONの座標・時刻・Weightには数値が必要です。");
			}
			const auto wide = value.get<double>();
			if (!std::isfinite(wide) || std::abs(wide) > (std::numeric_limits<float>::max)())
			{
				throw std::invalid_argument("Skin JSONの数値が有限floatの範囲を超えています。");
			}
			const auto result = static_cast<float>(wide);
			if (wide != 0 && result == 0)
			{
				throw std::invalid_argument("Skin JSONの数値がfloatへの縮小で0に消失します。");
			}
			return result;
		}

		void Array(const Json& value, std::size_t size)
		{
			if (!value.is_array() || value.size() != size)
			{
				throw std::invalid_argument("Skin JSONの配列要素数が不正です。");
			}
		}

		std::uint32_t Index(const Json& value)
		{
			if (!value.is_number_integer() || value < 0 || value > (std::numeric_limits<std::uint32_t>::max)())
			{
				throw std::invalid_argument("Skin JSONのjoint番号にはuint32の整数が必要です。");
			}
			return value.get<std::uint32_t>();
		}

		KT::Core::Math::Vector3 Vector(const Json& value)
		{
			Array(value, 3);
			return {Number(value[0]), Number(value[1]), Number(value[2])};
		}

		KT::Core::Math::Quaternion Rotation(const Json& value)
		{
			Array(value, 4);
			return {Number(value[0]), Number(value[1]), Number(value[2]), Number(value[3])};
		}

		KT::Core::Math::Matrix4 Matrix(const Json& value)
		{
			Array(value, 16);
			KT::Core::Math::Matrix4 result;
			for (std::size_t index = 0; index < 16; ++index)
			{
				result.elements[index] = Number(value[index]);
			}
			return result;
		}

		Json EncodeVector(KT::Core::Math::Vector3 value)
		{
			return Json::array({value.x, value.y, value.z});
		}

		Json EncodeRotation(KT::Core::Math::Quaternion value)
		{
			return Json::array({value.x, value.y, value.z, value.w});
		}
	}

	nlohmann::json EncodeSkin(const SkinDefinition& definition, std::span<const SkinInfluence> influences)
	{
		definition.Validate();
		Json result{{"joints", Json::array()}, {"bindings", Json::array()}, {"clips", Json::array()},
			{"modelToMesh", definition.modelToMesh.elements}, {"influences", Json::array()}};
		for (const auto& joint : definition.joints)
		{
			result["joints"].push_back(
				Json{{"name", joint.name}, {"parent", joint.parent}, {"translation", EncodeVector(joint.translation)},
					{"rotation", EncodeRotation(joint.rotation)}, {"scale", EncodeVector(joint.scale)}});
		}
		for (const auto& binding : definition.bindings)
		{
			result["bindings"].push_back(Json{{"joint", binding.joint}, {"inverseBind", binding.inverseBind.elements}});
		}
		for (const auto& clip : definition.clips)
		{
			Json tracks = Json::array();
			for (const auto& track : clip.tracks)
			{
				Json output{{"translations", Json::array()}, {"rotations", Json::array()}, {"scales", Json::array()}};
				for (const auto& key : track.translations)
				{
					output["translations"].push_back(Json{{"time", key.time}, {"value", EncodeVector(key.value)}});
				}
				for (const auto& key : track.rotations)
				{
					output["rotations"].push_back(Json{{"time", key.time}, {"value", EncodeRotation(key.value)}});
				}
				for (const auto& key : track.scales)
				{
					output["scales"].push_back(Json{{"time", key.time}, {"value", EncodeVector(key.value)}});
				}
				tracks.push_back(std::move(output));
			}
			result["clips"].push_back(Json{{"name", clip.name}, {"duration", clip.duration}, {"tracks", std::move(tracks)}});
		}
		for (const auto& influence : influences)
		{
			result["influences"].push_back(Json{{"joints", influence.joints}, {"weights", influence.weights}});
		}
		return result;
	}

	std::shared_ptr<const SkinDefinition> DecodeSkin(const nlohmann::json& value, std::vector<SkinInfluence>& influences)
	{
		Fields(value, {"joints", "bindings", "clips", "modelToMesh", "influences"});
		if (!value.at("joints").is_array() || value.at("joints").empty() || value.at("joints").size() > SkinDefinition::MaxJoints ||
			!value.at("bindings").is_array() || value.at("bindings").empty() || value.at("bindings").size() > SkinDefinition::MaxJoints ||
			!value.at("clips").is_array() || value.at("clips").size() > 256 || !value.at("influences").is_array())
		{
			throw std::invalid_argument("Skin JSONのjoint・binding・Clip・影響配列の種類または上限が不正です。");
		}
		auto result = std::make_shared<SkinDefinition>();
		result->modelToMesh = Matrix(value.at("modelToMesh"));
		for (const auto& joint : value.at("joints"))
		{
			Fields(joint, {"name", "parent", "translation", "rotation", "scale"});
			if (!joint.at("parent").is_number_integer() || joint.at("parent") < -1 || joint.at("parent") > 255)
			{
				throw std::invalid_argument("Skin JSONのparentは-1..255の整数が必要です。");
			}
			result->joints.push_back({joint.at("name").get<std::string>(), joint.at("parent").get<std::int32_t>(),
				Vector(joint.at("translation")), Rotation(joint.at("rotation")), Vector(joint.at("scale"))});
		}
		for (const auto& binding : value.at("bindings"))
		{
			Fields(binding, {"joint", "inverseBind"});
			result->bindings.push_back({Index(binding.at("joint")), Matrix(binding.at("inverseBind"))});
		}
		std::size_t keyCount = 0;
		for (const auto& clip : value.at("clips"))
		{
			Fields(clip, {"name", "duration", "tracks"});
			Array(clip.at("tracks"), result->joints.size());
			AnimationClip output{clip.at("name").get<std::string>(), Number(clip.at("duration")), {}};
			for (const auto& track : clip.at("tracks"))
			{
				Fields(track, {"translations", "rotations", "scales"});
				JointTrack keys;
				const auto decodeKeys = [&](const Json& source, auto& destination, auto decode)
				{
					if (!source.is_array() || source.size() > SkinDefinition::MaxKeys - keyCount)
					{
						throw std::invalid_argument("Skin JSONのkey配列の種類または100万件上限が不正です。");
					}
					keyCount += source.size();
					for (const auto& key : source)
					{
						Fields(key, {"time", "value"});
						destination.push_back({Number(key.at("time")), decode(key.at("value"))});
					}
				};
				decodeKeys(track.at("translations"), keys.translations, Vector);
				decodeKeys(track.at("rotations"), keys.rotations, Rotation);
				decodeKeys(track.at("scales"), keys.scales, Vector);
				output.tracks.push_back(std::move(keys));
			}
			result->clips.push_back(std::move(output));
		}
		std::vector<SkinInfluence> candidate;
		for (const auto& influence : value.at("influences"))
		{
			Fields(influence, {"joints", "weights"});
			Array(influence.at("joints"), 4);
			Array(influence.at("weights"), 4);
			SkinInfluence output;
			for (std::size_t index = 0; index < 4; ++index)
			{
				output.joints[index] = Index(influence.at("joints")[index]);
				output.weights[index] = Number(influence.at("weights")[index]);
			}
			candidate.push_back(output);
		}
		result->Validate();
		result->ValidateInfluences(candidate, candidate.size());
		influences.swap(candidate);
		return result;
	}
}
