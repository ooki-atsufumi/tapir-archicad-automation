# 引き継ぎ資料 — Archicad BIM → Blender ウォークスルー動画(2026-09-13)

別のセッション・別のリポジトリ(archicad-addon-cmake ベースのプロジェクトなど)で作業を続けるための
まとめです。末尾に **新しい Claude Code セッションにそのまま貼れるプロンプト** があります。

## 1. 目的

Archicad の BIM モデル内を「自分」が歩いているように見える動画を、無料ソフトだけで、
Claude Code から Tapir と同じ確度で自動生成する。Blender は 5.2 を使用。

## 2. 現状(このブランチ `claude/blender-photo-video-generation-zbcrdt` に入っているもの)

```
blender-automation/
  README.md                   全体説明(日本語)。到達可能レベルの評価、アバター作成 3 方式、使い方
  HANDOVER.md                 この資料
  archicad/                   Tapir 経由で IFC 書き出し(tapir_client.py, export_ifc.py)
  scripts/                    Blender 内部品(bpy): compat / import_ifc / walkthrough / avatar / lighting / render_settings
  pipeline/                   build_walkthrough.py(Blender 内)、run_pipeline.py(外部ドライバ)、config.example.json
  archicad-addon-cmake/       GRAPHISOFT テンプレート準拠の独立 Add-On「Blender Bridge」(C++、Tools はサブモジュール)
  mcp.example.json            Claude Code に blender-mcp を登録する設定
.github/workflows/blender_bridge_build_check.yml   Blender Bridge のビルドチェック
```

コミット:
- `e893c01` Add Blender walkthrough-video automation pipeline
- `5e78d11` Add Blender Bridge Add-On based on archicad-addon-cmake + handover
- `3219e75` Fix Blender Bridge Windows build (CI green on Windows AC27-29 / macOS AC29)

## 3. 設計の要点

1. **Archicad → IFC**: Tapir `IFCFileOperation {method:"save"}`、または独立 Add-On `BlenderBridge.ExportIFC`。
   どちらも Archicad 内蔵 IFC Add-On(moduleID 1198731108/138575850, 'IFCI')を `ACAPI_AddOnAddOnCommunication_Call` で呼ぶ。
2. **IFC → Blender**: Bonsai 拡張(`bpy.ops.bim.load_project`)。無い場合は glTF/FBX/OBJ/USD にフォールバック。
3. **カメラ**: 床面 waypoints(m、Archicad 座標)→ NURBS パス → Follow Path(等速)+ 視線先行ターゲット + 頭の上下動。
4. **アバター**: リグ付き FBX/glTF(Mixamo "Walking In Place" 等)を身長合わせ → 同じパスを追従 → Cycles F-modifier でループ。
5. **レンダ**: preview(EEVEE 720p)/ eevee_hq / final(Cycles 1080p, GPU 自動選択)→ H.264 mp4。
6. **互換層** `scripts/blender_compat.py`: EEVEE の engine ID 差、Slotted Action(4.4+)の F-curve 走査、拡張の有効化。

## 4. 未検証・既知のリスク

- Blender 5.2 実機で未実行(作業環境に Blender 無し、docs.blender.org もアクセス不可)。
  想定される要修正点: `bpy.ops.bim.load_project` の引数名、EEVEE 属性名(`use_raytracing` 等)、Follow Path の keyframe 補間。
- Blender Bridge Add-On は GitHub Actions で Windows AC27/28/29・macOS AC29 のビルドが通っています(コミット 3219e75)。
  実機での動作(メニュー、IFC 書き出し、JSON コマンド)はまだ未確認です。
- `RFIX/AddOnFix.grc` の MDID はプレースホルダ。自分の開発者 ID に差し替える。
- 参照 YouTube 動画(NpkjkMvUghg)は環境の制限で内容未確認。

## 5. 次にやること(優先順)

1. Blender Bridge の .apx / .bundle を CI の成果物または `build.bat 29` で作り、Archicad に読み込んで 3 コマンドとメニューを実機確認する。
2. Blender 5.2 + Bonsai で `run_pipeline.py --still 1` を実行し、`scripts/` の API 差分を直す。
3. 自宅モデルの waypoints を JSON に書き、preview 動画を出す。
4. 写真 → Hunyuan3D/TripoSR → Mixamo で FBX を作り `avatar.enabled=true` で三人称ショットを追加。
5. 材質名 → CC0 テクスチャ対応表による PBR 自動割当(写実性向上)。

## 6. 新しい Claude Code セッション用プロンプト(コピーして貼る)

```
リポジトリ ooki-atsufumi/tapir-archicad-automation のブランチ claude/blender-photo-video-generation-zbcrdt にある
blender-automation/ を引き継いでください。まず blender-automation/HANDOVER.md と README.md を読んでください。

やりたいこと: Archicad の BIM モデル(IFC)を Blender 5.2 に取り込み、私が中を歩いているウォークスルー動画を
無料ソフトだけで自動生成する。Claude Code から Tapir と同じ確度で Blender を操作したい。

今回のタスク: (ここに具体的な作業を書く。例)
- blender-automation/archicad-addon-cmake を GitHub Actions でビルドし、エラーを修正して push する
- Blender 5.2 で pipeline/run_pipeline.py --still 1 を実行したログ(添付)を見て scripts/ を修正する

制約: 有料ソフトは使わない。既存のフォルダ構成と blender_compat.py の互換層を維持する。
```
