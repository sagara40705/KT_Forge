# KT_Forge 設計資料

Windows x64 / DirectX 12 / C++20のゲーム基盤です。

| 資料 | 内容 |
| --- | --- |
| [project-goals](project-goals.md) | ゲームの目標と完成条件 |
| [architecture](architecture.md) | 責務・依存・所有、更新順、数学・Binding規約 |
| [render-graph](render-graph.md) | 画像/viewの借用、内容依存、記録と失敗時の契約 |
| [decisions](decisions.md) | 設計判断と採用理由 |

全体像はproject-goalsとarchitecture、描画の詳細はrender-graphを参照します。
採用契約・目標設計・未決定事項を区別します。設計資料は実行結果や動作保証ではありません。実装はSourceとCMakeを確認します。

外部依存の権利表記と説明資料は[ThirdParty](../ThirdParty/)に保持します。
コードにはAI支援による作成・修正と第三者ライブラリの利用が含まれます。

Git掲載対象は上記の設計資料とThirdParty内の資料です。
