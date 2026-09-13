@echo off
REM Cycles 1080p final render. Expect several seconds to a minute per frame depending on GPU.
set CFG=%1
if "%CFG%"=="" set CFG=pipeline\config.example.json
python pipeline\run_pipeline.py --config %CFG% --preset final --render --save ..\renders\walkthrough_final.blend
