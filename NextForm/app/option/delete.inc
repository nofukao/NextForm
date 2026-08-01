<?php
$TOOLS['page']['delete'] = array(
    'order' => 70,
    'show' => 'delete_tool'
    );

function delete_tool(&$page, $dom) {
    tools_add_link(
	$dom,
	array('name' => l('Delete'),
	      'disable' => empty($page),
	      'uri' => url_make($page['name'], array('option' => 'delete'))));
}

$OPTIONS['delete'] = array(
    'show' => 'delete_show',
    'write' => 'delete_write',
    );

function delete_show(&$args, $dom) {
    $page = &main_page();
    $has_permission = auth_warn($page['name'], 'write') && lock_warn($page);
    $has_admin_permission = auth_has_permission($page['name'], 'admin');
    title_set_pagename('Delete $pagename$', $page['name']);

    $form = dom_append_page_form($page, $dom,
				 array('option' => 'delete',
				       'action' => 'write',
				     ), 'deletepage');

    $ul = dom_append_child($form, 'ul', 'form');
    $li = dom_append_child($ul, 'li');
    dom_append_text($li, lf('Delete $pagename$', array('pagename' => $page['name'])));

    $submit = dom_append_child($li, 'input', '', array('type' => 'submit', 'value' => l('Run')));
    if(!$has_permission)
	dom_set_attribute($submit, 'disabled', 'disabled');

    $subpages = page_find($page['name'], array('is_pagename_only' => true));
    if(!empty($subpages)) {
	$li = dom_append_child($ul, 'li');
	$label = dom_append_child($li, 'label', 'delete_subpages');
    
	$check = dom_append_child($label, 'input', '',
				  array(
				      'type' => 'checkbox',
				      'name' => 'delete_subpages',
				      'value' => 'on',
				      ));
	dom_append_text($label, l('Delete follow subpages'));
	pagelist_append($li, $subpages, array('fields' => '[[relname]]', 'base_pagename' => $page['name']));
	foreach($subpages as $subpage) {
	    if(!auth_has_permission($subpage['name'], 'admin'))
		$has_admin_permission = false;
	}
    }

    if($has_admin_permission) {
	$li = dom_append_child($ul, 'li');
	$label = dom_append_child($li, 'label', 'delete_truncate');
	$check = dom_append_child($label, 'input', '',
				  array(
				      'type' => 'checkbox',
				      'name' => 'delete_truncate',
				      'value' => 'on',
				      ));
	dom_append_text($label, l('Delete history too'));
	dom_set_attribute($check, 'onchange', "if(this.checked) {this.checked = confirm('".l('Are you OK?')."');}");
    }

    return false;
}

function delete_write(&$args, $dom) {
    $page = &target_page($args);
    title_set_pagename('Delete $pagename$', $page['name']);

    $pages = array($page);
    if(!empty($args['delete_subpages']))
	$pages = array_merge($pages, page_find($page['name'], array()));

    $delete_function = 'page_delete';
    $action = 'write';
    if(!empty($args['delete_truncate']) && $args['delete_truncate'] === 'on') {
	$delete_function = 'page_truncate';
	$action = 'admin';
    }
    foreach($pages as &$dpage) {
	if(auth_has_permission($dpage['name'], $action) && $delete_function($dpage)) {
	    message_notice(lf('Delete $pagename$.',
			      array('pagename' => $dpage['name'])));
	} else {
	    message_error(lf('Delete $pagename$ failed.',
			     array('pagename' => $dpage['name'])));
	}
    }

    if(isset($args['back_page'])) {
	dom_append_link($dom, url_make($args['back_page']), l('Return'));
    }

    return false;
}
?>
