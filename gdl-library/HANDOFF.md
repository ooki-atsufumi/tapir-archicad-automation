# 引き継ぎ: SG エンジェル GDL オブジェクト

クラウドセッション（claude.ai/code のコンテナ）で作成したもの。
**Archicad 実機での確認ができていない**ため、`/GDL` スキルと Archicad が
両方使えるローカルセッションで仕上げてほしい。

- ブランチ: `claude/gdl-temporary-planning-model-en9dhv`
- 対象: `gdl-library/SG_Angel/`
- 実機環境: Archicad 29 / Tapir 1.5.9 / **ポート 19724**

---

## なぜ引き継ぐか

クラウドコンテナからは以下がすべて届かなかった:

| 必要なもの | クラウド | ローカル |
| --- | --- | --- |
| `localhost:19724` の Archicad | ✗ 到達不可 | ✓ |
| `LP_XMLConverter.exe` | ✗ 無い | ✓ |
| `/GDL` スキルと knowledge base | ✗ 利用不可 | ✓ |
| メーカーサイト gop.co.jp | ✗ プロキシで遮断 | ✓ |

---

## 済んでいること

- 諸元テーブル・派生値計算（`scripts/1d.gdl`）
- 平面シンボル（`scripts/2d.gdl`）／3D（`scripts/3d.gdl`）／選択肢・ロック（`scripts/vl.gdl`）
- パラメータ定義 `libpartdata.xml`（**手書き・未検証**）
- 静的検証のみ実施: `if`/`endif`、`for`/`next`、変換スタックの `DEL` 対応、XML の well-formed
- 起動中の Archicad へ登録するスクリプト `install_to_archicad.py`
  （Tapir の `GetArchicadLocation` → `AddFilesToEmbeddedLibrary` → `ReloadLibraries`）

---

## やってほしいこと

### 1. `libpartdata.xml` を正解形に直す（最優先）

ここが最大の未検証点。`/GDL` の knowledge base にある既存オブジェクト
（花ブロック等）の HSF が**実際に Archicad に通った書式**なので、それに合わせて作り直してほしい。

特に自信が無い箇所:

- ルート要素 `<Symbol>` の属性（`Version` / `Platform` / `SectVersion` の妥当な値）
- `<Ancestry>` を**省略している**。サブタイプ（一般 GDL オブジェクト）の
  MainGUID を入れるべきかどうか
- `<ParamSection SectVersion="36">` の版数
- `<Title>` / `<Boolean>` / `<PenColor>` / `<LineType>` / `<FillPattern>` /
  `<Material>` / `<RealNum>` / `<String>` の要素名と `<Value>` の書式

既存オブジェクトを `LP_XMLConverter libpart2hsf` で HSF に戻せば正解が見られる。

### 2. Archicad に入れて可動確認

```bat
cd gdl-library
python install_to_archicad.py --port 19724
```

確認項目:

- [ ] 平面: 天板外形・設置範囲の破線・手すり・キャスター・文字が出るか
- [ ] 3D: 天板／脚／キャスター／手すり／補助脚の位置関係
- [ ] `iType` を M↔L で切り替えて `hDeck` の選択肢が 700-900 / 1075-1475 に入れ替わるか
- [ ] `hDeck` に範囲外の値を入れて 100mm ピッチの段にスナップするか
- [ ] `nX` / `nY` を増やして連結架台になるか、外周だけに手すりが残るか
- [ ] `gapX` > 0 + `bBridgeX` でブリッジが架かるか
- [ ] `A` / `B` / `ZZYZX` と積算パラメータがロックされ、正しい値になるか
- [ ] `iLOD` 1/2/3 で描画が切り替わるか

### 3. GDL 構文エラーの修正

未検証で特に怪しい箇所:

- `DEFINE STYLE{2} "sgAngelLbl" "Arial", txtSize, 5, 0` の引数並び
- `POLY2_B{2} n, frame_fill, fill_pen, fill_background_pen, ...` の背景ペン 0（透明）指定
- `STR (hD * 1000, 4, 0)` の桁揃え（先頭空白を `STRSUB` で除去している）
- `PARAMETERS` によるマスタースクリプトからの `A`/`B`/`ZZYZX` 上書き
- `ROTy 180 - ASN (dxo / lenO)` による補助脚の向き（幾何は手計算で検証済み）

### 4. 諸元の裏取り

gop.co.jp が遮断されていたため、以下は**推定値**。カタログで確認して `1d.gdl` を修正してほしい。

- SG エンジェル750 の質量（M 25kg / L 30kg と仮置き）
- 長手側 感知フレームの質量（5.5kg と仮置き）
- 部材断面・脚位置・キャスター径・補助脚形状（同社可搬式作業台の一般的構成から起こした）
- ブリッジは「離れを渡す作業床」として一般化。実製品のエンジェルブリッジ
  1000/750・共通ブリッジ1500 の寸法体系とは未対応

規格表 PDF: <https://gop.co.jp/products/img/6d703fde9fab3c1e94ec5e6e708da3e2fa1782cd.pdf>

### 5. 同じブランチに push

`claude/gdl-temporary-planning-model-en9dhv`

---

## 確定している諸元（カタログ公表値）

| 項目 | 値 |
| --- | --- |
| 天板寸法 | 1500 × 1000 / 1500 × 750（SG エンジェル750） |
| 天板高さ M | 700 – 900（伸縮脚 100mm × 2 段） |
| 天板高さ L | 1075 – 1475（伸縮脚 100mm × 4 段） |
| 設置寸法（補助脚含む） | 長さ 1560 × 幅 1010 |
| 質量 | 28 – 33 kg |
| 許容積載荷重 | 単体 190 kg／連結 120 kgf/m² |
| 感知フレーム（短手側） | W1000 × H900（天板より）4.0 kg／W750 3.5 kg |
| 共通ブリッジ1500 | 8 kg（許容 150 kg） |
| エンジェルキーパー | 2.6 kg |
| キャスター | W キャスター（2 輪） |

連結には「足場の組立て等の業務に係る特別教育」修了者が必要。

---

## ローカルセッションに貼る指示文（例）

```
gdl-library/SG_Angel/ にクラウドセッションで作った SG エンジェル（GOP の
移動式室内足場）の GDL オブジェクトが入っている。Archicad 実機での確認が
できていないので仕上げてほしい。

まず gdl-library/HANDOFF.md を読んで。そのうえで:

1. libpartdata.xml が手書きで未検証。/GDL の knowledge base にある既存
   オブジェクトの HSF 書式に合わせて直して。
2. python install_to_archicad.py --port 19724 で Archicad に入れて、
   HANDOFF.md の確認項目を潰して。
3. GDL 構文エラーが出たら直して。
4. 同じブランチに push して。
```
