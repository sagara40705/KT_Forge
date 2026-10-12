# ozz-animation

0.16.0 / commit `6cbdc790123aa4731d82e255df187b3a8a808256`。
公式: https://github.com/guillaumeblanc/ozz-animation

MIT全文と著作権表示をLICENSEへ変更せずコピーした。
エンジンが利用する上流コードはSkeletonBuilder、AnimationBuilder、SamplingJob、BlendingJob、LocalToModelJob。
cmake/OzzAnimation.cmakeは上流を取得しruntime/offline/baseを静的リンクする。
付属tools/sample/FBX SDKは使わない。MSVC runtimeは製品と同じDLL版。

Content/Animation、Animation、World/Animation、RuntimeIntegration/AnimationとRendererのSkin接続はCodexによるAI支援実装。
上流のコードをエンジン作者独自の実装として扱わない。配布時はこのLICENSE全文と著作権表示を保持する。
