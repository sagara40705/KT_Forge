# KT_Forge の責務・依存・所有

更新: 2026-10-12 JST（Engine/Sandbox分離まで）。用語と設計境界の正本です。
以下では**採用契約**、**目標設計**、**未決定**を区別します。APIと配置は現物を読む入口であり、動作保証ではありません。目標のクラス名は生成指示ではありません。

## Engine / Sandbox の採用境界（2026-10-12）

Sourceの既存配置を維持し、Engine静的ライブラリとKT::Engine aliasを公開する。Sandboxは公開APIを使う最初の実行アプリ。
Sandbox/main・各Demo・StartupScene・SandboxUi・Assetsはアプリ側、制作CLIの引数解釈はTools側に置く。EngineはSandbox/Toolsの型・関数・固定素材名を参照しない。
同一Visual Studio solutionでEngine修正をビルドしSandboxを実行する。別フォルダのゲームもadd_subdirectoryとtarget_link_libraries(game PRIVATE KT::Engine)で利用する。
別ゲームから取り込む場合、KT_FORGE_BUILD_SANDBOXの既定はOFF。SDK/install/export、Hub、プラグインや共通Serviceを追加しない。
Sourceのinclude root、C++20、DirectX-Headersの版と探索順、必要なcompile定義と静的依存のリンク条件をtargetから伝える。

組込みMesh/Materialは従来のBuiltinAssetsとしてEngineへコンパイルする。実行時EngineAssetsとProjectAssetsは別rootでexe横に配置する。
SandboxはProjectAssets/UIからRML/RCSS/画像と、自分が選んだ固定版フォントを読む。相対パスの基準をCWDへ置かない。
素材copyは各buildで更新し、Shader生成はEngineの依存にする。実行時DLLも各アプリ横へ配置する。

新規ModelImport::CreateScene / ModelPlacement::AppendはAnimatorを自動追加しない。骨格のみならbind poseを表示する。
Clip/loop/playingはSandboxの明示Animatorで選び、保存済みAnimator設定を削除・上書きしない。

## 実行制御と終了寿命の採用契約（2026-10-12）

ApplicationがExecutionControlを直接所有する。Playing/Paused/StoppedはSceneの時間進行、RunStateはIdle/Running/Failed/Shutdownの稼働状態を表す。
初期はPlaying。Play/Pause/StopPlaybackは現在の完成Sceneを保持し、一コマ更新はPaused中のRequestSingleStepで一回を予約する。
消費前の重複要求は一回へまとめ、Play/StopPlaybackで取消す。失敗した一コマは自動で再試行しない。
停止は編集Sceneの復元ではない。停止してからPlayしても現在の状態を継続する。編集Sceneと実行Sceneの分離・巻戻しは将来のEditor設計。
固定delta1/60秒、最大4tick、同期1slotを維持する。停止時間と状態遷移した回の時間・時計端数をcatch-upへ持ち越さない。
停止時間は負荷によるDroppedSimulationSecondsへ加算しない。CompletedSimulationTicksはApplication全体で成功tickだけを累積する。
Runの順番はPollEvents→UI/UiUpdate→FrameUpdate→固定Scene/Effect tick→描画候補→Present/Wait。
FrameUpdateはUIなしでも操作できる同一threadの外側loop callback。停止中もUI/FrameUpdate/描画を継続し、固定Script/Physics/Animator/Effect時間を進めない。
callback中のPause/StopPlaybackは現在tickを完了し次tick前に反映する。実行制御・UI操作はScene transactionのrollback対象外。
UI()はRun前に空の窓口を準備する。Play/Pause/StopPlaybackは時間進行を変え、画面表示との同期はSandboxが行う。
Stoppedからの再開はPlayで明示する。SandboxはEscape/resumeをPlayへ解釈し、Stoppedからも再開できる。

初回表示は完成した起動Sceneの所有だけをActivateQueuedSceneで交換する。停止中の通常QueueSceneは次の再生tickまたは一コマtickまで保留する。
候補の準備・開始条件検査の失敗では旧Sceneを保持する。所有交換後の新Scene更新失敗では、新SceneをFailedとして保持し旧Sceneへ戻さない。
SceneHostは新しい所有を公開してから旧Sceneを破棄し、所有交換中のQueue/Activate/Update/Clear再入を拒否する。
Clearは更新・値編集・所有交換中に拒否し、active/pendingを両方切り離してから破棄する。
GetScene/GetRenderFrame/UI/Effectsの借用はそれぞれ次の交換・更新またはShutdown/Application破棄まで。終了後の窓口参照は利用しない。

RequestStopはRunから戻る要求で、完成Sceneを保持する。明示的な次Run開始時に時計を初期化する。
Runの例外は元例外をGetLastRunFailureへ保持し、GPU後始末の失敗で上書きせずRunState::Failedとして返す。
失敗原因を修正し、SceneがFailedならResetAfterFailureまたは新候補予約を行ってからRunを明示的に再開する。次Run開始時に前回例外を消去する。
ShutdownはRun外だけで実行し、繰返しは何もしない。GPU Frame/Queue完了→描画候補失効→pending/active Scene・Script→Renderer→GameUi→UiRenderer→SceneEffects→EffectPlayer→Swapchainの順。
Shader/Graph・World型の依存は増やさない。Device/Queue/PhysicsRuntime/素材/Input/Window/GLFWはApplication破棄まで保持する。
member宣言順もScene/ScriptがUI/Effectより先に破棄される順とし、部分起動失敗時の巻戻しも同じ借用条件を保つ。
Shutdown中は公開操作で新しい窓口を生成させない。終了後のRun/Play/Pause/素材窓口/Scene予約を拒否する。
GPU待機失敗では利用資源を解放せず例外を返す。destructorでも完了を確認できない場合は診断してterminateする。


## RmlUiのゲーム内UIの採用契約（2026-10-12）

ApplicationがUiRendererと空のGameUiをRun前のUI()で生成する。SandboxがHUD/ポーズ/日本語フォントを選んで明示Loadする。
RendererのFrameを先に破棄し、RmlUi Context、UiRenderer、Device/Queueの順に終了する。
RmlUiのglobal interfaceは一つのGameUiだけが同一threadで所有する。World/Core/Contentの値型へRmlUi型を持ち込まない。
RuntimeIntegration/UI/GameUiはdocument handleとSetText/SetImage/SetHealth/Show/Focus/TakeActionsを提供する。
UiDocumentは窓口識別＋単調番号。閉じたhandleは再利用せず、別窓口/Close後の操作を拒否する。
SetTextはUTF-8を検査しmarkupをescape。ボタンはdata-game-actionのclickを文字列として返し、操作名の意味と実行制御はアプリが決める。
SandboxのHUDとポーズはRML/RCSSを編集する。Sandboxは固定版Noto Sans CJK JP Regularを選び、EngineのFreeType接続で描画する。

UIは外側loopで一回採取したGLFW InputSnapshotを先に処理し、ゲームには消費していない値だけを渡す。
Run前のSetFrameKeysで予約したキーは停止中もFrameInputへ配送し、固定tickのGameInputへ渡さない。EscapeにEngine固有の意味はない。
mouse押下によるfocus変更後にキーを配送し、input/textareaの編集中は予約キーもUIへ渡す。UI文字入力のキーをアプリショートカットやゲームへ漏らさない。
0 simulation tickではedgeを保持し、最初の成功tick後にedgeを消費する。UI中の押しっぱなしは解除後も物理releaseまで遮断する。
UiUpdate callbackとUIのsteady clockはポーズ中も継続する。GameUpdate/Script/Physics/Animator/Effekseerの固定tickは停止し、最後の完成Sceneを表示する。
最初からポーズの場合はSceneHost::ActivateQueuedSceneで検証済み候補の所有だけを交換し、ゲーム/物理を進めない。
ポーズ中の通常Scene切替予約は次のsimulation開始まで保留する。長い停止時間を解除後のcatch-upへ持ち越さない。
UIの値変更・clickはScene transactionの復元対象ではない。UiUpdate/素材/描画の失敗はApplicationから例外を返す。

Renderer/UI/UiRendererは基本RenderInterface、immutable geometry、linear premultiplied画像、専用RootSignature/PSOを所有する。
Context::Renderでdraw列を捕捉する。生成/転送と前回Fence完了待機はGraph/Frame開始前に完了する。
Draw snapshotはshared_ptrで資源を保持し、上流Release後も最後の利用FenceまでBuffer/Texture/descriptorを失効させない。
UIのgeometryは不変UPLOAD/GENERIC_READ、Textureは転送済みPIXEL_SHADER_RESOURCEとしてGraph外で保持する。
UiPassはOpaque/Effect後の色ReadWriteだけを宣言し、深度なしで同じDirect Listへ合成する。barrier/Submit/Presentをadapterへ委譲しない。
半透明はONE/INV_SRC_ALPHA。RmlUiのpremultiplied sRGBをlinearへ変換し、製品SRGB RTVへ合成する。
Framebuffer寸法へlayoutし、Content座標を変換する。dp単位はFramebuffer/Content比へ追従し、GLFWの既存callbackを置き換えない。
初版は矩形clip、基本色/画像/フォント/2D transform。mask/filter/layer/gradient、文字入力/IME、gamepadのUI navigationは未接続。


## Effekseerの基本再生の採用契約（2026-10-12）

ApplicationがEffectPlayerとSceneEffectsを遅延生成して直接所有する。RendererのFrameを先に破棄し、Device/Queueは最後まで保持する。
RuntimeIntegration/Effects/SceneEffectsがゲーム向けLoad/Play/PlayAttached/Follow/SetTransform/Stop/StopAllと値handleを提供する。
一Player一窓口、同一thread。起動時に同期Loadし、素材はPlayer終了まで共有する。永続AssetCatalog/Scene保存への登録は初版に含めない。
ゲームcallback中の操作は予約とし、Scene成功後の完成CPU行列からlocal * targetWorldをコピーする。
Scene失敗では予約をswapで戻し、新handleの番号を再利用しない。対象削除/inactive/別Worldと旧Sceneの開始済み個体は停止する。
追従はEntityのTransform。骨socketへの直接追従は別の入力接続とする。
Renderer/Effects/EffectPlayerが上流Manager、DX12 Renderer、素材、pool/Listとnative handle対応を所有し、World/Entityを参照しない。
固定tickの秒×60で更新し、Presentでは時間を進めない。ゼロ/過小/過大scale等、上流のfloat行列分解で不正になる入力は変更前に拒否する。
上流再生の実行失敗は全Effect停止と例外で返し、成功済みScene更新を復元したことにしない。
Graph外で同じDirect QueueのFence完了を確認して上流poolを再利用。EffectPassはOpaque後の色ReadWriteとScene深度Readだけを宣言する。
Graphicsは既存D32画像のreadonly DSVを別descriptorで提供し、GraphがDEPTH_READへの遷移を担当する。
上流へ外部Listを渡して記録し、Submit/Present/barrierは委譲しない。半透明/加算、ZTest=true/ZWrite=false、LH/Reverse-Zと既存SRGBを維持する。
Sprite/Ribbon/Ring/Track/Modelを登録し、歪み/独自・照明Material/GPU Particle/音声は拒否、SoftParticleは無効化する。


## 基本トゥーン照明・主光源の採用契約（2026-10-12）

World/Lighting/DirectionalLightはWorld方向・線形光色/環境色・強度と、直交影の中心/半幅/半奥行/解像度/bias/strengthを持つ値Component。
kt.DirectionalLight version 1で設定だけを保存し、Scene更新transactionと完成CPU入力へ含める。
activeかつenabledな主光源は一つ、複数は抽出失敗、なしは環境色1/直接光0。EntityのTransformは方向/中心を変更しない。
RuntimeIntegrationが設定をRenderer/Lighting/SceneLightingの定数値へコピーし、Applicationが同じRenderFrameとともに渡す。
Content MaterialModel::ToonとToonSettingsはUV/Sampler/Opaque/Maskを既存UnlitSettingsと共有する。Toon JSON3、旧Unlit1/2は維持。
ToonはNdotLの二段階＋補間幅、暗部色、環境色、主光源、任意の影で線形色を求める。LDR/SRGB契約を継続。
静的48byte/Skin80byteと同じpaletteを可視/影で使用。Objectの逆転置と、Skin合成行列の逆転置で法線を変換する。
法線欠落/特異WorldのToonは当該候補を診断して除外。Skin合成が特異な頂点は元法線を使用。
Rendererがsampleable深度画像、MaterialStoreが共有Toon/Shadow PSOと色+影tableを所有する。互換のUnlitMaterial名を維持し、新登録窓口はAddSurface。
GraphicsはR32_TYPELESS画像・D32 DSV・R32_FLOAT SRV、PixelDepthView借用、ColorDepthTextureTableの同一heap配置と深度専用Drawを提供する。
同期1slot/Fence完了と旧List破棄後に影解像度を差し替え、候補生成失敗では旧画像/tableを保持する。
単一shadow map/9点PCF/全visible caster。範囲外は影なし、中心をtexel単位に丸める。cascade/透明影/複数光源は未実装。
この日付の追加契約を旧「Lit/Shadow/SRVは将来」の記述より優先する。

## 1 採用した方針と用語

### 骨格アニメーション追加の採用契約（2026-10-12）

Content/Animationは骨格TRS・Clip key・Skin bindingと頂点Weightの入力値を保持する。
Animation/SkeletonAnimationはozzの不変Skeleton/Animationを共有し、PoseSamplerは個体ごとの可変Contextを所有する。
World/Animation/Animatorは個体ごとの時計・速度・Clip・ブレンド進行だけを持ち、Content/GPU/ozz実体を持たない。
SceneのScript/ゲーム後にactive個体の時計を進め、既存更新transactionの成功公開とrollbackへ含める。
RuntimeIntegration/Animationは完成CPU入力と固定Catalogからpaletteと変形AABBを作り、Rendererへ値所有で渡す。
最も近い祖先Animatorを使い、配置rootで一個体の全Skin Meshを制御する。個体のclockとContextは共有しない。

paletteはrow-vectorの`inverseBind * jointModel * modelToMesh`。
ozzのcolumn-vector行列を転置してエンジン表現へコピーし、Meshのbind座標でGPU変形する。
静的頂点48byteを維持し、Skin頂点は骨番号16byte・Weight16byteを加えた80byte。
GPU定数は256行列のCBVを同Frame arenaで保持し、同じpaletteのSubmeshでは一つのsliceを使う。
Mesh JSON5にSkinを保存し、旧Mesh1..4を維持する。Sceneには再生設定だけを保存し、時刻はロード時にリセットする。

以下の旧節の将来Skinning/Animation拒否の記述より、この日付付き追加契約を優先する。

Windows x64 / DirectX 12 / C++20専用です。他OS、Vulkan、汎用RHI向けの抽象化は作りません。
WindowはGLFWを継続します。DX12接続でネイティブHWNDを使いますが、自前Win32 Windowへ移行する予定ではありません。
責務ごとの独立クラス・ファイルと、入口から直接追える更新順を優先します。

| 用語 | 意味 | KT_Forgeでの扱い |
| --- | --- | --- |
| Module | コードと依存の境界 | フォルダ/namespace単位で守る。全Moduleのlibrary化は未決定 |
| Subsystem | 実行時のまとまった責務 | Rendererなど。共通基底を要求しない |
| Service | 継続的な機能提供 | ContentServiceは目標案。Material/Shader毎に乱立させない |
| Store/Manager | 実体の所有管理 | 現行MeshStore/MaterialStore。名前だけで役割を増やさない |
| Cache | 再生成できる派生結果 | 原本や所有の正本と区別。汎用基盤は必要性を見て別設計 |
| ECS System | Componentからの計算・処理 | World内の一部処理。全機能をSystemにしない |

Rendererはまとまった一つの描画サブシステムです。内部のMesh/Material/Pipeline/Pass/Graphを独立クラスに分けることと、独立Serviceを多数作ることは別です。
共通Subsystem基底、Service Locator、virtual ISystem、汎用schedulerは必要ありません。

## 2 Moduleの責務と配置の入口

| Module | 責務 | 現状／目標 |
| --- | --- | --- |
| Core | 小さい共通utility、Log/Assert/Timer、値型Math | 実装あり。汎用Fileも現在Coreにある |
| Platform | GLFW Window/イベント、Windowsとの境界、OSファイル操作 | Window/GlfwContextとInputの入口あり。Content用OS I/Oの配置は目標で、既存Core/Fileの移動とは別 |
| Content | CPU MeshAsset/MaterialAsset、永続AssetId、素材JSON・Catalog・ProjectAssets・OBJ取込 | 起動時のプロジェクト索引と必要素材読込を実装。reload・ContentServiceは将来案 |
| World | entity/component所有、階層・active・Transform・Camera等 | ECSと4独立Systemあり。描画資源を所有しない |
| Physics | 剛体・衝突・問い合わせ・checkpoint | Joltを内部へ隠す。World/Entity/描画を参照しない |
| Graphics | DX12/DXGIの仕組み、Device/Queue、物理GPU資源/descriptor、記録・同期 | 実装あり。WorldやMaterialの意味を判断しない |
| Renderer | GPU Mesh/Material、shader選択/Binding、Pipeline、Pass、Graph | StaticSceneRendererがGPU素材共有と同期描画を所有。Worldは参照しない |
| RuntimeIntegration | Worldと独立した実行機能の接続 | SceneUpdater/RenderExtractionSystemの描画値コピーと、JoltScenePhysicsのEntity/Body対応を担当 |
| BuiltinAssets | 標準Cubeの生成・既定Material等のpreset | GPU Cube生成/白色登録あり。汎用クラス本体はRenderer |
| Application | 起動・終了、Windowと実行基盤、メインループの進行 | RunがWindow・Swapchainと記録/送信を接続する入口。|

Gameの具体的な操作やルールは必要になった時に置き場所を決めます。固定のModule数は完成条件ではありません。

## 3 依存の向き

**現状**: WorldはCore/標準C++とbackendを含まないPhysicsTypesのCPU領域。RuntimeIntegrationがWorldとRenderer/Physicsの接続を知り、RendererとPhysicsWorldはWorldを参照しません。
BuiltinAssets→Renderer/Graphicsで、Rendererから標準Cube/presetへの逆依存はありません。

**目標設計**:

```text
Application → EngineRuntime（普通のクラス、目標案）
EngineRuntime → SceneHost / ContentService / Graphics実行基盤 / Renderer
SceneHost → Scene → World（ECSのゲーム状態）
RuntimeIntegration → World + Rendererの描画用データ
Renderer → ContentのCPUデータ契約 + Graphics + Core
World → Core + 必要なら軽量AssetIdの定義だけ
Content → Core + PlatformのOS I/O
Graphics → Core + Windows/DX12、必要なPlatform境界
BuiltinAssets → ContentまたはRenderer/Graphics（生成対象に応じる）
```

Worldは軽量AssetId型を参照してよいですが、ContentService、Graphics、Rendererには依存しません。
RendererはWorld/entity/componentを再読取しません。RuntimeIntegrationがコピーしたRenderWorld/RenderViewを渡します。
ContentがRendererのGPUクラスを所有する依存へ反転させません。targetが一つでもこの論理境界を守ります。

## 4 所有の現状と目標

**CPU側の採用契約**: 一Sceneが一Worldを所有し、WorldがEntity/component/Script実体を所有します。SceneHostがunique_ptrでSceneを保持し、Loaderが別候補を検証してから更新開始境界で入れ替えます。SceneとWorldはcopy/move不可です。EditorのGameObjectはEntityの非所有表示で、Transform/親/UUIDの正本を別途持ちません。Scene Assetは保存定義で、実行時参照やScript実体を保持しません。

**配置の入口**: ApplicationはGlfwContext、Window、Input、GraphicsDevice、CommandQueue、Swapchain、固定AssetCatalog、SceneHost、SceneUpdater、StaticSceneRendererを保持します。
StaticSceneRendererはMeshStore/MaterialStore、深度画像、同期1slotのFrameResourcesを所有します。GraphとPreparedRenderFrameは一回の記録中だけ存在し、GPU借用先はRendererとApplicationが完了まで保持します。
独立consumerでは呼出側がMeshStore/MaterialStore、描画先、FrameResourcesを保持しています。

**目標設計**: ApplicationがWindowと実行基盤を所有しメインループを進めます。
EngineRuntimeはSceneHost/ContentService/Graphics実行基盤/Rendererをまとめる普通のクラスの目標案です。共通基底やLocatorは使いません。
RendererはGPU Mesh/Material/Pipelineと描画資源を所有し、GraphicsDevice/Queueを参照します。
EngineRuntime導入時のSwapchain/FrameResources保持場所・constructor引数は必要な接続を見て決めます。

| 対象 | 所有・借用の契約 |
| --- | --- |
| Swapchain | BackBufferとそのRTVを保持。Graphは借用する |
| FrameResources | CommandContextと一時定数arenaを所有。Queueを借用 |
| MeshStore | GPU Mesh実体をunique_ptrで所有。Deviceは登録検査用に借用 |
| MaterialStore | GPU Material実体、共有Unlit/Toon/Shadow PSO、Material別の色+影tableを所有 |
| RenderTarget/DepthBuffer | Texture画像と専用RTV/DSV heapを保持。sampleable DepthはCPU SRV heapも保持 |
| DescriptorHeap | descriptor領域を所有。Texture自体の所有とは別 |
| ColorTargetView | RenderTargetまたはSwapchain画像/RTVを非所有借用 |
| Graph Import / View | 画像descは外部画像だけ借用。別viewが親画像/rangeと排他的なRTV/DSV/Pixel深度SRV借用値を保持。Graph破棄で画像を解放しない |
| PreparedRenderFrame | Mesh/Materialと定数sliceを借用。World参照は持たない |

クラスがGraphicsにあることは、GraphicsDeviceが全インスタンスを所有することを意味しません。
CPU snapshotの寿命と、GPUが使うbuffer/画像/descriptor/定数領域の寿命も別です。

## 5 IDとパス

| 識別子 | 意味 | 契約・配置の入口 |
| --- | --- | --- |
| CPU AssetId | 素材の同一性。配置変更と独立させる | 128bit UUID。MeshAssetId/MaterialAssetIdを別型にし、未割当はoptional |
| 論理パス | engine://・project://等の配置/探索先 | Contentで解決する目標案 |
| Storeの実行時handle | Renderer内のGPU実体の参照 | 非zero uint64 ID。StaticSceneRendererがAssetIdとの対応を所有。世代付き型は将来案 |
| GraphResourceHandle / GraphViewHandle | Graph内のgraphid/index、画像とviewは別型 | 内部ID、形式検査IsValid、所属/範囲検査Contains。詳細はrender-graph |
| Entity | World ID/index/generationの非所有参照 | 実装あり。素材IDとは別 |
| DescriptorHandle/ConstantSlice | heap slot、arena owner/個体ID/epoch/範囲の借用token | 実装あり。Asset/Graph handleとは別 |

MeshRendererのmesh/materialはoptionalの永続素材参照、RenderObjectのmeshId/materialIdはGPU Store内のuint64です。CPU候補MeshRenderCandidateは永続IDとWorld行列・Submesh番号・SubmeshのWorld AABBを保持します。
AssetId→Store handleの対応はStaticSceneRendererが種類別に保持します。パスとGPUアドレスをAssetIdの代わりにせず、永続UUIDから数値handleを推測しません。

## 6 ContentとMaterialの初版契約

ApplicationがAssetCatalogを所有し、組込み素材とAssetSetup、指定時はProjectStartupのSceneが参照する素材を起動時に登録してFreezeします。CatalogはScene・候補より長く保持し、実行中の差替え・削除を禁止します。WorldはAssetIdの軽量定義だけに依存し、Contentの素材本体を保持しません。
AssetIdは素材作成時に発行してメタデータへ保存し、名前・配置・内容の変更で維持します。別素材としての複製は新しいID、組込みCube/白色Materialは固定UUIDです。JSONでは小文字の8-4-4-4-12表現を使用します。論理URIの探索、reload、GPU Cacheの退去は将来設計です。
MeshAssetは有限の頂点位置・uint32 Indexと、省略可能なUV0・単位法線・単位接線xyzwを所有する静的Triangle Listです。空配列、3の倍数でないIndex数、範囲外Index、32bit byte数超過を拒否し、参照頂点からLocal AABBを計算します。各属性は未指定か全頂点分で、接線には法線とUV0が必要です。接線XYZは法線と直交し、wは+1/-1でbitangent=cross(normal,tangent.xyz)*wです。SubmeshはfirstIndex/indexCount/materialSlotを持ち、Index列を順に隙間・重複なく覆います。IndexはVBへの絶対参照でBaseVertex=0です。Local/Submesh AABBは参照頂点から計算し、保存しません。MaterialAssetはUnlit/Toon、線形RGBA [0,1]、optional TextureAssetId、UV scale/offset、Point/Linear・Wrap/Clamp、Opaque/Mask・alpha閾値を保持します。Maskはalpha×色係数のalphaが閾値未満の画素を破棄し、残る画素はalpha=1で出力します。Blendは扱いません。Submeshごとに名前付きMaterial slotを解決し、同じslot名を持つ複数範囲も許します。
素材JSONはversion/assetId/name/type/dataを持ち、Sceneには参照だけを保存します。AssetJsonが解析・整形、AssetJsonFile.cppがサイズ制限付き読込と専用一時ファイルによる置換を担当します。初版のOS I/Oは既存SceneJsonと同じWin32方式で、汎用Platform I/Oへの集約は未実施です。Meshは旧version 1/2を読み、全Index/default slotの一Submeshへ変換します。法線・接線・複数/名前付きSubmesh・旧recipeを持つ場合はversion 3、OBJ最適化recipe付きはversion 4を保存します。単純な旧Meshはversion 1/2を維持します。Materialは従来のversion 1/2、Texture定義はversion 1を維持します。Skinning、LODは将来の拡張です。

BuiltinAssets::RegisterStaticMeshAssetsはGPUを作らずCube/白色MaterialのCPU素材を登録します。製品経路は他の素材と同じUUID解決でGPU化します。既存のCreateCubeMeshと白色GPU Material登録は独立consumer向けに保持します。

### 静的Mesh最適化（2026-10-12）

Content/MeshOptimizerが法線・接線生成とSubmesh結合後に、全属性bit完全一致の頂点統合→Submesh内のキャッシュ最適化→任意のOverdraw→全Meshの読み取り順最適化を行います。未参照頂点を除去し、位置/UV0/法線/接線を同じ対応表で並べ替えます。Submesh順・範囲・slot、三角形の向きと多重度を保持します。Noneは入力を保持します。RendererやWorldは外部最適化APIへ依存しません。

meshoptimizer v1.3のコミット9e1f07b159d3cb777f1c67ed31fc11fd117986f4を固定し静的リンク。新規取込はStandard、Overdrawは明示選択です。thresholdは1以上の有限float、既定1.05。CPUのfloat属性/uint32 Indexは継続し、GPU Mesh生成で実最大Indexが65535以下ならR16、65536以上ならR32を選びます。DEFAULT転送/Fence寿命は継続します。

最適化recordにはmode/threshold/pipelineVersion/library revisionを保存。旧OBJ recipe 1とModel recipe 1/2の通常再取込はNoneで再現し、暗黙に最適化しません。通常再取込は保存設定を継承し、未知の処理版を拒否。明示上書きで現行版へ移行します。保存済み幾何のロードでは実装版一致を要求しません。AssetId/ModelNodeId/slot、Scene個体反映と安全保存の既存契約を維持。JSON変換はContent/MeshOptimizationJson、公開候補と保存は既存Importer、GPU byte形式選択はRenderer/Meshの責務です。

量子化/LOD/圧縮/Meshletは別目標。

### Model資源・配置・再取込反映（2026-10-11）

制作ワークフロー拡張（2026-10-11 JST）: ModelWorkflowJsonがNode対応表の編集用出力/厳密読込と対象選択を担い、Application/ModelWorkflowCommandsへCLIを分離しました。ModelReconciler::PlanはModel ID/rootの両条件で個体を選び、非対象の保存定義と旧versionを保持します。Scene全体の参照・階層・実資源は引き続き検査し、共有Asset IDの内容変更まで非対象へ隔離しません。

復元はModelNodeResolution::restoreで抑止部分木を最新原本から生成します。保存されたObjectUuidを維持し、復元した管理値のoverrideを解除します。失われた追加Component/Scriptを復元せず、原本の欠落、抑止/切離し親、detach済み子孫、UUID衝突は拒否します。自動復活と履歴の自動削除は行いません。

Content/AssetTransferは新UUIDのMaterial複製と、旧projectから不足資源/依存画像の所有収集を担います。新原本の同一UUIDを優先し、ModelImportが一時bundleへ書き、画像展開/依存検証後に公開します。carry-sceneは現行MeshRendererとModelリンクから収集し、独自ComponentのAsset参照はretainedAssetsへ明示追加します。元Sceneの絶対パスをCLIで指定でき、旧Sceneのコピーは不要です。

ComponentDescriptor::enumerateReferencesとSceneReadContext::RequireReferenceがSceneオブジェクト参照の正本契約です。テンプレート登録はdecodeのRequireReference呼出を列挙し、直接登録はcallbackを明示します。未知契約は自動削除を保護。Scriptは型付きObjectUuidだけ、ModelInstanceは歴史情報として扱い、一般UUID文字列を推測しません。

ModelAssetは階層・Local Transform・Mesh参照・初期Material割当の不変共有定義です。素材JSONのkt.Model version 2で、親先行nodes配列、永続ModelNodeId、照合key、表示名、parent index/null、position/rotation/scale、mesh/material UUID/null、materialsのslot名割当を保存します。10万ノード・深さ128、複数rootを許可。Model AssetId、ModelNodeId、key、配置ObjectUuidは別の識別情報です。version 1はModel UUID/keyを名前空間に安定IDへ移行します。

ProjectAssets::LoadModelはModel→Mesh/Material→Textureを一意化し、存在・種類・slot・TextureのUV0を検査します。AssetCatalogはFreeze後だけ値を借用します。ModelRevisionはModelと依存定義・展開画素のSHA-256で、同じUUIDの内容変更も含めます。Assimp recipe 2はModel UUIDとkey→NodeIDを保存し、recipe 3はMesh最適化設定と処理版を追加します。旧recipe 1/2を互換読込します。通常key照合、rename/moveは旧key→新keyの明示対応です。未照合の削除と追加の同時発生は明示指定まで拒否し、曖昧な推測はしません。同名兄弟の順序変更など、元形式に安定IDがない変更の自動保証は行いません。

ModelPlacement::Appendは配置親と内部階層を一括追加し、新しいObjectUuidを発行します。配置rootの名前・親・TRSは常に個体所有です。配置rootに付くkt.ModelInstance version 1はModel ID、Revision、NodeID→ObjectUuid、前回原本の管理値、overrideと抑止/detach状態を保存します。現在のEntity/Componentがゲーム状態の唯一の正本です。CreateSceneは従来どおりリンクなしの取込用階層変換。旧配置は旧原本と全ノードの明示対応を渡してAttachします。

ModelReconcilerはRuntimeIntegrationの制作・保存用APIです。前回原本/個体現在値/新原本を正確に三者比較し、位置・Quaternion・scaleを個別の絶対値として、Materialをslot名とfallbackで保持します。不在と明示nullを区別。同値の明示override、解除/新原本復帰、削除の復活抑止、通常オブジェクトへのdetachを扱います。追加Component・子・外部UUID参照・編集済み削除・消えたoverride slot・欠落した保持資源は競合とします。親変更は明示したKeepLocal/KeepWorld/FollowSourceで解決し、KeepWorldの特異/shear/再構成不能は拒否します。

Planは元Sceneと候補を所有し、全体のRegistry値・資源・slot/UV・組込みCPU評価を検証します。Script/custom factoryやゲーム更新は呼びません。未解決競合のPreviewは未検証の途中候補で、Applyできません。ApplyはPlan後の編集を完全比較で拒否し、確保後のswapで一括反映します。SceneJson::Save(SceneAsset)は実体を生成せず安全保存します。起動時は展開済みComponentからWorldを生成し、Model原本を実行の依存にしません。

稼働中World、固定Catalog、GPUの差替えは別目標です。制作CLIは新Project snapshotへ全リンクを反映します。個体専用のMaterial設定は別UUIDの資源として保持し、新snapshotにも資源を配置します。外部ComponentのUUID参照は保存値のUUID文字列で保守的に検出し、独自の不透明符号化は推測しません。
### Texture付きUnlit（2026-10-11）

TextureAssetはsource（root相対UTF-8パス）、Srgb/Linear、Generate/Preserveと不変RGBA8画素を保持します。JSON解析だけでは未展開で、Catalog登録には展開済みの検証済みMipが必須です。画素はshared_ptr<const>で共有し、一括登録候補のコピーで複製しません。未割当Textureは単色、欠落/種類違いTextureとUV欠落は診断して当該候補だけ除外し、Scene参照を変更しません。

TextureLoaderはDirectXTex May 2026をCPU処理に使用し、PNG/JPEG/BMP/TGA/DDSを読めます。単一2D・非配列・非Cube・straight alphaに限定し、BC DDSもCPUでRGBA8へ展開します。HDR/深度/動画は対象外です。sourceはProjectAssetsのroot境界検査を通します。入力64MiB、各辺16384以下、入力展開と出力Mipは各256MiB以下を本体展開前に検査します。これはプロセス全体の使用量上限ではなく、変換中の一時画像や全素材の合計量は別です。

素材の色空間を正本とし、ファイルの色タグは上書き解釈します。Generateはbaseから1x1まで全Mipを再生成、Preserveは入力のMip列を保ちます。sRGBは線形空間でフィルタします。RGBだけをsRGB SRVで線形化し、色係数との積をsRGB RTVへ出力します。alphaは常に線形です。製品SwapchainのresourceはUNORM、RTVはSRGB。独立consumerは従来どおりUNORM出力を明示選択できます。

画像・SRV・Samplerは最終利用Fenceまで保持し、転送失敗後の中途画像は公開しません。初版はTextureごとに1個のshader-visible heapを所有し、描画ごとにheapを切り替えます。共有大規模heap、GPU圧縮形式、非同期転送、alpha coverage維持Mip、anisotropic、HDRは今後の拡張です。

### CPUプロジェクト読み込み（2026-10-11）

Content::ProjectAssets::Scan(root)は指定ディレクトリ内の*.asset.jsonを探索し、UUID・種類・名前・相対パスの索引を作ります。UUIDは素材JSON内の値を正本とし、ファイル名の変更では再発行しません。同じUUIDの複製や組込み素材との衝突は拒否します。索引は素材本体を保持しませんが、初版のScanは全素材を一度解析・検証します。選択読込は必要素材を再読込してCatalogへ保持する方式で、初回I/Oまで必要素材だけに限定するものではありません。

rootは素材とScene用のディレクトリを指定します。ファイル数/ディレクトリ数は各10万以下、対象素材の合計は512MiB以下、各JSONは既存の64MiB制限です。参照はroot相対パスで解決し、絶対パス・親への脱出・代替データstream・リンクによるroot外参照を拒否します。ディレクトリの重複到達/循環も拒否します。engine://やproject://のURI解決ではありません。ScanからLoadまで外部のファイル編集を行わず、変更後は再Scanします。同時編集の検出/監視やセキュリティsandboxを保証するAPIではありません。

RuntimeIntegration::ProjectSceneLoaderはSceneJsonの検証後、全kt.MeshRendererのmesh/materialとmaterialsの全slotを収集します。非表示・非Activeも対象です。共有UUIDは一度だけ読み込み、未参照素材はCatalogへ保持しません。MaterialのbaseColorTexture依存も収集し、Project素材とAssetSetup登録済みMaterialの両方を対象にします。Textureに素材間依存はありません。独自Component/Script内の素材参照は追加対象です。

未割当は通常状態、欠落と種類違いはSceneを保持してObjectUuid・Component・field・AssetId・期待/実種類を診断します。破損素材、重複UUID、Scene検証/実体生成失敗は例外です。全素材読込とScene生成後にRegisterBatchをcommitし、途中失敗では呼出側Catalogを変更しません。一括登録は起動時だけ既存Catalogを複製する方式です。CatalogはGPU資源や可変参照を公開せず、Freeze後は登録を拒否します。

Application::ProjectStartupはroot、Scene相対パス、任意のComponent/Script登録callbackを受けます。組込み登録→callback→Registry固定→プロジェクト読込→候補Scene予約→Catalog固定の順です。既存AssetSetupによる登録も継続可能です。初版は起動Scene用の一回の準備で、実行中の新規素材追加、Scene横断の自動load/unload、Hot Reloadは行いません。QueueSceneする別Sceneの素材は事前に用意する必要があります。

ObjMeshImporterは三角形OBJのv/vt/vn/f・s・usemtlを読み、法線・UV0・接線と名前付きSubmeshへ変換します。Preserveは明示法線を保持し欠落cornerを生成、Flatは面ごと、Smoothは位置Index共有で生成します。s未指定/offはFlat、UVの継ぎ目をまたぐ法線を先に生成してから属性の違いで頂点を分割します。MeshGeometryは角度重み付き法線と鏡像UVの符号別分割を行います。独自接線生成でMikkTSpace互換は保証しません。退化面/UV・相殺する属性は生成時に拒否し、接線不要のUnlitは生成を無効にできます。UVの混在、多角形、前方参照、未知命令は拒否。MTL内容とo/g階層は通知して省略します。既定は座標/面順を保持し、scale/flipZを明示選択、UVは既定で1-v、--keep-vで保持します。recipeは素材位置からの相対sourceと取込設定を保存し、--reimport-meshはUUID/名前を維持して原本を成功時だけ置換します。自動監視/runtime reloadは含みません。

製品実行ファイルのCPU専用入口は `--import-obj input.obj output.asset.json [--flip-z] [--keep-v] [--scale 数値] [--normals preserve/flat/smooth] [--no-tangents]`、`--reimport-mesh output.asset.json` と `--validate-project root scene相対パス` です。Window/Graphicsを生成せず終了します。検証の終了codeは0=成功、2=参照/候補の診断あり、1=失敗です。`--project root scene相対パス [--camera UUID]` は指定Sceneを継続描画します。無引数はStartupSceneのCubeとCameraを同じ経路へ渡します。
ModelImporterはAssimp 6.0.5の固定commitからMesh・Unlit Material・画像byte・親先行ノードをCPU値へコピーします。ContentはWorld/Rendererを生成せず、Assimp所有ポインタを外へ渡しません。RuntimeIntegrationのModelImportがScene定義と専用bundleを生成し、一時directoryへ保存→製品Loaderで画像/参照/CPU候補を検査→rename公開します。既存directoryを上書きせず、再取込は新規先へ同じkeyのUUIDと設定を引き継ぎます。名前/同名兄弟順/共有Meshの構成変更でkeyが変わる場合があり、手編集mergeや意味的同一性の推定は行いません。

Assimpの統一RHから既定でZ/面順反転、UVは左上原点へ変換します。Scaleは頂点とノード移動へ適用し、各ノードの行列をrow-vectorのTRSへ転置・再構成照合します。Shear/特異変換、Animation/Skinning/Morph、点/線、Blendや未対応Texture設定を拒否します。追加UV/頂点色・Camera/Light・PBR/NormalMap/両面は診断して省略します。UV有無の混在はidentity子ノードへ分け、未指定属性を捏造しません。Material slotは名前と同名出現番号で対応し、外部/埋込画像はbundleへ所有コピーします。接線は既存MeshGeometryで再生成し、元の接線やMikkTSpaceとの一致は保証しません。

bundleにはscene.json、preview.scene.json、UUID名のasset.json、images、model.import.jsonを保存します。原本SceneにはCameraを加えず、確認SceneだけにWorld AABBからCameraを作ります。製品CLIの--import-model/--reimport-modelで生成し、--project/--cameraで描画します。単体64MiB/重複除去後512MiB、件数/ノード深さ/JSON上限で入力を検査します。元directoryの外部依存を拒否しますが、Assimp解析前の全メモリ消費や同時編集のsandbox保証ではありません。

TextureLoaderは読込ごとにCOM scopeとWIC Factoryを所有し、FactoryをDirectXTexへ明示設定・解放します。global Factory操作をLoad全体で直列化し、COM終了後のFactory再利用を防ぎます。EngineのWIC利用はここへ限定します。外部のDirectXTex WICを直接併用する場合は、Factoryの所有と同期を別途揃える必要があります。

MeshRendererの保存versionは3で、mesh/materialのUUIDまたはnull、visible、名前付きmaterials objectを保存します。materialは共通fallback、materialsはslot名で上書きします。未登録slotはfallback、明示nullはfallbackを抑止します。旧version 2は登録した副作用のない変換callbackで読み、version 1の数値参照は従来の明示対応表で2へ移行します。欠落/種類違いは当該Submeshだけを除外し、診断に番号とslotを保持します。再取込で消えたslotへの割当も保持して診断します。map編集のrollbackはメンバ別noexcept swapで復元します。

RenderExtractionSystemは完成CPU Snapshotと固定Catalogを読み、継承Activeと自身のvisibleで選別します。visibleは子へ伝播しません。8頂点のWorld変換からAABBを計算し、有限AABBで表現できない対象も診断して除外します。候補順はEntity Slot順と各MeshのSubmesh順、元Entityの対応表と診断はRuntimeIntegrationだけが保持し、RendererへWorld/Entityを渡しません。候補は値所有で、Cameraなしでも生成します。

## 7 更新順と将来の接続点

**目標設計**: 入力/時間→World更新→Render抽出→描画準備→Graph構築/Compile/Record→送信/Present→Fenceで再利用判定。
EngineRuntime/RuntimeIntegrationの入口へ直接呼出を並べ、人が順番を追える形にします。
全段階をECS Systemへ変換しません。Graphics/Content/Rendererはそれぞれ普通の責務です。

**現状のCPU順（2026-10-12）**: Scene::Updateは開始予約の反映→Hierarchy/Activation/Transform→物理作業候補の準備/同期→active/enabled Scriptとゲーム処理→物理命令/固定Step→journal付きLocalTransform/RigidBody書戻し→ゲーム予約の反映→Hierarchy/Activation/Transform→物理終了同期/checkpoint→World/物理/CPU結果の公開を直接並べます。物理未接続の制作Sceneでは物理段階を使いません。
ゲーム処理中の派生値と更新対象は開始時に固定します。値編集は順次反映し、構造変更は予約します。後段で生成されたScriptは次Updateから動きます。
SceneUpdaterは完成SceneUpdateContextと固定CatalogからCPU候補を生成し、指定時だけ同じ入力からCameraを計算します。候補生成失敗時はFrameを公開せず、Camera計算失敗時は今回の候補・診断を保持してviewだけを空にします。Applicationは各CPU更新開始前に旧候補を失効させ、CPU成功後にCamera選択→候補/Camera計算→GPU準備/記録/送信→Present一回→Waitを行います。Camera選択失敗は候補生成前なので公開Frameなしです。確定済みSceneのrollbackと描画側の失敗は別です。

### 静的Sceneの製品描画

RuntimeIntegration::SelectSceneCameraは完成Snapshotの継承Activeで有効なCameraを選びます。未指定なら0台=Clearのみ、1台=自動選択、複数=例外です。SetRenderCamera/ProjectStartup.camera/CLI --cameraは永続ObjectUuidを使い、指定の欠落・無効・Cameraなしを拒否します。指定はScene切替後も維持するため、異なるSceneの自動選択へ戻す場合はnulloptを設定します。Cameraと祖先のscaleは全軸1の既存契約を継続します。

StaticSceneRendererは固定AssetCatalogから必要なMesh/Materialを遅延生成し、UUIDごとに一つのGPU実体を保持します。Scene切替・Resizeでも共有実体を再利用し、Renderer破棄まで退去しません。位置/UV0/法線/接線の48byte頂点と実最大IndexによるR16/R32 IndexのDEFAULT Meshと単色/TextureのUnlit Pipelineを使い、CPUのWorld行列を転置せず送ります。Graphは色Clear→DepthClear→Opaque、Reverse-Z clear0/GREATER、BackBufferはPRESENTへ戻します。深度は毎回COMMONから開始してCOMMONへ戻します。Cameraなしでは色Clearだけで、候補のGPU化は行いません。

FrameResourcesは容量不足時だけ再生成し、View一個＋各Draw二個の256byte整列定数領域を事前確保します。失敗した記録は送信せずFrameを破棄し、先に生成成功したGPU素材は再試行で利用します。GPU描画側の失敗では完成CPU状態を戻さずRunを終了します。記録前に前回Fenceを待ち、Submit後にPresent一回、Waitの順とします。

Windowが最小化中かFramebuffer寸法が0ならCPU更新と候補生成だけを続けます。復帰後はFramebufferの寸法をCamera・Swapchain・深度へ反映します。ResizeはRenderer::ReleaseFrameで旧Listを解放し、Swapchain内でPresentを含むQueue末尾を待ってからBackBuffer全参照を解放、ResizeBuffers、GetBuffer、RTV再作成の順です。Resize失敗後は寸法0で公開を止め、再試行成功までPresentしません。Application破棄でもQueue末尾まで待機します。
| 将来必要になる領域 | 示しておく接続点 | 今は作らないもの |
| --- | --- | --- |
| Physicsの拡張 | 既存のScenePhysicsと独立PhysicsWorld | Character、関節、Mesh形状、描画補間、並列job、性能最適化 |
| Audio | Worldから必要な位置/イベントの値を渡す | Audio backend/Serviceの実装 |
| Scripting | 明示した更新段階でWorldへの操作を許可する境界 | runtime/scheduler/全Componentの公開 |

### Jolt Physicsの所有と更新（2026-10-12）

Jolt 5.6.0を固定commitで静的リンクします。ApplicationのPhysicsRuntimeがプロセス共通登録と単一threadのjob/temp領域を保持し、Sceneより長生きします。Worldはbackendを含まないPhysicsTypesの値を使い、JPH型はPhysics内部へ限定します。Sceneが物理専用ScenePhysicsを所有し、RuntimeIntegration/Physicsが完全Entity handleとBodyの対応を管理します。Rendererは従来どおり完成CPU結果の値だけを受けます。

物理EntityはRigidBody・Collider・LocalTransformを持つrootで、scaleは全軸1です。拡縮する描画物は子Entityへ置きます。Static/Dynamic/KinematicとBox/Sphere/Capsuleに対応し、設定はcomponent置換、姿勢は物理命令で変更します。DynamicはJolt、Kinematicは1tickの目標、Staticは明示Teleportが姿勢を決めます。

初速度の長さは線速度500 m/s以下・角速度15π rad/s以下に制限し、Jolt生成前に検査します。構造同期は構成検証→旧Body削除→新Body追加の順で、4096体での同数入替えを許可します。sensor以外の支持物変更は移動元・移動先の近傍Bodyを候補内で休眠解除します。

確定Jolt worldを保持したまま、同一BodyID・生成設定・共有形状・checkpointから作業候補を構築します。SaveState/RestoreStateをrollback中に呼びません。候補のStep、World journal書戻し、終了構造同期、checkpoint保存まで成功してからswapで公開します。候補の生成・計算・checkpointの失敗では確定物理と前回CPU結果を保持し、SceneのFailed/ResetAfterFailure契約へ戻します。初版は毎tickの候補再構築を行い、二つのworldと状態転送の費用を受け入れます。上流assert/abortからの回復は保証しません。

BodyIDの世代はエンジン側で管理し、空slotを含む履歴を候補へ複製します。同じ候補内で廃止したIDは再利用せず、新Bodyへ旧contact cacheを適用しません。SceneHostは切替候補の更新開始条件を所有権交換前に検査し、不正delta等の拒否時には旧Sceneと候補を保持します。

Applicationは固定1/60秒、1描画最大4tickです。過大elapsedと超過tickを捨て、捨てた時間を統計で公開します。入力は各tickで確定し、0tickの間はedgeを保留します。描画は最後の完成状態を一回提示し、補間しません。接触通知は次のゲーム更新から参照し、削除・置換対象の古い通知を除外します。Removedはsleep時にも起きるJoltの通知です。

JSON version 2はScene物理設定を保存し、version 1を互換読込します。Loader・素材制作にはJolt初期化を要求せず、ApplicationのQueueScene前に物理まで候補準備します。

## 8 GPU寿命と同期1slot

現行FrameResources::Beginは前回提出Fenceを待ってからallocator/list/arenaを再利用します。
EndRecordingで定数Writeを禁止し、SubmitでExecute→Signal、Waitで完了を確認します。
Contextは同じFrameのarenaだけをDrawで受理します。別Frame/別Queueの有効sliceでも拒否します。ConstantBufferArenaは再利用しない非0個体IDをsliceにも保持し、同じアドレスへの再構築を区別します。Beginはepoch更新だけ、再構築は別ID、個体ID/epochの枯渇はwrapせず拒否します。
Frame-owned Contextを直接reset/送信してこの契約を迂回することも拒否します。

Mesh/Material/画像/heapは外部所有者がGPU完了まで保持します。Frame destructorの待機は外部資源の早期破棄を安全にしません。
記録失敗はContextを無効化し送信を拒否します。送信後のSignal/Wait失敗は現行実装でfatalです。
最後の利用Fence完了がGPU資源・Descriptor再利用の条件です。単なる経過frame数やGraph破棄を根拠にしません。
遅延解放、複数frame同時実行、Descriptorのfree/reset、汎用poolは将来実装です。

## 9 GPU部品の現在の制限

製品の静的Mesh VB/IBは不変DEFAULTです。COMMONで生成し、Mesh::UploadがCOPY_DESTへの遷移・stagingコピー・VB/IBの読取状態への遷移・完了待機を明示実行します。stagingは完了Fence後に解放し、未転送のPrepare/Drawを拒否します。初版はVB/IBそれぞれを同期転送します。旧raw constructorは独立consumer互換のUPLOAD/GENERIC_READを維持します。SubmeshのIndex範囲はPreparedFrame/OpaquePass/PacketからDrawIndexedInstancedまで保持・検査します。
描画先TextureはDEFAULT/COMMONで生成し、constructorがbarrier/submit/waitを隠しません。SampledTextureはDEFAULT/COPY_DESTと専用SRV heapを生成し、明示Uploadで全Mipを転送しPIXEL_SHADER_RESOURCEへ遷移、Fence完了後にstagingを解放します。以後は不変読取専用です。
色はRGBA/BGRA8 UNORM/SRGB、depth viewはD32_FLOAT/sample1（sampleable画像はR32_TYPELESS/SRV R32_FLOAT）、Reverse-Z clear0・GREATER・write ALLです。
Unlitはblend/culling無効、照明/受け影なし。casterとして主光源の影passへ提出することは可能。性能最適化済みとは扱いません。
DescriptorAllocatorはheapごとの単調割当、1allocator、単一thread、free/resetなしです。

## 10 BindingとCPU/HLSLの規約

現行unlitのb0=View、b1=Object、b2=Materialです。意味はRendererが定め、GraphicsへRootConstantBindingのslot/tokenを渡します。
View/Objectは64byte行列、Materialは48byte（RGBA、UV scale/offset、alphaCutoff、padding）。各Writeは別の256byte整列領域を確保し、上書きAPIはありません。
CPU/HLSLはrow-major格納、row-vector演算、HLSLのrow_major float4x4とmul(vector,matrix)で転置なしです。
順序はworld→viewProjection。定数の意味、配置、Root Signatureとshader宣言を一組で変更します。
Texture版はroot 3=t0単一SRV table、root 4=s0単一Sampler table（Pixelのみ）を使います。単色版は3CBVを維持します。IndexedDrawPacketはSampledTexture/Sampler所有者から借用し、種類・範囲・Device・Upload完了を命令追加前に検査します。旧DrawIndexedもpacket検証へ委譲します。Samplerは8組合せをMaterialStoreで遅延共有し、TextureはUUIDごとにStaticSceneRendererで共有します。

## 11 CMakeの現在の境界

製品はEngine library＋Sandbox executable targetです。rootはproject/deps/common、SourceはEngineの明示target_sources、Sandboxはアプリmainと各デモ、Toolsは制作CLIを登録します。
Engine内部の各Moduleを個別libraryに分けるかは未決定です。GLOB、検証用targetと製品targetを分離します。
ShaderBuild.cmakeのkt_forge_shaderがDXCを使い、製品ごとに一つのEngine_Shadersへ生成依存を登録します。
同じ製品の登録はRendererの同一CMake directoryへ集約。出力はbinary tree/構成別、生成headerの表示はGenerated Shadersの構成別です。
これは実装済みD20であり、コンパイル単位毎の個別補助targetを現行案として残しません。

## 12 現物から読む入口

Applicationの現状はSource/Application/Application.cpp、CPU全体の順番はSource/World/Scene/Scene.cpp、描画値の抽出入口はSource/RuntimeIntegration/SceneUpdater.cppです。

以下のAPI一覧は現行コードを読むための案内です。完成実装の先回りや新しい生成指示ではありません。

## 13 World・SceneとCPU更新

WorldはECSを唯一の実行ゲーム状態管理として使用します。EntityはWorld ID/index/generationで失効を区別し、オブジェクト保存には別の16byte ObjectUuidを使います。Scene生成ではPersistentIdとNameを構築してからEntityを公開します。UUIDは同World内で重複を拒否し、Nameは値編集だけを許可します。
componentは個別所有し、他の追加・削除で移動しません。参照は対象component/Entity/Worldの破棄まで有効です。単一threadです。

| 配置・クラス | 責務 |
| --- | --- |
| World直下のWorld/Entity | Entity/component所有、free list、SetParent、部分木削除 |
| World/Scene/Scene | World・予約・CPU結果の所有、一回更新全体の復元、失敗後の停止 |
| World/Scene/RollbackState | 実行時値の復元契約。JSON保存定義とは分離し、Restoreは確保なし/noexcept |
| World/Scene/WorldCommandBuffer | Create/Destroy/Add/Remove/SetParentの所有値予約と記録順反映 |
| World/Scene/SceneHost | 完成候補の予約と、更新開始境界でのScene切替 |
| World/Scene/SceneAsset・SceneLoader | UUIDと汎用Component定義の読込・取り出し。固定Registryで検査・復元 |
| World/Scene/ComponentRegistry・ScriptRegistry | 型ID/versionとC++処理を登録し、読込前に固定 |
| World/Scene/SceneJson | 標準JSONとの変換と失敗保持付きファイル保存 |
| World/Scene/ScriptComponent | 複数Scriptの設定とunique_ptr実体を所有。Missing Script設定も保持 |
| World/Scene/SceneUpdateContext | 入力コピー、階層・active・行列、World IDと更新番号を保持 |
| World/Systems | Hierarchy→Activation→Transform。Cameraは完成CPU入力への任意計算 |
| RuntimeIntegration | 同じCPU結果からCamera/描画用値をコピー。Worldを再読取しない |

**削除と編集**: 公開DestroyEntityは反映時点の全子孫を子から順に削除します。対象と作業領域を先に確保し、commit中は確保しません。単体slot破棄はprivateです。DestroySubtreeもこの公開契約を利用します。空きslot取得はfree listで定数時間、世代上限のslotは退役します。UUID重複検査や階層計算全体の定数時間を保証するものではありません。
親正本はHierarchy.parentで、SetParentはKeepLocal/KeepWorldを指定し、検証済み親索引で新親の祖先を辿って循環を拒否してから変更します。KeepWorldは反映時点のWorld行列から新Localを求め、shear・特異な親・TRSで保持できない値を拒否します。Hierarchy/PersistentIdを汎用Add/Removeで編集できず、借用・列挙でもconstとして渡します。Worldの列挙中は構造変更不可です。Sceneはconst Worldだけ公開し、値編集callbackと構造変更予約を提供します。借用値はcallback外に保持しません。

**生存Entity・型別所属索引（2026-10-11）**: World内部に生存handleのdense配列とslot→dense位置のsparse配列を持ち、型ごとにも同じ所属索引を持ちます。索引は完全なEntity（World ID/index/generation）だけを保持し、Componentの所有表・個別実体を移動しません。所属削除は末尾swapと逆引き修正でO(1)、所属照合もO(1)です。型索引の取得にはtype_indexのmap検索O(log 型数)が残ります。

Entitiesは生存索引から値をコピーし、GetHierarchyの入力収集と階層batchのリンク設定・根収集も生存集合だけを使います。ForEach<Ts...>は指定型の中で最少件数の型を候補にし、他の型のsparse所属を照合します。候補の型索引pointerは呼出時に一度取得し、交差の照合は候補数×型数です。合致したComponent実体の取得には従来の所有map検索が残ります。HasComponentは型索引で完全handleを照合し、Find/Getは従来の所有表を使います。未登録/空の型は列挙0件、重複型指定は同じ参照、const・入れ子列挙・列挙中の構造変更禁止を保ちます。Scene::ForEachActiveとScript収集も内部の候補列挙を共用します。

**Sceneの型別候補接続（2026-10-11）**: ForEachActiveは最少件数の指定型だけを候補にし、完全handleを固定ゲーム入力の階層索引で密なSnapshot位置へ引いてactiveを確認します。値は現在のComponent実体を使い、編集前のrollback捕捉とCPU変更通知を保ちます。更新途中のActiveSelf変更・構造変更予約は今回の有効対象へ反映しません。候補のslot順・型交差・重複参照・未登録/空型・入れ子の契約を維持し、候補配列を別コピーしません。

Script収集はScriptComponent型集合だけをslot順に走査し、disabled/inactiveも含む全実体の状態を先に捕捉します。その後、固定active・enabledを満たす対象をEntityのslot順・entryの登録順に実行します。Missing Scriptの設定を保ち、終了境界で追加したScriptは次回から更新します。公開World::ForEachは従来の列挙guardで構造変更を拒否し、Sceneは構造反映を更新境界へ限定して候補の寿命を保証します。内部候補列挙自体はguardを作らず、Sceneのrollback捕捉用MutationGuardを許可します。単一thread・callback外への参照保持禁止は継続します。

各索引には公開用のslot順配列を分けて保持します。生成/削除・型追加削除・rollbackの所属変更でDirtyにし、次の利用時にdenseをコピーしてslot順へ整列します。以後は同じ順を再利用し、内部のswap順を公開しません。生存集合の最初の整列はO(N log N)、型候補Mの整列はO(M log M)です。変更前のPrepareでdense・公開順・sparseを全て確保し、追加公開後の削除・復元・整列には新規確保を要求しません。Entitiesの戻り値コピーは別に確保します。

必須Name/PersistentIdを伴う生成、汎用Add/Remove、KeepLocal/KeepWorldの不足型追加、部分木削除へ接続し、既存journalの逆操作で元の所属と完全handleを戻します。Add/Remove記録には対象handleも保持し、同じslotの別世代と区別します。失敗途中の容量拡張・空の型索引は保持しますが、所属だけを戻し、公開順は次の利用で作り直します。型索引を削除・縮小しないため、rollbackは過去の確保容量内で完了します。sparseは各型が使った最大slotまでの領域、高水位のdense/公開順容量はWorld破棄まで残ります。型数・slot範囲が大きく所属が少ない場合のpaged sparse化、複合クエリ結果cache、格納変更、SIMD、並列化は今回実装しません。階層/CPUのslot索引初期化と全Entity入力捕捉・コピーは別に残ります。

**階層の派生索引（2026-10-11）**: World::GetHierarchyはslotから密なノード番号への索引、親子のノード番号、親から処理する幅優先順序と、深さ優先順の部分木半開区間・幅優先順位を遅延構築し、shared_ptr<const HierarchySnapshot>で共有します。根と兄弟はslot順です。Entity生成・削除・実際の親変更でキャッシュを失効させ、階層以外のComponent追加/削除や値編集では再利用します。ノード検索はWorld IDと世代も照合し、GetParent/GetChildren/部分木削除/Camera選択/KeepWorldの対象検索で共用します。ValidateHierarchyの既存の値返却APIは共有値のコピーを返します。

SceneUpdateContextは値入力と同時に共有階層を捕捉し、World参照を保持しません。構造が同じならCPU更新前後・次回更新で同じ階層を使います。入力配列は毎回所有値として捕捉しますが、通知世代一致かつ可変借用なしのLocalTransform/ActiveSelfは、Componentの再検索・読取を省いて前回入力をコピーします。Camera/Meshは毎回読みます。派生値は下記のDirty管理で差分計算します。古い共有値はWorldの変更・破棄後も捕捉時点のまま保持され、現在のWorldの生存保証とは区別します。transactionは更新前のキャッシュを保持し、rollbackで元のEntity世代と親を戻した後、再構築・確保なしでキャッシュも戻します。未構築だった場合は未構築へ戻します。単一thread限定です。

入力収集は生存Entity索引を使い、空きslotを走査しません。ただしslot→ノード索引の初期化と生存ノード/辺の処理は残り、slot範囲に比例する索引メモリーを使います。温まった親検索は定数時間、子列挙・部分木列挙は対象数に比例します。直接Worldを編集する場合は構造変更後の次の取得で再構築します。予約batchでは下記の作業索引を使い、完成snapshotの再構築を境界後のCPU入力捕捉へ集約します。

**DirtyによるCPU差分計算（2026-10-11）**: SceneUpdateContextは任意の比較元として同じWorldの完成CPU contextを受け付け、構築中に入力・変更世代と結果を捕捉・コピーします。比較元を後から借用しません。slot索引と完全なEntity（World ID/世代）を照合し、新規・再利用slotは両方Dirty、既存EntityはActiveSelfとLocalTransformの変更を別々に判定します。既存Entityの値比較は下記の通知・世代管理で候補を絞ります。親が変わったEntityは両方Dirtyです。Localのfloatは各成分のbit値で照合し、負の0を区別し、paddingを比較しません。Component欠落の既定値も照合対象で、同じ値の追加/削除は再計算不要です。

入力照合は変更したEntityを列挙し、部分木の開始順へ整列します。祖先と子の重複区間を除き、対象ノードだけを従来の幅優先順位へ整列します。ActivationSystem/TransformSystemはこの作業集合だけを処理し、無変更時には親順の全件走査も行いません。System内で配列をコピーせず、未公開contextへ直接計算します。途中失敗はFailedとして参照・続行を拒否します。行列が不要なActiveSelf単独変更でもFinalizedEntityのactiveは更新します。未変更結果は検証済みの完成値を使い、変更Localと親合成は従来のMath APIで非finite/zero Quaternion/overflowを拒否します。比較元なしのcontextは全件計算します。GetCpuUpdateStatisticsはそのcontextのActiveSelf/LocalTransform読取候補・値比較・再計算件数を別々に返します。Component欠落の既定値読取も数え、世代照合・配列コピー・時間とは区別します。

Sceneの開始CPUは前回完成結果、終了CPUはゲーム開始時の固定入力を比較元に使います。idle編集は完成結果の公開だけ失効させ、比較元を内部に保持します。失敗時は元のcontextと公開可否を確保なしで戻し、仮のDirtyと結果は廃棄します。更新開始前から公開が失効していた結果はResetAfterFailureでも公開しません。Entity全件の世代照合・入力所有値/完成結果のコピーはO(N)で残り、無変更時の更新全体をO(1)にはしません。Dirty根をD、統合した対象をKとすると、区間整列はO(D log D)、対象の幅優先整列はO(K log K)、System計算はO(K)です。全件対象は幅優先順を直接コピーします。KeepWorldは別のbatch cacheを使います。

**CPU入力の変更通知・世代（2026-10-11）**: WorldはEntity実体にActivation/Transform別の変更世代を持ちます。Scene::EditComponent/ForEachActiveは値の復元値を捕捉した後、該当世代を進めてから編集callbackへ渡します。予約Add/Removeは該当世代、実際の親変更（KeepWorldを含む）は両世代を進めます。Name/Camera/Mesh/Scriptなどはこの二つの世代を変えません。通知後の操作失敗や同一値の設定でも世代が進む場合がありますが、候補の値比較で再計算不要を判定します。世代一致かつ可変借用のない入力はComponentの再読取と値比較を省きます。世代はuint64で、発行上限を編集前に拒否し、発行済み番号はrollback後も消費します。

直接Worldの非const GetComponent/FindComponent/ForEachと公開AddComponentが可変参照を返すときは、LocalTransform/ActiveSelfの該当Component実体へ借用済み印を付けます。この実体は世代一致時も値を照合し、同じ参照から後で編集しても見落としません。const借用とScene callback・予約反映は実体へ永続する借用印を付けません。callback外へのScene値の持ち出しとconst参照の書換えは契約外です。Component削除で印は失効し、追加し直した実体へ引き継ぎません。rollbackで元の実体を戻す場合は、その実体が可変借用を返した事実も保持します。

編集callback中に独立のCPU contextを捕捉する場合は、捕捉時のCpuChangeStateへ一時的な照合必要状態を含めます。callbackの後半で同じ世代の値が変更される可能性があるため、現在か比較元のどちらかにこの状態または借用状態があるときは値比較を行います。編集中のWorldの印は入れ子・例外時も元へ戻し、通常のScene開始/終了CPUでは残りません。途中contextの入力はそのまま保持し、後からWorldを参照しません。

変更世代の復元はEntity実体ごとにtransaction内で一度だけ、既存構造journalへ記録します。構造の逆操作と同じ逆順で世代を確保なしに戻し、捕捉済みのCPU contextと整合させます。記録容量は値/構造変更前に確保します。GetCpuChangeStateは世代と借用状態のコピーを返し、Scene保存定義やComponent登録には含めません。

**予約batchの作業索引（2026-10-11）**: WorldCommandBuffer::Flushは親変更・削除があるbatchだけWorld内部に作業索引を一度構築します。slotごとの親・最初の子・前後の兄弟・生存状態を保持し、FIFO反映中は公開const snapshotを再構築せず、このリンクを更新します。生成時の索引拡張はslot/free list変更前に確保します。親変更は新親の祖先を検査し、Componentの変更成功後に確保なしでリンクをつなぎ直します。削除では子リストから部分木を収集し、兄弟をslot順へ整列して子から削除する既存の順序を保ちます。削除したslotのリンクは消去し、同じbatchの世代/UUID再利用と区別します。

KeepWorldは先行予約を含む作業索引から親順にWorld行列を計算します。batch開始の検証順を使い、生成・削除・親変更後だけ順序を再構築します。slotごとの完成行列とEntity世代・親slot・Transform変更世代を保持し、変更したEntityと子孫、可変借用実体だけ再計算します。初回は全件検証し、後続も全生存Entityの世代を走査して、無関係Entityの先行予約による不正Local/合成overflowも拒否します。検証済み未変更値だけを使い回し、公開階層snapshotとSceneUpdateContextを再構築しません。成功/途中失敗ともFlush終了時に作業索引を外し、失敗時はその後のScene rollbackが既存journalと更新前cacheを確保なしで戻します。作業索引の確保失敗は反映開始前の失敗として記録します。生成だけ・階層以外のComponent変更だけのbatchでは作業索引を作りません。

**予約**: buffer ID/batch/位置を持つDeferredEntityを生成予約に返します。同じ反映batchの後続操作に使用でき、反映結果から完全なEntityを取得します。過去batchや別bufferのtokenは拒否します。通常EntityもWorld IDと世代を含めて保持します。親子の重複Destroyは同じ反映中に削除済みの対象だけスキップし、他の失効操作は失敗します。型なし・重複Add・不正親等も失敗として報告します。引数は値を所有し、生ポインター/std::reference_wrapperを直接指定できません。内部に外部参照を埋め込むことも禁止します。

**予約解決と削除履歴（2026-10-11 13:54 JST）**: CommandFlushResult内部にcommand位置→生成順のcreated位置を保持します。Flush開始時に生成件数と階層batchの要否を一度収集し、createdは生成件数、位置表は生成のあるbatchだけ全command数を確保します。非生成・未反映位置は未登録値で初期化します。生成成功後の索引/診断登録は確保なしです。ResolveTarget・CommandFlushResult::Resolve・Scene::Resolveがこの直接表を使い、発行元/batch/範囲/生成済み/完全な予約一致を照合します。Sceneは現在の生存も確認し、診断結果だけのResolveは従来どおり生成時のhandleを返します。公開created配列が変更された場合も範囲外や別予約を返さず拒否します。結果のコピーは対応表も所有し、rollbackは両境界の生成配列・直接表を消してrolledBackにします。直接参照はO(1)、表の初期化/保持はcommand数Cに対してO(C)です。

削除済みの照合は、Flush内だけのunordered_setへWorld ID/index/generationを含むEntity全体を保持し、平均O(1)で行います。順序付きdestroyed配列は子からの履歴として別に保持します。同じslotの新しい世代は別キーで、単なる非生存を重複削除として省略しません。Worldは部分木の検証/収集後、履歴・構造journal・集合bucketと全対象の集合nodeを破棄前に準備します。node登録途中で失敗したときは今回追加した仮nodeだけ除去し、Worldと以前の履歴は保ちます。全準備成功後に子から削除し、この反映中に完了した全子孫を重複照合に含めます。公開World単独の削除は集合を作らず、従来の履歴と削除順を使います。

削除履歴と集合の予約要素数は容量不足時だけ約1.5倍へ増やし、必要数の加算・成長は上限を検査します。小刻みなreserve(size+1)を避け、削除/記録中に新規確保しない保証を保ちます。集合nodeの確保、部分木収集、階層batchのslot領域初期化は残ります。集合と削除履歴はFlush終了で破棄し、更新全体の復元には既存World journalを使います。Sceneの型別候補接続は上記経路へ実装済みです。Transaction作業領域の継続再利用は下記の経路へ実装済みです。

**Transaction作業領域（2026-10-11）**: World内にTransaction本体を保持し、Scene更新中だけactive pointerから参照します。開始準備は番号を消費し、空きslot一覧の領域を不足時だけ拡張して内容をコピーし、slot数と前の階層を記録します。準備成功後だけactiveを公開し、失敗時は未開始としてWorldを保持します。成功時は値復元記録を捨ててから削除実体・除去nodeを操作順に解放し、失敗時は値と構造を復元してから記録を消します。両境界で値/構造記録・空きslot一覧の内容・階層借用・更新情報を消し、activeを解除します。

値/構造journalと空きslotバックアップの配列容量はWorld破棄まで高水位で保持し、自動縮小しません。rollbackのfree list swapは二つの確保済み領域を交換し、容量を解放せず再利用します。Transaction本体の毎更新確保はありませんが、空きslot一覧のO(空き数)コピー、個々のComponent/ScriptのRollbackState生成、CommandFlushやCPU捕捉などの確保・コピーは残ります。Scene更新全体の無確保を保証するものではありません。

**一回更新全体の復元（2026-10-11改定）**: Scene::Updateは開始反映から終了CPU計算までを一つのtransactionにします。例外時は両境界のEntity生成/削除・Component追加/削除・親変更、EditComponent/ForEachActiveの値編集、Script enabledと内部状態を更新開始前へ戻します。Worldはコピーせず、削除したEntityとmap nodeを成功確定まで保持し、元の実体・Entity世代・Componentアドレスへ復元します。構造journalは逆順、削除実体の成功時解放は記録順で子から行います。復元の領域は変更前に用意し、ユーザー定義Restoreは確保なし/noexcept、destructorも例外を送出せずScene/Worldへ再入しない契約です。外部参照先/ファイル/ログ/SceneHostの切替などは復元対象に含めません。

通常Componentの値復元は、独立したコピーとnoexcept swapを使います。参照先の可変状態や非copy型を値編集する場合は、Component自身のCaptureRollbackでunique_ptr<RollbackState>を返します。非copy型の構造変更にはコピー契約を要求しません。更新中に新規生成したComponentの値は、実体の破棄で取り消します。待機中の値編集は次Updateの開始前状態であり、そのUpdateのrollbackで巻き戻しません。

失敗した操作と未実行予約は廃棄し、自動再試行しません。CommandFlushResult.rolledBackを両境界に設定して生成結果を消し、applied/skippedは実行診断として残します。反映自体のerrorがない境界にも更新全体の失敗原因を記録します。SceneはFailedとなり、旧CPU結果も隠します。ResetAfterFailureで明示再開すると旧結果を再公開し、次Updateで再検証します。更新番号・予約batch・発行済み世代は消費したままです。slotの生存世代と最後に発行した世代を分け、失敗中に取得した仮handleが再利用で有効になることを防ぎます。

**CPU作業領域（2026-10-11）**: Sceneは完成結果一つと、公開・比較に使わなくなった二つの作業contextを所有します。入力・active・完成行列・Dirty根/対象配列の容量を再利用し、更新中は旧公開結果と固定ゲーム入力を上書きしません。成功commit後に旧結果と入力を作業領域へ戻し、失敗時は旧結果を同じ実体で復元して途中入力だけを作業領域へ戻します。失敗contextはScene内部の再捕捉でのみ初期化できます。Script対象の配列も容量を再利用し、成功/失敗とも更新境界内で借用を消します。高水位容量はScene破棄まで保持し、自動縮小はしません。新規・拡張・比較元なしの捕捉では確保が残ります。

**CPU結果とScript**: 入力は一度捕捉し、ContextはWorld参照を保持せずcopy/move不可です。固定Stageの順序と失敗を検査します。CPUの完成はTransform段階で、GetCpuFrame/GetTransformsから利用できます。Sceneの結果は次Update成功・idle値編集・Scene破棄で参照が失効し、値コピーは過去snapshotとして保持できます。Update中と失敗後はGetSnapshotで公開しません。
Script対象はゲーム段階の開始時に固定し、active Entityのenabledかつ解決済みinstanceだけ呼びます。削除予約した対象も後段反映まで生存します。Script更新はC++ destructorから呼びません。未知のclassは設定を保持したままinstanceなしで読込可能です。
Script内部状態はScene保存の設定から復元せず、ScriptBehaviour::CaptureRollbackで捕捉します。無状態もStatelessRollbackを明示し、未実装・nullptrはUpdate前に拒否します。開始反映後に残る全instanceを、inactive/disabledも含めて捕捉し、全捕捉成功後にゲーム更新を開始します。開始反映で除去した実体は内部状態を変更せずjournalが保持しているため、捕捉せず復元できます。復元未対応Scriptも明示復旧後に除去する予約を反映できます。構造変更のconstructor/destructorは他実体を編集せず、捕捉も状態を変更しません。捕捉・復元・記録破棄中はScene/Worldへ再入しません。
欠落Hierarchy=root、ActiveSelf=true、LocalTransform=identityです。SceneAssetのComponent配列は登録の有無を保持し、空のScriptComponentも復元します。Camera未選択でも階層・Transformを検証し、Cameraの失敗は公開済みCPU結果を変更しません。Cameraと全祖先のlocal scaleはunit、非Cameraのzero/負scaleは許可します。

SceneLoaderはUUID・全設定・階層の検査→全生成予約→RegistryでComponent/Scriptを復元→解決済み親index復元→反映→CPU検証を行い、ScriptのUpdateを実行せず完成候補を返します。Validate/Loadは同じ内部処理でUUIDのhash索引とasset indexの親配列を一度作り、三色の反復走査で循環をO(N)検査します。Loadは親配列をそのまま予約配列へ対応付け、UUID索引を再構築しません。Component参照検査用のsetは一回作り復元時にも共有します。全UUID/階層/設定の検査が終わるまで候補SceneやScript factoryを生成しません。Captureは未反映予約と未登録Componentを拒否します。SceneAssetはSceneComponentDefinition（型ID・version・所有SceneValue）を配列で持ち、JSON型をWorld/Registryへ公開しません。起動側がComponentRegistry/ScriptRegistryを所有して明示登録し、Freeze後にLoaderへ借用させます。簡易Load/Captureは呼出中だけ組込み登録と空Script登録を所有します。

**JSON初版の契約**: `.scene.json`、UTF-8/BOMなしの整形出力。必須rootはformat=`KT_Forge.Scene`・version=1・name・objects。オブジェクトはuuid・name・parent（rootはnull）・components。Componentはtype・version・data（object）。UUIDは小文字8-4-4-4-12で出力し、入力の大小文字は同じ16byteへ変換します。全ゼロ・重複・不正参照・循環・同型Component重複・未定義フィールド・JSON重複キーを拒否します。名前は空/重複可。PersistentId/Name/Hierarchyは共通情報だけに保存します。object/Component順は意味を持たず、出力はUUID/型ID順、設定キーは名前順です。兄弟の表示順は保存しません。

保存値は制作時の入力だけです。World行列・最終active・Entity handle・pointer・GPU資源・Script実体/一時状態を保存しません。Quaternionはxyzw、数値は有限値、floatへの変換前に範囲と0へのunderflowを検査します。64bit整数/数値Asset IDは正規10進文字列。JSON整数tokenでdoubleの連続整数精度範囲を超える値は拒否します。SceneJsonの初版上限は64MiB/深さ128です。SceneValueはnull/bool/double/string/array/objectの所有値で、各Componentのdecodeが必須項目・型・範囲・UUID参照を検査します。decodeは副作用なしで、検査と予約時に呼ばれます。復元値に外部参照を埋め込みません。

Scriptはkt.ScriptComponentのdata.scriptsにclass/version/enabled/settingsを保存します。設定型をbool/int64/float64/string/objectUuidのタグで明示し、未指定UUIDはnull。Script配列順は更新順で、同クラス複数を許可します。未登録クラスはversion・設定を保持してinstanceなし、登録済みクラスの未対応versionは失敗です。未対応Scene/Component versionと未登録Componentも失敗です。暗黙の既定値補完・自動移行はしません。ゲーム側のvalidatorが公開設定項目を明示し、factoryは直接リンクするC++実体を生成します。DLL/hot reload、プロパティ反射、Editor、Awake/Start/OnDestroyは未実装です。

SceneJson::Read/ParseはWorldを生成せず検査済みCPU定義を返します。Saveは完成SceneからCaptureし、全JSONを作ってから同じディレクトリの専用一時ファイルへ書き、FlushFileBuffers/close成功後にMoveFileExWで置換します。失敗時に既存保存先を先に削除せず、この保存で作成した一時ファイルだけを除去します。通常の失敗保持とOS停止/電源断の保証は同一視しません。将来の独自バイナリは配布対応時に同じSceneAsset契約から変換し、C++のメモリ配置をそのまま保存しません。
## 14 型別Mathの現在の契約

Source/Core/Mathの各h/cppが入口です。SceneMath.hは互換umbrellaだけで、通常は必要な型を直接includeします。
LH/+Y up/+Z forward、meter/radian、Quaternion xyzw、Local=S*R*T、World=Local*Parent。
World行列を正本とし、非均一scaleのshearをTRSへ再分解しません。有限far Reverse-Zはnear=1/far=0、view=inverse(World)、VP=view*projectionです。

| 型/ファイル | 主なAPI・注意 |
| --- | --- |
| Constants/Scalar/Angle | Pi/IsFinite/Near/Clamp、Degrees/Radians、ToRadians/ToDegrees |
| Vector2/3/4 | Dot/Length/Normalized/Lerp、Vector3 Cross。基本演算子は通常float、検査付き関数と区別 |
| Quaternion | Normalize/FromAxisAngle/Multiply/Rotate/Slerp。first*secondはfirst→second、積を勝手に正規化しない |
| Matrix4 | LocalMatrix/TRS/Multiply/Inverse/Transform/ProjectPoint/PerspectiveReverseZ |
| AABB | finite min<=max、接触を含む。既定の±infだけは有効な空box |
| Plane | dot(normal,point)+distance=0。問合せは単位normal、内側>=0 |
| Ray | finite nonzero方向。直接生成のtは距離とは限らず、MakeRayは正規化。t>=0、no hitはoptional空 |
| Frustum | 有限far VPの列から内向き6plane。AABB判定は保守的で偽陽性あり |

検査付き演算は非finite/zero正規化/domain不正をinvalid_argument、float結果範囲外をoverflow_errorで拒否し入力を変えません。
Quaternionの既存Normalize名を保つため、vectorはNormalizedです。Slerpはt=[0,1]・最短弧。
Matrix4の添字0..3は呼出側契約。TransformPoint/Vectorはaffine専用。ProjectPointはw=0を拒否し、負wの数値結果を可視性と同一視しません。
Inverseの相対pivot閾値は1e-10です。空AABBのContains/Intersectsはfalse、Center/Extentsは拒否。
Plane問合せのunit許容差は1e-5。Ray/Planeの平行・coplanarは一意交点なし。
Frustum Near=column3-column2/Far=column2、IsValidは6plane検査で任意手入力平面の有界性は証明しません。
toleranceはfinite非負・単位normalへの距離。形状型はCPU演算の部品として扱います。

## 15 GPU部品とGraph接続の入口

Graphicsは以下の責務別フォルダーに配置します。namespaceはすべてKT::Graphicsのままです。

| 配置 | 部品 |
| --- | --- |
| Graphics直下 | GraphicsDevice、GraphicsValidation、明示Source登録 |
| Graphics/Commands | CommandContext、CommandQueue、FrameResources、IndexedDrawPacket |
| Graphics/Buffers | GpuBuffer、VertexBuffer、IndexBuffer、ConstantBufferArenaとConstantSlice |
| Graphics/Descriptors | DescriptorHeap、DescriptorAllocator、DescriptorHandle |
| Graphics/Textures | Texture、RenderTarget、DepthBuffer、ColorTargetView、DepthTargetView |
| Graphics/Pipeline | Shader、RootSignature、GraphicsPipelineState |
| Graphics/Presentation | Swapchain |

| ファイル/クラス | 現在の責務・API |
| --- | --- |
| Graphics/BuffersのGpuBuffer・VertexBuffer・IndexBuffer | 不変UPLOADまたは明示DEFAULT転送、byte span/stride、R16/R32 index、viewとlayout検査 |
| Shader・RootSignature・GraphicsPipelineState | bytecode/RootCBV/PSO所有。描画意味の選択はRenderer |
| DescriptorHeap・DescriptorAllocator・DescriptorHandle | heap所有、単調slot、heap固有token検査 |
| Texture・RenderTarget・DepthBuffer | DEFAULT画像、RTV/DSV heap所有、COMMON生成 |
| ColorTargetView・DepthTargetView | ownerから画像とRTV/DSVを借用する値 |
| ConstantBufferArena・ConstantSlice | Write、256byte単位、owner/個体ID/epoch/range検査 |
| FrameResources | Begin→GetContext/GetConstants→EndRecording→Submit→Wait |
| Renderer/Mesh・MeshStore | position float3/12byteとindex、Add(id,unique_ptr)/Get |
| UnlitPipeline・UnlitMaterial・MaterialStore | 共有PSO、finite RGBA[0,1]、AddUnlit/Get |
| RenderConstants・PreparedRenderFrame | Prepare、GetDraws、GetViewConstants/GetWidth/GetHeight、RecordDraw |
| BuiltinAssets/Meshes/CubeMesh | CreateCubeMesh(device)、原点中心/辺1m、8vertex/36index |
| BuiltinAssets/Materials/DefaultUnlitMaterial | AddDefaultUnlitMaterial(store,id)、白色presetの明示登録 |

非Graph consumerの順番はBuiltin生成→store登録→CPU RenderWorld/View→Frame Begin→Prepare→明示barrier/clear→RecordDraw→End/Submit/Waitです。
Prepare失敗時は部分frameを公開しませんが、既に書いたarena領域は消費済みです。PreparedFrameは次epochで作り直します。
Draw/clearはpipeline/viewport/IA状態を復元しません。画像state・Graph宣言・資源対応は呼出側で設計します。
GraphはPreparedRenderFrame/Mesh/Material/Worldを参照せず、OpaquePassが描画用snapshotを扱います。画像/view/外部VB・IB・定数と記録窓口の契約は[render-graph](render-graph.md)に集約します。

過去の独立consumerの成功を現在の製品Graphの動作保証へ読み替えません。





### ToonのBinding補足（2026-10-12）

Toonのroot 0=b0 View、1=b1 Object（world+normalWorld）、2=b2 Material（80byte）、
3=t0色/t1影の連続2 SRV、4=s0 Sampler、5=b4 Light（128byte）、Skinだけ6=b3 palette。
Toon単色でも有効なnull色SRVを同じtableへ置く。UnlitとShadowは従来の3/5 rootにSkin用末尾CBVを追加する構成を維持する。
Shadow PSOはNumRenderTargets=0/targetFormat=UNKNOWN、同じReverse-Z D32/GREATERとAlpha Maskを使う。
PCFは隣接texelの受け面深度を補正して、斜めの床の自己影を抑える。

