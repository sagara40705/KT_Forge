#pragma once
#include <Renderer/Graph/GraphStorage.h>
#include <Renderer/Graph/GraphCompiledPlan.h>
#include <span>

namespace KT::Renderer
{
	// 検証は入力を変更しない。null/同じ画像実体の二重import/別Graph/範囲外を拒否。
	// Viewの親画像とwrapper画像が一致し、format/サイズ/range/用途が適合することを検査。
	// 初版は2D/1Mip/1slice/1plane/sample1の全体RTVまたは書込DSVだけ。
	// monostate/SRV/UAV/readonly DSV/部分範囲/未対応stateを拒否する。
	// pass開始時の既存内容を先に検査し、WriteAllで範囲全体の出力を定義する。
	class GraphValidator
	{
	public:
		// 新規候補の画像検査。登録済み検査はstorage内descそのものを参照で渡す。
		// そのdesc自身だけを二重import比較から除外し、コピーなら新規候補として扱う。
		void ValidateImportedTexture(const GraphImportedTextureDesc& desc, const GraphStorage& storage) const;
		// Viewの検査
		void ValidateView(const GraphViewDesc& desc, const GraphStorage& storage) const;
		// Passの検査
		void ValidatePass(const GraphPassDesc& desc, const GraphStorage& storage) const;
		// 全体の検査
		void Validate(const GraphStorage& storage, std::span<const GraphResolvedPass> passes) const;
	};
}
