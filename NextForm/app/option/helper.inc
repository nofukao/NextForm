<?php
$OPTIONS['helper'] = array(
    'show' => 'helper_show',
    );

function helper_show(&$args, $dom) {
    $helper_info_completion = cache_serialized_get('', 'wiki-helper-info-completion');
    if($helper_info_completion === false) {
	$helper_info_completion = helper_make_info_completion();
	cache_serialized_create('', 'wiki-helper-info-completion', $helper_info_completion);
    }
    $helper_info_pagenames = helper_make_info_pagenames();

    $helper_info = $helper_info_completion + $helper_info_pagenames;

    header("Content-Type: application/json; charset=utf-8");
    echo(json_encode($helper_info));
    exit(0);
}

function helper_make_info_completion() {
    global $WIKI_OPERATORS_INLINE;
    global $WIKI_OPERATORS_LINE_HEAD;
    global $WIKI_OPERATORS_FUNCTION;
    global $WIKI_ALIASES;
    global $WIKI_ENTITY_REFERENCES;
    global $WIKI_ENTITY_IMAGES;

    $completion_candidates_basic = array(); 
    $completion_candidates_function = array(); 

    $function_names = array();
    $function_names = array_merge($function_names, array_keys($WIKI_OPERATORS_FUNCTION));
    $function_names = array_merge($function_names, array_keys($WIKI_ALIASES));

    $manual_pages = manual_parse();

    $manual_pages_exists = true;
    $cheat_sheet_page_names = array(
	MANUAL_PAGENAME . '/' . LANG . '/Wiki/CheatSheet/Standard',
	MANUAL_PAGENAME . '/' . LANG . '/Wiki/CheatSheet/Function',
	MANUAL_PAGENAME . '/' . LANG . '/Wiki/CheatSheet/FunctionAdvanced',
	);
    foreach($cheat_sheet_page_names as $cheat_sheet_page_name) {
	if(!isset($manual_pages[$cheat_sheet_page_name]))
	    continue;
	$dom = dom_create_document();
	if(!page_is_exists($cheat_sheet_page_name)) {
	    $manual_pages_exists = false;
	}
	
	$page = page_read($cheat_sheet_page_name);
	wiki_convert($page, $dom, $manual_pages[$cheat_sheet_page_name]['body']);

	$trs = dom_get_nodes_by_xpath($dom, '//tbody/tr');
	foreach($trs as $tr) {
	    if($tr->childNodes->length == 2) {
		$example = dom_text($tr->firstChild->nextSibling);
		if($tr->firstChild->nextSibling->firstChild->wholeText === '')
		    $example = ' ' . $example;
		$examples = preg_split('/\s+or\s+/i', $example, -1, PREG_SPLIT_NO_EMPTY);
		//$examples = array_map('trim', $examples);

		$url = '';
		$link = dom_get_node_by_xpath($tr->firstChild, './/a');
		if(!empty($link))
		    $url = dom_get_attribute($link, 'href');

		foreach($WIKI_OPERATORS_INLINE as $name => $inline) {
		    $pattern = preg_quote($inline['open'], '/') . '.*' . preg_quote($inline['close'], '/');
		    if(preg_match('/^' . $pattern . '/', $example)) {
			$completion_candidates_basic[$inline['open']] = 
			    array(
				'type' => 'inline',
				'description' => rtrim(dom_text($tr->firstChild)),
				'examples' => $examples,
				'manual_path' => $url
				);
			break;
		    }
		}

		foreach($WIKI_OPERATORS_LINE_HEAD as $name => $line_head) {
		    $pattern = $line_head['symbol'];
		    if(preg_match('/^(' . $pattern . ')/', $example, $matches)) {
			$completion_candidates_basic[$matches[1]] = 
			    array(
				'type' => 'linehead',
				'description' => rtrim(dom_text($tr->firstChild)),
				'examples' => $examples,
				'manual_path' => $url
				);
			break;
		    }
		}

		foreach($function_names as $function_name) {
		    $pattern = '&' . preg_quote($function_name, '/');
		    if(preg_match('/^' . $pattern . '\b/', $example)) {
			$completion_candidates_function[$pattern] = 
			    array(
				'type' => 'function',
				'description' => rtrim(dom_text($tr->firstChild)),
				'examples' => $examples,
				'manual_path' => $url
				);
			break;
		    }
		}
	    }
	}
    }

    foreach(array_keys($WIKI_ENTITY_IMAGES) as $name) {
	$src = '&' . $name . ';';
	$html_name = htmlspecialchars($name);
	$completion_candidates_function[$src] = 
	    array(
		'type' => 'function',
		'display' => '<i class="entity '  . $html_name . '">' . $html_name .'</i>',
		'examples' => array($src)
		);
    }

    /*
    foreach(array_keys($WIKI_ENTITY_REFERENCES) as $name) {
	$src = '&' . $name . ';';
	$completion_candidates[$src] = 
	    array(
		'display' => $src,
		'examples' => array($src)
		);
    }
    */

    ksort($completion_candidates_function);
    $completion_candidates = $completion_candidates_function + $completion_candidates_basic;
    $helper_info = array(
	'completion' => array(
	    'candidates' => $completion_candidates,
	    //'maxlength' => max(array_map('strlen', array_keys($completion_candidates))),
	    ),
	);
    if($manual_pages_exists) {
	$helper_info['manual_url_base'] = '';
    } else {
	$helper_info['manual_url_base'] = OFFICIAL_URL;
    }
    return $helper_info;
}

function helper_make_info_completion_compare($a, $b) {
    $type_order = array('linehead' => 0, 'inline' => 1, 'function' => 2);
    $a_value = $type_order[$a['type']];
    $b_value = $type_order[$b['type']];
    if($a_value != $b_value)
	return $a_value - $b_value;
    return strcmp($a['examples'][0], $b['examples'][0]);
}


function helper_make_info_pagenames() {
    $tree = array();
    $allpages = page_find('', array('is_pagename_only' => true));
    $pagenames = array();
    foreach($allpages as &$listpage) {
	$pagenames[] = $listpage['name'];
    }
    return array('pagenames' => $pagenames);
}
?>