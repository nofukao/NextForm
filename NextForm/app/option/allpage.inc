<?php
$TOOLS['site']['allpage'] = array(
    'order' => 30,
    'show' => 'allpage_tool'
    );

function allpage_tool(&$page, $dom) {
    tools_add_link(
	$dom,
	array('name' => l('All pages'),
	      'seo_follow' => true,
	      'uri' => url_make('', array('option' => 'allpage'))));
}

$OPTIONS['allpage'] = array(
    'show' => 'allpage_show',
    );

function allpage_show(&$args, $dom) {
    $list_options = array(
	'show' => 'table',
	'fields' => '[[pagename]] mtime',
	);
    if(isset($args['tag'])) {
	$tag = trim(default_value($args['tag'], ''));
	title_set_lf('$tag$ tagged pages', array('tag' => $tag));
	$allpages = page_find('', array('tags' => array(array($tag))));
	pagelist_append($dom, $allpages, $list_options);
    } else {
	title_set_l('All pages');
	$p = dom_append_child($dom, 'p');
	$new_args = array('option' => 'allpage');
	if(isset($args['tree'])) {
	    $text = l('Display table style');
	    $list_options['show'] = 'tree';
	    $list_options['fields'] = '[[basename]]';
	} else {
	    $text = l('Display tree style');
	    $new_args['tree'] = 'true';
	}
	dom_append_link($p, url_make('', $new_args), $text);
	$allpages = page_find('', array('is_pagename_only' => true));
	pagelist_append($dom, $allpages, $list_options);
    }
    
    return false;
}
?>
