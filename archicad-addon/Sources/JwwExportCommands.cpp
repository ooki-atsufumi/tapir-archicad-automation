// ============================================================================
// JwwExportCommands.cpp — ExportJwwJson (JWW Exporter v2 の Tapir 版)
//
// 方式: 現在の平面図ビューの要素を ShapePrims で 2D プリミティブに分解し、
//       属性テーブル(レイヤー/線種/ペン/フォント)と共に JSON 中間ファイルへ
//       ダンプする。JWW バイナリ化はパレット側 Python (jww変換.py) が行う。
//
// これにより Archicad 内蔵 Jw_cad 変換の 4 大問題を根本解決する:
//   ① 寸法の点が○になる  → 寸法要素は dimElems から寸法図形として再構築
//   ② 文字仕様が崩れる    → heightMM / widthFactor / フォント名を正確に受け渡す
//   ③ レイヤーがばらばら  → プリミティブ毎に元レイヤー index+名前を保持
//   ④ 線種がバラバラ      → 線種 index+名前を保持し Python 側で対応表マッピング
//
// 元実装: archicad-addon-cmake/JwwExporter (独立アドオン)。社内配布を
// Tapir apx + custom-scripts に一本化するため Tapir コマンド化した (2026-08-24)。
// ============================================================================

#include "JwwExportCommands.hpp"
#include "MigrationHelper.hpp"

#include <cstdio>
#include <cstring>
#include <string>

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
namespace {

std::string UniToUtf8 (const GS::UniString& us)
{
    return std::string (us.ToCStr (CC_UTF8).Get ());
}

// ---------------------------------------------------------------------------
// Minimal JSON writer
// ---------------------------------------------------------------------------
class JsonOut {
public:
    explicit JsonOut (FILE* fp) : fp_ (fp) {}

    void Raw (const char* s)            { fputs (s, fp_); }
    void Num (double v)                 { char b[64]; snprintf (b, sizeof b, "%.10g", v); fputs (b, fp_); }
    void Int (long v)                   { fprintf (fp_, "%ld", v); }

    void Str (const std::string& s)
    {
        fputc ('"', fp_);
        for (unsigned char c : s) {
            switch (c) {
                case '"':  fputs ("\\\"", fp_); break;
                case '\\': fputs ("\\\\", fp_); break;
                case '\n': fputs ("\\n", fp_);  break;
                case '\r': fputs ("\\r", fp_);  break;
                case '\t': fputs ("\\t", fp_);  break;
                default:
                    if (c < 0x20) fprintf (fp_, "\\u%04x", c);
                    else          fputc (c, fp_);
            }
        }
        fputc ('"', fp_);
    }

    void Key (const char* k)            { Str (k); fputc (':', fp_); }
    void KV (const char* k, double v)   { Key (k); Num (v); }
    void KVi (const char* k, long v)    { Key (k); Int (v); }
    void KVs (const char* k, const std::string& v) { Key (k); Str (v); }

private:
    FILE* fp_;
};

// ---------------------------------------------------------------------------
// ShapePrims collector
// ---------------------------------------------------------------------------
struct PrimCtx {
    JsonOut*    js          = nullptr;
    bool        first       = true;     // element array comma control
    bool        inHatchLines = false;
    bool        inArrow     = false;
    // current hatch border (valid between HatchBorderBeg/End)
    bool        hasHatch    = false;
    long        fillPen     = 0;
    long        fillBkgPen  = 0;
    bool        fillRgbValid = false;
    long        fillDet     = 0;        // APIHatch_DraftingFills/CutFills/CoverFills
    double      fillR = 0, fillG = 0, fillB = 0;
    long        primCount   = 0;
    long        textCount   = 0;        // 文字プリミティブ数 (フォールバック判定用)
    API_AddParType** curParams = nullptr;  // 処理中要素のGDLパラメータ (par1=null文字の解決用)
};

PrimCtx gCtx;

// GDL パラメータ配列から文字列パラメータを取り出す
// (TEXT2 がパラメータ参照で文字を描く場合、prim の par1 は null で
//  paramInd/ind1/ind2 がここを指す — 通り芯マーカーの X1 等がこのパターン)
std::string ParamString (API_AddParType** params, short paramInd, short ind1, short ind2)
{
    if (params == nullptr || *params == nullptr || paramInd <= 0)
        return "";
    const Int32 count = static_cast<Int32> (
        BMGetHandleSize (reinterpret_cast<GSConstHandle> (params)) / sizeof (API_AddParType));
    if (paramInd > count)
        return "";
    const API_AddParType& p = (*params)[paramInd - 1];
    if (p.typeID != APIParT_CString)
        return "";
    if (p.typeMod == 0) {
        GS::UniString us (p.value.uStr);
        return UniToUtf8 (us);
    }
    // 文字列配列: 各要素 = Int32(uchar数) + uchar列
    if (p.value.array == nullptr || *p.value.array == nullptr)
        return "";
    const Int32 d1 = (p.dim1 > 0) ? p.dim1 : 1;
    const Int32 d2 = (p.dim2 > 0) ? p.dim2 : 1;
    Int32 target = ((ind1 > 0 ? ind1 - 1 : 0) * d2) + (ind2 > 0 ? ind2 - 1 : 0);
    if (target < 0 || target >= d1 * d2)
        return "";
    const GSPtr data = *p.value.array;
    const Int32 handleSize = static_cast<Int32> (BMGetHandleSize (p.value.array));
    Int32 offs = 0;
    for (Int32 k = 0; k < d1 * d2; k++) {
        if (offs + static_cast<Int32> (sizeof (Int32)) > handleSize)
            return "";
        Int32 len = *reinterpret_cast<const Int32*> (data + offs);
        offs += sizeof (Int32);
        if (len < 0 || offs + len * static_cast<Int32> (sizeof (GS::uchar_t)) > handleSize)
            return "";
        if (k == target) {
            GS::UniString us (reinterpret_cast<const GS::uchar_t*> (data + offs));
            return UniToUtf8 (us);
        }
        offs += len * sizeof (GS::uchar_t);
    }
    return "";
}

void EmitBegin ()
{
    if (!gCtx.first) gCtx.js->Raw (",\n");
    gCtx.first = false;
    gCtx.js->Raw ("{");
}

void EmitCommonHead (const API_Prim_Head& head)
{
    JsonOut& js = *gCtx.js;
    js.Raw (",");
    js.KVi ("lay", GetAttributeIndex (head.layer));
    js.Raw (",");
    js.KVi ("pen", head.pen.penIndex);
    if (gCtx.inHatchLines) { js.Raw (","); js.KVi ("hl", 1); }
    if (gCtx.inArrow)      { js.Raw (","); js.KVi ("ar", 1); }
}

GSErrCode PrimCallback (const API_PrimElement* prim,
    const void* par1, const void* par2, const void* par3)
{
    if (prim == nullptr || gCtx.js == nullptr)
        return NoError;

    JsonOut& js = *gCtx.js;

    switch (prim->header.typeID) {
        case API_PrimLineID: {
            const API_PrimLine& l = prim->line;
            EmitBegin ();
            js.KVs ("t", "L");
            EmitCommonHead (l.head);
            js.Raw (","); js.KVi ("lt", GetAttributeIndex (l.ltypeInd));
            js.Raw (","); js.KV ("x1", l.c1.x * 1000.0);
            js.Raw (","); js.KV ("y1", l.c1.y * 1000.0);
            js.Raw (","); js.KV ("x2", l.c2.x * 1000.0);
            js.Raw (","); js.KV ("y2", l.c2.y * 1000.0);
            js.Raw ("}");
            gCtx.primCount++;
            break;
        }
        case API_PrimArcID: {
            const API_PrimArc& a = prim->arc;
            EmitBegin ();
            js.KVs ("t", "A");
            EmitCommonHead (a.head);
            js.Raw (","); js.KVi ("lt", GetAttributeIndex (a.ltypeInd));
            js.Raw (","); js.KV ("cx", a.orig.x * 1000.0);
            js.Raw (","); js.KV ("cy", a.orig.y * 1000.0);
            js.Raw (","); js.KV ("r",  a.r * 1000.0);
            js.Raw (","); js.KV ("a0", a.begAng);
            js.Raw (","); js.KV ("a1", a.endAng);
            js.Raw (","); js.KVi ("whole", a.whole ? 1 : 0);
            js.Raw (","); js.KVi ("refl", a.reflected ? 1 : 0);
            js.Raw (","); js.KVi ("solid", a.solid ? 1 : 0);
            js.Raw (","); js.KV ("ang", a.angle);
            js.Raw (","); js.KV ("ratio", a.ratio);
            js.Raw ("}");
            gCtx.primCount++;
            break;
        }
        case API_PrimTextID: {
            const API_PrimText& t = prim->text;
            std::string content;
            if (par1 != nullptr) {
                const unsigned short* u16 = static_cast<const unsigned short*> (par1);
                GS::UniString us (reinterpret_cast<const GS::uchar_t*> (u16));
                content = UniToUtf8 (us);
            }
            if (content.empty () && t.paramInd > 0) {
                // GDLパラメータ参照文字 (通り芯マーカーの通り番号等)
                content = ParamString (gCtx.curParams, t.paramInd, t.ind1, t.ind2);
            }
            if (content.empty ())
                break;
            EmitBegin ();
            js.KVs ("t", "T");
            EmitCommonHead (t.head);
            js.Raw (","); js.KVi ("font", t.font);
            js.Raw (","); js.KVi ("face", t.faceBits);
            js.Raw (","); js.KV ("x", t.loc.x * 1000.0);
            js.Raw (","); js.KV ("y", t.loc.y * 1000.0);
            js.Raw (","); js.KV ("h", t.heightMM);   // 図寸mm
            js.Raw (","); js.KV ("w", t.widthMM);
            js.Raw (","); js.KV ("ang", t.angle);
            js.Raw (","); js.KV ("wf", t.widthFactor);
            js.Raw (","); js.KVi ("anchor", t.anchor);
            js.Raw (","); js.KVs ("s", content);
            js.Raw ("}");
            gCtx.primCount++;
            gCtx.textCount++;
            break;
        }
        case API_PrimPLineID: {
            const API_PrimPLine& p = prim->pline;
            const API_Coord* coords = static_cast<const API_Coord*> (par1);
            const API_PolyArc* arcs = static_cast<const API_PolyArc*> (par3);
            if (coords == nullptr || p.nCoords < 2) break;
            EmitBegin ();
            js.KVs ("t", "PL");
            EmitCommonHead (p.head);
            js.Raw (","); js.KVi ("lt", GetAttributeIndex (p.ltypeInd));
            js.Raw (","); js.Key ("pts"); js.Raw ("[");
            for (Int32 i = 1; i <= p.nCoords; i++) {
                if (i > 1) js.Raw (",");
                js.Raw ("["); js.Num (coords[i].x * 1000.0); js.Raw (",");
                js.Num (coords[i].y * 1000.0); js.Raw ("]");
            }
            js.Raw ("]");
            if (arcs != nullptr && p.nArcs > 0) {
                js.Raw (","); js.Key ("arcs"); js.Raw ("[");
                for (Int32 i = 0; i < p.nArcs; i++) {
                    if (i > 0) js.Raw (",");
                    js.Raw ("["); js.Int (arcs[i].begIndex); js.Raw (",");
                    js.Int (arcs[i].endIndex); js.Raw (",");
                    js.Num (arcs[i].arcAngle); js.Raw ("]");
                }
                js.Raw ("]");
            }
            js.Raw ("}");
            gCtx.primCount++;
            break;
        }
        case API_PrimTriID: {
            const API_PrimTri& tri = prim->tri;
            EmitBegin ();
            js.KVs ("t", "TRI");
            EmitCommonHead (tri.head);
            js.Raw (","); js.KVi ("det", gCtx.hasHatch ? gCtx.fillDet : tri.determination);
            js.Raw (","); js.KVi ("solid", tri.solid ? 1 : 0);
            js.Raw (","); js.Key ("pts"); js.Raw ("[");
            for (int i = 0; i < 3; i++) {
                if (i > 0) js.Raw (",");
                js.Raw ("["); js.Num (tri.c[i].x * 1000.0); js.Raw (",");
                js.Num (tri.c[i].y * 1000.0); js.Raw ("]");
            }
            js.Raw ("]");
            if (gCtx.hasHatch) {
                js.Raw (","); js.KVi ("fillPen", gCtx.fillPen);
                if (gCtx.fillRgbValid) {
                    js.Raw (","); js.Key ("fillRgb"); js.Raw ("[");
                    js.Num (gCtx.fillR); js.Raw (","); js.Num (gCtx.fillG);
                    js.Raw (","); js.Num (gCtx.fillB); js.Raw ("]");
                }
            }
            js.Raw ("}");
            gCtx.primCount++;
            break;
        }
        case API_PrimPolyID: {
            const API_PrimPoly& poly = prim->poly;
            const API_Coord* coords = static_cast<const API_Coord*> (par1);
            const Int32* subEnds = static_cast<const Int32*> (par2);
            if (coords == nullptr || poly.nCoords < 3) break;
            EmitBegin ();
            js.KVs ("t", "PG");
            EmitCommonHead (poly.head);
            js.Raw (","); js.KVi ("det", gCtx.hasHatch ? gCtx.fillDet : poly.determination);
            js.Raw (","); js.KVi ("solid", poly.solid ? 1 : 0);
            js.Raw (","); js.KVi ("fillPen",
                gCtx.hasHatch ? gCtx.fillPen : poly.fillPen.penIndex);
            if (poly.useRgb) {
                js.Raw (","); js.Key ("fillRgb"); js.Raw ("[");
                js.Num (poly.rgbColor.f_red); js.Raw (",");
                js.Num (poly.rgbColor.f_green); js.Raw (",");
                js.Num (poly.rgbColor.f_blue); js.Raw ("]");
            } else if (gCtx.hasHatch && gCtx.fillRgbValid) {
                js.Raw (","); js.Key ("fillRgb"); js.Raw ("[");
                js.Num (gCtx.fillR); js.Raw (","); js.Num (gCtx.fillG);
                js.Raw (","); js.Num (gCtx.fillB); js.Raw ("]");
            }
            js.Raw (","); js.Key ("pts"); js.Raw ("[");
            for (Int32 i = 1; i <= poly.nCoords; i++) {
                if (i > 1) js.Raw (",");
                js.Raw ("["); js.Num (coords[i].x * 1000.0); js.Raw (",");
                js.Num (coords[i].y * 1000.0); js.Raw ("]");
            }
            js.Raw ("]");
            if (subEnds != nullptr && poly.nSubPolys > 1) {
                js.Raw (","); js.Key ("sub"); js.Raw ("[");
                for (Int32 i = 1; i <= poly.nSubPolys; i++) {
                    if (i > 1) js.Raw (",");
                    js.Int (subEnds[i]);
                }
                js.Raw ("]");
            }
            js.Raw ("}");
            gCtx.primCount++;
            break;
        }
        // ---- control codes -------------------------------------------------
        case API_PrimCtrl_HatchBorderBegID: {
            const API_PrimHatchBorder* hb = static_cast<const API_PrimHatchBorder*> (par1);
            gCtx.hasHatch = true;
            if (hb != nullptr) {
                gCtx.fillPen      = hb->fillPen.penIndex;
                gCtx.fillBkgPen   = hb->fillbkgPen;
                gCtx.fillRgbValid = (hb->fillPen.penIndex == 1008);
                gCtx.fillDet      = hb->determination;
                gCtx.fillR = hb->fillRgb.f_red;
                gCtx.fillG = hb->fillRgb.f_green;
                gCtx.fillB = hb->fillRgb.f_blue;
            }
            break;
        }
        case API_PrimCtrl_HatchBorderEndID:
            gCtx.hasHatch = false;
            break;
        case API_PrimCtrl_HatchLinesBegID:
            gCtx.inHatchLines = true;
            break;
        case API_PrimCtrl_HatchLinesEndID:
            gCtx.inHatchLines = false;
            break;
        case API_PrimCtrl_ArrowBegID:
            gCtx.inArrow = true;
            break;
        case API_PrimCtrl_ArrowEndID:
            gCtx.inArrow = false;
            break;
        default:
            break;
    }
    return NoError;
}

// ---------------------------------------------------------------------------
// Dimension export (custom, from element data — NOT ShapePrims)
// ---------------------------------------------------------------------------
std::string NoteContent (const API_NoteType& note, double dimVal)
{
    if (note.contentUStr != nullptr && !note.contentUStr->IsEmpty ())
        return UniToUtf8 (*note.contentUStr);
    if (note.content[0] != 0)
        return std::string (note.content);
    char buf[64];
    snprintf (buf, sizeof buf, "%.0f", dimVal * 1000.0);   // m -> mm
    return std::string (buf);
}

void ExportDimension (JsonOut& js, const API_Element& elem,
    const API_ElementMemo& memo, bool& first)
{
    if (memo.dimElems == nullptr) return;
    const Int32 n = BMGetHandleSize (reinterpret_cast<GSConstHandle> (memo.dimElems))
        / sizeof (API_DimElem);
    if (n < 2) return;
    const API_DimElem* de = *memo.dimElems;

    if (!first) js.Raw (",\n");
    first = false;
    js.Raw ("{");
    js.KVs ("t", "DIM");
    js.Raw (","); js.KVi ("lay", GetAttributeIndex (elem.header.layer));
    js.Raw (","); js.KVi ("pen", elem.dimension.linPen);
    js.Raw (","); js.KV ("dirx", elem.dimension.direction.x);
    js.Raw (","); js.KV ("diry", elem.dimension.direction.y);
    js.Raw (","); js.Key ("pts"); js.Raw ("[");
    for (Int32 i = 0; i < n; i++) {
        if (i > 0) js.Raw (",");
        js.Raw ("{");
        js.KV ("x",  de[i].pos.x * 1000.0);        // 寸法線上の点
        js.Raw (","); js.KV ("y",  de[i].pos.y * 1000.0);
        js.Raw (","); js.KV ("bx", de[i].base.loc.x * 1000.0);  // 測定点 (引出線の根元)
        js.Raw (","); js.KV ("by", de[i].base.loc.y * 1000.0);
        js.Raw (","); js.KV ("wit", de[i].witnessVal * 1000.0);
        js.Raw (","); js.KVi ("witForm", de[i].witnessForm);
        js.Raw (","); js.KV ("val", de[i].dimVal * 1000.0);
        js.Raw (","); js.KVs ("note", NoteContent (de[i].note, de[i].dimVal));
        js.Raw (","); js.KV ("nx", de[i].note.pos.x * 1000.0);
        js.Raw (","); js.KV ("ny", de[i].note.pos.y * 1000.0);
        js.Raw (","); js.KV ("nsize", de[i].note.noteSize);      // 図寸mm
        js.Raw (","); js.KV ("nang", de[i].note.noteAngle);
        js.Raw (","); js.KVi ("nfont", de[i].note.noteFont);
        js.Raw (","); js.KVi ("npen", de[i].note.notePen);
        js.Raw ("}");
    }
    js.Raw ("]}");
}

// ---------------------------------------------------------------------------
// Text fallback (プリミティブから文字が取れなかった要素用)
// ---------------------------------------------------------------------------
void EmitTextRecord (JsonOut& js, bool& first, long lay, long pen, long font,
    double x_m, double y_m, double sizeMM, double angRad, double wf, const std::string& s)
{
    if (s.empty ()) return;
    if (!first) js.Raw (",\n");
    first = false;
    js.Raw ("{");
    js.KVs ("t", "TE");
    js.Raw (","); js.KVi ("lay", lay);
    js.Raw (","); js.KVi ("pen", pen);
    js.Raw (","); js.KVi ("font", font);
    js.Raw (","); js.KV ("x", x_m * 1000.0);
    js.Raw (","); js.KV ("y", y_m * 1000.0);
    js.Raw (","); js.KV ("h", sizeMM);
    js.Raw (","); js.KV ("ang", angRad);
    js.Raw (","); js.KV ("wf", wf);
    js.Raw (","); js.KVs ("s", s);
    js.Raw ("}");
}

void ExportTextElement (JsonOut& js, const API_Element& elem,
    const API_ElementMemo& memo, bool& first)
{
    std::string content;
    if (memo.textContent != nullptr)
        content = UniToUtf8 (GS::UniString (*memo.textContent));
    EmitTextRecord (js, first,
        GetAttributeIndex (elem.header.layer), elem.text.pen, elem.text.font,
        elem.text.loc.x, elem.text.loc.y, elem.text.size,
        elem.text.angle, elem.text.widthFactor, content);
}

void ExportLabel (JsonOut& js, const API_Element& elem,
    const API_ElementMemo& memo, bool& first)
{
    if (elem.label.labelClass != APILblClass_Text)
        return;
    std::string content;
    if (memo.textContent != nullptr)
        content = UniToUtf8 (GS::UniString (*memo.textContent));
    if (content.empty ())
        return;
    const API_TextType& t = elem.label.u.text;
    EmitTextRecord (js, first,
        GetAttributeIndex (elem.header.layer), t.pen, t.font,
        t.loc.x, t.loc.y, t.size, t.angle, t.widthFactor, content);
}

void ExportZoneStamp (JsonOut& js, const API_Element& elem, bool& first)
{
    std::string name = UniToUtf8 (GS::UniString (elem.zone.roomName));
    std::string no   = UniToUtf8 (GS::UniString (elem.zone.roomNoStr));
    std::string s = name;
    if (!no.empty ()) s += "\n" + no;
    EmitTextRecord (js, first,
        GetAttributeIndex (elem.header.layer), elem.zone.pen, 0,
        elem.zone.pos.x, elem.zone.pos.y, 3.0,
        elem.zone.stampAngle, 1.0, s);
}

// ---------------------------------------------------------------------------
// Attribute tables
// ---------------------------------------------------------------------------
void ExportAttributeTables (JsonOut& js)
{
    // layers (名前 + 表示状態: 非表示レイヤーも書き出し、JWW側で非表示として引き継ぐ)
    js.Raw ("\"layers\":{");
    {
        GS::Array<API_Attribute> attrs;
        bool first = true;
        if (ACAPI_Attribute_GetAttributesByType (API_LayerID, attrs) == NoError) {
            for (const API_Attribute& a : attrs) {
                if (!first) js.Raw (",");
                first = false;
                char key[16];
                snprintf (key, sizeof key, "%d", GetAttributeIndex (a.header.index));
                js.Str (key); js.Raw (":");
                js.Str (UniToUtf8 (GS::UniString (a.header.name)));
            }
        }
    }
    js.Raw ("},\n");
    js.Raw ("\"hiddenLayers\":[");
    {
        GS::Array<API_Attribute> attrs;
        bool first = true;
        if (ACAPI_Attribute_GetAttributesByType (API_LayerID, attrs) == NoError) {
            for (const API_Attribute& a : attrs) {
                if ((a.header.flags & APILay_Hidden) == 0) continue;
                if (!first) js.Raw (",");
                first = false;
                js.Int (GetAttributeIndex (a.header.index));
            }
        }
    }
    js.Raw ("],\n");

    // line types
    js.Raw ("\"ltypes\":{");
    {
        GS::Array<API_Attribute> attrs;
        bool first = true;
        if (ACAPI_Attribute_GetAttributesByType (API_LinetypeID, attrs) == NoError) {
            for (const API_Attribute& a : attrs) {
                if (!first) js.Raw (",");
                first = false;
                char key[16];
                snprintf (key, sizeof key, "%d", GetAttributeIndex (a.header.index));
                js.Str (key); js.Raw (":");
                js.Str (UniToUtf8 (GS::UniString (a.header.name)));
            }
        }
    }
    js.Raw ("},\n");

    // pens (rgb 0-255) — アクティブペンテーブルから
    js.Raw ("\"pens\":{");
    {
        bool first = true;
        auto emitPen = [&] (unsigned int index, const API_RGBColor& rgb) {
            if (!first) js.Raw (",");
            first = false;
            char key[16];
            snprintf (key, sizeof key, "%u", index);
            js.Str (key); js.Raw (":[");
            js.Int (static_cast<long> (rgb.f_red   * 255.0 + 0.5)); js.Raw (",");
            js.Int (static_cast<long> (rgb.f_green * 255.0 + 0.5)); js.Raw (",");
            js.Int (static_cast<long> (rgb.f_blue  * 255.0 + 0.5)); js.Raw ("]");
        };
#ifdef ServerMainVers_2700
        UInt32 penCount = 0;
        ACAPI_Attribute_GetPenNum (penCount);
        for (UInt32 i = 1; i <= penCount; i++) {
            API_Pen pen = {};
            pen.index = static_cast<short> (i);
            if (ACAPI_Attribute_GetPen (pen) != NoError) continue;
            emitPen (i, pen.rgb);
        }
#else
        // Archicad 25/26: pens are ordinary attributes (API_PenID); API_Pen does not exist yet.
        API_AttributeIndex penCount = 0;
        if (ACAPI_Attribute_GetNum (API_PenID, &penCount) == NoError) {
            for (API_AttributeIndex i = 1; i <= penCount; i++) {
                API_Attribute attr = {};
                attr.header.typeID = API_PenID;
                attr.header.index = i;
                if (ACAPI_Attribute_Get (&attr) != NoError) continue;
                emitPen (static_cast<unsigned int> (i), attr.pen.rgb);
            }
        }
#endif
    }
    js.Raw ("},\n");

    // fonts
    js.Raw ("\"fonts\":{");
    {
        bool first = true;
        auto emitFont = [&] (Int32 i, const GS::UniString& name) {
            if (name.IsEmpty ()) return;
            if (!first) js.Raw (",");
            first = false;
            char key[16];
            snprintf (key, sizeof key, "%d", i);
            js.Str (key); js.Raw (":");
            js.Str (UniToUtf8 (name));
        };
#ifdef ServerMainVers_2700
        Int32 numFonts = ACAPI_Font_GetFontNum ();
        for (Int32 i = 1; i <= numFonts; i++) {
            API_FontType font = {};
            GS::UniString name;
            font.head.index = i;
            font.head.uniStringNamePtr = &name;
            if (ACAPI_Font_GetFont (font) != NoError) continue;
            emitFont (i, name);
        }
#else
        // Archicad 25/26: fonts are ordinary attributes (API_FontID); ACAPI_Font_* does not exist yet.
        API_AttributeIndex numFonts = 0;
        if (ACAPI_Attribute_GetNum (API_FontID, &numFonts) == NoError) {
            for (API_AttributeIndex i = 1; i <= numFonts; i++) {
                API_Attribute attr = {};
                attr.header.typeID = API_FontID;
                attr.header.index = i;
                if (ACAPI_Attribute_Get (&attr) != NoError) continue;
                emitFont (static_cast<Int32> (i), GS::UniString (attr.header.name));
            }
        }
#endif
    }
    js.Raw ("},\n");
}

} // namespace

// ---------------------------------------------------------------------------
// ExportJwwJsonCommand
// ---------------------------------------------------------------------------
ExportJwwJsonCommand::ExportJwwJsonCommand () :
    CommandBase (CommonSchema::NotUsed)
{
}

GS::String ExportJwwJsonCommand::GetName () const
{
    return "ExportJwwJson";
}

GS::Optional<GS::UniString> ExportJwwJsonCommand::GetInputParametersSchema () const
{
    return R"({
        "type": "object",
        "properties": {
            "outputJsonPath": {
                "type": "string",
                "description": "Absolute path of the JSON intermediate file to write (UTF-8)."
            },
            "database": {
                "type": "string",
                "enum": ["floorPlan", "masterLayout"],
                "description": "Source database. floorPlan (default): current floor plan view. masterLayout: first master layout (for extracting title frames)."
            }
        },
        "additionalProperties": false,
        "required": ["outputJsonPath"]
    })";
}

GS::Optional<GS::UniString> ExportJwwJsonCommand::GetRawResponseSchema () const
{
    return R"({
        "type": "object",
        "properties": {
            "scale": { "type": "number" },
            "guids": { "type": "integer" },
            "elems": { "type": "integer" },
            "dims": { "type": "integer" },
            "prims": { "type": "integer" },
            "errs": { "type": "integer" },
            "dbTypeWas": { "type": "integer" },
            "masterLayouts": { "type": "integer" }
        },
        "additionalProperties": false,
        "required": ["scale", "guids", "elems", "dims", "prims", "errs"]
    })";
}

GS::ObjectState ExportJwwJsonCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    GS::UniString outputJsonPath;
    parameters.Get ("outputJsonPath", outputJsonPath);
    if (outputJsonPath.IsEmpty ()) {
        return CreateErrorResponse (APIERR_BADPARS, "outputJsonPath is required");
    }
    GS::UniString databaseMode = "floorPlan";
    parameters.Get ("database", databaseMode);
    const bool wantMaster = (databaseMode == "masterLayout");

#ifdef _WIN32
    FILE* fp = _wfopen (reinterpret_cast<const wchar_t*> (outputJsonPath.ToUStr ().Get ()), L"wb");
#else
    FILE* fp = fopen (outputJsonPath.ToCStr (CC_UTF8).Get (), "wb");
#endif
    if (fp == nullptr) {
        return CreateErrorResponse (APIERR_GENERAL, "cannot open output file: " + outputJsonPath);
    }
    JsonOut js (fp);

    // 出力対象DBの決定。
    //  floorPlan   : 平面図 (3D等がアクティブなら平面図へ切替)
    //  masterLayout: マスターレイアウト (会社の図面枠テンプレートから枠を取り出す用途)
    API_DatabaseInfo dbInfo = {};
    ACAPI_Database_GetCurrentDatabase (&dbInfo);
    GSErrCode dbErr = NoError;
    const long dbTypeWas = dbInfo.typeID;
    long masterCount = 0;
    if (wantMaster) {
        // マスターレイアウト → 無ければ通常レイアウトの順で探す
        GS::Array<API_DatabaseUnId> masters;
        API_DatabaseUnId* rawIds = nullptr;
        dbErr = ACAPI_Database_GetMasterLayoutDatabases (&rawIds, &masters);
        if (rawIds != nullptr) BMpFree (reinterpret_cast<GSPtr> (rawIds));
        API_WindowTypeID wtype = APIWind_MasterLayoutID;
        if (masters.IsEmpty ()) {
            masters.Clear ();
            rawIds = nullptr;
            dbErr = ACAPI_Database_GetLayoutDatabases (&rawIds, &masters);
            if (rawIds != nullptr) BMpFree (reinterpret_cast<GSPtr> (rawIds));
            wtype = APIWind_LayoutID;
        }
        masterCount = static_cast<long> (masters.GetSize ());
        if (!masters.IsEmpty ()) {
            API_DatabaseInfo mdb = {};
            mdb.typeID = wtype;
            mdb.databaseUnId = masters[0];
            dbErr = ACAPI_Database_ChangeCurrentDatabase (&mdb);
        }
    } else if (dbInfo.typeID != APIWind_FloorPlanID) {
        API_DatabaseInfo planDb = {};
        planDb.typeID = APIWind_FloorPlanID;
        dbErr = ACAPI_Database_ChangeCurrentDatabase (&planDb);
    }

    js.Raw ("{\n");
    double drawingScale = 100.0;
    ACAPI_Drawing_GetDrawingScale (&drawingScale);
    js.Key ("scale"); js.Num (drawingScale); js.Raw (",\n");
    ExportAttributeTables (js);

    // elements
    js.Raw ("\"elements\":[\n");
    gCtx = PrimCtx ();
    gCtx.js = &js;
    bool firstElem = true;

    // 非表示レイヤーの要素も書き出す (JWW側でレイヤー非表示として引き継ぐ)
    GS::Array<API_Guid> guids;
    GSErrCode err = ACAPI_Element_GetElemList (API_ZombieElemID, &guids,
        APIFilt_OnActFloor);
    long nElem = 0, nDim = 0, nErr = 0;
    if (err == NoError) {
        for (const API_Guid& guid : guids) {
            API_Element elem = {};
            elem.header.guid = guid;
            if (ACAPI_Element_Get (&elem) != NoError) { nErr++; continue; }

            if (GetElemTypeId (elem.header) == API_DimensionID) {
                API_ElementMemo memo = {};
                if (ACAPI_Element_GetMemo (guid, &memo) == NoError) {
                    gCtx.first = firstElem;
                    ExportDimension (js, elem, memo, gCtx.first);
                    firstElem = gCtx.first;
                    nDim++;
                }
                ACAPI_DisposeElemMemoHdls (&memo);
                continue;
            }

            const API_ElemTypeID tid = GetElemTypeId (elem.header);
            // GDLパラメータ参照文字の解決とフォールバック用に、文字を持ちうる要素は
            // ShapePrims の前に memo を取っておく
            const bool needMemo = (tid == API_ObjectID || tid == API_LampID ||
                tid == API_ZoneID || tid == API_LabelID || tid == API_TextID ||
                tid == API_WindowID || tid == API_DoorID);
            API_ElementMemo elemMemo = {};
            bool hasMemo = false;
            if (needMemo && ACAPI_Element_GetMemo (guid, &elemMemo) == NoError)
                hasMemo = true;

            gCtx.first        = firstElem;
            gCtx.inHatchLines = false;
            gCtx.inArrow      = false;
            gCtx.hasHatch     = false;
            gCtx.curParams    = hasMemo ? elemMemo.params : nullptr;
            const long textBefore = gCtx.textCount;
            GSErrCode perr = ACAPI_DrawingPrimitive_ShapePrims (elem.header, PrimCallback);
            gCtx.curParams = nullptr;
            firstElem = gCtx.first;
            if (perr != NoError) nErr++;
            else nElem++;
            // 文字がプリミティブとして出なかった要素はフォールバック出力
            if (gCtx.textCount == textBefore && hasMemo) {
                if (tid == API_TextID)
                    ExportTextElement (js, elem, elemMemo, firstElem);
                else if (tid == API_ZoneID)
                    ExportZoneStamp (js, elem, firstElem);
                else if (tid == API_LabelID)
                    ExportLabel (js, elem, elemMemo, firstElem);
            }
            if (hasMemo)
                ACAPI_DisposeElemMemoHdls (&elemMemo);
        }
    }
    js.Raw ("\n],\n");
    js.Key ("stats"); js.Raw ("{");
    js.KVi ("getListErr", err); js.Raw (",");
    js.KVi ("dbErr", dbErr); js.Raw (",");
    js.KVi ("dbTypeWas", dbTypeWas); js.Raw (",");
    js.KVi ("masterLayouts", masterCount); js.Raw (",");
    js.KVi ("guids", static_cast<long> (guids.GetSize ())); js.Raw (",");
    js.KVi ("elems", nElem); js.Raw (",");
    js.KVi ("dims", nDim); js.Raw (",");
    js.KVi ("prims", gCtx.primCount); js.Raw (",");
    js.KVi ("errs", nErr);
    js.Raw ("}}\n");
    fclose (fp);

    GS::ObjectState response;
    response.Add ("scale", drawingScale);
    response.Add ("guids", static_cast<Int32> (guids.GetSize ()));
    response.Add ("elems", static_cast<Int32> (nElem));
    response.Add ("dims", static_cast<Int32> (nDim));
    response.Add ("prims", static_cast<Int32> (gCtx.primCount));
    response.Add ("errs", static_cast<Int32> (nErr));
    response.Add ("dbTypeWas", static_cast<Int32> (dbTypeWas));
    response.Add ("masterLayouts", static_cast<Int32> (masterCount));
    return response;
}
