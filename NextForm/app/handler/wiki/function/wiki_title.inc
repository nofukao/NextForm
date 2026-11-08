<?php
$WIKI_OPERATORS_FUNCTION['title'] = true;
function wiki_operator_title(&$context, $dom, $tree) {
    $is_show = false;
    $is_short = false;
    wiki_parse_parameters(
	$context, $tree,
	array(
	    'long_flags' => array(
		'show' => &$is_show,
		'short' => &$is_short,
		)
	    )
	);

    if($is_show)
	wiki_convert_from_tokens($context, $dom, $tree->target);

    if($is_short) {
	/*
	 * 短いタイトル。ページの題名と見出しには触らず、meta['short_title'] にだけ書く
	 * (page_write)。本文から消せば消える (wiki_normalize)。
	 */
	$short_dom = dom_create_document();
	wiki_convert_from_tokens($context, $short_dom, $tree->target);
	$context['page']['short_title'] = trim(preg_replace('/\s+/u', ' ', dom_text($short_dom)));
	return;
    }

    $title_dom = dom_create_document();
    $context['page']['title_dom'] = $title_dom;
    wiki_convert_from_tokens($context, $title_dom, $tree->target);
    $context['page']['title'] = trim(dom_text($title_dom));
    title_append_actual($title_dom, dom_pagename($context['page']['name']));
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[タイトルの設定:../../Function/title]]|&&title(show){タイトル} &&title(short){短いタイトル}|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|[[page title:../../Function/title]]|&&title(show){page title} &&title(short){short title}|
*/
/**
@manual $manualpage$/ja/Wiki/Function/title
&title &&title

*概要
 &title(show, short){ タイトル }
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
:{short|''短いタイトル''を設定します．ページのタイトルと見出しは変わりません．
短いタイトルは，タイトルから置き場所で分かること(日付など)を除いたものです．
カレンダー([[&&calendar:../calendar]])のマスに出ます．
タイトルと同じく本文が決め，ソースから消して保存すると消えます．
サイトの検索の対象にはなりません．
}

*対象
対象にはタイトルを書きます．

*例
 &title{ほげほげ}

日付をタイトルに含める日記のページで，カレンダーには日付を除いて出す例です．
 &title(short){カレンダーの手直し}
 &title{2026-11-06（金）カレンダーの手直し}
&&title(short) は&&titleより前に書いてください．
&&title(short)を知らない古い版のNextFormでは両方がタイトルとして読まれ，後に書いたほうが使われます．

*/


/**
@manual $manualpage$/en/Wiki/Function/title
&title &&title

*Overview
 &title(show, short){ the title }
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
:{short|set the ''short title''. The title and the heading of the page do not change.
The short title is the title without what its place already tells (a date, for example).
It is shown in the boxes of a calendar ([[&&calendar:../calendar]]).
Like the title it is decided by the text: remove it from the source and save, and it
is removed. It is not searched by the site search.
}

*Target
Write the title as the target.

*Example
 &title{Some title}

A diary page whose title includes the date, showing the title without the date in a
calendar:
 &title(short){Fixing the calendar}
 &title{2026-11-06 (Fri) Fixing the calendar}
Write &&title(short) before &&title.
Older versions of NextForm that do not know &&title(short) read both as the title and
use the one written later.

*/
?>
