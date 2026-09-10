<?php
$WIKI_OPERATORS_FUNCTION['wikiexample'] = true;
function wiki_operator_wikiexample(&$context, $dom, $tree) {
    $dl = dom_append_child_with_position($dom, 'dl', $tree);
    $dt = dom_append_child($dl, 'dt');
    dom_append_text($dt, l('Source'));
    $dd = dom_append_child($dl, 'dd');
    $pre = dom_append_child($dd, 'pre');
    dom_append_text($pre, wiki_tokens_to_string_without_escape($tree->target));
    
    $dt = dom_append_child($dl, 'dt');
    dom_append_text($dt, l('Display'));
    $dd = dom_append_child($dl, 'dd');
    $tokens = wiki_tokens_trim_space($tree->target);
    wiki_convert_from_tokens($context, $dd, $tokens);
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[記法の例:../../Function/wikiexample]]|&&wikiexample{対象}|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|notation example|&&wikiexample{target}|
*/
/**
@manual $manualpage$/ja/Wiki/Function/wikiexample
&title &&wikiexample

*概要
 &wikiexample{ 対象 }
対象を「ソース」と「表示」に並べて出します．
「ソース」には書いたとおりの文字が，「表示」にはそれをWiki記法として
変換した結果が出ます．

記法の説明を書くときに使います．同じ原文を2度書かなくて済むので，
説明と実際の出方が食い違いません．このマニュアル自身がこのファンクションで
書かれています．

*パラメータ
ありません．

*対象
説明したいWiki記法．複数行でも構いません．

*例

 &wikiexample{
 - 箇条書きの項目
 - 箇条書きの項目
 }

と書くと，次のように出ます．

&wikiexample{
- 箇条書きの項目
- 箇条書きの項目
}

*種別Markdownでは
種別Markdownのページでは，ファンクションは使えません．
同じことは[[記法の例:../../../Markdown/Extra#記法の例]] (&":::example&") でできます．
*/
/**
@manual $manualpage$/en/Wiki/Function/wikiexample
&title &&wikiexample

*Overview
 &wikiexample{ the target }
Shows the target as ''Source'' and ''Display'', side by side.
''Source'' shows the characters exactly as written; ''Display'' shows the result of
converting them as Wiki notation.

Use it when you explain a notation. The text is written once, so the explanation
and the actual result cannot drift apart. This manual itself is written with it.

*Parameters
None.

*Target
The Wiki notation you want to explain. It may span several lines.

*Example

 &wikiexample{
 - a list item
 - a list item
 }

is shown as:

&wikiexample{
- a list item
- a list item
}

*In Markdown pages
Functions cannot be used on pages of type Markdown.
[[Notation examples:../../../Markdown/Extra#Notation examples]] (&":::example&") does the
same thing there.
*/
?>
