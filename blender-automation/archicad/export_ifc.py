"""Export the currently opened Archicad project to IFC through the Tapir Add-On.

Usage (Archicad must be running with the Tapir Add-On loaded):
    python export_ifc.py --out C:/work/model.ifc [--port 19723] [--file-type ifc|ifczip]

The IFC translator used is the one currently selected in Archicad's
File > Interoperability > IFC > IFC Translators dialog. For Blender import choose a
translator that exports geometry as BREP/extrusions with surface colours
("General Translator" or "Coordination View 2.0" work well with Bonsai).
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tapir_client import TapirClient  # noqa: E402


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--out', required=True, help='Target .ifc path (absolute)')
    parser.add_argument('--host', default='http://127.0.0.1')
    parser.add_argument('--port', type=int, default=19723)
    parser.add_argument('--file-type', default='ifc', choices=['ifc', 'ifcxml', 'ifczip', 'ifcxmlzip'])
    args = parser.parse_args()

    out = os.path.abspath(args.out)
    os.makedirs(os.path.dirname(out), exist_ok=True)

    client = TapirClient(args.host, args.port)
    info = client.run('API.GetProductInfo')
    print(f'Connected to Archicad {info.get("version")} build {info.get("buildNumber")}')

    client.run_tapir('IFCFileOperation', {
        'method': 'save',
        'ifcFilePath': out,
        'fileType': args.file_type,
    })
    if not os.path.isfile(out):
        raise SystemExit(f'IFC export reported success but {out} was not created')
    print(f'IFC written: {out} ({os.path.getsize(out) / 1e6:.1f} MB)')


if __name__ == '__main__':
    main()
