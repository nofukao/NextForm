<?php
$OPTIONS['feed'] = array(
    'show' => 'feed_show',
    );

function feed_show(&$args, $dom) {
    global $main_raw_pagename;

    $feed_dom = dom_create_document();

    $main_page =& main_page();
    $pagename = default_value($main_raw_pagename, '');
    $num = default_value($args['num'], FEED_DEFAULT_PAGE_NUM);
    $pages = page_find($pagename, 
		       array('sort' => 'mtime', 'is_pagename_only' => true, 'limit' => $num, 'is_ignore_system_page' => true));
    
    if(isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] == 'on')
	$scheme = 'https://';  
    else
	$scheme = 'http://';
    $path = preg_replace('/\?.*$/', '', $_SERVER['REQUEST_URI']);
    $site_url = $scheme . $_SERVER['HTTP_HOST'] . $path;
    $self_url = $scheme . $_SERVER['HTTP_HOST'] . $_SERVER['REQUEST_URI'];
    $domain = preg_replace('/:.*$/', '', $_SERVER['HTTP_HOST']);
    //$site_id = 'tag:' . $domain . ',2012' . ':' . $path . '/' . urlencode($pagename);

    $feed = dom_append_child($feed_dom, 'feed', '', array('xmlns' => 'http://www.w3.org/2005/Atom'));
    $id = dom_append_child($feed, 'id');
    dom_append_text($id, $site_url);

    if(defined('THEME_IMAGE_ICON') && THEME_IMAGE_ICON !== '') {
	$icon = dom_append_child($feed, 'icon');
	dom_append_text($icon, $site_url . THEME_IMAGE_ICON);
    }
    
    $title = dom_append_child($feed, 'title');
    $subtitle = dom_append_child($feed, 'subtitle');
    if($pagename === '') {
	dom_append_text($title, SITE_NAME);
	dom_append_text($subtitle, lf('$sitename$ updating', array('sitename' => SITE_NAME)));
    } else {
	dom_append_text($title, $pagename . ' - ' . SITE_NAME);
	dom_append_text($subtitle, lf('Subpages of $pagename$ updating', array('pagename' => $pagename)));
    }

    $link = dom_append_child($feed, 'link', '', array('href' => $site_url));
    $link = dom_append_child($feed, 'link', '', array('href' => $self_url, 'rel' => 'self', 'type' => 'application/atom+xml'));
    if(!empty($pages[0]['mtime'])) {
	$time = $pages[0]['mtime'];
	$updated = dom_append_child($feed, 'updated');
	dom_append_text($updated, time_xmlformat($time));
    }

    foreach($pages as &$page) {
	$page = page_read($page['name']);
	//TODO: filter in page_find
	if(isset($page['meta']['last_modify_user']) && $page['meta']['last_modify_user'] !== '') {
	    $user = $page['meta']['last_modify_user'];
	    if(isset($args['withoutuser']) && $args['withoutuser'] === $user) continue;
	} else {
	    $user = 'unkown';
	}
	
	$entry = dom_append_child($feed, 'entry');
	$title = dom_append_child($entry, 'title');
	dom_append_text($title, $page['name']);
	$url = $site_url . url_make($page['name']);
	$title = dom_append_child($entry, 'link', '', array('href' => $url));

	//$site_id = 'tag:' . $domain . ',' . strftime('%F', $page['mtime']) . ':' . $path . '/' . urlencode($page['name']);
	$id = dom_append_child($entry, 'id');
	dom_append_text($id, $url);

	$author = dom_append_child($entry, 'author');
	$name = dom_append_child($author, 'name');
	dom_append_text($name, $user);

	$updated = dom_append_child($entry, 'updated');
	dom_append_text($updated, time_xmlformat($page['mtime']));

	if(!empty($page['meta']['btime'])) {
	    $published = dom_append_child($entry, 'published');
	    dom_append_text($published, time_xmlformat($page['meta']['btime']));
	}
    }
    
    header('Content-type: application/atom+xml');
    $feed_text = dom_save_html($feed_dom);
    echo('<?xml version="1.0" encoding="utf-8"?>');
    echo($feed_text);
    exit(0);
}
?>
