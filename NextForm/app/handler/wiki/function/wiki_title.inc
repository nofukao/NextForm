<?php
$WIKI_OPERATORS_FUNCTION['title'] = true;
function wiki_operator_title(&$context, $dom, $tree) {
    $is_show = false;
    wiki_parse_parameters(
	$context, $tree,
	array(
	    'long_flags' => array(
		'show' => &$is_show,
		)
	    )
	);

    if($is_show)
	wiki_convert_from_tokens($context, $dom, $tree->target);

    $title_dom = dom_create_document();
    $context['page']['title_dom'] = $title_dom;
    wiki_convert_from_tokens($context, $title_dom, $tree->target);
    $context['page']['title'] = trim(dom_text($title_dom));
    title_append_actual($title_dom, dom_pagename($context['page']['name']));
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[タイトルの設定:../../Function/title]]|&&title(show){タイトル}|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|page title|&&title(show){page title}|
*/
/**
@manual $manualpage$/ja/Wiki/Function/title
&title &&title

*概要
 &title(show){ タイトル }
ページのタイトルを変更します．
ページのタイトルは通常はページ名ですが，
このファンクションを用いると好きなタイトルに変更することができます．
ページのどこに記述されていてもかまいませんが，
複数指定された場合は最後のものが優先されます．

*注意
&&titleはページのメタ情報に設定されたタイトルを上書きします．
またwikiのソース上で&&titleを削除してもページのメタ情報にはタイトルが保存されています．
タイトルを削除したい場合は&&titleをソースから削除した上でメタ情報のタイトルも削除して下さい．

*パラメータ
:{show|タイトルを変更するとともにタイトルをテキストとしても用います．
例えば節の見出しと同時にタイトルを変更したい場合に
 * &title(show) { 本当のタイトル }
のように記述します．
}

*対象
対象にはタイトルを書きます．

*例
 &title{ほげほげ}

*/

?>
