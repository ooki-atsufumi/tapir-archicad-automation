#pragma once

#include "APIEnvir.h"
#include "ACAPinc.h"
#include "ObjectState.hpp"

// All commands are invoked through Archicad's JSON interface as
//   API.ExecuteAddOnCommand { addOnCommandId: { commandNamespace: "BlenderBridge", commandName: <Name> }, addOnCommandParameters: {...} }
// which is exactly how Tapir commands are called (only the namespace differs).

class BridgeCommandBase : public API_AddOnCommand
{
public:
    virtual GS::String GetNamespace () const override final;
    virtual API_AddOnCommandExecutionPolicy GetExecutionPolicy () const override final;
    virtual void OnResponseValidationFailed (const GS::ObjectState& response) const override final;
#ifdef ServerMainVers_2600
    virtual bool IsProcessWindowVisible () const override final;
#endif
    virtual GS::Optional<GS::UniString> GetSchemaDefinitions () const override final;
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
};

GS::ObjectState CreateFailedResult (GSErrCode errorCode, const GS::UniString& errorMessage);
GS::ObjectState CreateSuccessfulResult ();

// Shared with the menu handler.
GSErrCode ExportProjectToIFC (const GS::UniString& ifcFilePath, const GS::UniString& fileType, GS::UniString& errorMessage);
bool      GetProjectFolderAndName (GS::UniString& folderPath, GS::UniString& projectName);

// Returns information about the opened project and a suggested IFC path next to it.
class GetProjectPathCommand : public BridgeCommandBase
{
public:
    virtual GS::String GetName () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

// Saves the opened project as IFC (same mechanism as Tapir's IFCFileOperation "save").
class ExportIFCCommand : public BridgeCommandBase
{
public:
    virtual GS::String GetName () const override;
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

// Launches blender-automation/pipeline/run_pipeline.py (or any program) detached from Archicad.
class RunBlenderPipelineCommand : public BridgeCommandBase
{
public:
    virtual GS::String GetName () const override;
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
