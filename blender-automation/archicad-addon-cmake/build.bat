@echo off
REM Build the Blender Bridge Add-On for one Archicad version (default 29).
REM The DevKit is downloaded automatically by Tools/BuildAddOn.py.
set AC=%1
if "%AC%"=="" set AC=29
pushd %~dp0
if not exist Tools\BuildAddOn.py (
    echo Tools submodule missing. Run: git submodule update --init --recursive
    popd & exit /b 1
)
python Tools\BuildAddOn.py --configFile config.json --acVersion %AC% --buildConfig RelWithDebInfo --package
popd
