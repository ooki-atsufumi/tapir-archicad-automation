@echo off
REM Quick 720p EEVEE preview (frame 1 still + full video). Run from this folder.
set CFG=%1
if "%CFG%"=="" set CFG=pipeline\config.example.json
python pipeline\run_pipeline.py --config %CFG% --preset preview --still 1 --render --save ..\renders\walkthrough.blend
