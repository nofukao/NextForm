<?php
function tag_set_page_tags(&$page, $tags) {
    $uniq_tags = array();
    foreach($tags as $tag) {
	$uniq_tags[$tag] = true;
    }
    $uniq_tags = array_keys($uniq_tags);

    $old_tags = tag_get_page_tags($page);
    
    $page['write_tag'] = list_to_string($uniq_tags);

    $fp = page_open_contents($page);
    $status = page_write($page, $fp, $page['meta']['ticket']);
    if($status)
	tag_alltags_update($old_tags, $uniq_tags);
    return $status;
}

function tag_get_page_tags(&$page) {
    if(!isset($page['meta']['tag']))
	return array();
    return string_to_list($page['meta']['tag']);
}

function tag_get_tags_from_pages(&$pages) {
    $tags = array();
    foreach($pages as &$page) {
	$page_tags = tag_get_page_tags($page);
	foreach($page_tags as $tag) {
	    //printf("%s %s<br>\n", $page['name'], $tag);
	    if(!isset($tags[$tag]))
		$tags[$tag] = 1;
	    else
		$tags[$tag]++;
	}
    }
    return $tags;
}

function tag_alltags_get() {
    $data = cache_serialized_get('', 'alltag');
    if($data !== false)
	return $data;
    return array();
}

function tag_alltags_set(&$alltags) {
    cache_serialized_create('', 'alltag', $alltags);
}

function tag_alltags_update($deleted_tags, $added_tags) {
    $alltags = tag_alltags_get();
    foreach($deleted_tags as $tag) {
	if(isset($alltags[$tag])) {
	    $alltags[$tag]--;
	    if($alltags[$tag] <= 0)
		unset($alltags[$tag]);
	}
    }
    foreach($added_tags as $tag) {
	if(isset($alltags[$tag]))
	    $alltags[$tag]++;
	else
	    $alltags[$tag] = 1;
    }
    tag_alltags_set($alltags);
}

function tag_find($base_pagename, $find_condition) {
    unset($find_condition['is_pagename_only']);
    $pages = page_find($base_pagename, $find_condition);
    return tag_get_tags_from_pages($pages);
}

function tag_append_tag_ul($dom, $tags, $urlpage = '', $urlargs = array()) {
    $ul = dom_append_child($dom, 'ul', 'tags');
    foreach($tags as $tag) {
	$li = dom_append_child($ul, 'li');
	$args = $urlargs;
	$args['tag'] = $tag;
	$a = dom_append_link($li, url_make($urlpage, $args), $tag);
	dom_set_attribute($a, 'rel', 'tag');
	dom_set_attribute($a, 'data-link-pagename', $urlpage);
	dom_set_attribute($a, 'data-link-args', url_make_query($args));
    }
    return $ul;
}

?>
