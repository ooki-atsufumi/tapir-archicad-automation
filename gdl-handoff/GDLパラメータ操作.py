# -*- coding: utf-8 -*-
"""
GDLパラメータ操作.py — Archicad + Tapir アドオンによる GDLパラメータの一覧出力・一括変更

Archicad で選択中の要素(オブジェクト・ドア・窓・ランプ等)の GDLパラメータを
CSV に一覧出力します。--set を付けると一括変更もできます(既定はドライラン)。

必要なもの:
  - Archicad 25 以降 (プロジェクトを開き、対象要素を選択しておく)
  - Tapir アドオン (https://github.com/ENZYME-APD/tapir-archicad-automation/releases/latest)
  - Python 3.8 以降 (追加パッケージ不要。標準ライブラリのみで動作)

使い方:
  python GDLパラメータ操作.py
      … 選択中の要素の全GDLパラメータを CSV に出力(読み取りのみ・安全)

  python GDLパラメータ操作.py --type Object
      … 選択の代わりにプロジェクト内の指定種別(Object/Door/Window/Lamp)を対象にする

  python GDLパラメータ操作.py --set gs_cont_pen=95
      … 変更内容のプレビュー(ドライラン)。まだ何も変更しません

  python GDLパラメータ操作.py --set gs_cont_pen=95 --apply
      … 選択中の要素のパラメータを実際に変更する
        (変更後も Archicad の[元に戻す]で取り消せます)

  そのほか: --outdir 出力先 / --port ポート番号 / --no-pause

コマンド仕様: https://enzyme-apd.github.io/tapir-archicad-automation/archicad-addon
"""

import argparse
import csv
import datetime
import json
import os
import sys
import urllib.error
import urllib.request

CHUNK_SIZE = 100  # 1回のコマンド呼び出しで処理する要素数


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
            'Archicad に接続できませんでした ({}:{})。\n'
            '  - Archicad が起動していてプロジェクトが開いているか確認してください。\n'
            '  - Archicad を複数起動している場合は --port 19724 のように指定してください。\n'
            '  詳細: {}'.format(host, port, e))
    print('Archicad {} (build {}) に接続しました。'.format(
        info.get('version', '?'), info.get('buildNumber', '?')))
    try:
        tapir = conn.run_tapir('GetAddOnVersion')
        print('Tapir アドオン v{} を確認しました。'.format(tapir.get('version', '?')))
    except Exception:
        raise SystemExit(
            'Tapir アドオンが見つかりませんでした。\n'
            '  https://github.com/ENZYME-APD/tapir-archicad-automation/releases/latest から\n'
            '  お使いの Archicad バージョンに合ったファイルをダウンロードし、\n'
            '  [オプション > アドオンマネージャー] からインストールしてください。')
    return conn


def get_target_elements(conn, element_type):
    if element_type:
        elements = conn.run('API.GetElementsByType', {'elementType': element_type}).get('elements', [])
        source = '種別 {} の全要素'.format(element_type)
    else:
        elements = conn.run_tapir('GetSelectedElements').get('elements', [])
        source = '選択中の要素'
    if not elements:
        raise SystemExit(
            '対象要素が0件でした。Archicad でオブジェクト・ドア・窓などを選択してから実行するか、\n'
            '--type Object のように種別を指定してください。')
    print('対象: {} ({} 件)'.format(source, len(elements)))
    return elements


def get_gdl_parameters(conn, elements):
    """要素ごとの GDLパラメータリストを返す。"""
    results = []
    for start in range(0, len(elements), CHUNK_SIZE):
        chunk = elements[start:start + CHUNK_SIZE]
        response = conn.run_tapir('GetGDLParametersOfElements', {'elements': chunk})
        results.extend(response.get('gdlParametersOfElements', []))
        print('  取得中... {}/{} 件'.format(min(start + CHUNK_SIZE, len(elements)), len(elements)))
    return results


def format_value(value):
    if isinstance(value, (list, dict)):
        return json.dumps(value, ensure_ascii=False)
    if isinstance(value, bool):
        return 'true' if value else 'false'
    return value if value is not None else ''


def export_csv(elements, gdl_lists, outdir):
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    path = os.path.join(outdir, 'GDLパラメータ一覧_{}.csv'.format(stamp))
    header = ['要素No', 'GUID', 'パラメータ名', '型', '値', 'ロック', '配列(次元1)', '配列(次元2)']
    count = 0
    with open(path, 'w', newline='', encoding='utf-8-sig') as f:
        writer = csv.writer(f)
        writer.writerow(header)
        for i, (element, entry) in enumerate(zip(elements, gdl_lists)):
            guid = element.get('elementId', {}).get('guid', '')
            if not isinstance(entry, dict) or 'parameters' not in entry:
                writer.writerow([i + 1, guid, '(GDLパラメータなし、または取得エラー)', '', '', '', '', ''])
                continue
            for param in entry['parameters']:
                writer.writerow([
                    i + 1, guid,
                    param.get('name', ''),
                    param.get('type', ''),
                    format_value(param.get('value')),
                    'ロック' if param.get('isLocked') else '',
                    param.get('dimension1', ''),
                    param.get('dimension2', ''),
                ])
                count += 1
    print('出力しました: {} (パラメータ {} 行)'.format(path, count))
    return path


def parse_set_argument(set_arg):
    """--set name=value を (名前, 値) に分解する。値はJSONとして解釈を試みる。"""
    if '=' not in set_arg:
        raise SystemExit('--set は パラメータ名=値 の形式で指定してください。例: --set gs_cont_pen=95')
    name, raw = set_arg.split('=', 1)
    name = name.strip()
    raw = raw.strip()
    try:
        value = json.loads(raw)  # 数値・true/false・"文字列"・[配列] を解釈
    except ValueError:
        value = raw              # そのまま文字列として扱う
    return name, value


def set_parameter(conn, elements, gdl_lists, name, value, apply_changes):
    # 対象パラメータを持つ要素だけに絞り込む(ロック中は除外)
    targets = []
    for element, entry in zip(elements, gdl_lists):
        params = entry.get('parameters', []) if isinstance(entry, dict) else []
        match = next((p for p in params if p.get('name') == name), None)
        if match is None:
            continue
        if match.get('isLocked'):
            print('  スキップ: {} はロック中 (GUID {})'.format(
                name, element.get('elementId', {}).get('guid', '')[:8]))
            continue
        targets.append((element, match.get('value')))

    if not targets:
        raise SystemExit('パラメータ「{}」を持つ変更可能な要素がありませんでした。\n'
                         'CSV一覧(引数なし実行)で正しいパラメータ名を確認してください。'.format(name))

    print()
    print('変更内容: パラメータ「{}」 → {}'.format(name, format_value(value)))
    for element, current in targets[:10]:
        print('  GUID {}...: 現在値 {} → 新しい値 {}'.format(
            element.get('elementId', {}).get('guid', '')[:8],
            format_value(current), format_value(value)))
    if len(targets) > 10:
        print('  ... ほか {} 件'.format(len(targets) - 10))

    if not apply_changes:
        print()
        print('※ ドライランのため変更していません。実際に変更するには --apply を付けて再実行してください。')
        return

    response = conn.run_tapir('SetGDLParametersOfElements', {
        'elementsWithGDLParameters': [{
            'elementId': element['elementId'],
            'gdlParameters': [{'name': name, 'value': value}],
        } for element, _ in targets]})
    results = response.get('executionResults', [])
    ok = sum(1 for r in results if isinstance(r, dict) and r.get('success'))
    failed = len(results) - ok
    print('変更しました: 成功 {} 件 / 失敗 {} 件'.format(ok, failed))
    for r in results:
        if isinstance(r, dict) and not r.get('success'):
            print('  失敗詳細: {}'.format(json.dumps(r.get('error', r), ensure_ascii=False)))
    print('※ Archicad 側の [編集 > 元に戻す] でこの変更は取り消せます。')


def main():
    parser = argparse.ArgumentParser(description='Archicad + Tapir による GDLパラメータの一覧出力・一括変更')
    parser.add_argument('--host', default='http://127.0.0.1')
    parser.add_argument('--port', type=int, default=19723)
    parser.add_argument('--type', dest='element_type', default=None,
                        help='選択の代わりに要素種別で対象指定 (Object / Door / Window / Lamp など)')
    parser.add_argument('--set', dest='set_arg', default=None, metavar='名前=値',
                        help='パラメータを一括変更する (例: --set gs_cont_pen=95)。既定はドライラン')
    parser.add_argument('--apply', action='store_true', help='--set の変更を実際に適用する')
    parser.add_argument('--outdir', default=None, help='CSVの出力先フォルダ (省略時はスクリプトと同じ場所)')
    parser.add_argument('--no-pause', action='store_true', help='終了時にEnter入力を待たない')
    args, _ = parser.parse_known_args()

    conn = connect(args.host, args.port)
    elements = get_target_elements(conn, args.element_type)
    gdl_lists = get_gdl_parameters(conn, elements)

    outdir = args.outdir or os.path.dirname(os.path.abspath(__file__))
    os.makedirs(outdir, exist_ok=True)
    export_csv(elements, gdl_lists, outdir)

    if args.set_arg:
        name, value = parse_set_argument(args.set_arg)
        set_parameter(conn, elements, gdl_lists, name, value, args.apply)
    else:
        print('ヒント: --set パラメータ名=値 で一括変更のプレビュー、さらに --apply で適用できます。')

    print('完了しました。')


if __name__ == '__main__':
    pause = '--no-pause' not in sys.argv
    try:
        main()
        exit_code = 0
    except SystemExit as e:
        if e.code not in (0, None):
            print('\nエラー: {}'.format(e))
        exit_code = 0 if e.code in (0, None) else 1
    except Exception as e:
        print('\n予期しないエラーが発生しました: {}'.format(e))
        exit_code = 1
    if pause:
        try:
            input('\nEnterキーを押すと終了します...')
        except EOFError:
            pass
    sys.exit(exit_code)
