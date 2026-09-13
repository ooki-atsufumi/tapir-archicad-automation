# セッション引き継ぎ — Archicad BIM × Blender 5.2 で「自分が歩く」動画を作る(2026-09-13)

このファイルは、クラウド上の Claude Code セッション(PC に接続できない環境)から、
**PC に接続できる Claude Code セッション**へ作業を引き継ぐためのものです。
新しいセッションでは、まずこのファイルを読ませてから「7. 次セッション用プロンプト」を貼ってください。

---

## 1. ゴールと方針(合意済み)

- Archicad の自宅 BIM モデルの中を、**自分(写真から作るアバター)が歩いている**ように見える動画を作る。
- **有料ソフトは使わない**(Blender / Bonsai / blender-mcp / Mixamo / オープンソースの写真→3D)。
- Claude Code から Blender を Tapir と同じ確度で操作する。Blender は `bpy` で全操作がスクリプト化できるため可能。
  対話操作には blender-mcp(MIT)を使う。
- 到達レベルの正直な評価: 建物とカメラワークは高い写実性が出せる。写真 1 枚からの「自分」は 2〜5 m 離れれば
  本人と分かるレベルが上限。顔アップは無料では無理。本編は一人称視点、要所だけ三人称で自分を出す構成を推奨。

## 2. ユーザー環境(把握している事実)

| 項目 | 値 |
|---|---|
| OS | Windows(ユーザー `sdh001905`) |
| Blender | 5.2(インストール済み・起動中) |
| Archicad | 起動中(バージョン未確認。Tapir Add-On の有無も未確認) |
| 自分の写真 | `C:\Users\sdh001905\Desktop\ooki` に一覧あり(枚数・全身写真の有無は未確認) |
| GitHub | `ooki-atsufumi/tapir-archicad-automation`(Tapir のフォーク) |
| 作業ブランチ | `claude/blender-photo-video-generation-zbcrdt` |

## 3. リポジトリの現状(プッシュ済み)

```
git clone --recurse-submodules https://github.com/ooki-atsufumi/tapir-archicad-automation
git checkout claude/blender-photo-video-generation-zbcrdt
```

コミット履歴(新しい順):

| コミット | 内容 |
|---|---|
| 6fae33e | Blender Bridge のビルドチェックを手動実行のみに変更(失敗メール対策) |
| c85d9a0 | 引き継ぎ資料更新(CI 成功を反映) |
| 3219e75 | Blender Bridge の Windows ビルド修正(UniString 変換)、macOS は fork/exec に変更 |
| 5e78d11 | archicad-addon-cmake ベースの独立 Add-On「Blender Bridge」+ HANDOVER.md |
| e893c01 | Blender ウォークスルー動画パイプライン一式 |

フォルダ構成:

```
blender-automation/
  README.md                 全体説明(日本語)。到達レベル評価、アバター作成 3 方式、使い方
  HANDOVER.md               設計要点と未検証項目(前回分)
  SESSION_HANDOVER.md       このファイル
  archicad/
    tapir_client.py         Archicad HTTP/JSON クライアント(ポート 19723)
    export_ifc.py           Tapir IFCFileOperation(save) で IFC 書き出し
  scripts/                  Blender 内部品(bpy)
    blender_compat.py       4.2〜5.x の API 差異吸収(EEVEE ID、Slotted Action の F-curve 走査、拡張有効化)
    import_ifc.py           Bonsai で IFC 読込(bpy.ops.bim.load_project)。無ければ glTF/FBX/OBJ/USD
    walkthrough.py          waypoints→NURBS パス→等速カメラ、視線先行、頭の上下動、DoF
    avatar.py               リグ付き FBX/glTF を身長合わせ、パス追従、歩行ループ
    lighting.py             Nishita 空 / HDRI、太陽、天井フィルライト、AgX
    render_settings.py      preview(EEVEE 720p) / eevee_hq / final(Cycles 1080p, GPU 自動)→ mp4
  pipeline/
    build_walkthrough.py    Blender 内オーケストレータ(blender -b -P ... -- --config x.json)
    run_pipeline.py         外部ドライバ(IFC 書き出し→Blender 起動)。BLENDER_PATH 環境変数対応
    config.example.json     設定サンプル(waypoints は床面の点、m、Archicad 座標)
  archicad-addon-cmake/     独立 Add-On「Blender Bridge」(GRAPHISOFT テンプレート準拠、Tools はサブモジュール)
                            JSON: BlenderBridge.GetProjectPath / ExportIFC / RunBlenderPipeline、メニュー 3 項目
                            CI: Windows AC27/28/29・macOS AC29 でビルド成功。実機動作は未確認。MDID はプレースホルダ
  mcp.example.json          Claude Code に blender-mcp を登録する設定
  run_preview.bat / run_final.bat
.github/workflows/blender_bridge_build_check.yml   手動実行のみ(workflow_dispatch)
```

## 4. 検証状況

| 項目 | 状態 |
|---|---|
| Python スクリプトの構文チェック・ドライラン | 済 |
| Blender Bridge Add-On のビルド | 済(GitHub Actions) |
| Blender 5.2 実機でのスクリプト実行 | **未**(クラウド環境に Blender 無し) |
| Archicad 実機での IFC 書き出し・Add-On 動作 | **未** |
| 写真からのアバター作成 | **未**(写真フォルダに到達できず) |
| 参照 YouTube 動画(NpkjkMvUghg)の内容確認 | **未**(アクセス遮断) |

想定される要修正点: `bpy.ops.bim.load_project` の引数名、EEVEE 属性名(`use_raytracing` 等)、
Follow Path の keyframe 補間、Bonsai が生成するマテリアル名。

## 5. ユーザーから指示された作業手順(未実施。次セッションで実行)

1. Blender 5.2 に Bonsai 拡張をインストールし、`config.example.json` を複製して waypoints を自宅モデルの座標(m)に合わせる。
2. `python pipeline/run_pipeline.py --config <設定> --export-ifc --still 1` でフレーム 1 の静止画を確認する。
3. 全身写真から Hunyuan3D または TripoSR で 3D 化し、Mixamo で「Walking(In Place)」を付けた FBX を
   `examples/assets/` に置いて `avatar.enabled` を true にする。
4. 材質名→CC0 テクスチャ対応表による PBR 自動割当(写実性向上)。

## 6. 着手途中だった設計(コードは未作成。次セッションで実装してよい)

前セッションが中断された時点で、上記 1〜4 を PC 上で一括実行できるよう次の追加を設計していました。

- `archicad/suggest_waypoints.py` — Tapir で waypoints を自動生成。
  `API.GetElementsByType {elementType:"Zone"}` → `GetDetailsOfElements {elements:[{elementId}]}`。
  応答は `detailsOfElements[]` に `type:"Zone"`, `floorIndex`, `details.name`, `details.stampPosition{x,y}`,
  `details.zCoordinate`。最下階のゾーンのスタンプ位置を、名前が「玄関/entrance/hall」のものを起点に
  最近傍順で並べて waypoints に書き込む。`GetZoneBoundaries {zoneElementId}` で境界ポリゴン(3D)も取れる。
- `tools/install_bonsai.py`(Blender 内)— `bpy.ops.extensions.repo_sync_all()` →
  `bpy.ops.extensions.package_install(repo_index=<blender_org>, pkg_id="bonsai")` → `addon_utils.enable("bl_ext.blender_org.bonsai")`。
  事前に `preferences.system.use_online_access = True`。
- `tools/prepare_photo_avatar.py` — 写真フォルダから全身写真(縦長・最大)を選び、`rembg`(pip、無料)で背景を抜いて
  `examples/assets/me_cutout.png` を作る。GPU 不要。
- `scripts/avatar_billboard.py` — 上記の切り抜き PNG を身長 1.7 m の板(ビルボード)に貼り、カメラ追従(Track To)で
  常に正面を向かせ、パス上を歩かせる(上下動+わずかな揺れ)。**写真 1 枚から即日で「自分」を入れられる暫定手段**。
  Hunyuan3D/TripoSR + Mixamo のリグ付き FBX ができたら `avatar.py` に差し替える。
  config 案: `"avatar": {"type": "billboard" | "rigged", "path": ..., "height_m": 1.7}`。
- `scripts/materials.py` + `pipeline/material_map.json` + `tools/fetch_textures.py` — 材質名/IFC クラス
  (床・フローリング・wood → WoodFloor051、壁・クロス・plaster → Plaster001、天井、タイル、コンクリート、ガラス、金属 等)
  → ambientCG の 1K JPG(`https://ambientcg.com/get?file=<AssetId>_1K-JPG.zip`、CC0)を取得し、
  Texture Coordinate(Object)+ Mapping + Image(Box 投影)で UV 無しで貼る。取得失敗時は色だけの手続きマテリアルに
  フォールバック。
- `setup_local.bat`(pip: rembg onnxruntime pillow、Bonsai インストール)と `run_all.bat`
  (IFC 書き出し → waypoints 生成 → 写真切り抜き → テクスチャ取得 → 静止画 → preview 動画)。

## 7. 次セッション用プロンプト(コピーして貼る)

```
リポジトリ ooki-atsufumi/tapir-archicad-automation のブランチ claude/blender-photo-video-generation-zbcrdt を
--recurse-submodules でクローンし、blender-automation/SESSION_HANDOVER.md を最初に読んでください。

この PC には Blender 5.2 と Archicad が起動しており、私の写真は C:\Users\sdh001905\Desktop\ooki にあります。
SESSION_HANDOVER.md の「5. 作業手順」を上から順に実行し、「6. 着手途中だった設計」のツールも必要に応じて実装してください。
まず Archicad に Tapir Add-On が入っているか(ポート 19723 に API.GetProductInfo)を確認し、無ければ
blender-automation/archicad-addon-cmake の Blender Bridge か Tapir のインストール手順を案内してください。
各ステップで出たエラーはその場で修正し、フレーム 1 の静止画 → 720p preview 動画の順に成果物を見せてください。
有料ソフトは使わないこと。写真から即日で自分を入れるにはビルボード方式(rembg 切り抜き)を先に実装し、
その後 Hunyuan3D/TripoSR + Mixamo のリグ付き FBX に差し替えてください。
```

## 8. その他の決定事項

- GitHub の失敗メールが煩わしいとの要望で、Blender Bridge のビルドチェックは手動実行のみに変更済み。
  自動実行に戻すには workflow ファイル内のコメントを外す。アカウント側の通知設定(Settings > Notifications > Actions)は
  API から変更できないため未変更。
- リポジトリ `ooki-atsufumi/-` はマンション内装検査手順書の HTML で、今回の作業とは無関係。
