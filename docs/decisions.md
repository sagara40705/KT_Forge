# KT_Forge 判断と変更理由

更新: 2026-10-12 JST。採否と理由の正本です。

## 現行判断

最初に実行制御と寿命管理の基盤を整備する。
Applicationに小さなExecutionControlを直接所有させ、Runの稼働状態とSceneの時間進行を分ける。
StopPlaybackは完成Sceneを保持する停止。RequestStopはRun終了、Shutdownは実行窓口の終了とし、意味を混ぜない。
Editorがない現段階でScene複製・停止時の設定復元を導入せず、現在のScene/Script/Physics状態を継続する。
UIなしの停止中も操作できるFrameUpdateを追加し、固定tickの順序・RenderGraph・同期1slotを維持する。
GPU借用解消後にScene/ScriptをUI/Effect窓口より先に破棄する。共通Subsystem、Service Locator、EngineRuntime一括移行は行わない。


作品制作を優先してRenderGraphを維持しForwardで進める。
多数の敵・Particleと多数の動的ライトは別の負荷として扱う。
初期のアニメ調の主光源と透明エフェクトを同じ方式で扱い、Deferred/G-bufferを先に増やさない。
Forward+は計測で同時ライトが支配的になった場合の候補。

ozz-animation 0.16.0をcommit固定して採用する。
共有Skeleton/圧縮Clip、個体別Context、Sampling/Blending/LocalToModelを利用し、再生時間の正本はWorldのAnimatorに置く。
既存Assimp取込とGPU Skinningをエンジン側で接続する。付属FBX SDK/tool/viewerの依存は追加しない。
上流の著作権/MIT全文をThirdParty/OzzAnimationへ保持する。

| ID | 状態・日付 | 判断と理由 |
| --- | --- | --- |
| D01/D02 | 採用、2026-10-08再確認 | Windows x64/DX12/C++20専用。Vulkan/他OS/汎用RHIの抽象化は不要 |
| D05 | 採用契約 | Graphは内部一意ID、引数なしconstructor、copy/move禁止。手動constructorへ戻さない。|
| D06 | 採用契約 | contentsDefinedから毎Validateのローカル状態を作り、pass開始時の全読み前提を確認後に書込結果を反映 |
| D07 | 採用契約 | WriteAllはrange全体定義の約束。組込みClearはWriteAll、Opaqueは先行定義＋ReadWrite |
| D08 | 採用契約 | 別viewの依存を親画像/rangeへ正規化し、競合や未定義読みを隠さない |
| D21 | 採用、2026-10-08 | ECSはWorld内部。Rendererはまとまった描画責務。独立class/file、直接読める呼出順を優先し、共通Subsystem基底/Locator/Service乱立を作らない |
| D22 | 採用、2026-10-08 | GLFW継続、Windows専用。自前Win32移行は予定せず、DX12接続のHWND取得とは区別 |
| D24 | 後日の前提反映、2026-10-09 UTC | 単一Deviceを前提にする。毎Recordの全import Device照合は現行契約へ戻さない。Compile失敗保持、Record/Contextの失敗保持、arena個体IDは継続 |
| D27 | 採用、2026-10-10 JST | ECSをゲーム状態の唯一の正本とする。一Scene一World、EditorはEntity表示、保存は永続UUIDと設定、Loaderで実行状態を復元 |
| D28 | 採用、2026-10-10 JST | 公開DestroyEntityは部分木削除、単体slot破棄はprivate。free listを使用し、破棄前に全対象と容量を確保 |
| D29 | 採用、2026-10-11改定 | 更新前後に予約反映とCPU計算。予約はFIFO。失敗時はD31の一回更新全体rollbackで戻し、予約を廃棄。Failed後は明示再開。Cameraと描画抽出はCPU完了と分離 |
| D30 | 採用、2026-10-10 JST | 編集用SceneはJSON。汎用SceneAsset・安定型ID/version・起動側所有の明示Component/Script Registryを採用。独自バイナリは配布対応時。nlohmann/json 3.12.0を固定し、WorldにJSON型を持ち込まない |
| D31 | 採用、2026-10-11 JST | 全体rollbackの単位はScene::Update一回。元の実体を保持する構造journalと値の復元記録を使い、Worldをコピーしない。Script内部状態はCaptureRollback、Restoreは確保なし/noexcept。外部副作用は対象外。失敗中に発行した世代・予約番号は再利用しない |
| D32 | 初版採用、D36で拡張、2026-10-11 JST | 静的Mesh一つ＋UnlitOpaque Material一つ。128bitの永続AssetIdと種類別参照、null未割当、Scene Component version 2を採用。欠落を保持し当該候補だけ除外。固定CPU CatalogとCamera独立の候補・World AABBを作り、GPU Storeのhandleと分離する |
| D33 | 初版採用、D35/D36で拡張、2026-10-11 JST | root内の素材JSONをUUIDで索引化し、起動Sceneの全MeshRenderer参照を一意化して読込。Scene生成後にCatalogへ一括反映し、欠落は参照元付き診断として保持。モデル取込は位置頂点の三角形OBJに限定し、再取込で保存先UUIDを維持。起動時固定を継続し、実行中reload/自動監視は別目標 |
| D34 | 採用、2026-10-11 JST | StaticSceneRendererが固定CatalogのUUIDをGPU Store handleへ解決し実体を共有。Cameraは有効一台を自動選択、複数はUUID指定、なしはClearのみ。同期1slotで毎回Graph記録→送信→Present一回→Wait。Resizeは旧Listと画像参照を解放し、最小化中もCPU更新を継続。EngineRuntimeや個別Serviceは増やさない |
| D35 | 採用、2026-10-11 JST | DirectXTex May 2026をcommit固定。CPU Texture定義・永続UUID・Material依存を追加。RGBA8へ統一しGenerate/Preserve Mip、UV、Sampler、Alpha Maskを接続。単一2Dの不変Textureを明示同期転送、sRGB入力/出力を一貫させる。照明/PBR/Blendは別目標。旧素材JSONと単色経路を維持 |
| D36 | 採用、2026-10-11 JST | 法線・接線・UV0、名前付きSubmesh/複数Material、Mesh JSON3/Component3と旧版互換、設定付きOBJ再取込を採用。slot名で再取込時の割当を維持し、欠落は当該範囲だけ除外。CPU生成は原本を保持し、独自接線のMikkTSpace互換は要求しない。GPUはconstructorと転送を分け、同期DEFAULT転送完了後にSubmesh範囲で描画。Lit/Skinning/LOD/runtime reloadは別目標 |
| D37 | 採用、2026-10-11 JST | Assimp 6.0.5をcommit固定し、既定importerとzlibを静的リンク。ContentでCPU原本へコピーし、RuntimeIntegrationで階層/素材/Sceneを専用bundleへ一括保存する。再取込は旧bundleを保持して新規先へ同じkeyのUUIDを維持する。モデルCamera/LightとPBR/Animation等は対象外。Importer破棄後の値所有、Unicode、画像の連続読込、既存DEFAULT/Submesh描画へ接続する。上流全文と同梱noticeをThirdPartyへ保存 |
| D38 | 採用、2026-10-11 JST | 永続NodeIDと依存内容Revision、配置rootのModelInstanceを採用。前回原本/現在値/新原本の三者比較でフィールド別編集を保持し、削除/親変更/slot/参照の競合は明示解決。Plan/Applyと安全保存で制作Sceneを一括反映する。原本と個体はECS以外の実行状態へ二重化せず、稼働中reload・Editor UIは別目標。曖昧renameは明示対応、Material固有内容は別UUIDへ分ける |


D24は現行RenderGraph.hの単一Device前提を採用します。

## D21の目標設計と理由

Core/Platform/Content/World/Graphics/Renderer/RuntimeIntegration/BuiltinAssets/Applicationを論理責務として整理します。EngineRuntimeはSceneHost/ContentService/Graphics基盤/Rendererをまとめる普通のクラスの目標案です。SceneがWorldを所有する現在の採用契約はD27を参照します。

Worldは軽量AssetIdとbackendを含まないPhysicsTypesの値定義を参照可能とし、ContentService/Graphics/Renderer/Jolt実体へ依存しません。RuntimeIntegrationでWorldからRenderWorld/RenderViewをコピーし、RendererはWorldを再読取しません。

ContentのCPU Asset、RendererのGPU実体、Graphの借用handle、GPUのFence寿命を分けます。人が所有と処理順を追い、Windows/DX12と現在のゲーム開発へ集中できることが採用理由です。目標設計と実装状況は区別します。

## D38 ModelAssetとMeshAssetを分ける（2026-10-11）

Meshは形状・Submesh/slot、Modelはノード階層・TRS・Mesh参照・初期Material割当を持つ共有原本とします。単一Meshにモデル全体の配置責務を加えず、同じMeshを複数ノード/個体で共有できるようにします。

ModelはContentのCPU資源で、RuntimeIntegrationがScene定義へ展開し、SceneLoaderがWorld/Entityを生成します。ゲーム状態はECSだけで所有します。配置親で個体全体のTRSを指定し、各配置に独立したObjectUuidを発行します。Sceneへ展開した後の自動追従や任意Component/ScriptのPrefabは将来の別契約です。Assimpのノードkeyは名前/同名兄弟番号由来で、改名・並替えを意味的に追跡する保証はありません。

## D39 meshoptimizerを取込時の静的Mesh変換へ導入（2026-10-12）

v1.3を固定し、頂点統合・キャッシュ/読み取り順・任意OverdrawをContentの変換へ閉じ込めます。属性生成/結合後に一度実行し、毎Frameには実行しません。Submeshと永続IDを保持し、CPUのfloat/uint32表現は継続。Rendererは実最大IndexからR16/R32を選択します。

旧recipeの通常再取込はNoneを使用して幾何を維持し、明示設定でのみ移行。新規recipeは設定と処理版を保存し、未知の有効処理版を通常再取込で拒否します。保存済み幾何の読込は処理実装と独立させます。量子化・LOD・圧縮・Meshletは初版に含めません。MIT本文をThirdPartyへ保持し、

## 未決定事項

| 項目 | 次に判断する場面 |
| --- | --- |
| EngineRuntimeの保持/constructor境界 | 現行Applicationの直接所有で不足する起動・実行形態が生じたとき |
| ContentServiceの必要性 | D32の固定AssetCatalogで不足する取得・更新要件が生じたとき |
| GPU素材の退去・再読込・世代付きhandle | D34のRenderer寿命まで固定する共有で不足する場合 |
| engine://・project://の解決、reload/Cache | D33のroot相対配置と起動時読込では不足する要件が出たとき |
| Color SRV/UAV/部分range | 実Graphics bindingと状態/descriptor契約を揃えて拡張するとき。readonly DSVはD43で採用済み |
| InputSnapshotのゲームでの使い方、ゲーム企画/MVP/期限 | 遊びの入力・ルールを決めるとき |
| JoltのCharacter/関節/大規模運用、GPU allocator | 初版の剛体・query機能を超える要件が生じたとき |
| 非同期/batch転送、遅延解放、複数frame同時実行 | 同期1slotから拡張する必要が出たとき |
| HDR、G-buffer/描画方式 | ゲームの描画に必要になるとき |
| SDK/install/export、バイナリ配布の内容 | Engine/Sandboxのsource利用境界を超える配布が必要になったとき |

LH/row-vector/有限far Reverse-Z、D32/sample1/clear0/GREATERとGLFW継続は採用済みです。PhysicsはD40で採用し、Audioや新しいScript backendの導入・順番は別途判断します。

## D40 Jolt PhysicsをSceneの成功公開へ接続（2026-10-12）

v5.6.0を固定commitで静的リンク。独立PhysicsWorldとRuntimeIntegrationのEntity対応を分け、Sceneは物理専用接続を所有します。Applicationが固定60Hzを管理し、Script・ゲーム・物理のdeltaを一致させます。

既存Sceneの更新全体rollbackを維持するため、確定物理を変更しない作業候補方式を採用。Jolt RestoreStateは候補準備でのみ使い、rollbackに割当や復元を要求しません。初版は候補を毎tick構築し、共有形状と生成設定・BodyID・checkpointを転送します。性能より所有と失敗保持の明確さを優先し、将来の再利用最適化は同じ契約を満たす実測後の変更とします。

物理rootの単位scale、明示Teleport/KinematicTarget、境界でのcomponent置換、値だけの通知、旧Scene JSONとの互換を採用。Character・関節・Mesh形状・並列worker・補間は初版へ含めません。MIT本文とAI接続コードの由来をThirdParty/JoltPhysicsへ保持します。





## D42 基本Toonと単一主光源の影（2026-10-12）

RenderGraph + Forward、既存Unlit/静的/Skinを維持し、MaterialごとのToon選択を追加する。
主光源一つと、ゲームから中心を追従させる直交シャドウマップ/9点PCFを初版とする。見下ろしの必要範囲へ影予算を集中し、cascadeや多光源を先に増やさない。
陰影の閾値/補間幅/暗部色、光の色/環境色/強度、影解像度/bias/strengthを保存可能にする。
GraphのPixel深度SRVと実二画像table/深度のみPSOを一緒に追加し、手動barrierや別の描画経路を隠さない。
旧素材・Scene形式を維持し、ToonはMaterial JSON3、DirectionalLightはComponent1。新依存は追加しない。

## D43 Effekseerを基本エフェクトの制作経路へ接続（2026-10-12）

Effekseer 1.80.7（tag 1807、commit b87d1a2e3ff3731ba25cad34f8d8abeae382b21f）と上流固定LLGIを静的リンクする。
制作に必要な半透明/加算、深度遮蔽、再生停止、Entity追従に範囲を絞り、既存RenderGraph/Forward/静的/Skin/Toon/影を維持する。
ゲーム窓口と上流adapterをRuntimeIntegration/Effects、Renderer/Effectsへ分け、RendererがWorldを読まない契約を維持する。
Opaque後の一EffectPassへ記録し、readonly DSV/DepthStencilRead/DEPTH_READをGraphicsとGraphへ同時に追加する。
素材ZTestを有効、ZWriteを無効に統一し、深度の書き換えや歪み用背景コピーを初版へ持ち込まない。
起動時読込と同期1slotの資源保持を採用し、秒単位の固定tick更新をScene成功後に反映する。
上流独自handleを公開せず、Player識別と単調番号のゲームhandleを使う。素材退去/reload、永続Effect Component、骨socket、性能最適化は必要時に別判断する。
上流コードは変更しない。MIT/LLGI/stb/flatbuffers本文をThirdParty/Effekseerへ保持し、AI接続コードと第三者素材の由来を区別する。

## D44 RmlUiをゲーム内HUD/ポーズへ接続（2026-10-12）


RmlUi 6.3（ba95ffe8bfb6370efb2cdcca927eaad4710c5413）、FreeType 2.14.1（526ec5c47b9ebccc4754c85ac0c0cdf7c85a5e9b）を静的リンク。
日本語フォントはNoto Sans CJK JP Regular Sans 2.004（523d033d6cb47f4a80c58a35753646f5c3608a78）、SHA256固定で取得する。
RML/RCSSと値APIで制作できる基本UIを先に接続する。Editor用ImGuiの採否とは別で、ブラウザー互換を前提にしない。
上流の独立Window/Submitを使わず、基本RenderInterfaceを自作adapterへ閉じ込め、既存RenderGraphの最終UiPassへ記録する。
既存Forward/静的/Skin/Unlit/Toon/影/Effekseerと同期1slotを維持する。一般RHI/GUI共通Serviceは追加しない。
UIはSceneの外でunscaled更新し、ポーズ中もボタンと描画を継続する。固定ゲーム/物理/Animation/Effect tickは停止する。
押しっぱなしの入力は物理releaseまで抑制。日本語入力/IME、gamepad menu、高度なmask/filter/reloadは初版へ含めない。
上流は変更しない。MIT/同梱Container/FreeType FTLと同梱notice/Noto OFLをThirdParty/RmlUiと製品UI/Licensesへ保持する。




## D46 Engine を再利用する最初のアプリとして Sandbox を分離（2026-10-12）

本人の実装依頼により、Engineを一つの静的ライブラリ、Sandboxを別の実行targetにする。
既存Sourceの大規模移動を避け、main・デモ・起動Scene・固定UI/素材・初期Animation方針をアプリ側へ置く。
既存UiUpdate/FrameUpdate/SceneFactoryを利用し、公開APIとCMakeの使用条件で別フォルダのゲームからもEngineを利用できる境界を作る。

EngineはUIの所有・描画・入力配送と時間進行を提供する。HUD/pause/font/Escape/pause-resume actionの意味はSandboxが決める。
SetFrameKeys/FrameInputで停止中の操作を固定tickから分離し、UI文字編集focusを優先する。
新規モデル配置/取込では先頭Clipを自動選択せず、bind poseを既定とする。明示再生と保存済みAnimatorの保持を分ける。
既存のScene rollback・物理候補・Skin境界・RenderGraphアルゴリズムは変更しない。
SDK、Hub、プラグイン、汎用Service、別Git repository、ゲーム内容の拡張は追加しない。

D44の導入時に作った標準画面と自動操作解釈はSandboxへ移し、Engineの旧APIは利用者移行後に削除する。