<?php
$TOOLS['page']['debug'] = array(
    'order' => 1000,
    'show' => 'debug_tool'
    );

function debug_tool(&$page, $dom) {
    if(DEBUG_MODE == 'true') {
	tools_add_link(
	    $dom,
	    array('name' => '(' . l('Debug info') . ')',
		  'disable' => empty($page),
		  'uri' => url_make($page['name'], array('option' => 'debuginfo'))));
    }
}

$OPTIONS['debuginfo'] = array(
    'show' => 'debuginfo_show',
    );

function debuginfo_show(&$args, $dom) {
    $page = &main_page();
    title_set_pagename('Debug info of $pagename$', $page['name']);

    $dom = dom_append_child($dom, 'section', 'page');

    $section = dom_append_child($dom, 'section', 'section');
    $h = dom_append_child($section, 'h1');
    dom_append_text($h, l('Basic'));
    $dl = dom_append_child($section, 'dl', 'meta');
    $dt = dom_append_child($dl, 'dt');
    dom_append_text($dt, l('pageid'));
    $dd = dom_append_child($dl, 'dd');
    dom_append_text($dd, $page['id']);
    

    $section = dom_append_child($dom, 'section', 'section');
    $h = dom_append_child($section, 'h1');
    dom_append_text($h, l('Meta info'));
    $dl = dom_append_child($section, 'dl', 'meta');
    foreach($page['meta'] as $key => $value) {
	$dt = dom_append_child($dl, 'dt');
	dom_append_text($dt, $key);
	$dd = dom_append_child($dl, 'dd');
	dom_append_text($dd, $value);
    }

    if(page_get_type($page) == 'wiki') {
	$section = dom_append_child($dom, 'section', 'section');
	$context = wiki_default_context($page);
	$contents = page_get_contents($page);
	$tokens = &wiki_lex($context, $contents);
	array_unshift($tokens, new WikiLineHeadToken('', $tokens[0]->position, 0));

	if(isset($args['token'])) {
	    $h = dom_append_child($section, 'h1');
	    dom_append_text($h, l('Tokens'));
	    $pre = dom_append_child($section, 'pre');
	    dom_append_text($pre, wiki_show_tokens($tokens, "\n"));
	}

	$subtrees = &wiki_parse_tokens_to_subtrees($context, $tokens);
	$blocks = &wiki_subtrees_to_blocks($subtrees);
	$tree = &wiki_blocks_to_tree($blocks);

	$h = dom_append_child($section, 'h1');
	dom_append_text($h, l('Syntax tree'));
	$pre = dom_append_child($section, 'pre');
	dom_append_text($pre, wiki_show_tree($tree, '', $context));
    }

    $section = dom_append_child($dom, 'section', 'section');
    $h = dom_append_child($section, 'h1');
    dom_append_text($h, l('Texts'));
    $texts_info = search_page_texts($page);
    $pre = dom_append_child($dom, 'pre');

    $texts = array();
    if(isset($texts_info['title']))
	$texts[] = l('Title') . ': ' . $texts_info['title'];
    if(isset($texts_info['description']))
	$texts[] = l('Description') . ': ' . $texts_info['description'];
    if(isset($texts_info['tag']))
	$texts[] = l('Tags') . ': ' . $texts_info['tag'];
    $texts = array_merge($texts, $texts_info['body']);
    dom_append_text($pre, implode("\n", $texts));

    return false;
}
?>
