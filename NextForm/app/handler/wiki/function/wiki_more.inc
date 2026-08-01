<?php
$WIKI_OPERATORS_FUNCTION['more'] = true;
function wiki_operator_more(&$context, $dom, $tree) {
    if(!empty($context['texts_only']))
	return;

    if(!empty($context['page']['abstract'])) {
	if(!empty($context['root_dom']))
	    $dom = $context['root_dom'];
	$a = dom_append_empty_page_link($dom, $context['page']['name']);
	dom_append_class($a, 'continue');
	if(wiki_tokens_is_whitespace($tree->target)) {
	    dom_append_text($a, l('Continue reading'));
	} else {
	    wiki_convert_from_tokens($context, $a, $tree->target);
	}
	return 'abort';
    }
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[抜粋部分の指定:../../Function/more]]|&&more;|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|seperator of abstract|&&more;|
*/
/**
@manual $manualpage$/ja/Wiki/Function/more
&title &&more

*概要
 &more{抜粋時のリンクのメッセージ};
ページの一覧表示([[&&pages(contents):../pages]]や[[&&datepages:../datepages]])の際に
抜粋として表示される部分を指定します．&&moreより前の部分が抜粋になり，&&more自体はページへのリンクになります．
通常のページの閲覧では&&moreは無視されます．&&moreは他のWiki記法の中には記述しないで下さい．

*パラメータ
パラメータは指定しません． 

*対象
対象には抜粋時のリンクのメッセージを指定できます．省略した場合は"`l('Continue reading')`"になります．

*例
 &more もっと記事を読む...;

*/

?>
