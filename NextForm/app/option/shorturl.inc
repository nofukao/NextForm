<?php
$OPTIONS['shorturl'] = array(
    'show' => 'shorturl_show'
    );

function shorturl_show(&$args, $dom) {
    if(!isset($args['key']))
	return true;
    title_set_l('Shorten URL');

    $cachekey = 'short_url-' . $args['key'];
    if(($pagename = cache_get('', $cachekey)) !== false) {
	shorturl_redirect($pagename);
	return false;
    }

    if(($pagename = shorturl_find($args['key'], page_find('', array('is_pagename_only' => true)))) !== false ||
       ($pagename =  shorturl_find($args['key'], page_find_deleted())) !== false) {
	cache_create('', $cachekey, $pagename);
	shorturl_redirect($pagename);
	return false;
    }

    message_error(l('Page not found.'));
    
    return false;
}

function shorturl_find($key, $pages) {
    foreach($pages as $page) {
	$pkey = shorturl_encode($page['name']);
	if($pkey == $key)
	    return $page['name'];
    }
    return false;
}

function shorturl_encode($pagename) {
    return page_get_shortid($pagename);
}

function shorturl_get_url($pagename, $force = false) {
    return url_make('') . '&' . shorturl_encode($pagename);
}

function shorturl_redirect($pagename) {
    header('HTTP/1.1 302 Found');
    header('Location: ' . url_make($pagename));
    exit(0);
}
?>