"""HSF フォルダを .gsm に変換し、起動中の Archicad の埋め込みライブラリに登録する。

Tapir アドオンが読み込まれた Archicad が起動している状態で、このスクリプトを
Archicad と同じ PC 上で実行すること。

    python install_to_archicad.py                # SG_Angel を登録
    python install_to_archicad.py --hsf <path>   # 別の HSF フォルダを登録
    python install_to_archicad.py --keep-gsm out # 変換した .gsm も残す

LP_XMLConverter は起動中の Archicad の場所から自動で探すが、
見つからない場合は --converter で明示できる。
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile

sys.path.insert (0, os.path.join (os.path.dirname (os.path.abspath (__file__)),
                                  '..', 'archicad-addon', 'Examples'))
import aclib


def ParseArgs ():
    parser = argparse.ArgumentParser (
        description = 'HSF オブジェクトを起動中の Archicad の埋め込みライブラリに登録する。')
    parser.add_argument ('--hsf', dest = 'hsf', type = str, default = None,
                         help = 'HSF フォルダ (libpartdata.xml を含む)。既定は ./SG_Angel')
    parser.add_argument ('--converter', dest = 'converter', type = str, default = None,
                         help = 'LP_XMLConverter の実行ファイルパス')
    parser.add_argument ('--name', dest = 'name', type = str, default = None,
                         help = '埋め込みライブラリ内でのファイル名。既定は HSF フォルダ名 + .gsm')
    parser.add_argument ('--type', dest = 'type', type = str, default = 'Object',
                         help = 'ライブラリ部品の種別 (Object / Lamp / Door / Window ...)')
    parser.add_argument ('--keep-gsm', dest = 'keepGsm', type = str, default = None,
                         help = '変換した .gsm を残すフォルダ')
    # --host / --port は aclib 側でも解釈される。ここでは --help に出すために宣言する。
    # Tapir パレットの About ダイアログに実際のポート番号が表示される。
    parser.add_argument ('--host', dest = 'host', type = str, default = 'http://127.0.0.1',
                         help = 'Archicad のホスト (既定: http://127.0.0.1)')
    parser.add_argument ('--port', dest = 'port', type = int, default = 19723,
                         help = 'Tapir のポート番号 (既定: 19723)')
    args, _ = parser.parse_known_args ()
    return args


def FindConverter (explicitPath):
    """LP_XMLConverter を探す。起動中の Archicad の場所を Tapir に問い合わせる。"""
    if explicitPath:
        if not os.path.isfile (explicitPath):
            sys.exit ('指定された LP_XMLConverter が見つからない: {}'.format (explicitPath))
        return explicitPath

    exeName = 'LP_XMLConverter.exe' if os.name == 'nt' else 'LP_XMLConverter'

    result = aclib.RunTapirCommand ('GetArchicadLocation', {}, debug = False)
    if result and 'archicadLocation' in result:
        acLoc = result['archicadLocation']
        # Windows: <dir>\Archicad.exe / macOS: <...>/Archicad NN.app/Contents/MacOS/Archicad
        candidates = [os.path.join (os.path.dirname (acLoc), exeName)]
        if acLoc.endswith ('.app') or '.app' in acLoc:
            appRoot = acLoc.split ('.app')[0] + '.app'
            candidates.append (os.path.join (appRoot, 'Contents', 'MacOS', exeName))
        for c in candidates:
            if os.path.isfile (c):
                return c
        print ('Archicad の場所: {}'.format (acLoc))

    found = shutil.which (exeName)
    if found:
        return found

    sys.exit (
        'LP_XMLConverter が自動で見つからなかった。\n'
        '  Archicad のインストールフォルダ内にあるので、--converter で指定してほしい。\n'
        '  例) --converter "C:\\Program Files\\GRAPHISOFT\\Archicad 28\\LP_XMLConverter.exe"')


def main ():
    args = ParseArgs ()

    here = os.path.dirname (os.path.abspath (__file__))
    hsfDir = os.path.abspath (args.hsf) if args.hsf else os.path.join (here, 'SG_Angel')

    if not os.path.isfile (os.path.join (hsfDir, 'libpartdata.xml')):
        sys.exit ('HSF フォルダに libpartdata.xml が無い: {}'.format (hsfDir))

    gsmName = args.name if args.name else os.path.basename (hsfDir.rstrip (os.sep)) + '.gsm'
    if not gsmName.lower ().endswith ('.gsm'):
        gsmName += '.gsm'

    # --- 1. 接続確認 ---------------------------------------------------------
    try:
        version = aclib.RunTapirCommand ('GetAddOnVersion', {}, debug = False)
    except Exception as e:
        version = None
        print ('接続エラー: {}'.format (e), file = sys.stderr)
    if not version:
        sys.exit ('{}:{} の Tapir アドオンに接続できない。\n'
                  '  ・Archicad が起動しているか\n'
                  '  ・Tapir アドオンが読み込まれているか\n'
                  '  ・ポート番号が合っているか\n'
                  '    （Tapir パレットの ? ボタンで開く About に表示される。\n'
                  '      既定の 19723 でない場合は --port <番号> を付ける）\n'
                  '  ・このスクリプトを Archicad と同じ PC で実行しているか'
                  .format (aclib.host, aclib.port))
    print ('Tapir アドオン: {} ({}:{})'.format (
        version.get ('version', '?'), aclib.host, aclib.port))

    converter = FindConverter (args.converter)
    print ('LP_XMLConverter: {}'.format (converter))

    # --- 2. HSF -> .gsm ------------------------------------------------------
    tmpDir = tempfile.mkdtemp (prefix = 'gdl_')
    gsmPath = os.path.join (tmpDir, gsmName)

    print ('変換中: {} -> {}'.format (hsfDir, gsmName))
    proc = subprocess.run ([converter, 'hsf2libpart', hsfDir, gsmPath],
                           capture_output = True, text = True)
    if proc.stdout.strip ():
        print (proc.stdout.strip ())
    if proc.returncode != 0 or not os.path.isfile (gsmPath):
        print (proc.stderr.strip (), file = sys.stderr)
        shutil.rmtree (tmpDir, ignore_errors = True)
        sys.exit ('LP_XMLConverter による変換に失敗した。\n'
                  '  libpartdata.xml がお使いのバージョンのスキーマと合っていない可能性がある。\n'
                  '  その場合は SG_Angel/README.md の「方法 B」で手動登録してほしい。')

    # --- 3. 埋め込みライブラリへ登録 -----------------------------------------
    print ('埋め込みライブラリへ登録中: {}'.format (gsmName))
    result = aclib.RunTapirCommand ('AddFilesToEmbeddedLibrary', {
        'files': [{
            'inputPath': gsmPath,
            'outputPath': gsmName,
            'type': args.type
        }]
    }, debug = False)

    ok = False
    if result and 'executionResults' in result:
        for execResult in result['executionResults']:
            if execResult.get ('success'):
                ok = True
            else:
                print ('  失敗: {}'.format (execResult.get ('error', {}).get ('message', '?')))

    if args.keepGsm:
        os.makedirs (args.keepGsm, exist_ok = True)
        shutil.copy2 (gsmPath, os.path.join (args.keepGsm, gsmName))
        print ('.gsm を保存した: {}'.format (os.path.join (args.keepGsm, gsmName)))

    shutil.rmtree (tmpDir, ignore_errors = True)

    if not ok:
        sys.exit ('埋め込みライブラリへの登録に失敗した。')

    # --- 4. ライブラリ再読み込み ---------------------------------------------
    print ('ライブラリを再読み込み中...')
    aclib.RunTapirCommand ('ReloadLibraries', {}, debug = False)

    print ('\n完了。オブジェクトツールの「埋め込みライブラリ」に "{}" が入っている。'.format (gsmName))


if __name__ == '__main__':
    main ()
