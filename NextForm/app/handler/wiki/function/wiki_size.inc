<?php
$WIKI_OPERATORS_FUNCTION['size'] = true;
function wiki_operator_size(&$context, $dom, $tree) {
    $params = wiki_get_parameters_string($tree);
    $size = default_value($params[0], '100%');
    if(preg_match('/^(\d+%|[\d.]+(em|ex|px|pt))$/i', $size))
	wiki_append_element($context, $dom, $tree->target, 'span', array('style' => 'font-size: ' . $size . ';'));
    else
	dom_append_error($dom, l('&size invalid size.'));
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[文字の大きさ:../../Function/size]]|&&size(拡大率%){文字} or &&size(サイズpt){文字}|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|text size|&&size(N%){text} or &&size(Npt){text}|
*/
/**
@manual $manualpage$/ja/Wiki/Function/size
&title &&size

*概要
 &size(拡大率%, サイズpt, サイズpx, サイズem, サイズex){文字}
文字の大きさを変更します．

*注意
[[ファンクションの基本:..#ファンクションの基本 ]]の説明では分かり易さを優先してこのファンクションを例にあげましたが，
このファンクションは非推奨です．文章を記述しているのであれば文字の大きさに意味はありません．
強調したい場合は[[強調の記法:../../Basic#強調]]を，重要性を示したい場合は[[重要の記法:../../Basic#重要]]を使用して下さい．
文章の意味の記述ではなく読みやすさの演出ために使用するものと考えて下さい．

*パラメータ
:サイズ|文字の大きさを数値で指定します．単位は%, pt, px, em, exのいずれかです．

*対象
大きさを変更したい文字

*例

&wikiexample{
&size(150%) { 1.5倍の大きさ }

&size(6em) { 6emの大きさ }
}
*/


/**
@manual $manualpage$/en/Wiki/Function/size
&title &&size

*Overview
 &size(percent%, size pt, size px, size em, size ex){text}
Changes the size of text.

*Note
This function was used as the example in
[[the basics of functions:..#The basics of functions]] because it makes things easy to
follow, but the function itself is not recommended.
If you are writing prose, the size of the text carries no meaning.
Use [[the notation for emphasis:../../Basic#Emphasis]] when you want to emphasize, and
[[the notation for strong:../../Basic#Strong]] when you want to mark importance.
Think of this function as staging for readability, not as a statement of what the
text means.

*Parameters
:size|set the size of the text as a number. The unit is one of %, pt, px, em or ex.

*Target
The text whose size you want to change.

*Example

&wikiexample{
&size(150%) { one and a half times the size }

&size(6em) { the size of 6em }
}
*/
?>
