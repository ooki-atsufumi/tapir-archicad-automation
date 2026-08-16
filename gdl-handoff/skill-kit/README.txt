GDLスキルキット (Claude Code用 /gdl スキル)
=============================================

このZIPには、Claude Code に「/gdl 作りたいもの」と入力すると
GDLオブジェクトを自動生成して Archicad に配置するスキルが入っています。

【配置方法】
1. 作業用フォルダを1つ作ります (例: デスクトップの「GDL作業」フォルダ)。
2. このZIPの中身を「フォルダ構成ごと」その中に解凍します。
   解凍後にこうなっていればOKです:

   GDL作業\
     .claude\
       skills\
         gdl\
           SKILL.md            … スキル本体 (Claudeへの指示書)
           gdl_to_archicad.py  … 変換・投入スクリプト
           template.xml        … GDLオブジェクトのXMLテンプレート
     README.txt (このファイル)

3. コマンドプロンプト(またはターミナル)でそのフォルダに移動し、
   claude と入力して Claude Code を起動します。
   例:  cd デスクトップ\GDL作業
        claude

4. Archicad (Tapirアドオン入り) でプロジェクトを開いた状態で、
   Claude Code に次のように入力します:

   /gdl 幅900 奥行450 高さ1800 の3段の本棚を作って

詳しい手順・必要なもの・料金は、同梱の引き継ぎ書HTMLを参照してください。
