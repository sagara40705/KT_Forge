#include <RuntimeIntegration/ModelImport.h>
#include <RuntimeIntegration/ProjectSceneLoader.h>
#include <RuntimeIntegration/SceneUpdater.h>
#include <Content/AssetJson.h>
#include <World/Scene/SceneJson.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <set>
#include <algorithm>
#include <cmath>
#include <limits>

namespace KT::RuntimeIntegration
{
	namespace
	{
		using Json = nlohmann::json;

		std::string Utf8(const std::filesystem::path& path)
		{
			const auto text = path.generic_u8string();
			return {text.begin(), text.end()};
		}

		std::filesystem::path Path(const std::string& text)
		{
			return std::filesystem::path(std::u8string(text.begin(), text.end()));
		}

		// 新規専用directoryにだけ書き込む。flush/closeの失敗を公開前に検出する。
		void Write(const std::filesystem::path& path, std::span<const std::uint8_t> bytes)
		{
			std::filesystem::create_directories(path.parent_path());
			std::ofstream file(path, std::ios::binary | std::ios::trunc);
			if (!file || !file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
			{
				throw std::runtime_error("モデルbundleの書込に失敗しました: " + Utf8(path));
			}
			file.flush();
			if (!file)
			{
				throw std::runtime_error("モデルbundleのflushに失敗しました。");
			}
			file.close();
			if (!file)
			{
				throw std::runtime_error("モデルbundleのcloseに失敗しました。");
			}
		}

		void WriteText(const std::filesystem::path& path, const std::string& text)
		{
			if (text.size() > KT::Content::AssetJson::MaxBytes)
			{
				throw std::invalid_argument("モデルbundleのJSONが64MiBの上限を超えています。");
			}
			Write(path, {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
		}

		Json ReadRecipe(const std::filesystem::path& path)
		{
			std::ifstream input(path, std::ios::binary | std::ios::ate);
			const std::streamoff size = input.tellg();
			if (!input || size <= 0 || size > static_cast<std::streamoff>(KT::Content::AssetJson::MaxBytes))
			{
				throw std::invalid_argument("モデルrecipeを開けないか、サイズが不正です。");
			}
			std::string text(static_cast<std::size_t>(size), '\0');
			input.seekg(0);
			if (!input.read(text.data(), static_cast<std::streamsize>(text.size())))
			{
				throw std::runtime_error("モデルrecipeを最後まで読み込めません。");
			}
			// UUID対応表をDOMの上書きで失わないよう、重複キーを構文解析中に検出する。
			std::vector<std::set<std::string>> keys;
			const auto recipe = Json::parse(text,
				[&](int depth, Json::parse_event_t event, Json& value)
				{
					if (depth > 4)
					{
						throw std::invalid_argument("モデルrecipeのJSON階層が上限4を超えています。");
					}
					if (event == Json::parse_event_t::object_start)
					{
						keys.emplace_back();
					}
					else if (event == Json::parse_event_t::key)
					{
						if (!keys.back().insert(value.get<std::string>()).second)
						{
							throw std::invalid_argument("モデルrecipeのJSONキーが重複しています。");
						}
					}
					else if (event == Json::parse_event_t::object_end)
					{
						keys.pop_back();
					}
					return true;
				});
			if (!recipe.is_object() || recipe.size() != 11)
			{
				throw std::invalid_argument("モデルrecipeに不足または未知のフィールドがあります。");
			}
			if (recipe.at("format") != "KT_Forge.AssimpModel" || !recipe.at("version").is_number_integer() || recipe.at("version") != 1 ||
				recipe.at("assimp") != "392a658f9c271be965271f45e7521a1b80ea4392")
			{
				throw std::invalid_argument("モデルrecipeの形式・version・Assimp版が未対応です。");
			}
			if (!recipe.at("assets").is_object() || !recipe.at("objects").is_object() || !recipe.at("normals").is_number_integer())
			{
				throw std::invalid_argument("モデルrecipeのUUID対応表または法線生成方針の型が不正です。");
			}
			const auto source = recipe.at("source").get<std::string>();
			if (source.empty() || source.find('\0') != std::string::npos || Path(source).has_root_path())
			{
				throw std::invalid_argument("モデルrecipeのsourceにはbundleからの相対パスが必要です。");
			}
			return recipe;
		}

		// 保存先の親内に、この操作専用の一時directoryだけを作る。
		struct StagingBundle
		{
			std::filesystem::path path;
			bool published = false;

			~StagingBundle()
			{
				if (!published)
				{
					// 自分が生成した兄弟directory以外は削除しない。
					std::error_code ignored;
					std::filesystem::remove_all(path, ignored);
				}
			}
		};

		ModelImportResult SaveBundle(const std::filesystem::path& source, const std::filesystem::path& destination,
			KT::Content::ModelImportOptions options, const std::map<std::string, KT::Content::AssetId>& assetIds,
			const std::map<std::string, KT::World::ObjectUuid>& objectIds)
		{
			// 最初に全モデルを変換し、既存directoryや元モデルを変更しない。
			if (destination.filename().empty())
			{
				throw std::invalid_argument("モデルの保存先には新規bundle directory名を指定してください。");
			}
			const auto output = std::filesystem::absolute(destination).lexically_normal();
			if (std::filesystem::exists(output))
			{
				throw std::invalid_argument("モデルbundleの保存先が既に存在します。新規directoryを指定してください。");
			}
			const auto model = KT::Content::ModelImporter::Load(source, options, assetIds);
			std::set<KT::Content::AssetId> uniqueIds;
			for (const auto& asset : model.assets)
			{
				if (!uniqueIds.insert(asset.id).second)
				{
					throw std::invalid_argument("モデルrecipeの異なる素材に同じUUIDが割り当てられています。");
				}
			}
			const auto definition = ModelImport::CreateScene(model, objectIds);
			KT::World::ComponentRegistry components;
			KT::World::RegisterEngineComponents(components);
			components.Freeze();
			KT::World::ScriptRegistry scripts;
			scripts.Freeze();
			const auto sceneText = KT::World::SceneJson::Stringify(definition, components, scripts);
			std::filesystem::create_directories(output.parent_path());
			const auto stagingPath = output.parent_path() / (".kt-model-" + KT::Content::AssetId::Generate().ToString());
			if (!std::filesystem::create_directory(stagingPath))
			{
				throw std::runtime_error("モデルbundle用の新規一時directoryを作れません。");
			}
			StagingBundle staging{stagingPath};
			for (const auto& asset : model.assets)
			{
				KT::Content::AssetJson::Save(stagingPath / (asset.id.ToString() + ".asset.json"), asset);
			}
			for (const auto& [relative, bytes] : model.images)
			{
				Write(stagingPath / Path(relative), bytes);
			}
			WriteText(stagingPath / "scene.json", sceneText);
			Json recipe{{"format", "KT_Forge.AssimpModel"}, {"version", 1}, {"assimp", "392a658f9c271be965271f45e7521a1b80ea4392"},
				{"source", Utf8(std::filesystem::relative(std::filesystem::canonical(source), output))}, {"scale", options.scale},
				{"flipZ", options.flipZ}, {"flipV", options.flipV}, {"normals", static_cast<int>(options.normals)},
				{"generateTangents", options.generateTangents}, {"assets", Json::object()}, {"objects", Json::object()}};
			for (std::size_t index = 0; index < model.assets.size(); ++index)
			{
				recipe["assets"][model.assetKeys[index]] = model.assets[index].id.ToString();
			}
			for (std::size_t index = 0; index < model.nodes.size(); ++index)
			{
				recipe["objects"][model.nodes[index].key] = definition.objects[index].uuid.ToString();
			}
			WriteText(stagingPath / "model.import.json", recipe.dump(2));

			// 画像展開・UUID参照・World生成を製品Loaderで検査してから、一括公開する。
			const auto project = KT::Content::ProjectAssets::Scan(stagingPath);
			KT::Content::AssetCatalog catalog;
			const auto loaded = ProjectSceneLoader::Load(project, "scene.json", catalog);
			if (!loaded.diagnostics.empty())
			{
				throw std::invalid_argument("モデルbundleに欠落または種類違いの素材参照があります。");
			}
			// 原本SceneにはCameraを加えず、同じモデルを表示する確認用Sceneだけを別保存する。
			catalog.Freeze();
			SceneUpdater updater(catalog);
			const auto frame = updater.Update(loaded.scene->GetSnapshot());
			if (frame.candidates.objects.empty() || !frame.diagnostics.empty())
			{
				throw std::invalid_argument("モデルbundleから有効な描画候補を作れません。");
			}
			auto bounds = frame.candidates.objects.front().worldBounds;
			for (const auto& candidate : frame.candidates.objects)
			{
				bounds = KT::Core::Math::Expanded(bounds, candidate.worldBounds.minimum);
				bounds = KT::Core::Math::Expanded(bounds, candidate.worldBounds.maximum);
			}
			// AABBの加減算はdoubleで行い、極端な座標によるfloatのoverflowを防ぐ。
			const auto previewNumber = [](double value)
			{
				if (!std::isfinite(value) || std::abs(value) > (std::numeric_limits<float>::max)())
				{
					throw std::invalid_argument("モデルのAABBから有限floatの確認Cameraを生成できません。");
				}
				return static_cast<float>(value);
			};
			const double centerX = (double(bounds.minimum.x) + bounds.maximum.x) * 0.5;
			const double centerY = (double(bounds.minimum.y) + bounds.maximum.y) * 0.5;
			const double centerZ = (double(bounds.minimum.z) + bounds.maximum.z) * 0.5;
			const double distance = std::max(0.01,
				std::max({double(bounds.maximum.x) - bounds.minimum.x, double(bounds.maximum.y) - bounds.minimum.y,
					double(bounds.maximum.z) - bounds.minimum.z}) *
					2);
			const auto camera = loaded.scene->Commands().CreateEntity("ModelPreviewCamera");
			loaded.scene->Commands().AddComponent<KT::World::LocalTransform>(camera,
				KT::World::LocalTransform{
					{previewNumber(centerX), previewNumber(centerY), previewNumber(centerZ - distance)}, {}, {1, 1, 1}});
			loaded.scene->Commands().AddComponent<KT::World::Camera>(
				camera, KT::World::Camera{1.0471975512f, previewNumber(distance * 0.001), previewNumber(std::max(1000.0, distance * 4))});
			loaded.scene->Update(0);
			KT::World::SceneJson::Save(stagingPath / "preview.scene.json", *loaded.scene, components, scripts);
			std::filesystem::rename(stagingPath, output);
			staging.published = true;
			return {output / "scene.json", model.assets.size(), model.nodes.size(), model.diagnostics};
		}
	}

	KT::World::SceneAsset ModelImport::CreateScene(
		const KT::Content::ImportedModel& model, const std::map<std::string, KT::World::ObjectUuid>& previousIds)
	{
		KT::World::SceneAsset scene;
		scene.name = "ImportedModel";
		for (std::size_t index = 0; index < model.nodes.size(); ++index)
		{
			const auto& node = model.nodes[index];
			const auto previous = previousIds.find(node.key);
			KT::World::SceneObjectDefinition object;
			object.uuid = previous == previousIds.end() ? KT::World::ObjectUuid::Generate() : previous->second;
			object.name = node.name;
			if (node.parent)
			{
				if (*node.parent >= index)
				{
					throw std::invalid_argument("モデルノードの親は同じ配列の先行ノードが必要です。");
				}
				object.parent = scene.objects[*node.parent].uuid;
			}
			const auto vector = [](const KT::Core::Math::Vector3& value) -> KT::World::SceneValue
			{ return KT::World::SceneValue::Array{double(value.x), double(value.y), double(value.z)}; };
			object.components.push_back({"kt.LocalTransform", 1,
				KT::World::SceneValue::Object{{"position", vector(node.position)}, {"scale", vector(node.scale)},
					{"rotation",
						KT::World::SceneValue::Array{
							double(node.rotation.x), double(node.rotation.y), double(node.rotation.z), double(node.rotation.w)}}}});
			if (node.mesh)
			{
				KT::World::SceneValue::Object materials;
				for (const auto& [slot, id] : model.materials.at(*node.mesh))
				{
					materials.emplace(slot, id ? KT::World::SceneValue(id->ToString()) : KT::World::SceneValue{});
				}
				object.components.push_back({"kt.MeshRenderer", 3,
					KT::World::SceneValue::Object{{"mesh", model.meshes.at(*node.mesh).ToString()}, {"material", KT::World::SceneValue{}},
						{"visible", true}, {"materials", std::move(materials)}}});
			}
			scene.objects.push_back(std::move(object));
		}
		return scene;
	}

	ModelImportResult ModelImport::Import(
		const std::filesystem::path& source, const std::filesystem::path& destination, KT::Content::ModelImportOptions options)
	{
		return SaveBundle(source, destination, options, {}, {});
	}

	ModelImportResult ModelImport::Reimport(const std::filesystem::path& previous, const std::filesystem::path& destination)
	{
		try
		{
			const auto recipe = ReadRecipe(previous / "model.import.json");
			KT::Content::ModelImportOptions options;
			const auto& scale = recipe.at("scale");
			if (!scale.is_number() || scale <= 0 || scale > (std::numeric_limits<float>::max)())
			{
				throw std::invalid_argument("モデルrecipeのScaleには有限floatの正数が必要です。");
			}
			options.scale = scale.get<float>();
			options.flipZ = recipe.at("flipZ").get<bool>();
			options.flipV = recipe.at("flipV").get<bool>();
			const auto& normalMode = recipe.at("normals");
			if (normalMode < 0 || normalMode > 2)
			{
				throw std::invalid_argument("モデルrecipeの法線生成方針が不正です。");
			}
			options.normals = static_cast<KT::Content::MeshNormalMode>(normalMode.get<int>());
			options.generateTangents = recipe.at("generateTangents").get<bool>();
			std::map<std::string, KT::Content::AssetId> assets;
			std::map<std::string, KT::World::ObjectUuid> objects;
			for (const auto& [key, value] : recipe.at("assets").items())
			{
				assets.emplace(key, KT::Content::AssetId::Parse(value.get<std::string>()));
			}
			for (const auto& [key, value] : recipe.at("objects").items())
			{
				objects.emplace(key, KT::World::ObjectUuid::Parse(value.get<std::string>()));
			}
			return SaveBundle(previous / Path(recipe.at("source").get<std::string>()), destination, options, assets, objects);
		}
		catch (const Json::exception& error)
		{
			throw std::invalid_argument("モデルrecipeのJSON構文・UTF-8・設定型が不正です。JSONエラーID=" + std::to_string(error.id));
		}
	}
}
