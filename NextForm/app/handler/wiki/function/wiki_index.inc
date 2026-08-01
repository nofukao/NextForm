<?php
$WIKI_OPERATORS_FUNCTION['index'] = true;
function wiki_operator_index(&$context, $dom, $tree) {
    if(!empty($context['texts_only']))
	return;

    $pagename = $context['page']['name'];
    wiki_parse_parameters($context, $tree, array('pagename' => &$pagename));
    if(page_is_exists($pagename)) {
	$page = page_read($pagename);
	wiki_summary($page, $dom, true);
    } else {
	dom_append_error($dom, l('&index page not found'));
    }
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[目次:../../Function/index]]|&&index(&[&[ページ名&]&]);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|table of contents|&&index(&[&[page name&]&]);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/index
&title &&index

*概要
 &index([[ページ名]]);
指定されたページの目次を表示します．ページ名が省略された場合は現在のページの目次が表示されます．
目次は&&indexを使用しなくても''ツール''の''目次''をクリックすると表示されます．

*パラメータ
:&[&[ページ名&]&]|指定したページの目次が表示されます．省略した場合は現在のページになります．

*対象
対象は指定しません．

*例

&wikiexample{
&index();
}
*/
?>
