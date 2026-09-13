# Blender automation — Archicad BIM ウォークスルー動画を Claude Code で作る

Tapir が Archicad を JSON コマンドで操作するのと同じ考え方で、**Blender 5.x を Claude Code から
操作して Archicad モデルの中を歩く動画を生成する**ためのツール一式です。
使用ソフトはすべて無料(Blender / Bonsai / blender-mcp / Mixamo / オープンソースの写真→3D)です。

```
Archicad ──(Tapir: IFCFileOperation save)──▶ model.ifc
                                               │
                    Blender 5.x + Bonsai ◀──────┘  IFC 読込(形状・色・IFC属性を保持)
                          │
        pipeline/build_walkthrough.py   カメラ経路・照明・アバター・レンダ設定を JSON から構築
                          │
                       blender -b (ヘッドレス)  または  blender-mcp 経由で Claude Code が対話操作
                          │
                     renders/walkthrough.mp4 (H.264)
```

## Tapir と同じ「精度」で Blender を操作できるか

**できます。むしろ Blender の方が有利です。**

| | Archicad (Tapir) | Blender |
|---|---|---|
| 自動化 API | Add-On が公開する JSON コマンドのみ | Python (`bpy`) で GUI 操作の 100 % が公式にスクリプト化可能 |
| ヘッドレス実行 | 不可(Archicad が起動していること) | `blender -b -P script.py` で GUI なしに実行可 |
| Claude Code からの生操作 | HTTP 19723 番ポート | [blender-mcp](https://github.com/ahujasid/blender-mcp) (MIT) が同じ役割。`execute_blender_code` で任意の bpy を実行 |

Tapir が「Archicad に足りない操作を Add-On で補っている」のに対し、Blender は最初から全機能が
Python から見えているため、Claude Code が生成したスクリプトをそのまま確定的に再現できます。

## 「私がリアルに歩いている動画」の到達可能レベル(正直な評価)

| 要素 | 無料で到達できるレベル | 手段 |
|---|---|---|
| 建物の写実性 | **高い**(Cycles パストレース、物理ベース照明。素材テクスチャを足せばレンダリング業者レベル) | Bonsai + Cycles + CC0 テクスチャ(ambientCG, Poly Haven) |
| 歩く動き・カメラ | **高い**(等速歩行、視線先行、頭の上下動、被写界深度) | `scripts/walkthrough.py` |
| 「自分」の外見(写真 1 枚から) | **中程度**。2〜5 m 離れた位置なら「本人と分かる」。顔のアップは無料では写実になりません | 下記 A/B |
| 「自分」の外見(動画から) | **高い**(実写そのもの) | 下記 C |

写真 1 枚から映画品質のデジタルダブルを作れる無料ツールは 2026 年時点でも存在しません。
おすすめは **「一人称視点のウォークスルー(アバター無し)」を本編にして、要所だけ三人称で自分を
写す** 構成です。一人称は最も写実的で、最も安く作れます(このリポジトリの初期設定)。

### アバターの作り方(無料)

- **A. 写真 → 3D メッシュ → 自動リグ**(手軽・推奨)
  1. [Hunyuan3D 2.x](https://github.com/Tencent/Hunyuan3D-2) または [TripoSR](https://github.com/VAST-AI-Research/TripoSR)(いずれもオープンソース)に全身写真を入力し、テクスチャ付き `.glb/.obj` を得る(GPU が無い場合は Hugging Face の無料 Space で実行可)。
  2. [Mixamo](https://www.mixamo.com)(Adobe アカウントで無料)にアップロード → Auto-Rigger → アニメーション "Walking" を **In Place にチェック**して FBX でダウンロード。
  3. `examples/assets/me_walking_inplace.fbx` に置き、`config.json` の `avatar.enabled` を `true`。
- **B. MPFB2 で体を作り顔写真を投影**(手間はかかるが調整自由)
  - [MPFB2](https://github.com/makehumancommunity/mpfb2)(GPL、Blender 4.2 以降対応)で体型を合わせ、Blender 標準の Texture Paint「Project from View」で顔写真を投影。リグは MPFB2 の Rigify / Mixamo 互換リグを使用。
- **C. 実写合成**(最も「本人」らしい)
  - 無地の壁の前でスマホで自分が歩く動画を撮影 → [Robust Video Matting](https://github.com/PeterL1n/RobustVideoMatting) や `rembg`(オープンソース)で背景を抜く → Blender でカメラ正面を向く板(ビルボード)に貼って経路上を移動。写真ではなく動画が要りますが、無料で実写品質になります。

## セットアップ(Windows / macOS 共通)

1. **Blender 5.x**(公式ビルド。FFmpeg 同梱なので mp4 出力に追加ソフト不要)
2. **Bonsai** — Blender の *Edit > Preferences > Get Extensions* で "Bonsai" を検索してインストール(IFC 読込)。
3. **blender-mcp**(Claude Code から対話操作したい場合のみ)
   ```bash
   # uv をインストール後
   uvx blender-mcp install-addon      # Blender 側アドオンを配置 → Blender の設定で有効化
   claude mcp add blender -- uvx blender-mcp   # Claude Code に登録(mcp.example.json と同内容)
   ```
   Blender のサイドバー *MCP for Blender > Start MCP Server*(ポート 9876)を押すと、
   Claude Code が Tapir と同じ感覚で Blender に命令できます。
4. **Archicad + Tapir Add-On**(IFC 書き出しを自動化する場合)

## 使い方

```bash
cd blender-automation
# 1. 設定ファイルを複製して経路(waypoints)を自分のモデルの座標に合わせる
copy pipeline\config.example.json pipeline\my_house.json

# 2. Archicad から IFC を書き出し → Blender でシーン構築 → フレーム 1 の静止画で確認
python pipeline/run_pipeline.py --config pipeline/my_house.json --export-ifc --still 1 --save ../renders/check.blend

# 3. 720p の下見動画(EEVEE、数分)
python pipeline/run_pipeline.py --config pipeline/my_house.json --preset preview --render

# 4. 本番 1080p(Cycles。GPU で 1 フレーム 10〜40 秒、CPU では数分/フレーム)
python pipeline/run_pipeline.py --config pipeline/my_house.json --preset final --render
```

`run_preview.bat` / `run_final.bat` は上記 3・4 のショートカットです。
Blender が PATH に無い場合は環境変数 `BLENDER_PATH` に `blender.exe` のフルパスを設定してください。

### 経路(waypoints)の決め方

`camera.waypoints` は **Archicad のプロジェクト座標(m)で、床面上の点** を順番に並べます
(IFC と同じ原点なので、Archicad の座標をそのまま使えます)。Tapir の `GetZoneBoundaries` や
`GetDetailsOfElements` でゾーンや扉の座標を取れば、Claude Code に「玄関→LDK→バルコニー」の
順路を計算させることも可能です。目線高さ・歩行速度・焦点距離は同じ JSON で調整します。

## ファイル構成

```
blender-automation/
  archicad/
    tapir_client.py        Archicad HTTP/JSON クライアント(Examples/aclib の単独版)
    export_ifc.py          Tapir IFCFileOperation(save) で IFC 書き出し
  scripts/                 Blender 内で動く部品(bpy)
    blender_compat.py      4.2 LTS〜5.x の API 差異を吸収(EEVEE の ID、Slotted Action など)
    import_ifc.py          Bonsai で IFC 読込(無ければ glTF/FBX/OBJ/USD にフォールバック)
    walkthrough.py         経路カーブ・カメラ・視線先行・頭の上下動・DoF
    avatar.py              リグ付きアバターの読込、身長合わせ、歩行ループ、経路追従
    lighting.py            Nishita 空 / HDRI、太陽光、天井フィルライト、AgX 色管理
    render_settings.py     preview / eevee_hq / final プリセット、GPU 自動選択、mp4 出力
  pipeline/
    build_walkthrough.py   Blender 内オーケストレータ(JSON → シーン → レンダ)
    run_pipeline.py        外部ドライバ(IFC 書き出し → blender -b 起動)
    config.example.json    設定サンプル
  archicad-addon-cmake/  GRAPHISOFT archicad-addon-cmake 準拠の独立 Add-On「Blender Bridge」
                         (JSON コマンド BlenderBridge.ExportIFC / RunBlenderPipeline + メニュー)。詳細は同フォルダの README
  mcp.example.json         Claude Code 用 blender-mcp 設定
  HANDOVER.md              他セッション・他リポジトリへの引き継ぎ資料
  run_preview.bat / run_final.bat
```

Tapir を入れていない Archicad でも使えるように、`archicad-addon-cmake/` に IFC 書き出しとパイプライン起動だけを
行う小さな Add-On(Blender Bridge)を同梱しています。Tapir がある場合は `archicad/export_ifc.py` だけで十分です。

## 制約・注意

- スクリプトは Blender 4.2 LTS〜5.x の API を対象に書いていますが、**5.2 実機での動作確認は
  まだ行っていません**(この作業環境に Blender が無いため)。初回実行でエラーが出た場合は
  そのログを Claude Code に渡せば修正できます。互換性の差分は `blender_compat.py` に集約しています。
- IFC には表面の色は載りますが、Archicad のテクスチャ画像は原則載りません。写実性を上げるには
  Blender 側で CC0 テクスチャの PBR マテリアルを当てます(材質名→テクスチャの対応表を JSON で
  持たせる拡張を次のステップとして想定)。
- Mixamo は無料ですが Adobe アカウントが必要です。完全オフラインにしたい場合は MPFB2 +
  CMU モーションキャプチャ(BVH、パブリックドメイン)の歩行データを使ってください。
