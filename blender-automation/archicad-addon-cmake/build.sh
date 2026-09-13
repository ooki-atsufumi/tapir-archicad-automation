#!/bin/sh
# Build the Blender Bridge Add-On for one Archicad version (default 29) on macOS.
AC=${1:-29}
cd "$(dirname "$0")"
if [ ! -f Tools/BuildAddOn.py ]; then
  echo "Tools submodule missing. Run: git submodule update --init --recursive"; exit 1
fi
python3 Tools/BuildAddOn.py --configFile config.json --acVersion "$AC" --buildConfig RelWithDebInfo --package
