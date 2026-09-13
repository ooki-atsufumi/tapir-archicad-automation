#include "BlenderBridgeCommands.hpp"
#include "MigrationHelper.hpp"
#include "ProcessLauncher.hpp"

#include "FileSystem.hpp"
#include "Location.hpp"

constexpr const char* CommandNamespace = "BlenderBridge";

// ---------------------------------------------------------------------------
// Base class
// ---------------------------------------------------------------------------

GS::String BridgeCommandBase::GetNamespace () const
{
    return CommandNamespace;
}

API_AddOnCommandExecutionPolicy BridgeCommandBase::GetExecutionPolicy () const
{
    return API_AddOnCommandExecutionPolicy::ScheduleForExecutionOnMainThread;
}

void BridgeCommandBase::OnResponseValidationFailed (const GS::ObjectState& /*response*/) const
{
}

#ifdef ServerMainVers_2600
bool BridgeCommandBase::IsProcessWindowVisible () const
{
    return false;
}
#endif

GS::Optional<GS::UniString> BridgeCommandBase::GetSchemaDefinitions () const
{
    return {};
}

GS::Optional<GS::UniString> BridgeCommandBase::GetInputParametersSchema () const
{
    return {};
}

GS::Optional<GS::UniString> BridgeCommandBase::GetResponseSchema () const
{
    return {};
}

GS::ObjectState CreateFailedResult (GSErrCode errorCode, const GS::UniString& errorMessage)
{
    GS::ObjectState error;
    error.Add ("code", errorCode);
    error.Add ("message", errorMessage.ToCStr ().Get ());
    GS::ObjectState result ("error", error);
    result.Add ("success", false);
    return result;
}

GS::ObjectState CreateSuccessfulResult ()
{
    return GS::ObjectState ("success", true);
}

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

bool GetProjectFolderAndName (GS::UniString& folderPath, GS::UniString& projectName)
{
    API_ProjectInfo projectInfo = {};
    if (ACAPI_ProjectOperation_Project (&projectInfo) != NoError || projectInfo.untitled || projectInfo.location == nullptr) {
        return false;
    }
    IO::Location folder (*projectInfo.location);
    folder.DeleteLastLocalName ();
    folderPath = folder.ToDisplayText ();
    if (projectInfo.projectName != nullptr) {
        projectName = *projectInfo.projectName;
    } else {
        IO::Name name;
        projectInfo.location->GetLastLocalName (&name);
        projectName = name.ToString ();
        const UIndex dot = projectName.FindLast ('.');
        if (dot != MaxUIndex) {
            projectName = projectName.GetSubstring (0, dot);
        }
    }
    return true;
}

GSErrCode ExportProjectToIFC (const GS::UniString& ifcFilePath, const GS::UniString& fileType, GS::UniString& errorMessage)
{
    API_IOParams ioParams = {};
    ioParams.fileTypeID = APIFType_IfcFile;
    ioParams.method = IO_SAVEAS;
    if (fileType.IsEmpty () || fileType == "ifc") {
        ioParams.refCon = 1;
    } else if (fileType == "ifcxml") {
        ioParams.refCon = 2;
    } else if (fileType == "ifczip") {
        ioParams.refCon = 3;
    } else if (fileType == "ifcxmlzip") {
        ioParams.refCon = 4;
    } else {
        errorMessage = "fileType parameter is invalid";
        return APIERR_BADPARS;
    }

    IO::Location ifcFileLocation (ifcFilePath);
    IO::Name lastLocalName;
    if (ifcFileLocation.GetLastLocalName (&lastLocalName) != NoError) {
        errorMessage = "ifcFilePath parameter is invalid";
        return APIERR_BADPARS;
    }
    ioParams.fileLoc = &ifcFileLocation;
    ioParams.saveFileIOName = &lastLocalName;
    ioParams.noDialog = true;
    ioParams.fromDragDrop = false;

    // The IFC In/Out Add-On shipped with Archicad (same module id Tapir uses).
    API_ModulID moduleID = { 1198731108, 138575850 };
    const GSErrCode err = ACAPI_AddOnAddOnCommunication_Call (&moduleID, 'IFCI', 1, reinterpret_cast<GSHandle> (&ioParams), nullptr, ioParams.noDialog);
    if (err != NoError) {
        errorMessage = "The IFC add-on failed to save the file";
    }
    return err;
}

// ---------------------------------------------------------------------------
// GetProjectPath
// ---------------------------------------------------------------------------

GS::String GetProjectPathCommand::GetName () const
{
    return "GetProjectPath";
}

GS::Optional<GS::UniString> GetProjectPathCommand::GetResponseSchema () const
{
    return R"({
        "type": "object",
        "properties": {
            "isUntitled": { "type": "boolean" },
            "projectFolder": { "type": "string", "description": "Folder containing the .pln/.pla file." },
            "projectName": { "type": "string" },
            "suggestedIfcPath": { "type": "string", "description": "<projectFolder>/<projectName>_blender.ifc" }
        },
        "additionalProperties": false,
        "required": [ "isUntitled" ]
    })";
}

GS::ObjectState GetProjectPathCommand::Execute (const GS::ObjectState& /*parameters*/, GS::ProcessControl& /*processControl*/) const
{
    GS::UniString folder, name;
    GS::ObjectState response;
    if (!GetProjectFolderAndName (folder, name)) {
        response.Add ("isUntitled", true);
        return response;
    }
    response.Add ("isUntitled", false);
    response.Add ("projectFolder", folder);
    response.Add ("projectName", name);
    IO::Location ifcLocation (folder);
    ifcLocation.AppendToLocal (IO::Name (name + "_blender.ifc"));
    response.Add ("suggestedIfcPath", ifcLocation.ToDisplayText ());
    return response;
}

// ---------------------------------------------------------------------------
// ExportIFC
// ---------------------------------------------------------------------------

GS::String ExportIFCCommand::GetName () const
{
    return "ExportIFC";
}

GS::Optional<GS::UniString> ExportIFCCommand::GetInputParametersSchema () const
{
    return R"({
        "type": "object",
        "properties": {
            "ifcFilePath": {
                "type": "string",
                "description": "Absolute path of the IFC file to write. The translator selected in Archicad's IFC Translators dialog is used.",
                "minLength": 1
            },
            "fileType": {
                "type": "string",
                "description": "The type of the IFC file. The default is 'ifc'.",
                "enum": ["ifc", "ifcxml", "ifczip", "ifcxmlzip"]
            }
        },
        "additionalProperties": false,
        "required": [ "ifcFilePath" ]
    })";
}

GS::Optional<GS::UniString> ExportIFCCommand::GetResponseSchema () const
{
    return R"({
        "type": "object",
        "properties": {
            "success": { "type": "boolean" },
            "ifcFilePath": { "type": "string" },
            "error": { "type": "object" }
        },
        "required": [ "success" ]
    })";
}

GS::ObjectState ExportIFCCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    GS::UniString ifcFilePath;
    if (!parameters.Get ("ifcFilePath", ifcFilePath) || ifcFilePath.IsEmpty ()) {
        return CreateFailedResult (APIERR_BADPARS, "ifcFilePath parameter is missing");
    }
    GS::UniString fileType;
    parameters.Get ("fileType", fileType);

    GS::UniString errorMessage;
    const GSErrCode err = ExportProjectToIFC (ifcFilePath, fileType, errorMessage);
    if (err != NoError) {
        return CreateFailedResult (err, errorMessage);
    }
    GS::ObjectState response = CreateSuccessfulResult ();
    response.Add ("ifcFilePath", ifcFilePath);
    return response;
}

// ---------------------------------------------------------------------------
// RunBlenderPipeline
// ---------------------------------------------------------------------------

GS::String RunBlenderPipelineCommand::GetName () const
{
    return "RunBlenderPipeline";
}

GS::Optional<GS::UniString> RunBlenderPipelineCommand::GetInputParametersSchema () const
{
    return R"({
        "type": "object",
        "properties": {
            "executable": {
                "type": "string",
                "description": "Program to start. Default: 'python' (must be on PATH). Use the full path of python.exe or blender.exe if needed.",
                "minLength": 1
            },
            "arguments": {
                "type": "array",
                "description": "Command line arguments, e.g. [\"pipeline/run_pipeline.py\", \"--config\", \"pipeline/my_house.json\", \"--render\"].",
                "items": { "type": "string" }
            },
            "workingDirectory": {
                "type": "string",
                "description": "Working directory for the process, typically the blender-automation folder.",
                "minLength": 1
            }
        },
        "additionalProperties": false,
        "required": [ "arguments" ]
    })";
}

GS::Optional<GS::UniString> RunBlenderPipelineCommand::GetResponseSchema () const
{
    return R"({
        "type": "object",
        "properties": {
            "success": { "type": "boolean" },
            "processId": { "type": "integer" },
            "commandLine": { "type": "string" },
            "error": { "type": "object" }
        },
        "required": [ "success" ]
    })";
}

GS::ObjectState RunBlenderPipelineCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    GS::UniString executable = "python";
    parameters.Get ("executable", executable);
    GS::UniString workingDirectory;
    parameters.Get ("workingDirectory", workingDirectory);

    GS::Array<GS::UniString> arguments;
    parameters.Get ("arguments", arguments);

    const ProcessLauncher::LaunchResult launch = ProcessLauncher::LaunchDetached (executable, arguments, workingDirectory);
    if (!launch.success) {
        return CreateFailedResult (APIERR_GENERAL, launch.errorMessage);
    }
    GS::ObjectState response = CreateSuccessfulResult ();
    response.Add ("processId", launch.processId);
    response.Add ("commandLine", launch.commandLine);
    return response;
}
