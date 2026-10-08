#pragma once
#include <Renderer/MaterialStore.h>

namespace KT::BuiltinAssets
{
	// 標準白色RGBA(1,1,1,1)のpresetを、呼出側指定IDで明示登録する。
	// 汎用material機構はRendererに残す。static初期登録/Graph接続なし。
	const KT::Renderer::UnlitMaterial& AddDefaultUnlitMaterial(KT::Renderer::MaterialStore& store, std::uint64_t id);
}
