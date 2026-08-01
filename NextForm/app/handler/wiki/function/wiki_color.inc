<?php
$WIKI_OPERATORS_FUNCTION['color'] = true;
$WIKI_OPERATORS_FUNCTION['bgcolor'] = true;

function wiki_operator_color(&$context, $dom, $tree) {
    $params = wiki_get_parameters_string($tree);
    $color = default_value($params[0], 'red');
    $styles = '';
    if(color_is_valid($color))
	$styles .= 'color: ' . $color . ';';
    if(isset($params[1]) && color_is_valid($params[1]))
	$styles .= 'background-color: ' . $params[1] . ';';

    wiki_append_element($context, $dom, $tree->target, 'span', array('style' => $styles));
}

function wiki_operator_bgcolor(&$context, $dom, $tree) {
    $params = wiki_get_parameters_string($tree);
    $color = default_value($params[0], 'red');
    $styles = ' ';
    if(color_is_valid($color)) {
	$styles .= 'background-color: ' . $color . ';';
	$original_styles = dom_get_attribute($dom, 'style');
	$styles = $original_styles . $styles;
	dom_set_attribute($dom, 'style', $styles);
    }

    wiki_convert_from_tokens($context, $dom, $tree->target);
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[文字の色:../../Function/color]]|&&color(文字色){文字} or &&color(文字色, 背景色){文字}|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|text color|&&color(text color){text} or &&color(text color, background color){text}|
*/
/**
@manual $manualpage$/ja/Wiki/Function/color

&title &&color

*概要
 &color(文字色){ 対象 }
 &color(文字色, 背景色){ 対象 }
文字に色をつけます．文字の背景色も設定できます．

*注意
[[ファンクションの基本:..#ファンクションの基本 ]]の説明では分かり易さを優先してこのファンクションを例にあげましたが，
このファンクションは非推奨です．文章を記述しているのであれば文字の色に意味はありません．
強調したい場合は[[強調の記法:../../Basic#強調]]を，重要性を示したい場合は[[重要の記法:../../Basic#重要]]を使用して下さい．
文章の意味の記述ではなく読みやすさの演出ために使用するものと考えて下さい．

*パラメータ
:{文字色|文字の色を指定します．
red, blueなどの色の名前や#f84, #ff8844の形式が使用できます．省略された場合はredが指定されます．}
:背景色|文字の背景色を指定します．

*対象
色を変更したい文字

*例

&wikiexample{
青い文字は&color(blue){このように}書きます．
背景色も&color(navy,#aaff00)このように
設定できます．
}
*/

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[段落等の背景色:../../Function/bgcolor]]|&&bgcolor(背景色);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|paragraph background color|&&bgcolor(background color);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/bgcolor

&title &&bgcolor

*概要
 &bgcolor(背景色);
現在の段落やセルの背景色を変更します．
同時に複数の指定がされた場合は最後に書かれたものが優先されます．

*パラメータ
:背景色|文字の背景色を指定します．red, blueなどの色の名前や#f84, #ff8844の形式が使用できます．

*対象
対象は指定しません．

*例

&wikiexample{
&bgcolor(#8f3); 色のついた段落
}
*/


?>
