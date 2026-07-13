#include "ViewCreationCommands.hpp"
#include "MigrationHelper.hpp"

// ProjectMap を再帰的に走査し、floorNum 一致の StoryNavItem を探す。debug に走査内容を記録。
static bool SearchStoryRecursive (const API_Guid& parentGuid, short floorNum, API_Guid& outGuid,
                                  Int32& storyCount, GS::UniString& debug, Int32 depth)
{
    API_NavigatorItem parent = {};
    parent.guid = parentGuid;
    parent.mapId = API_ProjectMap;
    GS::Array<API_NavigatorItem> children;
    GSErrCode err = ACAPI_Navigator_GetNavigatorChildrenItems (&parent, &children);
    if (err != NoError) {
        debug += GS::UniString::Printf ("[d%d children err=%d] ", depth, (int) err);
        return false;
    }
    debug += GS::UniString::Printf ("[d%d n=%u:", depth, (unsigned) children.GetSize ());
    for (const auto& ch : children) {
        debug += GS::UniString::Printf ("t%d/f%d ", (int) ch.itemType, (int) ch.floorNum);
        if (ch.itemType == API_StoryNavItem) {
            storyCount++;
            if (ch.floorNum == floorNum) {
                outGuid = ch.guid;
                debug += "]FOUND";
                return true;
            }
        }
    }
    debug += "]";
    // 子を再帰(ストーリー以外は下層を持ちうる)
    for (const auto& ch : children) {
        if (ch.itemType != API_StoryNavItem && depth < 4) {
            if (SearchStoryRecursive (ch.guid, floorNum, outGuid, storyCount, debug, depth + 1)) {
                return true;
            }
        }
    }
    return false;
}

static bool FindStoryProjectMapItem (short floorNum, API_Guid& outGuid, Int32& storyCount, GS::UniString& debug)
{
    API_NavigatorSet set = {};
    set.mapId = API_ProjectMap;
    if (ACAPI_Navigator_GetNavigatorSet (&set) != NoError) {
        debug = "GetNavigatorSet(ProjectMap) failed";
        return false;
    }
    return SearchStoryRecursive (set.rootGuid, floorNum, outGuid, storyCount, debug, 0);
}

static API_Guid GetPublicViewMapRoot ()
{
    API_NavigatorSet set = {};
    set.mapId = API_PublicViewMap;
    if (ACAPI_Navigator_GetNavigatorSet (&set) != NoError) {
        return APINULLGuid;
    }
    return set.rootGuid;
}

CreateViewsFromStoriesCommand::CreateViewsFromStoriesCommand () :
    CommandBase (CommonSchema::Used)
{
}

GS::String CreateViewsFromStoriesCommand::GetName () const
{
    return "CreateViewsFromStories";
}

GS::Optional<GS::UniString> CreateViewsFromStoriesCommand::GetInputParametersSchema () const
{
    return R"({
        "type": "object",
        "properties": {
            "views": {
                "type": "array",
                "description": "Views to create by cloning story viewpoints into the View Map.",
                "items": {
                    "type": "object",
                    "properties": {
                        "floorIndex": {
                            "type": "integer",
                            "description": "The story index the view refers to."
                        },
                        "viewName": {
                            "type": "string",
                            "description": "Name of the created view."
                        },
                        "layerCombination": {
                            "type": "string",
                            "description": "Optional layer combination name to store on the view."
                        },
                        "scale": {
                            "type": "integer",
                            "description": "Optional drawing scale denominator (e.g. 50 for 1:50)."
                        }
                    },
                    "required": ["floorIndex", "viewName"],
                    "additionalProperties": false
                }
            }
        },
        "additionalProperties": false,
        "required": ["views"]
    })";
}

GS::Optional<GS::UniString> CreateViewsFromStoriesCommand::GetResponseSchema () const
{
    return R"({
        "type": "object",
        "properties": {
            "views": {
                "type": "array",
                "items": {
                    "type": "object",
                    "properties": {
                        "viewName": { "type": "string" },
                        "navigatorItemId": { "$ref": "#/NavigatorItemId" }
                    },
                        "error": { "type": "string" }
                    },
                    "required": ["viewName"],
                    "additionalProperties": false
                }
            }
        },
        "additionalProperties": false,
        "required": ["views"]
    })";
}

GS::ObjectState CreateViewsFromStoriesCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    GS::Array<GS::ObjectState> viewsIn;
    parameters.Get ("views", viewsIn);

    const API_Guid parentGuid = GetPublicViewMapRoot ();
    if (parentGuid == APINULLGuid) {
        return CreateErrorResponse (APIERR_GENERAL, "Failed to get the public view map root.");
    }

    GS::ObjectState response;
    const auto& outViews = response.AddList<GS::ObjectState> ("views");

    for (const GS::ObjectState& v : viewsIn) {
        Int32 floorIndex = 0;
        v.Get ("floorIndex", floorIndex);
        GS::UniString viewName;
        v.Get ("viewName", viewName);

        API_Guid storyGuid = APINULLGuid;
        Int32 storyCount = 0;
        GS::UniString debug;
        if (!FindStoryProjectMapItem (static_cast<short> (floorIndex), storyGuid, storyCount, debug)) {
            GS::ObjectState o;
            o.Add ("viewName", viewName);
            o.Add ("error", GS::UniString::Printf ("Story not found (floorIndex=%d, seen=%d) tree=%T", floorIndex, storyCount, debug.ToPrintf ()));
            outViews (o);
            continue;
        }

        API_Guid clonedGuid = APINULLGuid;
        GSErrCode err = ACAPI_Navigator_CloneProjectMapItemToViewMap (&storyGuid, &parentGuid, &clonedGuid);
        if (err != NoError || clonedGuid == APINULLGuid) {
            GS::ObjectState o;
            o.Add ("viewName", viewName);
            o.Add ("error", GS::UniString ("Failed to clone story to view map"));
            outViews (o);
            continue;
        }

        // ビュー設定(名前・レイヤー組合せ・スケール)を反映
        API_NavigatorItem navItem = {};
        navItem.guid = clonedGuid;
        navItem.mapId = API_PublicViewMap;
        API_NavigatorView navView = {};
        if (ACAPI_Navigator_GetNavigatorView (&navItem, &navView) == NoError) {
            GS::ucscpy (navItem.uName, viewName.ToUStr ().Get ());
            navItem.customName = true;

            GS::UniString layerComb;
            if (v.Get ("layerCombination", layerComb) && !layerComb.IsEmpty ()) {
                navView.saveLaySet = true;
                CHTruncate (layerComb.ToCStr ().Get (), navView.layerCombination, API_AttrNameLen);
                navView.layerStats = nullptr;
            }
            Int32 scale = 0;
            if (v.Get ("scale", scale) && scale > 0) {
                navView.saveDScale = true;
                navView.drawingScale = scale;
            }
            ACAPI_Navigator_ChangeNavigatorView (&navItem, &navView);
        }

        GS::ObjectState o;
        o.Add ("viewName", viewName);
        o.Add ("navigatorItemId", CreateGuidObjectState (clonedGuid));
        outViews (o);
    }

    return response;
}
