#pragma once

#include "CommandBase.hpp"

// Grid Element (通り芯オブジェクト) commands.
// In the Archicad API a grid element is an API_ObjectID element with the variation
// APIVarId_GridElement ('GREL'), driven by a library part. Neither the official JSON API
// nor upstream Tapir can create or list them; these commands add that.

class GetGridElementDefaultsCommand : public CommandBase
{
public:
    GetGridElementDefaultsCommand ();
    virtual GS::String GetName () const override;
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetRawResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

class GetGridElementsCommand : public CommandBase
{
public:
    GetGridElementsCommand ();
    virtual GS::String GetName () const override;
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetRawResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

class CreateGridElementsCommand : public CommandBase
{
public:
    CreateGridElementsCommand ();
    virtual GS::String GetName () const override;
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetRawResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
