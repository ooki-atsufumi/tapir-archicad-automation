#ifndef JWW_EXPORT_COMMANDS_HPP
#define JWW_EXPORT_COMMANDS_HPP

#include "CommandBase.hpp"

// 現在の平面図(またはマスターレイアウト)を ShapePrims で 2D プリミティブへ分解し、
// 属性テーブル(レイヤー/線種/ペン/フォント)と共に JSON 中間ファイルへ書き出す。
// JWW バイナリ化はパレット側 Python (jww変換.py) が行う。
class ExportJwwJsonCommand : public CommandBase
{
public:
    ExportJwwJsonCommand ();
    virtual GS::String GetName () const override;
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetRawResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

#endif
