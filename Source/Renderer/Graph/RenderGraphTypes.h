#pragma once
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace KT::Renderer
{ 
    // Graph内のリソースのHandle
    struct GraphResourceHandle
    {
        // どのGraphか。0は無効
        std::uint64_t graphid = 0;

        // Graph内のリソース番号。最大値は無効
        std::uint32_t index = (std::numeric_limits<std::uint32_t>::max)();

        // Graph側で別途、graphidの一致とindexの範囲を検査する
        bool IsValid() const
        {
            return graphid != 0 && index != (std::numeric_limits<std::uint32_t>::max)();
        }
    };

    // アクセスの種類
    enum class GraphResourceAccess
    {
        Read,       // 前の内容を読む。定義済みの入力が必要
        WriteAll,   // 前の内容に依存せず、処理後にリソース全体の内容を定義する
        ReadWrite,  // 定義済みの内容を保全しながら更新する
    };
    // 何の用途で使うか
	enum class GraphResourceUsage
	{
		Unspecified,    // CPU宣言では用途未指定を許可する
		RenderTarget,   // レンダーターゲットとして使う
	};

    // どのリソースをどう使うか
    struct GraphResourceUse
    {
        GraphResourceHandle resource;
        GraphResourceAccess access = GraphResourceAccess::Read;
		GraphResourceUsage usage = GraphResourceUsage::Unspecified;
    };

    // GraphのパスのDesc。
    struct GraphPassDesc
    {
        // ログに表示するパス名
        std::string name;

        // このパスが使うリソースの一覧
        std::vector<GraphResourceUse> resources;
    };
}