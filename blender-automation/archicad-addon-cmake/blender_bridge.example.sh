#!/bin/sh
# Copy next to your .pln as "blender_bridge.sh" (chmod +x) and adjust the two paths.
AUTOMATION="$HOME/work/tapir-archicad-automation/blender-automation"
CONFIG="$AUTOMATION/pipeline/my_house.json"
cd "$AUTOMATION"
python3 pipeline/run_pipeline.py --config "$CONFIG" --preset preview --still 1 --render --save ../renders/walkthrough.blend
