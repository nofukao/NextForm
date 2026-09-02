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
タイトルを決めるのは''本文''です．
&&titleはページのメタ情報に設定されたタイトルを上書きします．
ソースから&&titleを削除して保存すると，''メタ情報のタイトルも消えます''．
メタ情報の画面で付けたタイトルも，次に本文を保存したときに置き換わります．
ソースを見ればページの状態が分かるようにするためです．

種別Markdownのページのフロントマター(""title:"")も同じ約束です．

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


/**
@manual $manualpage$/en/Wiki/Function/title
&title &&title

*Overview
 &title(show){ the title }
Changes the title of the page.
The title of a page is normally its page name, but this function changes it to any
title you like.
It may be written anywhere in the page; when several are given, the last one wins.

*Note
It is ''the text'' that decides the title.
&&title overrides the title set in the metadata of the page.
Remove &&title from the source and save, and ''the title in the metadata is
removed too''.
A title set on the metadata screen is replaced the next time the text is saved.
This is so that the state of a page can be seen from its source.

The front matter (""title:"") of a page of type Markdown follows the same promise.

*Parameters
:{show|change the title and use the title as text as well.
For example, to change the title and make a section heading at the same time, write
 * &title(show) { the real title }
}

*Target
Write the title as the target.

*Example
 &title{Some title}

*/
?>
