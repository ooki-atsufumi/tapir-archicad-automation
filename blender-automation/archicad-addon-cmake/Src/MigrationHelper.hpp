#pragma once

// Maps the Archicad 27+ API names onto the pre-27 equivalents so the same source
// compiles for Archicad 25..29 (same technique as the Tapir Add-On).

#include "ACAPinc.h"

#ifndef ServerMainVers_2700

#define ACAPI_MenuItem_RegisterMenu ACAPI_Register_Menu
#define ACAPI_MenuItem_InstallMenuHandler ACAPI_Install_MenuHandler
#define ACAPI_AddOnAddOnCommunication_InstallAddOnCommandHandler ACAPI_Install_AddOnCommandHandler
#define ACAPI_AddOnAddOnCommunication_Call ACAPI_Command_Call

inline GSErrCode ACAPI_ProjectOperation_Project (API_ProjectInfo* projectInfo)
{
    return ACAPI_Environment (APIEnv_ProjectID, projectInfo);
}

#endif
