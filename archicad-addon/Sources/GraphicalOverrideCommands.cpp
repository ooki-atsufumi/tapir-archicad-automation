#include "GraphicalOverrideCommands.hpp"
#include "MigrationHelper.hpp"

static GSErrCode FindOrCreateRuleGroup (const GS::UniString& name, API_Guid& outGuid)
{
    GS::Array<API_Guid> groupIds;
    GSErrCode err = ACAPI_GraphicalOverride_GetOverrideRuleGroupList (groupIds);
    if (err != NoError) {
        return err;
    }
    for (const API_Guid& id : groupIds) {
        API_OverrideRuleGroup group = {id, ""};
        if (ACAPI_GraphicalOverride_GetOverrideRuleGroup (group) == NoError && group.name == name) {
            outGuid = id;
            return NoError;
        }
    }
    API_OverrideRuleGroup group = {APINULLGuid, name};
    err = ACAPI_GraphicalOverride_CreateOverrideRuleGroup (group);
    if (err != NoError) {
        return err;
    }
    outGuid = group.guid;
    return NoError;
}

CreateGraphicalOverrideRuleCommand::CreateGraphicalOverrideRuleCommand () :
    CommandBase (CommonSchema::Used)
{
}

GS::String CreateGraphicalOverrideRuleCommand::GetName () const
{
    return "CreateGraphicalOverrideRule";
}

GS::Optional<GS::UniString> CreateGraphicalOverrideRuleCommand::GetInputParametersSchema () const
{
    return R"({
        "type": "object",
        "properties": {
            "name": {
                "type": "string",
                "description": "Name of the override rule. If a rule with the same name exists in the rule group, it is overwritten."
            },
            "ruleGroupName": {
                "type": "string",
                "description": "Name of the rule group. Created if it does not exist. Default: 'Tapir'."
            },
            "criterionXML": {
                "type": "string",
                "description": "Optional criterion XML (as saved from Archicad). Empty means the rule applies to all elements. Use GetGraphicalOverrideRules to harvest the XML of an existing (manually created) rule."
            },
            "surfaceRGB": {
                "type": "object",
                "description": "Optional surface (cut and uncut) color override. Components are in the 0..1 range.",
                "properties": {
                    "red": { "type": "number" },
                    "green": { "type": "number" },
                    "blue": { "type": "number" }
                },
                "required": ["red", "green", "blue"],
                "additionalProperties": false
            },
            "linePenIndex": {
                "type": "integer",
                "description": "Optional pen index override for lines, markers and texts."
            },
            "overrideContours": {
                "type": "boolean",
                "description": "Optional. If true, contours of the overridden surfaces are also recolored."
            }
        },
        "additionalProperties": false,
        "required": ["name"]
    })";
}

GS::Optional<GS::UniString> CreateGraphicalOverrideRuleCommand::GetRawResponseSchema () const
{
    return R"({
        "type": "object",
        "properties": {
            "ruleId": { "$ref": "#/AttributeId" }
        },
        "additionalProperties": false,
        "required": ["ruleId"]
    })";
}

GS::ObjectState CreateGraphicalOverrideRuleCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
#ifdef ServerMainVers_2700
    GS::UniString name;
    parameters.Get ("name", name);
    GS::UniString ruleGroupName = "Tapir";
    parameters.Get ("ruleGroupName", ruleGroupName);

    API_Guid groupGuid = APINULLGuid;
    GSErrCode err = FindOrCreateRuleGroup (ruleGroupName, groupGuid);
    if (err != NoError) {
        return CreateErrorResponse (err, "Failed to find or create the override rule group.");
    }

    API_OverrideRuleStyle style = {};
    style.lineMarkerTextPen = APINullValue;

    Int32 linePenIndex = 0;
    if (parameters.Get ("linePenIndex", linePenIndex) && linePenIndex > 0) {
        style.lineMarkerTextPen = static_cast<short> (linePenIndex);
    }

    const GS::ObjectState* surfaceRGBOS = parameters.Get ("surfaceRGB");
    if (surfaceRGBOS != nullptr) {
        API_RGBColor color = GetColorFromObjectState (*surfaceRGBOS);
        style.surfaceOverride = color;
        style.surfaceType.overrideCutSurface = true;
        style.surfaceType.overrideUncutSurface = true;
    }

    bool overrideContours = false;
    if (parameters.Get ("overrideContours", overrideContours)) {
        style.overrideContours = overrideContours;
    }

    GS::UniString criterionXML;
    parameters.Get ("criterionXML", criterionXML);

    API_OverrideRule rule = {APINULLGuid, name, style, criterionXML};
    err = ACAPI_GraphicalOverride_CreateOverrideRule (rule, groupGuid);
    if (err == APIERR_NAMEALREADYUSED) {
        API_OverrideRule existing = {APINULLGuid, name};
        err = ACAPI_GraphicalOverride_GetOverrideRuleByName (existing, groupGuid);
        if (err == NoError) {
            rule.guid = existing.guid;
            err = ACAPI_GraphicalOverride_ChangeOverrideRule (rule);
        }
    }
    if (err != NoError) {
        return CreateErrorResponse (err, "Failed to create or change the override rule.");
    }

    GS::ObjectState response;
    response.Add ("ruleId", CreateGuidObjectState (rule.guid));
    return response;
#else
    UNUSED_PARAMETER (parameters);
    return CreateErrorResponse (APIERR_NOTSUPPORTED, "This command requires Archicad 27 or newer (graphical override rule groups).");
#endif
}

GetGraphicalOverrideRulesCommand::GetGraphicalOverrideRulesCommand () :
    CommandBase (CommonSchema::NotUsed)
{
}

GS::String GetGraphicalOverrideRulesCommand::GetName () const
{
    return "GetGraphicalOverrideRules";
}

GS::Optional<GS::UniString> GetGraphicalOverrideRulesCommand::GetRawResponseSchema () const
{
    return R"({
        "type": "object",
        "properties": {
            "rules": {
                "type": "array",
                "items": {
                    "type": "object",
                    "properties": {
                        "guid": { "type": "string" },
                        "name": { "type": "string" },
                        "criterionXML": { "type": "string" }
                    },
                    "required": ["guid", "name", "criterionXML"],
                    "additionalProperties": false
                }
            }
        },
        "additionalProperties": false,
        "required": ["rules"]
    })";
}

GS::ObjectState GetGraphicalOverrideRulesCommand::Execute (const GS::ObjectState& /*parameters*/, GS::ProcessControl& /*processControl*/) const
{
#ifdef ServerMainVers_2700
    GS::ObjectState response;
    const auto& rules = response.AddList<GS::ObjectState> ("rules");

    GS::Array<API_Guid> ruleIds;
    GSErrCode err = ACAPI_GraphicalOverride_GetOverrideRuleList (ruleIds);
    if (err != NoError) {
        return CreateErrorResponse (err, "Failed to get the override rule list.");
    }
    for (const API_Guid& id : ruleIds) {
        API_OverrideRule rule = {id};
        if (ACAPI_GraphicalOverride_GetOverrideRuleById (rule) != NoError) {
            continue;
        }
        GS::ObjectState ruleOS;
        ruleOS.Add ("guid", APIGuidToString (rule.guid));
        ruleOS.Add ("name", rule.name);
        ruleOS.Add ("criterionXML", rule.criterionXML);
        rules (ruleOS);
    }
    return response;
#else
    return CreateErrorResponse (APIERR_NOTSUPPORTED, "This command requires Archicad 27 or newer (graphical override rule groups).");
#endif
}

CreateGraphicalOverrideCombinationCommand::CreateGraphicalOverrideCombinationCommand () :
    CommandBase (CommonSchema::NotUsed)
{
}

GS::String CreateGraphicalOverrideCombinationCommand::GetName () const
{
    return "CreateGraphicalOverrideCombination";
}

GS::Optional<GS::UniString> CreateGraphicalOverrideCombinationCommand::GetInputParametersSchema () const
{
    return R"({
        "type": "object",
        "properties": {
            "name": {
                "type": "string",
                "description": "Name of the override combination. If a combination with the same name exists, its rule list is replaced."
            },
            "ruleNames": {
                "type": "array",
                "description": "Names of the override rules the combination should contain (first match by name across all rule groups).",
                "items": { "type": "string" }
            }
        },
        "additionalProperties": false,
        "required": ["name", "ruleNames"]
    })";
}

GS::Optional<GS::UniString> CreateGraphicalOverrideCombinationCommand::GetRawResponseSchema () const
{
    return R"({
        "type": "object",
        "properties": {
            "combinationGuid": { "type": "string" }
        },
        "additionalProperties": false,
        "required": ["combinationGuid"]
    })";
}

GS::ObjectState CreateGraphicalOverrideCombinationCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
#ifdef ServerMainVers_2700
    GS::UniString name;
    parameters.Get ("name", name);
    GS::Array<GS::UniString> ruleNames;
    parameters.Get ("ruleNames", ruleNames);

    GS::Array<API_Guid> allRuleIds;
    GSErrCode err = ACAPI_GraphicalOverride_GetOverrideRuleList (allRuleIds);
    if (err != NoError) {
        return CreateErrorResponse (err, "Failed to get the override rule list.");
    }

    GS::Array<API_Guid> ruleGuids;
    for (const GS::UniString& ruleName : ruleNames) {
        bool found = false;
        for (const API_Guid& id : allRuleIds) {
            API_OverrideRule rule = {id};
            if (ACAPI_GraphicalOverride_GetOverrideRuleById (rule) == NoError && rule.name == ruleName) {
                ruleGuids.Push (id);
                found = true;
                break;
            }
        }
        if (!found) {
            return CreateErrorResponse (APIERR_BADPARS, "Override rule not found: " + ruleName);
        }
    }

    API_OverrideCombination combination = {APINULLGuid, name};
    err = ACAPI_GraphicalOverride_CreateOverrideCombination (combination, ruleGuids);
    if (err == APIERR_NAMEALREADYUSED) {
        API_OverrideCombination existing = {APINULLGuid, name};
        err = ACAPI_GraphicalOverride_GetOverrideCombination (existing);
        if (err == NoError) {
            combination.guid = existing.guid;
            err = ACAPI_GraphicalOverride_ChangeOverrideCombination (combination, &ruleGuids);
        }
    }
    if (err != NoError) {
        return CreateErrorResponse (err, "Failed to create or change the override combination.");
    }

    GS::ObjectState response;
    response.Add ("combinationGuid", APIGuidToString (combination.guid));
    return response;
#else
    UNUSED_PARAMETER (parameters);
    return CreateErrorResponse (APIERR_NOTSUPPORTED, "This command requires Archicad 27 or newer (graphical override rule groups).");
#endif
}
