#pragma once

#include "APIEnvir.h"
#include "ACAPinc.h"

namespace ProcessLauncher {

struct LaunchResult {
    bool            success = false;
    Int64           processId = 0;
    GS::UniString   commandLine;
    GS::UniString   errorMessage;
};

// Starts an external program without waiting for it (the pipeline may run for hours).
// On Windows a new console window is opened so the Blender/Python log stays visible.
LaunchResult LaunchDetached (const GS::UniString& executable,
                             const GS::Array<GS::UniString>& arguments,
                             const GS::UniString& workingDirectory);

}
