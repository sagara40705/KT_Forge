#include <World/Animation/AnimatorSerialization.h>
#include <World/Animation/Animator.h>

namespace KT::World
{
	void RegisterAnimatorComponent(ComponentRegistry& registry)
	{
		registry.Register<Animator>(
			"kt.Animator", 1,
			[](const Animator& input) -> SceneValue
			{
				input.Validate();
				return SceneValue::Object{{"clip", input.clip}, {"speed", input.speed}, {"loop", input.loop}, {"playing", input.playing},
					{"enabled", input.enabled}};
			},
			[](const SceneValue& data, const SceneReadContext&) -> Animator
			{
				data.RequireFields({"clip", "speed", "loop", "playing", "enabled"});
				Animator result;
				result.clip = data.At("clip").AsString();
				result.speed = data.At("speed").AsNumber();
				result.loop = data.At("loop").AsBool();
				result.playing = data.At("playing").AsBool();
				result.enabled = data.At("enabled").AsBool();
				result.Validate();
				return result;
			});
	}
}
