# Jolt Physics

採用版: v5.6.0 / commit `e77f175595e64cb44218cc9d9d56fc365ad0e36a`。
上流: https://github.com/jrouwe/JoltPhysics

`LICENSE`は固定版から変更せずコピーしたMITライセンス全文です。
Jolt本体はCMake FetchContentで取得し、Windows x64の静的ライブラリとして製品へリンクします。
Joltのソースの著作権表示を保持してください。バイナリ配布時もこのLICENSEを同梱してください。

KT_Forge側の接続コードと導入設定は、2026-10-12にCodexのAI支援で作成しました。
単一精度・SSE2基準・単一threadのJobSystemを使用します。GPU compute・付属sample・viewerは組み込みません。
