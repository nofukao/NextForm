<?php
$WIKI_OPERATORS_FUNCTION['mtime'] = true;
function wiki_operator_mtime(&$context, $dom, $tree) {
    if(!empty($context['texts_only']))
	return;
    $pagename = $context['page']['name'];
    $values = array();
    wiki_parse_parameters($context, $tree, array('pagename' => &$pagename, 'values' => &$values));
    $last_value = array_pop($values);
    $format = default_value($last_value, TIME_FORMAT);
    $page = page_read($pagename);
    if(!empty($page['mtime']))
	dom_append_text($dom, nf_date($format, $page['mtime']));
    wiki_convert_from_tokens($context, $dom, $tree->target);
}

$WIKI_OPERATORS_FUNCTION['btime'] = true;
function wiki_operator_btime(&$context, $dom, $tree) {
    if(!empty($context['texts_only']))
	return;

    $pagename = $context['page']['name'];
    $values = array();
    wiki_parse_parameters($context, $tree, array('pagename' => &$pagename, 'values' => &$values));
    $last_value = array_pop($values);
    $format = default_value($last_value, TIME_FORMAT);
    $page = page_read($pagename);
    if(!empty($page['meta']['btime']))
	dom_append_text($dom, nf_date($format, $page['meta']['btime']));
    wiki_convert_from_tokens($context, $dom, $tree->target);
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[更新日時:../../Function/mtime]]|&&mtime(&[&[ページ名&]&], フォーマット);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|modification time|&&mtime(&[&[page name&]&], format);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/mtime
&title &&mtime

*概要
 &mtime([[ページ名]], フォーマット);
指定したページの更新日時を指定したフォーマットで表示します．

*パラメータ
:&[&[ページ名&]&]|指定したページの更新日時を表示します．省略した場合は現在のページになります．
:{フォーマット|[[strftimeのフォーマット:http://php.net/manual/ja/function.strftime.php]]に従います．
デフォルトは`TIME_FORMAT`です．}

*対象
対象は指定しません．

*例

&wikiexample{
&mtime();

&mtime([[Top]]);

&mtime(%Y/%m/%d);
}
*/

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[作成日時:../../Function/btime]]|&&btime(&[&[ページ名&]&], フォーマット);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|birth time|&&btime(&[&[page name&]&], format);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/btime
&title &&btime

*概要
 &btime([[ページ名]], フォーマット);
指定したページの作成日時を指定したフォーマットで表示します．

*パラメータ
:&[&[ページ名&]&]|指定したページの作成日時を表示します．省略した場合は現在のページになります．
:{フォーマット|[[strftimeのフォーマット:http://php.net/manual/ja/function.strftime.php]]に従います．
デフォルトは`TIME_FORMAT`です．}

*対象
対象は指定しません．

*例

&wikiexample{
&btime();

&btime([[Top]]);

&btime(%Y/%m/%d);
}
*/

?>
