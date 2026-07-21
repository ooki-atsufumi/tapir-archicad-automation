

#pragma once











































































#pragma once




'STR#' 32000 "Add-on Name and Description" {
		"Tapir Additional JSON Commands"
		"Tapir Additional JSON Commands"
}

'STR#' 32001 "Add-On Menu" {
        "Tapir"
        "About Tapir...^E3^ES^EE^EI^ED^EL^EW^ET^EM^32503"
}

'STR#' 32002 "Add-On Menu" {
        "Tapir"
        "Tapir Palette^E3^ES^EE^EI^ED^EL^EW^ET^EM"
}

'STR#' 32003 "Add-On Menu" {
        "Tapir"
        "Check for Updates...^E3^ES^EE^EI^ED^EL^EW^ET^EM^32510"
}

'STR#' 32020 "Add-On Menu" {
        "Tapir"
        "Run Script Shortcut 1^E3^ES^EE^EI^ED^EL^EW^ET^EM"
}

'STR#' 32021 "Add-On Menu" {
        "Tapir"
        "Run Script Shortcut 2^E3^ES^EE^EI^ED^EL^EW^ET^EM"
}

'STR#' 32022 "Add-On Menu" {
        "Tapir"
        "Run Script Shortcut 3^E3^ES^EE^EI^ED^EL^EW^ET^EM"
}

'STR#' 32023 "Add-On Menu" {
        "Tapir"
        "Run Script Shortcut 4^E3^ES^EE^EI^ED^EL^EW^ET^EM"
}

'STR#' 32024 "Add-On Menu" {
        "Tapir"
        "Run Script Shortcut 5^E3^ES^EE^EI^ED^EL^EW^ET^EM"
}

'STR#' 32025 "Add-On Menu" {
        "Tapir"
        "Run Script Shortcut 6^E3^ES^EE^EI^ED^EL^EW^ET^EM"
}

'STR#' 32010 "Tapir Palette Strings" {
 "Select Python Scripts"
 "Add"
}

'STR#' 32011 "Tapir Update Strings" {
 "Tapir Update"
 "You are using Tapir version %s. A new version %T is available."
 "Would you like to update to the latest version for the best experience?"
 "Update and Restart Archicad"
 "Continue with this version"
 "Tapir Update"
 "You are using the latest Tapir version %s."
 "OK"
}

'GDLG' 32003 Modal     40   40  200  237  "Tapir" {
 Button                     10  204  180   23    LargePlain "OK"
 Icon                       36   10  128  128    32502
 CenterText                 10  148  180   23    LargeBold vCenter "Tapir Archicad Automation"
 CenterText                 10  171  180   23    LargePlain vCenter "Version: %T, Port: %T"
}

'DLGH' 32003 Tapir_About_Dialog {
1    ""        OkButton
2    ""        TapirLogo
3    ""        TapirText
4    ""        VersionText
}

'GDLG' 32004 Palette | leftCaption | close   0   0  428  28  "Tapir Palette"  {
 IconButton                 0   0  28  28    32503 noFrame
 PopupControl              28   3 260  22    80  3
 IconButton               288   0  28  28    32504 noFrame
 IconButton               316   0  28  28    32506 noFrame
 IconButton               344   0  28  28    32507 noFrame
 IconButton               372   0  28  28    32508 noFrame
 IconButton               400   0  28  28    32511 noFrame
}

'DLGH' 32004 Tapir_Palette {
1    ""        TapirButton
2    ""        ScriptSelection
3    ""        RunButton
4    ""        OpenButton
5    ""        AddButton
6    ""        DelButton
7    ""        ManageShortcutsButton
}

'GDLG' 32041 Palette | leftCaption | close | grow   0   0  600  450  "Tapir Script UI"  {
 Browser                     0   0  600  450
}

'DLGH' 32041 Tapir_ScriptUI_Palette {
1    ""        BrowserItem
}

'GDLG' 32030 Modal | noGrow    80   80  500  270  "Script Shortcuts" {
 LeftText     110    6  210   16    SmallPlain  vCenter  "Script"
 LeftText     330    6  150   16    SmallPlain  vCenter  "Toolbar label (optional)"
 LeftText      10   32   90   18    SmallPlain  vCenter  "Shortcut 1:"
 PopupControl 110   30  210   22    80  3
 TextEdit     330   30  150   22    SmallPlain  60
 LeftText      10   62   90   18    SmallPlain  vCenter  "Shortcut 2:"
 PopupControl 110   60  210   22    80  3
 TextEdit     330   60  150   22    SmallPlain  60
 LeftText      10   92   90   18    SmallPlain  vCenter  "Shortcut 3:"
 PopupControl 110   90  210   22    80  3
 TextEdit     330   90  150   22    SmallPlain  60
 LeftText      10  122   90   18    SmallPlain  vCenter  "Shortcut 4:"
 PopupControl 110  120  210   22    80  3
 TextEdit     330  120  150   22    SmallPlain  60
 LeftText      10  152   90   18    SmallPlain  vCenter  "Shortcut 5:"
 PopupControl 110  150  210   22    80  3
 TextEdit     330  150  150   22    SmallPlain  60
 LeftText      10  182   90   18    SmallPlain  vCenter  "Shortcut 6:"
 PopupControl 110  180  210   22    80  3
 TextEdit     330  180  150   22    SmallPlain  60
 Button       410  224   70   24    LargePlain  "Close"
}

'DLGH' 32030 Tapir_Shortcuts_Dialog {
1    ""        ScriptColumnHeader
2    ""        LabelColumnHeader
3    ""        ShortcutLabel1
4    ""        ShortcutPopUp1
5    ""        ShortcutLabelEdit1
6    ""        ShortcutLabel2
7    ""        ShortcutPopUp2
8    ""        ShortcutLabelEdit2
9    ""        ShortcutLabel3
10   ""        ShortcutPopUp3
11   ""        ShortcutLabelEdit3
12   ""        ShortcutLabel4
13   ""        ShortcutPopUp4
14   ""        ShortcutLabelEdit4
15   ""        ShortcutLabel5
16   ""        ShortcutPopUp5
17   ""        ShortcutLabelEdit5
18   ""        ShortcutLabel6
19   ""        ShortcutPopUp6
20   ""        ShortcutLabelEdit6
21   ""        CloseButton
}
