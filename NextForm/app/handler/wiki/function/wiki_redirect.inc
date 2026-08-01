<?php
$WIKI_OPERATORS_FUNCTION['redirect'] = true;
function wiki_operator_redirect(&$context, $dom, $tree) {
    if(!empty($context['texts_only']))
	return;
    if(!empty($context['redirect']))
	return;

    $pagename = '';
    $timeout = 3;
    wiki_parse_parameters(
        $context, $tree,
        array(
            'pagename' => &$pagename,
	    'number' => &$timeout,
	    'equalities' => array(
		'timeout' => &$timeout,
		),
            )
        );
    if($pagename === '') {
	dom_append_error($dom, l('&redirect pagename not specified'));
	return;
    }

    $nav = dom_append_child($dom, 'nav', 'redirect');
    $message = wiki_tokens_trim_space($tree->target);
    if(empty($message)) {
	ld($nav, 'This page will redirect to $pagename$.', array('pagename' => dom_pagename($pagename)));
    } else {
	wiki_convert_from_tokens($context, $nav, $tree->target);
    }
    
    $context['redirect'] = array(
	'timeout' => $timeout,
	'pagename' => $pagename,
	);
}
/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[ページの転送:../../Function/redirect]]|&&redirect(&[&[ページ名&]&]);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|redirection|&&redirect(&[&[page name&]&]);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/redirect

&title &&redirect
*概要
 &include([[ページ名]], timeout=待ち時間);
指定されたページへ自動的に転送(移動)するようにします．

*パラメータ
:&[&[ページ名&]&]|転送先のページ名を指定します．
:timeout=待ち時間|転送が行われるまでの時間を秒数で指定します．デフォルトは3秒です．

*対象
対象は必須ではありません．対象を書いた場合は転送に際してのメッセージになります．

*例
 &redirect([[NewPage]], timeout=10){このページは[[NewPage]]へ移設されました．このページの情報は古いです．}
*/
?>
