#pragma once
#include <Physics/PhysicsRuntimeInternal.h>
#include <Physics/PhysicsWorld.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/StateRecorder.h>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>

namespace KT::Physics
{
	// 左手系との境界でZを反転する。角速度・torqueは軸性vectorなので別変換する。
	inline JPH::Vec3 ToPolar(Vector3 value)
	{
		return {value.x, value.y, -value.z};
	}

	inline Vector3 FromPolar(JPH::Vec3Arg value)
	{
		return {value.GetX(), value.GetY(), -value.GetZ()};
	}

	inline JPH::Vec3 ToAxial(Vector3 value)
	{
		return {-value.x, -value.y, value.z};
	}

	inline Vector3 FromAxial(JPH::Vec3Arg value)
	{
		return {-value.GetX(), -value.GetY(), value.GetZ()};
	}

	inline JPH::Quat ToRotation(Quaternion value)
	{
		value = KT::Core::Math::Normalize(value);
		return {-value.x, -value.y, value.z, value.w};
	}

	inline Quaternion FromRotation(JPH::QuatArg value)
	{
		return {-value.GetX(), -value.GetY(), value.GetZ(), value.GetW()};
	}

	// Joltのlock内で例外を外へ出さない。確保失敗は呼出完了後に検出する。
	class CheckpointStream final : public JPH::StateRecorder
	{
	public:
		CheckpointStream() = default;
		// 復元呼出中だけ不変checkpointを借用する。cursorは各streamが独立して持つ。
		explicit CheckpointStream(std::span<const std::byte> input)
			: input_(input)
		{
		}

		// 保存用の所有領域。読取専用streamでは使用しない。
		std::vector<std::byte> bytes;
		std::size_t cursor = 0;
		bool failed = false;

		void WriteBytes(const void* data, std::size_t size) override
		{
			if (input_)
			{
				failed = true;
				return;
			}
			if (failed)
			{
				return;
			}
			try
			{
				const auto* begin = static_cast<const std::byte*>(data);
				bytes.insert(bytes.end(), begin, begin + size);
			}
			catch (...)
			{
				failed = true;
			}
		}

		void ReadBytes(void* data, std::size_t size) override
		{
			const auto input = input_.value_or(std::span<const std::byte>(bytes));
			if (cursor > input.size() || size > input.size() - cursor)
			{
				failed = true;
				std::memset(data, 0, size);
				return;
			}
			std::memcpy(data, input.data() + cursor, size);
			cursor += size;
		}

		bool IsEOF() const override
		{
			return cursor == (input_ ? input_->size() : bytes.size());
		}

		bool IsFailed() const override
		{
			return failed;
		}

	private:
		// 所有者のClone呼出中だけ有効。保存済みbytesの内容・cursorを変更しない。
		std::optional<std::span<const std::byte>> input_;
	};

	// 静止物と可動物の二つのbroadphase。ObjectLayerの下位3bitはゲーム層。
	class BroadLayers final : public JPH::BroadPhaseLayerInterface
	{
	public:
		JPH::uint GetNumBroadPhaseLayers() const override
		{
			return 2;
		}

		JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
		{
			return JPH::BroadPhaseLayer(static_cast<JPH::uint8>(layer / LayerCount));
		}
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
		{
			return layer.GetValue() == 0 ? "Static" : "Moving";
		}
#endif
	};

	class BroadFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
	{
	public:
		bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer other) const override
		{
			return layer >= LayerCount || other.GetValue() != 0;
		}
	};

	class PairFilter final : public JPH::ObjectLayerPairFilter
	{
	public:
		explicit PairFilter(const SceneSettings& settings)
			: masks_(settings.collisionMasks)
		{
		}

		bool ShouldCollide(JPH::ObjectLayer first, JPH::ObjectLayer second) const override
		{
			return (first >= LayerCount || second >= LayerCount) && (masks_[first % LayerCount] & (1u << (second % LayerCount))) != 0;
		}

	private:
		std::array<std::uint8_t, LayerCount> masks_;
	};

	// callbackではIDと値だけをコピーする。SceneやBodyInterfaceを呼ばない。
	class ContactCollector final : public JPH::ContactListener
	{
	public:
		struct Entry
		{
			ContactType type;
			JPH::BodyID first;
			JPH::BodyID second;
			Vector3 normal;
		};

		std::vector<Entry> entries;
		bool failed = false;

		void Record(ContactType type, JPH::BodyID first, JPH::BodyID second, Vector3 normal) noexcept
		{
			try
			{
				entries.push_back({type, first, second, normal});
			}
			catch (...)
			{
				failed = true;
			}
		}

		void OnContactAdded(
			const JPH::Body& first, const JPH::Body& second, const JPH::ContactManifold& manifold, JPH::ContactSettings&) override
		{
			Record(ContactType::Added, first.GetID(), second.GetID(), FromPolar(manifold.mWorldSpaceNormal));
		}

		void OnContactPersisted(
			const JPH::Body& first, const JPH::Body& second, const JPH::ContactManifold& manifold, JPH::ContactSettings&) override
		{
			Record(ContactType::Persisted, first.GetID(), second.GetID(), FromPolar(manifold.mWorldSpaceNormal));
		}

		void OnContactRemoved(const JPH::SubShapeIDPair& pair) override
		{
			Record(ContactType::Removed, pair.GetBody1ID(), pair.GetBody2ID(), {});
		}
	};

	struct PhysicsWorld::Impl
	{
		static constexpr JPH::uint32 MaxBodies = 4096;

		// Joltの自動採番に依存せず、複製後も各slotの発行済み世代を引き継ぐ。
		struct BodySlot
		{
			JPH::uint8 sequence = 0;
			bool occupied = false;
		};

		// 形状と生成設定を保持し、同一IDで作業worldを再構成できるようにする。
		struct Record
		{
			JPH::Body* body = nullptr;
			JPH::BodyCreationSettings creation;
			MotionType motion = MotionType::Static;
			bool enabled = true;
		};

		Impl(std::shared_ptr<PhysicsRuntime::State> runtime, const SceneSettings& input);
		~Impl();
		Record& Find(BodyHandle body);
		const Record& Find(BodyHandle body) const;
		void RequireMutable() const;
		// 支持物の変更範囲にある休眠Bodyを、候補world内だけで起こす。
		void WakeOverlappingBodies(JPH::AABox bounds);

		// runtimeとfilterはsystemより長生きする。記録したBodyの実体はsystemが所有する。
		std::shared_ptr<PhysicsRuntime::State> runtime;
		SceneSettings settings;
		BroadLayers layers;
		BroadFilter broadFilter;
		PairFilter pairFilter;
		ContactCollector listener;
		JPH::PhysicsSystem system;
		std::map<BodyHandle, Record> bodies;
		std::map<JPH::uint32, BodyHandle> handles;
		std::array<BodySlot, MaxBodies> bodySlots{};
		// 削除・無効化したBodyの古い接触を次tickへ復元しない。
		std::set<JPH::uint32> retired;
		CheckpointStream checkpoint;
		std::uint64_t nextHandle = 1;
		bool sealed = false;
	};
}
