<?php
/*
 * タグの画面 (option/tag.inc) からの書き込み。
 *
 * タグ一覧の件数を直すのは page_write() の側。write_tag を使う経路が
 * ここだけではなくなったため (Markdown のフロントマター)、両方で呼ぶと
 * 二重に数えてしまう。
 */
function tag_set_page_tags(&$page, $tags) {
    $uniq_tags = array();
    foreach($tags as $tag) {
	$uniq_tags[$tag] = true;
    }
    $page['write_tag'] = list_to_string(array_keys($uniq_tags));

    $fp = page_open_contents($page);
    return page_write($page, $fp, $page['meta']['ticket']);
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

/*
 * タグ一覧 (?option=alltag) に出すタグと件数。
 *
 * 件数の控え (tag_alltags_get()) はサイト全体で数えてあり、見る人の権限を
 * 知らない。ページごとの権限があると、読めないページにしか付いていないタグと
 * その件数まで見えてしまうので、そういう人には読めるページから数え直して出す。
 * どのページも同じ権限の人 (ページごとの権限が無い人) は、全部読めるか
 * 全部読めないかなので、控えをそのまま使うか、何も出さないかで済む。
 */
function tag_alltags_for_viewer() {
    $permission = auth_get_permission();
    if(!is_array($permission))
	return auth_has_permission(DEFAULT_PAGENAME, 'read') ? tag_alltags_get() : array();
    return tag_find('', array());
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
