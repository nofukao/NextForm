<?php
$TOOLS['page']['meta'] = array(
    'order' => 110,
    'show' => 'meta_tool'
    );

function meta_tool(&$page, $dom) {
    tools_add_link(
	$dom,
	array('name' => l('Meta info'),
	      'disable' => empty($page),
	      'uri' => url_make($page['name'], array('option' => 'meta'))));
}

$OPTIONS['meta'] = array(
    'show' => 'meta_show',
    'write' => 'meta_write',
    );

function meta_show(&$args, $dom) {
    $page = &main_page();
    $has_permission = auth_has_permission($page['name'], 'write') && lock_warn($page);
    title_set_pagename('Meta info of $pagename$', $page['name']);

    $form = dom_append_page_form($page, $dom,
				 array('option' => 'meta',
				       'action' => 'write',
				     ), 'meta');

    $dl = dom_append_child($form, 'dl', 'form');
    $dt = dom_append_child($dl, 'dt');
    dom_append_text($dt, l('Title'));
    $dd = dom_append_child($dl, 'dd');
    $default_value = isset($page['meta']['title']) ? $page['meta']['title'] : '';
    dom_append_child($dd, 'input', 'title',
		     array('type' => 'text', 'name' => 'title', 'value' => $default_value));
    if(page_get_type($page) === 'wiki') {
	$small = dom_append_child($dd, 'small');
	dom_append_text($small, l('overwritten with &title wiki function'));
    }

    $dt = dom_append_child($dl, 'dt');
    dom_append_text($dt, l('Description'));
    $dd = dom_append_child($dl, 'dd');
    $textarea = dom_append_child($dd, 'textarea', 'description', array('name' => 'description'));
    $default_value = isset($page['meta']['description']) ? $page['meta']['description'] : '';
    dom_append_text($textarea, $default_value);
    $small = dom_append_child($dd, 'small');
    dom_append_text($small, l('referred by search engines'));
    
    $submit = dom_append_child($form, 'input', '', array('type' => 'submit', 'value' => l('Save')));
    if(!$has_permission)
	dom_set_attribute($submit, 'disabled', 'disabled');

    return false;
}

function meta_write(&$args, $dom) {
    $page = &target_page($args);
    title_set_pagename('Meta info of $pagename$', $page['name']);
   
    if(isset($args['title'])) {
	$dummy = dom_create_document();
	handle($page, $dummy, 'show');
	if(!isset($page['title_dom'])) {
	    if($args['title'] === '') {
		unset($page['title']);
		unset($page['meta']['title']);
	    } else
		$page['title'] = trim($args['title']);
	}
    }
    if(isset($args['description'])) {
	if($args['description'] === '')
	    unset($page['description']);
	else
	    $page['description'] = $args['description'];
    }

    $page['keep_mtime'] = true;
    $fp = page_open_contents($page);
    if(page_write($page, $fp, $page['meta']['ticket'])) {
	header('Location: ' . url_make($page['name']));
	exit(0);
	/*
	message_notice(lf('Save meta info of $pagename$.',
			  array('pagename' => $page['name'])));
	*/
    } else {
	message_error(l('Page write failed.'));
    }

    if(isset($args['back_page'])) {
	dom_append_link($dom, url_make($args['back_page']), l('Return'));
    }

    return false;
}
?>
