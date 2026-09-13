#include "APIEnvir.h"
#include "ACAPinc.h"
#include "MigrationHelper.hpp"

#include "ResourceIds.hpp"
#include "BlenderBridgeCommands.hpp"
#include "ProcessLauncher.hpp"

#include "RS.hpp"
#include "DGModule.hpp"
#include "UniString.hpp"
#include "Location.hpp"
#include "FileSystem.hpp"

static GS::UniString GetString (Int32 index)
{
    return RSGetIndString (ID_ADDON_STRINGS, index, ACAPI_GetOwnResModule ());
}

static void ShowInfo (const GS::UniString& text, const GS::UniString& detail = GS::EmptyUniString)
{
    DGAlert (DG_INFORMATION, ADDON_NAME, text, detail, "OK");
}

static void ShowError (const GS::UniString& text, const GS::UniString& detail = GS::EmptyUniString)
{
    DGAlert (DG_ERROR, ADDON_NAME, text, detail, "OK");
}

// Menu: export <project>_blender.ifc next to the opened project file.
static void ExportIFCNextToProject ()
{
    GS::UniString folder, name;
    if (!GetProjectFolderAndName (folder, name)) {
        ShowError (GetString (ID_STR_PROJECT_UNTITLED));
        return;
    }
    IO::Location ifcLocation (folder);
    ifcLocation.AppendToLocal (IO::Name (name + "_blender.ifc"));
    const GS::UniString ifcPath = ifcLocation.ToDisplayText ();

    GS::UniString errorMessage;
    const GSErrCode err = ExportProjectToIFC (ifcPath, "ifc", errorMessage);
    if (err != NoError) {
        ShowError (GetString (ID_STR_EXPORT_FAILED), errorMessage);
        return;
    }
    ShowInfo (GetString (ID_STR_EXPORT_DONE), ifcPath);
}

// Menu: run <projectFolder>/blender_bridge.bat (Windows) or blender_bridge.sh (macOS) with
// the IFC path as first argument. The script is written by the user / Claude Code and typically
// calls blender-automation/pipeline/run_pipeline.py.
static void RunPipelineScriptNextToProject ()
{
    GS::UniString folder, name;
    if (!GetProjectFolderAndName (folder, name)) {
        ShowError (GetString (ID_STR_PROJECT_UNTITLED));
        return;
    }
#if defined (WINDOWS)
    const GS::UniString scriptName = "blender_bridge.bat";
#else
    const GS::UniString scriptName = "blender_bridge.sh";
#endif
    IO::Location scriptLocation (folder);
    scriptLocation.AppendToLocal (IO::Name (scriptName));
    bool exists = false;
    if (IO::fileSystem.Contains (scriptLocation, &exists) != NoError || !exists) {
        ShowError (GetString (ID_STR_PIPELINE_CONFIG_MISSING), scriptLocation.ToDisplayText ());
        return;
    }

    IO::Location ifcLocation (folder);
    ifcLocation.AppendToLocal (IO::Name (name + "_blender.ifc"));

    GS::Array<GS::UniString> arguments;
#if defined (WINDOWS)
    const GS::UniString executable = "cmd.exe";
    arguments.Push ("/c");
    arguments.Push (scriptLocation.ToDisplayText ());
#else
    const GS::UniString executable = "/bin/sh";
    arguments.Push (scriptLocation.ToDisplayText ());
#endif
    arguments.Push (ifcLocation.ToDisplayText ());

    const ProcessLauncher::LaunchResult launch = ProcessLauncher::LaunchDetached (executable, arguments, folder);
    if (!launch.success) {
        ShowError (launch.errorMessage, launch.commandLine);
        return;
    }
    ShowInfo (GetString (ID_STR_PIPELINE_STARTED), launch.commandLine);
}

static GSErrCode MenuCommandHandler (const API_MenuParams* menuParams)
{
    if (menuParams->menuItemRef.menuResID != ID_ADDON_MENU) {
        return NoError;
    }
    switch (menuParams->menuItemRef.itemIndex) {
        case ID_ADDON_MENU_EXPORT_IFC:
            ExportIFCNextToProject ();
            break;
        case ID_ADDON_MENU_RUN_PIPELINE:
            RunPipelineScriptNextToProject ();
            break;
        case ID_ADDON_MENU_ABOUT:
            ShowInfo (GS::UniString::Printf (GetString (ID_STR_ABOUT), ADDON_VERSION));
            break;
    }
    return NoError;
}

template <typename CommandType>
static GSErrCode RegisterCommand ()
{
    GS::Owner<CommandType> command = GS::NewOwned<CommandType> ();
    return ACAPI_AddOnAddOnCommunication_InstallAddOnCommandHandler (command.Pass ());
}

API_AddonType CheckEnvironment (API_EnvirParams* envir)
{
    RSGetIndString (&envir->addOnInfo.name, ID_ADDON_INFO, 1, ACAPI_GetOwnResModule ());
    RSGetIndString (&envir->addOnInfo.description, ID_ADDON_INFO, 2, ACAPI_GetOwnResModule ());
    return APIAddon_Normal;
}

GSErrCode RegisterInterface (void)
{
    return ACAPI_MenuItem_RegisterMenu (ID_ADDON_MENU, 0, MenuCode_UserDef, MenuFlag_Default);
}

GSErrCode Initialize (void)
{
    GSErrCode err = ACAPI_MenuItem_InstallMenuHandler (ID_ADDON_MENU, MenuCommandHandler);
    err |= RegisterCommand<GetProjectPathCommand> ();
    err |= RegisterCommand<ExportIFCCommand> ();
    err |= RegisterCommand<RunBlenderPipelineCommand> ();
    return err;
}

GSErrCode FreeData (void)
{
    return NoError;
}
