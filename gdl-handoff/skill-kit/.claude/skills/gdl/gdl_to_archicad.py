# -*- coding: utf-8 -*-
"""
gdl_to_archicad.py — GDLオブジェクト(XML)を Archicad に自動投入するヘルパー

/gdl スキル(Claude Code)から呼び出される想定ですが、単体でも使えます。
Python 標準ライブラリのみで動作します(pip不要)。

サブコマンド:
  check                       接続・Tapir・LP_XMLConverter の存在確認
  build --xml F --name N      XML→gsm変換→埋め込みライブラリ登録→再読込→配置
        [--x 0 --y 0 --z 0] [--no-place] [--type Object] [--converter PATH]
  template --seed F --out F   既存の .gsm から XMLテンプレートを逆生成(libpart2xml)

共通オプション: --host http://127.0.0.1  --port 19723
"""

import argparse
import glob
import json
import os
import subprocess
import sys
import urllib.error
import urllib.request


# ------------------------------------------------------------
# Archicad JSON インターフェースとの通信
# ------------------------------------------------------------

class ArchicadConnection:
    def __init__(self, host, port):
        self.url = '{}:{}'.format(host, port)

    def run(self, command, parameters=None):
        request = urllib.request.Request(self.url)
        request.add_header('Content-Type', 'application/json')
        body = json.dumps({'command': command, 'parameters': parameters or {}}).encode('utf8')
        response = json.loads(urllib.request.urlopen(request, body).read())
        if not response.get('succeeded'):
            error = response.get('error', {})
            raise RuntimeError('コマンド {} が失敗しました: {}'.format(
                command, error.get('message', json.dumps(error, ensure_ascii=False))))
        return response.get('result', {})

    def run_tapir(self, command, parameters=None):
        result = self.run('API.ExecuteAddOnCommand', {
            'addOnCommandId': {
                'commandNamespace': 'TapirCommand',
                'commandName': command,
            },
            'addOnCommandParameters': parameters or {},
        })
        return result.get('addOnCommandResponse', {})


def connect(host, port):
    conn = ArchicadConnection(host, port)
    try:
        info = conn.run('API.GetProductInfo')
    except urllib.error.URLError as e:
        raise SystemExit(
            'NG: Archicad に接続できません ({}:{})。Archicad を起動しプロジェクトを開いてください。\n'
            '    複数起動時は --port 19724 などを指定。詳細: {}'.format(host, port, e))
    print('OK: Archicad {} (build {}) に接続'.format(info.get('version', '?'), info.get('buildNumber', '?')))
    try:
        tapir = conn.run_tapir('GetAddOnVersion')
        print('OK: Tapir アドオン v{}'.format(tapir.get('version', '?')))
    except Exception:
        raise SystemExit(
            'NG: Tapir アドオンが見つかりません。\n'
            '    https://github.com/ENZYME-APD/tapir-archicad-automation/releases/latest から\n'
            '    Archicad のバージョンに合うファイルを入れてください([オプション>アドオンマネージャー])。')
    return conn


# ------------------------------------------------------------
# LP_XMLConverter の探索と実行
# ------------------------------------------------------------

def find_converter(conn, override=None):
    """LP_XMLConverter 実行ファイルを探す。Archicad の場所は Tapir から取得。"""
    if override:
        if os.path.isfile(override):
            return override
        raise SystemExit('NG: 指定された converter が見つかりません: {}'.format(override))

    archicad_location = conn.run_tapir('GetArchicadLocation').get('archicadLocation', '')
    print('    Archicad の場所: {}'.format(archicad_location))

    candidates = []
    exe_name = 'LP_XMLConverter.exe' if os.name == 'nt' else 'LP_XMLConverter'

    search_roots = []
    location_dir = os.path.dirname(archicad_location) if archicad_location else ''
    if location_dir:
        search_roots.append(location_dir)                    # ARCHICAD.exe と同じフォルダ (Windows)
        search_roots.append(os.path.dirname(location_dir))   # 1つ上
        search_roots.append(os.path.dirname(os.path.dirname(location_dir)))  # 2つ上 (Mac .app 内対応)
    if archicad_location and archicad_location.endswith('.app'):
        search_roots.append(os.path.join(archicad_location, 'Contents', 'MacOS'))
        search_roots.append(os.path.dirname(archicad_location))

    seen = set()
    for root in search_roots:
        if not root or root in seen or not os.path.isdir(root):
            continue
        seen.add(root)
        candidates.extend(glob.glob(os.path.join(root, exe_name)))
        candidates.extend(glob.glob(os.path.join(root, '*', exe_name)))
        candidates.extend(glob.glob(os.path.join(root, 'LP_XMLConverter*', 'Contents', 'MacOS', exe_name)))
        candidates.extend(glob.glob(os.path.join(root, '*.app', 'Contents', 'MacOS', exe_name)))

    for path in candidates:
        if os.path.isfile(path):
            return path

    raise SystemExit(
        'NG: LP_XMLConverter が見つかりませんでした。\n'
        '    Archicad のインストールフォルダ内 (例: C:\\Program Files\\Graphisoft\\Archicad 29) を\n'
        '    エクスプローラーで "LP_XMLConverter" と検索し、見つかったパスを --converter で指定してください。')


def run_converter(converter, args_list):
    command = [converter] + args_list
    print('    実行: {}'.format(' '.join(command)))
    result = subprocess.run(command, capture_output=True, text=True)
    if result.stdout:
        print(result.stdout.strip())
    if result.returncode != 0:
        if result.stderr:
            print(result.stderr.strip())
        raise SystemExit('NG: LP_XMLConverter がエラーで終了しました (終了コード {})。\n'
                         '    上記のエラーメッセージを確認して XML を修正してください。'.format(result.returncode))


# ------------------------------------------------------------
# サブコマンド
# ------------------------------------------------------------

def cmd_check(args):
    conn = connect(args.host, args.port)
    converter = find_converter(conn, args.converter)
    print('OK: LP_XMLConverter: {}'.format(converter))
    print('準備完了です。')


def cmd_build(args):
    if not os.path.isfile(args.xml):
        raise SystemExit('NG: XMLファイルが見つかりません: {}'.format(args.xml))
    conn = connect(args.host, args.port)
    converter = find_converter(conn, args.converter)

    # 1) XML → gsm 変換
    outdir = os.path.join(os.path.dirname(os.path.abspath(args.xml)), 'build')
    os.makedirs(outdir, exist_ok=True)
    gsm_path = os.path.join(outdir, args.name + '.gsm')
    if os.path.isfile(gsm_path):
        os.remove(gsm_path)
    run_converter(converter, ['xml2libpart', os.path.abspath(args.xml), gsm_path])
    if not os.path.isfile(gsm_path):
        raise SystemExit('NG: 変換後の gsm が生成されませんでした。LP_XMLConverter の出力を確認してください。')
    print('OK: gsm 生成: {}'.format(gsm_path))

    # 2) 埋め込みライブラリへ登録
    response = conn.run_tapir('AddFilesToEmbeddedLibrary', {'files': [{
        'inputPath': gsm_path,
        'outputPath': 'Claude GDL/{}.gsm'.format(args.name),
        'type': args.type,
    }]})
    results = response.get('executionResults', [])
    if results and isinstance(results[0], dict) and not results[0].get('success'):
        raise SystemExit(
            'NG: 埋め込みライブラリへの登録に失敗: {}\n'
            '    同名オブジェクトが既にある場合は --name を変えて(例: {}_v2)再実行してください。'.format(
                json.dumps(results[0].get('error', results[0]), ensure_ascii=False), args.name))
    print('OK: 埋め込みライブラリに登録 (Claude GDL/{}.gsm)'.format(args.name))

    # 3) ライブラリ再読込
    conn.run_tapir('ReloadLibraries')
    print('OK: ライブラリを再読込')

    # 4) 配置
    if args.no_place:
        print('完了: 配置はスキップしました。オブジェクトツールから「{}」を選んで配置できます。'.format(args.name))
        return
    response = conn.run_tapir('CreateObjects', {'objectsData': [{
        'libraryPartName': args.name,
        'coordinates': {'x': args.x, 'y': args.y, 'z': args.z},
    }]})
    print('OK: 配置しました (x={}, y={}, z={})'.format(args.x, args.y, args.z))
    print(json.dumps(response, ensure_ascii=False))
    print('完了: Archicad の平面図/3Dで「{}」を確認してください。不要になったら要素を選択して削除、'
          '変更は[元に戻す]で取り消せます。'.format(args.name))


def cmd_template(args):
    if not os.path.isfile(args.seed):
        raise SystemExit('NG: シードファイルが見つかりません: {}'.format(args.seed))
    conn = connect(args.host, args.port)
    converter = find_converter(conn, args.converter)
    run_converter(converter, ['libpart2xml', os.path.abspath(args.seed), os.path.abspath(args.out)])
    print('OK: テンプレートXMLを書き出しました: {}'.format(args.out))
    print('    この構造(Symbol属性・SectVersion・Ancestry)を流用し、スクリプトとパラメータを差し替えてください。')


def main():
    parser = argparse.ArgumentParser(description='GDLオブジェクト(XML)を Archicad に自動投入する')
    parser.add_argument('--host', default='http://127.0.0.1')
    parser.add_argument('--port', type=int, default=19723)
    parser.add_argument('--converter', default=None, help='LP_XMLConverter のパスを直接指定')
    sub = parser.add_subparsers(dest='subcommand')

    sub.add_parser('check', help='接続・Tapir・LP_XMLConverter の存在確認')

    p_build = sub.add_parser('build', help='XML→gsm→埋め込みライブラリ→配置')
    p_build.add_argument('--xml', required=True, help='ライブラリ部品XMLのパス')
    p_build.add_argument('--name', required=True, help='オブジェクト名 (ライブラリ部品名)')
    p_build.add_argument('--type', default='Object', help='部品種別 (Object/Door/Window/Lamp など)')
    p_build.add_argument('--x', type=float, default=0.0)
    p_build.add_argument('--y', type=float, default=0.0)
    p_build.add_argument('--z', type=float, default=0.0)
    p_build.add_argument('--no-place', action='store_true', help='配置せず登録まで行う')

    p_template = sub.add_parser('template', help='既存gsmからテンプレートXMLを逆生成')
    p_template.add_argument('--seed', required=True, help='元にする .gsm ファイル')
    p_template.add_argument('--out', required=True, help='出力するXMLパス')

    args = parser.parse_args()
    if args.subcommand == 'check':
        cmd_check(args)
    elif args.subcommand == 'build':
        cmd_build(args)
    elif args.subcommand == 'template':
        cmd_template(args)
    else:
        parser.print_help()
        sys.exit(1)


if __name__ == '__main__':
    try:
        main()
    except SystemExit as e:
        if e.code not in (0, None):
            print('\n{}'.format(e))
            sys.exit(1)
        raise
    except Exception as e:
        print('\n予期しないエラー: {}'.format(e))
        sys.exit(1)
