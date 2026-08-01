<?php
$title_dom = null;
$title_set_enable = true;

function title_set_enable() {
    global $title_set_enable;
    $title_set_enable = true;
}
function title_set_disable() {
    global $title_set_enable;
    $title_set_enable = false;
}

function title_init() {
    global $title_dom;
    $title_dom = dom_create_document();
}
function title_dom() {
    global $title_dom, $title_set_enable;
    if($title_set_enable) {
	title_init();
	return $title_dom;
    } else {
	$dummy = dom_create_document();
	return $dummy;
    }
}

function title_set_text($t) {
    global $title_dom, $title_set_enable;
    if($title_set_enable) {
	title_init();
	dom_append_text($title_dom, $t);
    }
}

function title_set_l($key) {
    global $title_dom, $title_set_enable;
    if($title_set_enable) {
	title_init();
	dom_append_text($title_dom, l($key));
    }
}

function title_set_lf($key, $vars = array()) {
    global $title_dom, $title_set_enable;
    if($title_set_enable) {
	title_init();
	dom_append_text($title_dom, lf($key, $vars));
    }
}

function title_set_ld($key, $vars = array()) {
    global $title_dom, $title_set_enable;
    if($title_set_enable) {
	title_init();
	ld($title_dom, $key, $vars);
    }
}

function title_set_pagename($key, $pagename) {
    global $title_dom, $title_set_enable;
    if($title_set_enable) {
	title_init();
	ld($title_dom, $key, array('pagename' => dom_pagename($pagename)));
    }
}

function title_html() {
    global $title_dom, $title_set_enable;
    return dom_save_html($title_dom);
}

function title_text() {
    global $title_dom, $title_set_enable;
    return dom_text($title_dom);
}

function title_head($page_template, $site_template) {
    global $main_page;
    global $main_raw_pagename;
    if($main_raw_pagename === '' && !empty($main_page)) {
	$template = $site_template;
    } else {
	$template = $page_template;
    }
    $variables = array(
	'pagename' => htmlspecialchars(title_text()),
	'sitename' => htmlspecialchars(SITE_NAME)
	);
    return template_replace_variables($template, $variables);
}

function title_append_actual($dom, $pagename_dom) {
    $small = dom_append_child($dom, 'small', 'actual_title');
    dom_append_text($small, '(');
    ld($small, '$pagename$', array('pagename' => $pagename_dom));
    dom_append_text($small, ')');
}

function title_remove_actual($dom) {
    $xpath = new DOMXPath(dom_owner($dom));
    $smalls = $xpath->query("//*[@class='actual_title']");
    if(!is_null($smalls)) {
	foreach($smalls as $small) {
	    $small->parentNode->removeChild($small);
	}
    }
}

?>
