# KT_Forge RenderGraph の契約

更新: 2026-10-12 JST（RmlUi/UiPassまで）。Graphの採用契約と対応範囲の正本です。
宣言や本体の存在だけで契約充足・実行成功を主張しません。

## 1 画像・view・所有

2026-10-12のSkin描画追加も、既存Clear/DepthClear/Opaqueと画像借用の契約を維持する。
paletteはFrameResourcesの同一epoch定数arenaへ置くCBVで、Graph画像handleやAssetIdへ変換しない。
Texture tableのroot3/4を維持し、Skin CBVは単色root3/Texture版root5、shader register b3を使用する。
Shaderは最大256行列、CPUも同じrow-major/row-vectorでコピーする。再利用は1slotの最後のFence完了後。

| 定義 | 保持する内容・意味 |
| --- | --- |
| GraphResourceHandle / GraphViewHandle | 独立したgraphid/index。形式とGraph所属の検査は別 |
| GraphTextureRange | firstMip/mipCount、firstSlice/sliceCount、firstPlane/planeCount。画素矩形ではない |
| GraphImportedTextureDesc | name、resource、initialState、finalState、contentsDefined、requireDefineAtEnd |
| GraphViewDesc | name、親resource handle、range、排他的なGraphics借用view |
| GraphViewBinding | variant&lt;monostate, ColorTargetView, DepthTargetView&gt;。非所有の値 |
| GraphViewUse / GraphResourceUse | viewまたは画像とrange、access、usage |

画像descへrtv/dsv/kindを混ぜず、viewが親画像とrangeを明示します。monostateは未指定として拒否します。同じ画像pointerの二重importを拒否し、別viewが同じ画像を使う依存は親画像/rangeへ正規化します。

Graphは画像・heapを所有しません。借用wrapper元の一時オブジェクトを保持する必要はありませんが、実画像とdescriptor heapの所有者は最後の利用Fenceまで保持します。ColorTargetViewはRenderTargetまたはSwapchain、DepthTargetViewはDepthBufferから画像・descriptor・寸法・formatを借用します。

## 2 Graph本体・ID・登録

RenderGraphは引数なしconstructorで内部一意IDを発行し、copy/moveは禁止です。単一スレッドの操作を前提とします。AcquireGraphIdはプロセス内の非0 IDを再利用せず、最大値を枯渇用に予約し、到達後はoverflow_errorで停止します。

両handleはgraphid=0またはindex=uint32最大値を無効とします。IsValidは形式だけを検査します。現行Containsは形式・同Graph・範囲を確認し、不正なら例外、正常ならtrueを返すAPIです。falseを返す問合せとして使いません。

```text
RenderGraph()
Contains(resource) / Contains(view)
RegisterResource(name) / ImportTexture(desc) / AddView(desc)
AddPass(desc, record) / Validate() / Compile() / Record(FrameResources&)
```

GraphStorageがresource/view/passを所有し、公開可変getterは設けません。登録数はuint32への縮小前に検査し、入力検査後に候補を追加します。RegisterResourceはCPU宣言用で、GPU Compileは全登録画像のimportを要求します。

CompileはResolve→Validate→計画作成→計画保持の順に進み、成功後にCompiledを公開します。失敗はBuildingと登録内容を保持し、部分計画を公開しません。StateはBuilding→Compiled→Recording→Recorded、記録開始後の失敗はFailedです。Reset・再Recordは扱いません。

## 3 passの内容依存

GraphPassDescはname、views、resources、GraphRecordFnはGraphExecutionContextを受け取るcallbackです。GraphはPreparedRenderFrame/Mesh/Material/Worldを参照しません。OpaquePassがRendererの描画用snapshotを扱います。

| access | pass開始時の前提と結果 |
| --- | --- |
| Read | 範囲が定義済みであること。定義状態を保つ |
| WriteAll | 宣言range全体を定義する約束。旧内容に依存しない |
| ReadWrite | 範囲が定義済みであること。内容を保って更新 |

ValidateはimportのcontentsDefinedからローカル状態を毎回作り、同passの全読み前提を開始時点で確認してからWriteAllを反映します。最後にrequireDefineAtEndを確認します。同passのReadとWriteAllをまとめ、未定義読みを隠しません。

WriteAllはshaderの全画素到達を自動証明しません。組込みClearをWriteAll、Opaqueのcolor/depthをReadWriteとし、先行定義を要求します。別の全範囲書込passの採用は、その内容契約を明示して判断します。

usageはUnspecified/RenderTarget/DepthStencil/DepthStencilRead/PixelShaderReadです。Pixel深度SRVとreadonly DSVはReadだけを受理します。CPUの直接Resource宣言はUnspecified＋Read/WriteAll/ReadWriteを受理します。書込ViewはRTV=RenderTarget、書込DSV=DepthStencilで、accessはWriteAll/ReadWriteです。readonly DSVはDepthStencilReadを使います。RTV/書込DSVのRead、readonly DSV/SRVのWrite、ViewのUnspecified、種類違いusage、直接ResourceのGPU usage、未知enumは拒否します。CPUで受理した宣言を、GPU Compileで使用可能と取り違えません。

## 4 各classの責務と対応範囲

| class | 責務 |
| --- | --- |
| GraphUseResolver | 宣言を検査し、view/直接画像の使用を親画像とrangeへ正規化 |
| GraphValidator | import/view/pass、元宣言と正規化結果、内容依存・終了要求の検査 |
| GraphCompiler | 登録順の必要state、各passと終了時の遷移計画をローカルで作成 |
| RenderGraph | storage/計画/状態の管理、遷移とcallbackの記録 |
| GraphExecutionContext | 現在passのView宣言・access/usageを確認してGraphicsへ記録依頼 |

現行は単一Device・単一Direct Queue、宣言順、1Mip/1slice/1plane、sample count1/quality0の全範囲です。書込可能RTV/DSV、readonly DSVとPixel深度SRVを使い、colorは対応RGBA/BGRA8、depth viewはD32_FLOAT/Reverse-Z clear0/GREATERです。sampleable深度の画像はR32_TYPELESS、SRVはR32_FLOATです。

画像とviewのresource・format・寸法を照合します。rangeの0件/越境/加算overflow、部分range、任意Color SRV/UAV、Pixel以外のsampling stage、MSAAは未対応として拒否します。初期/最終stateはCOMMON/PRESENT、RENDER_TARGET、DEPTH_WRITE、DEPTH_READとsampleable深度のPIXEL_SHADER_RESOURCEの対応範囲で、画像のformat/flags/用途と照合します。

自動並替え、culling、aliasing、multi-queue、画像生成/poolは初版に含めません。rangeの型があることを部分subresource対応の証拠にしません。

## 5 記録窓口・外部buffer・定数

GraphExecutionContextはGraphだけが生成し、callback中だけ有効です。raw command listは公開しません。窓口はClearColor(view,color)、ClearDepth(view)、DrawIndexed(colorView,depthView,IndexedDrawPacket)です。

Graphが検査する対象は画像宣言とstate・内容依存です。shaderや外部GPU参照の全未宣言参照を自動検出する機能ではありません。製品のVB/IBはGraph外で同期転送済みの静的不変DEFAULTで、VBはVERTEX_AND_CONSTANT_BUFFER、IBはINDEX_BUFFERを維持します。旧raw consumerだけUPLOAD/GENERIC_READを許します。Index範囲はPacketで検査し、GraphがBuffer依存を自動推論するものではありません。

定数sliceは同じFrameResourcesのarena owner/個体ID/epoch/範囲で検査します。同じ格納アドレスに再構築しても新個体IDになり、旧sliceを受理しません。再利用はFence完了後です。

IndexedDrawPacketはPSO/VB/IBの借用pointer、RootConstantBindingのspan、expectedWidth/expectedHeightを持ちます。nullと寸法0を拒否し、色・深度両方の寸法、layout/index/format、Device、同Frame定数を命令追加前に検査します。spanの参照先は即時Draw呼出中、GPU実体は最後の利用Fenceまで保持します。

OpaquePassはPreparedRenderFrameを値snapshot捕捉し、各drawでローカルbindingsとpacketを作ります。期待寸法はsnapshotのGetWidth/GetHeightから渡します。Drawがない場合は未使用PSO・constants・packetを検査せず、宣言画像/Viewは通常検証します。

ClearPassはfinite色の前検査とWriteAll/RenderTarget宣言、DepthClearPassはWriteAll/DepthStencilと全域clear0、OpaquePassはcolor/depthのReadWriteです。

## 6 Recordと失敗保持

Recordは記録だけを担当し、FrameのBegin/EndRecording/Submit、Present、Fence待機を行いません。同じframeの記録可能なContextとarenaを取得します。

単一Deviceを呼出側の前提とします。現行Recordは全importのDeviceを毎回走査・照合するAPIではありません。GraphicsのDraw検査、画像/view/descriptorの対応、外部所有者による寿命保持はそれぞれ別の条件です。

記録前の状態/計画/Context検査の失敗はCompiledを保持します。記録開始後は、各passの全遷移→callback一回→Context健全性確認の順に進み、最後に終了遷移と成功前の健全性確認を行います。

GraphExecutionContextの検査・記録例外はInvalidateして再送出し、callbackがcatchしても失敗を保持します。記録開始後の失敗はGraph=Failed、Context=無効とし、追加記録・送信を禁止します。listをResetせず、記録済み命令を取り消したことにしません。

Compile/Record完了、画像内容定義、DX12 state、GPU寿命/Fenceは別条件です。barrierやoptimized clear valueは初期化命令ではありません。

## 7 Validatorの呼出契約

ValidateImportedTexture(desc,storage)は新規候補の追加前検査または全Graph検査に使います。全Graph検査ではstorage内descそのものの参照だけを自己比較から除外し、コピーや別recordの同画像は拒否します。ImportTexture内でこの全検査を呼んでいるかは実装観測で区別します。

ValidateViewはhandle形式/所属/範囲、親import、全range、Texture2Dの寸法/sample、binding、画像/viewの対応とformat/flagsを検査します。Device記録先・descriptor中身・寿命まで証明したとは扱いません。

ValidatePassは名前、View/Resourceの所属・範囲・型/用途、親画像、同pass内の競合を検査します。同じ画像の二つ以上の使用は同usageのReadだけ受理します。

全Validateは元宣言と正規化結果のresource/range/access/usage、pass数・宣言順、追加/欠落/余分な重複を照合します。完全一致Readの統合は受理できますが、Resolverが必ず統合するという契約ではありません。callback確認と内容依存/終了要求の検査も行い、Storageを変更しません。

将来のColor SRV/UAV等を追加するときは、実際のGraphics binding、shader stage/read state、descriptor heap/GPU table、UAV ordering、既存readonly DSVとの併用条件を一緒に設計します。

## 不変Material Textureとの境界（2026-10-11）

SampledTextureはGraph外で全Mipの同期転送を完了させ、その後はPIXEL_SHADER_RESOURCE固定で使用します。Graphへ書込画像としてImportする経路は持たず、IndexedDrawPacketで所有者付きSRV/Samplerを借用します。Graphはこれらの参照を自動推論しません。可変の深度Textureは以下のPixelDepthViewとGraph Read契約へ分けます。Color RenderTextureの入力は未対応です。

ColorTargetViewはSwapchainが生成したSRGB RTVのformatを保持します。ValidateViewは同一formatに加え、RGBA8_UNORM resource + RGBA8_UNORM_SRGB RTVを許可します。任意raw descriptorや他のformat変換を許可する変更ではありません。



## Pixel深度SRVとShadow passの追加契約（2026-10-12）

PixelDepthViewをGraphViewBindingへ追加。sampleable DepthBufferのR32_TYPELESS画像とCPU専用R32_FLOAT SRV heapを非所有借用する。
PixelShaderReadはReadだけで、同一画像のDSV writeと同passでは併用しない。全range/単一Device/Direct Queue/宣言順は維持する。
Validatorは画像一致/format/flags/内容定義/usage/accessを検査し、CompilerはPIXEL_SHADER_RESOURCEへの遷移を計画する。
初期/最終stateは既存COMMON/対応writeに加え、R32_TYPELESS深度のPIXEL_SHADER_RESOURCEだけを受理する。
DrawDepthは先行Clear後のReadWrite DSV、DrawToonはcolor/depth ReadWriteとshadow Readを要求し、実table内の深度画像を宣言SRVへ照合する。
従来DrawIndexedから二画像tableを使うことは拒否する。不変Material Textureは引き続きGraph外のPixel読取固定で、t0/t1を同じheapへ置いて借用する。
影のClearはWriteAll、Shadow Drawは部分深度更新のReadWrite。影OFFでもToon tableが参照する深度はClearで内容を定義する。
Graphは資源/tableを所有せず、Begin/End/Submit/Present/Waitもしない。このShadow追加時点ではUAV/任意Color SRV/部分subresource/readonly DSVを追加していない。readonly DSVは以下のEffect追加で接続した。
任意SRV全般の対応として扱わず、深度のPixel読取に限定する。

## EffekseerのEffectPass追加（2026-10-12）

DepthBufferは同じD32画像に書込DSVとREAD_ONLY_DEPTH付きDSVを所有し、DepthTargetViewはreadonly指定を保持する。
DepthStencilReadはreadonly DSVだけ、Read accessだけを受理する。CompilerはDEPTH_READを選び、OpaqueのDEPTH_WRITEから遷移して最後は宣言finalStateへ戻す。
EffectPassはOpaque後の色RTV ReadWriteとScene深度DSV Readを宣言する。未定義の色/深度読みは通常の内容依存検査で拒否する。
DrawEffects窓口は宣言と借用viewを検査し、EffectPlayerへ渡す。通常DrawIndexed/ClearDepthへreadonly DSVを渡すことは拒否する。
EffectPlayerは同じDevice/Queueの上流DX12 adapterを保持し、外部Direct Listへ記録する。RenderGraph自身へWorld/Entity/再生時間の管理を持ち込まない。
上流poolの再利用とGPU完了待機はStaticSceneRendererのGraph記録前に完了させ、callback内部でbarrier/Submit/Present/Waitを行わない。
Playerの素材/heap/bufferは最後の利用Fenceまで外部で保持する。Blend/AddはZTest有効、ZWrite無効。SoftParticle/背景歪みのsamplingは追加しない。

## RmlUiのUiPass追加（2026-10-12）

UiPassはOpaque/Effect後に色RenderTarget/ReadWriteを宣言する。Cameraがない回もClear後にUIを描く。
DrawUi窓口は同Graph所属・当該passのRTVとReadWriteを確認し、UiRenderer::Recordへ渡す。未定義色読取はCompileで拒否する。
UiRendererはGraphの画像/viewを所有しない。RTV寸法/format/Deviceと捕捉済み列を照合し、列をRecord一回で消費する。
深度をbindせず、viewport/scissor/専用PSO/premultiplied alphaで既存色へ合成する。
UI geometry/TextureはGraph外の不変資源。UPLOAD/GENERIC_READと転送済みPixel読取を保持し、Graph画像handleへ変換しない。
Application::PresentSceneでContext::Render、Texture生成/転送、前回UI利用Fenceの完了待機をGraph/Frame開始前に済ませる。
列は資源の共有所有を捕捉し、RmlUi Release/Close/Shutdown後も最後の利用Fenceまで保持する。
callback内部は記録だけ。barrier/Submit/Present/待機は既存Graph/FrameResources/Applicationが担当する。
丸角/変形mask、filter/opacity layer、背景ぼかし、gradient shaderは初版未対応で、要求時は日本語例外を返す。

