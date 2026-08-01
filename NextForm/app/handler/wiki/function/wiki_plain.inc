<?php
$WIKI_OPERATORS_INLINE['plain'] = array('open' => '""',  'close' => '""');
$WIKI_OPERATORS_FUNCTION['plain'] = true;

function wiki_operator_plain(&$context, $node, $tree) {
    $is_break = false;
    wiki_parse_parameters($context, $tree, 
			  array('long_flags' => array(
				    'break' => &$is_break,
				    )
			      ));
    $plain_text = wiki_tokens_to_string_without_escape($tree->target);

    if($is_break) {
	$lines = explode("\n", $plain_text);
	$is_first = true;
	foreach($lines as $line) {
	    if($is_first)
		$is_first = false;
	    else
		dom_append_child($node, 'br');
	    dom_append_text($node, $line);
	}
    } else {
	dom_append_text($node, $plain_text);
    }
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[そのまま表示:../../Function/plain]]|&&plain(break){対象テキスト}|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|plain text|&&plain(break){text}|
*/
/**
@manual $manualpage$/ja/Wiki/Function/plain
&title &&plain

*概要
 &plain(break){対象テキスト}
対象テキストをWiki記法ではなく通常のテキストとして表示します．
&&記号を多様する場面等，Wiki記法として解釈されては困る場合に使用します．
対象テキストが一行で収まる場合は&"&"(ダブルクォート2つ)で囲むことで同様の効果が得られます．

*パラメータ
:{break|対象テキスト中の改行をそのまま改行として表示します．
しかし，通常の文章において改行は何の意味も持ちません．
本当に改行に意味があるのならば，
[[整形済みテキスト:../../LineHead#整形済みテキスト]]や[[引用 :../../LineHead#引用]]を使用して下さい．
}

*対象
対象テキストにはそのまま

*例

&wikiexample{
&plain(break){そのまま
表示させたい
&や[[などを含んだ
テキスト
}
}
*/
?>
