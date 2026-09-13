# Blender Bridge — archicad-addon-cmake ベースの独立 Add-On

[GRAPHISOFT/archicad-addon-cmake](https://github.com/GRAPHISOFT/archicad-addon-cmake) テンプレートを
そのまま踏襲した、**Tapir 本体に依存しない小さな Archicad Add-On** です。
Blender ウォークスルー動画パイプライン(`../pipeline/`)を Archicad 側から起動するための
最小限の機能だけを持ちます。Tapir をインストールしていない環境でも動きます。

| 種類 | 名前 | 内容 |
|---|---|---|
| JSON コマンド | `BlenderBridge.GetProjectPath` | 開いているプロジェクトのフォルダ・名前と、推奨 IFC パス(`<名前>_blender.ifc`)を返す |
| JSON コマンド | `BlenderBridge.ExportIFC` | `ifcFilePath` に IFC を保存(Tapir の `IFCFileOperation save` と同じ仕組み) |
| JSON コマンド | `BlenderBridge.RunBlenderPipeline` | 任意のプログラム(既定 `python`)を Archicad から切り離して起動し、PID を返す |
| メニュー | Blender Bridge > Export IFC for Blender | .pln と同じフォルダに `<名前>_blender.ifc` を書き出す |
| メニュー | Blender Bridge > Run Blender pipeline script | .pln と同じフォルダの `blender_bridge.bat` / `.sh` を IFC パス付きで実行 |

## ビルド

```bash
git clone --recurse-submodules https://github.com/ooki-atsufumi/tapir-archicad-automation
cd tapir-archicad-automation/blender-automation/archicad-addon-cmake
# Windows(Visual Studio 2022 以降 + CMake 3.19+ + Python 3.10+)
build.bat 29          # -> Build/ 以下に BlenderBridge.apx
# macOS(Xcode)
./build.sh 29
```

`Tools/BuildAddOn.py`(サブモジュール archicad-addon-cmake-tools)が DevKit を自動ダウンロードします。
サブモジュールが空の場合は `git submodule update --init --recursive` を実行してください。
CI: `.github/workflows/blender_bridge_build_check.yml` が Windows AC27〜29 / macOS AC29 をビルドします。

インストールは Tapir と同じです: *オプション > アドオンマネージャー > 利用可能なアドオンリストを編集 > 追加*。

## Claude Code / Python からの呼び方

Tapir と同じ `API.ExecuteAddOnCommand` で、名前空間だけ `BlenderBridge` に変えます。

```python
import sys; sys.path.insert(0, '../archicad')
from tapir_client import TapirClient
c = TapirClient()
def bridge(name, params=None):
    r = c.run('API.ExecuteAddOnCommand', {
        'addOnCommandId': {'commandNamespace': 'BlenderBridge', 'commandName': name},
        'addOnCommandParameters': params or {}})
    return r['addOnCommandResponse']

info = bridge('GetProjectPath')
bridge('ExportIFC', {'ifcFilePath': info['suggestedIfcPath']})
bridge('RunBlenderPipeline', {
    'workingDirectory': r'C:\work\tapir-archicad-automation\blender-automation',
    'arguments': ['pipeline/run_pipeline.py', '--config', 'pipeline/my_house.json', '--preset', 'preview', '--render']})
```

## ファイル

```
config.json                 Add-On 名 / バージョン / 言語(INT, JPN)。ADDON_NAME, ADDON_VERSION マクロの元
CMakeLists.txt              テンプレートと同一構成(PCH のパスのみ変更)
Tools/                      archicad-addon-cmake-tools サブモジュール(CMakeCommon.cmake, BuildAddOn.py)
Src/AddOnMain.cpp           メニュー登録・ハンドラ、JSON コマンド登録(RegisterCommand<T>)
Src/BlenderBridgeCommands.* API_AddOnCommand 派生の 3 コマンド + IFC 書き出し共通関数
Src/ProcessLauncher.*       CreateProcessW(Win)/ posix_spawnp(mac)で非同期起動
Src/MigrationHelper.hpp     AC25/26 向けの API 名マッピング(Tapir と同じ手法)
RFIX/AddOnFix.grc           MDID(開発者 ID — 自分のものに差し替える)、アイコン
RINT/ RJPN/ AddOn.grc       文字列リソース(英語 / 日本語)
blender_bridge.example.*    .pln の隣に置く起動スクリプトの例
```

## 注意

- **MDID**(`RFIX/AddOnFix.grc`)はプレースホルダです。[archicadapi.graphisoft.com](https://archicadapi.graphisoft.com)
  で無料の開発者 ID を取得して差し替えてください(他の Add-On と重複すると読み込まれません)。
- このコードは Tapir の実装(IFCFileOperation、CommandBase、MigrationHelper)から API の使い方を写して
  書いていますが、**この作業環境ではコンパイルしていません**(DevKit は Windows/macOS 専用)。
  初回ビルドのエラーは CI ログまたはローカルのログを Claude Code に渡してください。
