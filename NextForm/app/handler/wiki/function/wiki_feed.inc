<?php
$WIKI_OPERATORS_FUNCTION['feed'] = true;
function wiki_operator_feed(&$context, $dom, $tree) {
    if(!empty($context['texts_only']))
	return;

    $pagename = '';
    $num = FEED_DEFAULT_PAGE_NUM;
    wiki_parse_parameters($context, $tree,
			  array('pagename' => &$pagename,
				'number' => &$num,
				'equalities' => array(
				    'num' => &$num,
				    )));

    $a = dom_append_link($dom, url_make($pagename, array('option' => 'feed', 'num' => $num)));
    dom_set_attribute($a, 'class', 'feed');
    dom_append_text($a, '');
    wiki_convert_from_tokens($context, $a, $tree->target);
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[フィード:../../Function/feed]]|&&feed(&[&[ページ名&]&], 件数);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|feed|&&feed(&[&[page name&]&], number of articles);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/feed

&title &&feed

*概要
 &feed([[ページ名]], 件数);
ページの更新情報のフィードを作成し，フィードへのリンクを表示します．
ページ名を指定すると，そのページ以下の子ページのみに対象を限定します．

*パラメータ
:&[&[ページ名&]&]|指定したページ以下の子ページをフィードの対象とします．
:件数|フィードに記載する件数を指定します．デフォルトは`FEED_DEFAULT_PAGE_NUM`です．


*対象
対象は指定しません．

*例

&wikiexample{&feed(10);}
*/

?>
