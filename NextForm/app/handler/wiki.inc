<?php
define('WIKI_URI_REGEX', '((https?|ftp|file)(:\/\/[-_.,!~*\'a-zA-Z0-9;\/?:\@&=+\$%#]+[-_.!~*\'a-zA-Z0-9;\/?:\@&=+\$%#]))');
//'((https?|ftp|file)(:\/\/[-_.,!~*\'a-zA-Z0-9;\/?:\@&=+\$%#]+))' => 'WikiURIToken',// ()
//'([\w\d]+[\w\d\._-]*@[\w\d_-]+[\w\d\._-]+)( +\w*)?( +[\d\.]+)' => 'WikiURIToken',

$HANDLERS['wiki'] = array(
    'show' => 'wiki_show',
    'source' => 'wiki_source',
    'summary' => 'wiki_summary',
    'edit' => 'wiki_edit',
    'write' => 'wiki_write',
    'texts' => 'wiki_texts',
    'normalize' => 'wiki_normalize',
    );
$PAGE_TYPES_TEXTBASE[] = 'wiki';

$WIKI_TOKEN_SYMBOLS = array(
    "'''",
    "''", '/*', '*/', '//', '%%', '[[', ']]', '=:', '""',
    '|', '(', ')', '{', '}', '&', ',', '~', ' ', ';', '#', '<', '>', '_', '^', ':',
    );
$WIKI_TOKEN_REGEX = array(
    '(\*+)'               => 'WikiSymbolToken',
    '([\-\+]+)'           => 'WikiSymbolToken',
    '(=+)'                => 'WikiSymbolToken',
    WIKI_URI_REGEX        => 'WikiURIToken',
    );
$WIKI_TOKEN_REGEX_HEAD = array(
    'f' => true, 'h' => true, '*' => true, '-' => true, '+' => true, '=' => true,
    );

$WIKI_OPERATORS_LINE_HEAD = array(
    'section'    => array('symbol' => '\*+'),
    'list'       => array('symbol' => '[\+\-]+', 'group' => true),
    'deflist'    => array('symbol' => ':',       'group' => true),
    'blockquote' => array('symbol' => '\>',      'group' => true),
    'hr'         => array('symbol' => '={4,}'),
    'through'    => array('symbol' => '~'),
    );

$WIKI_OPERATORS_INLINE = array(
    'em'         => array('open' => "'''", 'close' => "'''"),
    'strong'     => array('open' => "''",  'close' => "''"),
    'del'        => array('open' => '%%',  'close' => '%%'),
    'bracelink'  => array('open' => '[[',  'close' => ']]'),
    );

$WIKI_OPERATORS_FUNCTION = array(
    'root' => true,
    'section' => true,
    'bracelink' => true,
    'blockquote' => true,
    'through' => true,
    'hr' => true,
    'br' => true,
    'em' => true,
    'strong' => true,
    'del' => true,
    );

$WIKI_ENTITY_REFERENCES = array();
$WIKI_OPERATOR_INCLUDE_URI_HANDLERS = array();

require_once(APP_DIR_PATH . '/handler/wiki/wiki_lib.inc');
require_once(APP_DIR_PATH . '/handler/wiki/wiki_parse.inc');
require_once(APP_DIR_PATH . '/handler/wiki/wiki_convert.inc');
foreach(glob_array(APP_DIR_PATH . '/handler/wiki/function/*.inc') as $file) require_once($file);

function wiki_show(&$page, $dom) {
    wiki_convert($page, $dom);
}

function wiki_edit(&$page, $dom) {
    text_edit($page, $dom, 'wiki');
}

function wiki_write(&$page, $dom) {
    $contents = default_value($page['args']['contents'], '');
    $contents = wiki_normalize($page, $contents);
    $page['args']['contents'] = $contents;

    if(!empty($page['args']['preview'])) {
	wiki_convert($page, $dom, $contents);
	text_edit($page, $dom, 'wiki', $contents, default_value($page['args']['ticket'], ''));
    } else {
	text_write($page, $dom, 'wiki');
    }
}

function wiki_source(&$page, $dom) {
    title_set_pagename('Source of $pagename$', $page['name']);
    $pre = dom_append_child($dom, 'pre', 'wiki_source');
    dom_append_text($pre, page_get_contents($page));
}

function wiki_summary(&$page, $dom, $is_link = false) {
    global $WIKI_CONVERT_CACHE;
    $context = wiki_default_context($page);
    $context['summarize_link'] = $is_link || !empty($page['args']['summarize_link']);

    if(!empty($WIKI_CONVERT_CACHE[$page['name']]['tree'])) {
	$tree = &$WIKI_CONVERT_CACHE[$page['name']]['tree'];
    } else {
	$contents = page_get_contents($page);
	$tokens = &wiki_lex($context, $contents);
	if(!empty($tokens) &&
	   (!empty($context['append_line_head']) &&
	    ($context['display'] == 'block' || !ctype_space($tokens[0]->t))))
	    array_unshift($tokens, new WikiLineHeadToken('', $tokens[0]->position, 0));
	$subtrees = &wiki_parse_tokens_to_subtrees($context, $tokens);
	$blocks = &wiki_subtrees_to_blocks($subtrees);
	$tree = &wiki_blocks_to_tree($blocks);
    }
    $dom = dom_append_child($dom, 'section', 'summary', array('data-pagename' => $page['name']));
    wiki_summarize_tree($context, $dom, $tree);
    if(!$dom->hasChildNodes()) {
	$texts_info = wiki_texts($page);
	$text = '';
	if(!empty($texts_info['body'])) {
	    $words = preg_split(LANGUAGE_SEARCH_SPLIT_REGEX, $texts_info['body'][0], -1, PREG_SPLIT_NO_EMPTY);
	    if(!empty($words))
		$text = $words[0];
	}
	if($text === '')
	    $text = $page['name'];
	    
	if($is_link)
	    dom_append_link($dom, url_make($page['name']), $text);
	else
	    dom_append_text($dom, $text);
    }
}

function wiki_texts(&$page) {
    $context = wiki_default_context($page);
    $context['texts_only'] = true;
    $context['nocache'] = true;
    $source = page_get_contents($page);
    $search_dom = dom_create_document();
    wiki_convert_from_source($context, $search_dom, $source, array(), 0);
    $texts_info = array('body' => array());
    dom_texts($search_dom, $texts_info['body']);
    return $texts_info;
}

function wiki_normalize(&$page, $text) {
    $trial_dom = dom_create_document();
    wiki_convert($page, $trial_dom, $text);
    $offset = 0;
    $xpath = new DOMXPath(dom_owner($trial_dom));
    $tables = $xpath->query('//table[@data-twp]');
    if(!is_null($tables)) {
	foreach($tables as $table) {
	    $max_widths = array();
	    $rows = array();

	    $dom_trs = $xpath->query('.//tr[@data-twp]', $table);
	    $trs = array();
	    if(!is_null($dom_trs))
		foreach($dom_trs as $tr)
		    $trs[] = $tr;
	    usort($trs, 'wiki_normalize_table_compare');

	    $targets = array();
	    foreach($trs as $tr) {
		$position = dom_get_attribute($tr, 'data-twp');
		$length = dom_get_attribute($tr, 'data-twl');
		$tr_text = substr($text, $position + $offset, $length);

		if($tr_text[0] != '|' || substr_count($tr_text, "\n") > 1)
		    continue;

		$fields = preg_split('/(?<!\&)\|/', $tr_text);
		$i = 0;
		foreach($fields as &$field) {
		    $field = rtrim($field, ' ');
		    $width = common_strwidth($field);
		    if(!isset($max_widths[$i]) || $width > $max_widths[$i])
			$max_widths[$i] = $width;
		    $i++;
		}
		$targets[] = array(
		    'position' => $position,
		    'length' => $length,
		    'fields' => $fields,
		    );
	    }
	    $max_widths[0] = 0;
	    $max_widths[count($max_widths) - 1] = 0;
	    foreach($targets as $target) {
		$position = $target['position'] + $offset;
		$length = $target['length'];
		$fields = $target['fields'];
		$i = 0;
		$fields_num = count($fields);
		for($i = 0; $i < $fields_num - 1; $i++) {
		    $field = &$fields[$i];
		    $field = $field . str_repeat(' ', max(0, $max_widths[$i] - common_strwidth($field)));
		}
		$tr_text = implode('|', $fields);
		$new_length = strlen($tr_text);
		$text = substr($text, 0, $position) . $tr_text . substr($text, $position + $length);
		$offset += $new_length - $length;
	    }
	}
    }
    return $text;
}

function wiki_normalize_table_compare($a, $b) {
    $av = (int)dom_get_attribute($a, 'data-twp');
    $bv = (int)dom_get_attribute($b, 'data-twp');
    return $av - $bv;
}

function wiki_compare($dom, &$page1, &$page2, &$contents1, &$contents2) {
    $tokens1 = explode("\n", $contents1);
    $tokens2 = explode("\n", $contents2);
    $diff_result = diff($tokens1, $tokens2);

    $article1 = dom_append_child($dom, 'article', 'compare1');
    handle($page1, $article1);
    $article2 = dom_append_child($dom, 'article', 'compare2');
    handle($page2, $article2);
    foreach($diff_result as $diff_token) {
	if($diff_token['change'] === 'delete') {
	    wiki_append_diff_class($page1, $article1, $diff_token, $contents1);
	} else if($diff_token['change'] === 'add') {
	    wiki_append_diff_class($page2, $article2, $diff_token, $contents2);
	}
    }
}

function wiki_append_diff_class(&$page, $dom, $diff, $contents) {
    $near_dom = dom_find_near_node($dom, $diff['position'], $diff['length'], true, '*', '');

    while(!empty($near_dom)) {
	$p = dom_get_attribute($near_dom, 'data-twp');
	$l = dom_get_attribute($near_dom, 'data-twl');
	if($p !== '' && $l !== '' &&
	   $p <= $diff['position'] && $diff['position'] + $diff['length'] <= $p + $l) {
	    break;
	}
	if(!($near_dom->parentNode instanceof DOMElement))
	    return false;
	$near_dom = $near_dom->parentNode;
    }
    if(empty($near_dom))
	return false;

    $part_before = substr($contents, $p, $l);
    $part_after = 
	substr($part_before, 0, $diff['position'] - $p) . 
	substr($part_before, $diff['position'] - $p + $diff['length']);

    $dom_before = dom_create_document();
    $dom_after = dom_create_document();
    wiki_convert($page, $dom_before, $part_before);
    wiki_convert($page, $dom_after, $part_after);
    if(!dom_equals_recursive($dom_before, $dom_after)) {
	dom_append_class($near_dom, $diff['change']);
    }
}

?>
