# -*- coding: utf-8 -*-
"""
BIM自動積算.py — Archicad + Tapir アドオンによる数量自動集計スクリプト

Archicad で開いているプロジェクトの全要素(または選択中の要素)から
数量(高さ・幅・厚み・長さ・面積・体積など)を取り出し、
「明細CSV」と「集計CSV」の2ファイルを出力します。
単価表(UNIT_PRICES)を設定すれば概算金額も自動計算します。

必要なもの:
  - Archicad 25 以降 (プロジェクトを開いた状態で起動しておく)
  - Tapir アドオン (https://github.com/ENZYME-APD/tapir-archicad-automation/releases/latest)
  - Python 3.8 以降 (追加パッケージのインストールは不要。標準ライブラリのみで動作)

使い方:
  python BIM自動積算.py                 ... プロジェクト内の全要素を集計
  python BIM自動積算.py --selected      ... Archicad で選択中の要素だけを集計
  python BIM自動積算.py --outdir C:\\out ... CSV の出力先フォルダを指定
  python BIM自動積算.py --port 19724    ... Archicad を複数起動している場合のポート指定

出力 (スクリプトと同じフォルダ、または --outdir):
  BIM積算_明細_YYYYMMDD-HHMMSS.csv ... 1要素=1行の明細
  BIM積算_集計_YYYYMMDD-HHMMSS.csv ... 要素種別×階ごとの合計と種別合計

このスクリプトは Archicad の JSON インターフェース (http://127.0.0.1:19723) と
Tapir アドオンの追加コマンド (TapirCommand) を使用します。
コマンド一覧: https://enzyme-apd.github.io/tapir-archicad-automation/archicad-addon
"""

import argparse
import csv
import datetime
import json
import os
import re
import sys
import urllib.error
import urllib.request

# ============================================================
# ▼ 単価表 (カスタマイズ箇所)
#    キー   : 要素種別の日本語名 (明細CSVの「種別」列と同じ文字列)
#    値     : (基準, 単価[円])
#    基準   : '体積' (円/m3) / '面積' (円/m2) / '長さ' (円/m) / '個' (円/個)
#    設定した種別だけ金額が計算されます。例を外して使ってください。
# ============================================================
UNIT_PRICES = {
    # '壁':     ('体積', 45000),
    # 'スラブ': ('体積', 38000),
    # '柱':     ('体積', 52000),
    # '梁':     ('体積', 50000),
    # 'ドア':   ('個',   80000),
    # '窓':     ('個',   60000),
}

# 要素種別の英語名 → 日本語名 (未定義の種別は英語名のまま表示されます)
TYPE_NAMES_JA = {
    'Wall': '壁',
    'Column': '柱',
    'Beam': '梁',
    'Slab': 'スラブ',
    'Roof': '屋根',
    'Shell': 'シェル',
    'Mesh': 'メッシュ(盛土)',
    'Zone': 'ゾーン',
    'CurtainWall': 'カーテンウォール',
    'CurtainWallSegment': 'CWセグメント',
    'CurtainWallFrame': 'CWフレーム',
    'CurtainWallPanel': 'CWパネル',
    'CurtainWallJunction': 'CWジャンクション',
    'CurtainWallAccessory': 'CWアクセサリ',
    'Stair': '階段',
    'Riser': '蹴上',
    'Tread': '踏面',
    'StairStructure': '階段構造',
    'Railing': '手すり',
    'Door': 'ドア',
    'Window': '窓',
    'Skylight': '天窓',
    'Opening': '開口',
    'Object': 'オブジェクト',
    'Lamp': 'ランプ',
    'Morph': 'モルフ',
    'Hotlink': 'ホットリンク',
}

# 数量列と、その値を取り出す組み込みプロパティ名の候補 (存在するものだけ使用)
COLUMN_CANDIDATES = [
    ('ElementID', ['General_ElementID'], None),
    ('高さ(m)',   ['General_Height'],    None),
    ('幅(m)',     ['General_Width'],     None),
    ('厚み(m)',   ['General_Thickness'], None),
    ('長さ(m)',   ['General_Length', 'General_1DLength'],
                  r'^General_.*Length$'),
    ('面積(m2)',  ['General_Area', 'General_NetArea', 'General_ReferenceArea'],
                  r'^General_.*(?<!Surface)Area$'),
    ('体積(m3)',  ['General_Volume', 'General_NetVolume', 'General_GrossVolume'],
                  r'^General_.*Volume$'),
]

QTY_BASIS_COLUMN = {'長さ': '長さ(m)', '面積': '面積(m2)', '体積': '体積(m3)'}

CHUNK_SIZE = 300  # 1回のコマンド呼び出しで処理する要素数


# ------------------------------------------------------------
# Archicad JSON インターフェースとの通信
# ------------------------------------------------------------

class ArchicadConnection:
    def __init__(self, host, port):
        self.url = '{}:{}'.format(host, port)

    def run(self, command, parameters=None):
        """Archicad 公式 JSON コマンドを実行する。"""
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
        """Tapir アドオンの追加コマンドを実行する。"""
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
            '  - Archicad を複数起動している場合は --port 19724 のように指定してください\n'
            '    (ポートは 19723 から順に使われます)。\n'
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


# ------------------------------------------------------------
# データ取得
# ------------------------------------------------------------

def get_elements(conn, selected_only):
    command = 'GetSelectedElements' if selected_only else 'GetAllElements'
    elements = conn.run_tapir(command).get('elements', [])
    if not elements:
        raise SystemExit('対象要素が0件でした。' +
                         (' 要素を選択してから実行してください。' if selected_only else ''))
    print('対象要素: {} 件 ({})'.format(len(elements), '選択要素のみ' if selected_only else '全要素'))
    return elements


def get_details(conn, elements):
    details = []
    for start in range(0, len(elements), CHUNK_SIZE):
        chunk = elements[start:start + CHUNK_SIZE]
        response = conn.run_tapir('GetDetailsOfElements', {'elements': chunk})
        details.extend(response.get('detailsOfElements', []))
    return details


def get_story_names(conn):
    """階インデックス → 階名 の辞書を返す。"""
    names = {}
    try:
        for story in conn.run_tapir('GetStories').get('stories', []):
            index = story.get('index')
            name = story.get('name') or ''
            names[index] = name if name else '{}F'.format(index)
    except Exception:
        pass
    return names


def resolve_quantity_properties(conn):
    """存在する組み込みプロパティから数量列を決定し、[(列名, propertyId), ...] を返す。"""
    available = set()
    for prop in conn.run('API.GetAllPropertyNames').get('properties', []):
        if prop.get('type') == 'BuiltIn':
            available.add(prop.get('nonLocalizedName'))

    chosen = []  # (列名, nonLocalizedName)
    for column, candidates, fallback_pattern in COLUMN_CANDIDATES:
        name = next((c for c in candidates if c in available), None)
        if name is None and fallback_pattern:
            regex = re.compile(fallback_pattern)
            name = next((a for a in sorted(available) if regex.match(a)), None)
        if name:
            chosen.append((column, name))
        else:
            print('注意: 列「{}」に対応する組み込みプロパティが見つからないためスキップします。'.format(column))

    if not chosen:
        raise SystemExit('数量用の組み込みプロパティが1つも見つかりませんでした。')

    response = conn.run('API.GetPropertyIds', {
        'properties': [{'type': 'BuiltIn', 'nonLocalizedName': n} for _, n in chosen]})
    columns = []
    for (column, name), entry in zip(chosen, response.get('properties', [])):
        if 'propertyId' in entry:
            columns.append((column, {'propertyId': entry['propertyId']}))
        else:
            print('注意: プロパティ {} のIDを取得できなかったため列「{}」をスキップします。'.format(name, column))
    return columns


def get_property_values(conn, elements, columns):
    """要素ごとのプロパティ値を [{列名: 値}] で返す。"""
    property_ids = [pid for _, pid in columns]
    values_per_element = []
    for start in range(0, len(elements), CHUNK_SIZE):
        chunk = elements[start:start + CHUNK_SIZE]
        response = conn.run_tapir('GetPropertyValuesOfElements', {
            'elements': chunk, 'properties': property_ids})
        for entry in response.get('propertyValuesForElements', []):
            row = {}
            for (column, _), value_entry in zip(columns, entry.get('propertyValues', [])):
                row[column] = extract_value(value_entry)
            values_per_element.append(row)
        done = min(start + CHUNK_SIZE, len(elements))
        print('  数量取得中... {}/{} 件'.format(done, len(elements)))
    return values_per_element


def extract_value(value_entry):
    if not isinstance(value_entry, dict) or 'propertyValue' not in value_entry:
        return None
    pv = value_entry['propertyValue']
    if pv.get('status', 'normal') != 'normal':
        return None
    return pv.get('value')


# ------------------------------------------------------------
# 集計・出力
# ------------------------------------------------------------

def as_number(value):
    return value if isinstance(value, (int, float)) and not isinstance(value, bool) else None


def calc_price(type_ja, row_values, count=1):
    """単価表に基づいて金額を計算する。設定がなければ None。"""
    if type_ja not in UNIT_PRICES:
        return None
    basis, unit_price = UNIT_PRICES[type_ja]
    if basis == '個':
        return count * unit_price
    column = QTY_BASIS_COLUMN.get(basis)
    quantity = as_number(row_values.get(column)) if column else None
    return quantity * unit_price if quantity is not None else None


def format_number(value, digits=4):
    number = as_number(value)
    if number is None:
        return '' if value is None else value
    return int(round(number)) if digits == 0 else round(number, digits)

def write_csv(path, header, rows):
    with open(path, 'w', newline='', encoding='utf-8-sig') as f:
        writer = csv.writer(f)
        writer.writerow(header)
        writer.writerows(rows)
    print('出力しました: {}'.format(path))


def main():
    parser = argparse.ArgumentParser(description='Archicad + Tapir による数量自動集計')
    parser.add_argument('--host', default='http://127.0.0.1')
    parser.add_argument('--port', type=int, default=19723)
    parser.add_argument('--selected', action='store_true', help='選択中の要素だけを集計する')
    parser.add_argument('--outdir', default=None, help='CSVの出力先フォルダ (省略時はスクリプトと同じ場所)')
    parser.add_argument('--no-pause', action='store_true', help='終了時にEnter入力を待たない')
    args, _ = parser.parse_known_args()

    conn = connect(args.host, args.port)
    elements = get_elements(conn, args.selected)
    details = get_details(conn, elements)
    story_names = get_story_names(conn)
    columns = resolve_quantity_properties(conn)
    quantity_columns = [c for c, _ in columns if c != 'ElementID']
    values = get_property_values(conn, elements, columns)

    # ---- 明細行の作成 ----
    detail_header = ['No', '種別', '種別(英語)', 'ElementID', '階', 'GUID'] + quantity_columns + ['金額(円)']
    detail_rows = []
    summary = {}  # (種別, 階) -> {'count': n, '<列>': 合計, '金額': 合計}
    for i, element in enumerate(elements):
        guid = element.get('elementId', {}).get('guid', '')
        detail = details[i] if i < len(details) and isinstance(details[i], dict) else {}
        type_en = detail.get('type', '(不明)')
        type_ja = TYPE_NAMES_JA.get(type_en, type_en)
        floor_index = detail.get('floorIndex')
        story = story_names.get(floor_index, '' if floor_index is None else str(floor_index))
        row_values = values[i] if i < len(values) else {}
        price = calc_price(type_ja, row_values)

        detail_rows.append(
            [i + 1, type_ja, type_en, row_values.get('ElementID', '') or '', story, guid]
            + [format_number(row_values.get(c)) for c in quantity_columns]
            + [format_number(price, 0)])

        key = (type_ja, story)
        bucket = summary.setdefault(key, {'count': 0, '金額': 0.0, '金額あり': False})
        bucket['count'] += 1
        for column in quantity_columns:
            number = as_number(row_values.get(column))
            if number is not None:
                bucket[column] = bucket.get(column, 0.0) + number
        if price is not None:
            bucket['金額'] += price
            bucket['金額あり'] = True

    # ---- 集計行の作成 (種別×階 + 種別合計) ----
    summary_header = ['種別', '階', '個数'] + ['{} 合計'.format(c) for c in quantity_columns] + ['金額(円) 合計']
    summary_rows = []
    for type_ja in sorted({k[0] for k in summary}):
        type_total = {'count': 0, '金額': 0.0, '金額あり': False}
        keys = sorted((k for k in summary if k[0] == type_ja), key=lambda k: k[1])
        for key in keys:
            bucket = summary[key]
            summary_rows.append(
                [type_ja, key[1], bucket['count']]
                + [format_number(bucket.get(c), 3) for c in quantity_columns]
                + [format_number(bucket['金額'], 0) if bucket['金額あり'] else ''])
            type_total['count'] += bucket['count']
            for column in quantity_columns:
                if column in bucket:
                    type_total[column] = type_total.get(column, 0.0) + bucket[column]
            if bucket['金額あり']:
                type_total['金額'] += bucket['金額']
                type_total['金額あり'] = True
        summary_rows.append(
            [type_ja, '★合計', type_total['count']]
            + [format_number(type_total.get(c), 3) for c in quantity_columns]
            + [format_number(type_total['金額'], 0) if type_total['金額あり'] else ''])

    # ---- 出力 ----
    outdir = args.outdir or os.path.dirname(os.path.abspath(__file__))
    os.makedirs(outdir, exist_ok=True)
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    write_csv(os.path.join(outdir, 'BIM積算_明細_{}.csv'.format(stamp)), detail_header, detail_rows)
    write_csv(os.path.join(outdir, 'BIM積算_集計_{}.csv'.format(stamp)), summary_header, summary_rows)
    print('完了しました。要素 {} 件 / 種別 {} 種を集計しました。'.format(
        len(elements), len({k[0] for k in summary})))
    if not UNIT_PRICES:
        print('ヒント: スクリプト冒頭の UNIT_PRICES に単価を設定すると概算金額も計算されます。')


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
