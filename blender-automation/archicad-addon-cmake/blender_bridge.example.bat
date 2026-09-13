@echo off
REM Copy this file next to your .pln as "blender_bridge.bat" and adjust the two paths.
REM Archicad's "Blender Bridge > Run Blender pipeline script" menu calls it with the IFC path as %1.
set AUTOMATION=C:\work\tapir-archicad-automation\blender-automation
set CONFIG=%AUTOMATION%\pipeline\my_house.json
cd /d %AUTOMATION%
python pipeline\run_pipeline.py --config "%CONFIG%" --preset preview --still 1 --render --save ..\renders\walkthrough.blend
pause
