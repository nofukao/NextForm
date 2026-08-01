<?php
$OPTIONS['search_index'] = array(
    'show' => 'search_index_show',
    'write' => 'search_index_write',
    );

function search_index_show(&$args, $dom) {
    auth_warn(false, 'admin');
    title_set_l('Rebuild search index');
    $form = dom_append_form($dom, 
			    array('option' => 'search_index', 
				  'action' => 'write'),
			    'search_index');
    dom_append_text($form, l('Rebuild search index'));
    $submit = dom_append_child($form, 'input', 'run', array('type' => 'submit', 'value' => l('Run')));
}

function search_index_write(&$args, $dom) {
    auth_check(false, 'admin');
    title_set_l('Rebuilding search index..');
    $allpages = page_find('', array('is_pagename_only' => true));

    $show_options = array(
	'progress' => '$num$ / $totalnum$ pages ($percent$%)',
	'no_progress' => 'Rebuilding search index..'
	);
    
    $alltags = tag_alltags_get();
    run_queue($args, $dom, $show_options,
	      $allpages,
	      'search_index_processor',
	      'search_index_initializer',
	      'search_index_finalizer', $alltags);
    tag_alltags_set($alltags);
}

function search_index_initializer($args, $dom, &$alltags) {
    if(SEARCH_ENGINE === 'builtin')
	cache_delete('', 'search_index_');
    else
	search_engine_index_clear();
    $alltags = array();
    tag_alltags_set($alltags);
    return true;
}

function search_index_processor($args, $dom, &$alltags, &$queue_page) {
    $page = page_read($queue_page['name']);
    $tags = tag_get_page_tags($page);
    foreach($tags as $tag) {
	if(isset($alltags[$tag]))
	    $alltags[$tag]++;
	else
	    $alltags[$tag] = 1;
    }
    if(SEARCH_ENGINE === 'builtin')
	search_page_index_add($page);
    else
	search_engine_index_update($page);
}

function search_index_finalizer($args, $dom, &$alltags) {
    $p = dom_append_child($dom, 'p');
    dom_append_text($p, l('Done.'));
    $p = dom_append_child($dom, 'p');
    dom_append_link($p, url_make('', array('option' => 'admin')), l('Return'));
    return true;
}
?>
