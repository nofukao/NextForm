<?php
$WIKI_OPERATORS_FUNCTION['align'] = true;
function wiki_operator_align(&$context, $dom, $tree) {
    $params = wiki_get_parameters_string($tree);
    $styles = ' ';
    $align = default_value($params[0], '');
    if(in_array($align, array('left', 'center', 'right'))) {
	$styles .= 'text-align: ' . $align . ';';
	$original_styles = dom_get_attribute($dom, 'style');
	$styles = $original_styles . $styles;
	dom_set_attribute($dom, 'style', $styles);
    }

    wiki_convert_from_tokens($context, $dom, $tree->target);
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[文字位置の指定:../../Function/align]]|&&align(left, center, right);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|align|&&align(left, center, right);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/align
&title &&align

*概要
 &align(left, center, right);
現在の段落やセルを左寄せ，中央寄せ，右寄せの表示に変更します．
同時に複数の指定がされた場合は最後に書かれたものが優先されます．

*パラメータ
:left|左寄せにします
:center|中央寄せにします
:right|右寄せにします

*対象
対象は指定しません．

*例

&wikiexample{
&align(left); 左寄せの段落

中央寄せの段落 &align(center);

&align(left); 左寄せとみせかけ右寄せの段落 &align(right);
}
*/
?>
