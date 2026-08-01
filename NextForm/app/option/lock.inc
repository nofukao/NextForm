<?php
$TOOLS['page']['lock'] = array(
    'order' => 60,
    'show' => 'lock_tool'
    );

function lock_tool(&$page, $dom) {
    if(empty($page)) {
	$lock_tool = array('name' => l('Lock'), 'disable' => true);
    } else {
	if(!empty($page['meta']['lock'])) {
	    $lock_tool =
		array('name' => l('Unlock'),
		      'uri' => url_make($page['name'], array('option' => 'unlock')));
	} else {
	    $lock_tool =
		array('name' => l('Lock'),
		      'uri' => url_make($page['name'], array('option' => 'lock')));
	}
	if(!auth_has_permission($page['name'], 'lock'))
	    $lock_tool['disable'] = true;
    }
    tools_add_link($dom, $lock_tool);
}

$OPTIONS['lock'] = array(
    'show' => 'lock_show',
    'write' => 'lock_write',
    );
$OPTIONS['unlock'] = array(
    'show' => 'unlock_show',
    'write' => 'unlock_write',
    );

function lock_show(&$args, $dom) {
    $page = &main_page();
    $has_permission = auth_warn($page['name'], 'lock');
    title_set_pagename('Lock $pagename$', $page['name']);

    $form = dom_append_page_form($page, $dom,
				 array('option' => 'lock',
				       'action' => 'write',
				     ), 'lock');

    $ul = dom_append_child($form, 'ul', 'form');
    $li = dom_append_child($ul, 'li');
    dom_append_text($li, lf('Lock $pagename$', array('pagename' => $page['name'])));

    $submit = dom_append_child($li, 'input', '', array('type' => 'submit', 'value' => l('Run')));
    if(!$has_permission)
	dom_set_attribute($submit, 'disabled', 'disabled');

    if(($user = auth_get_user()) !== false) {
	$li = dom_append_child($ul, 'li');
	$label = dom_append_child($li, 'label', 'lock_user');
	$check = dom_append_child($label, 'input', '',
				  array(
				      'type' => 'checkbox',
				      'name' => 'lock_user',
				      'value' => 'on',
				      ));
	dom_append_text($label, lf('Enable only $user$ unlocking', array('user' => $user['name'])));
    }

    $subpages = page_find($page['name'], array('is_pagename_only' => true));
    if(!empty($subpages)) {
	$li = dom_append_child($ul, 'li');
	$label = dom_append_child($li, 'label', 'lock_subpages');
    
	$check = dom_append_child($label, 'input', '',
				  array(
				      'type' => 'checkbox',
				      'name' => 'lock_subpages',
				      'value' => 'on',
				      'checked' => 'checked',
				      ));
	dom_append_text($label, l('Lock follow subpages'));
	pagelist_append($li, $subpages, array('fields' => '[[relname]]', 'base_pagename' => $page['name']));
    }
    return false;
}

function lock_write(&$args, $dom) {
    main_page();
    $page = &target_page($args);
    title_set_pagename('Lock $pagename$', $page['name']);

    $username = '';
    if(!empty($args['lock_user']) && ($user = auth_get_user()) !== false) {
	$username = $user['name'];
    }

    if(page_lock($page, $username)) {
	ld(message_notice(), 'Lock $pagename$.',
	   array('pagename' => dom_pagename($page['name'])));
    } else {
	ld(message_error(), 'Lock $pagename$ failed.',
	   array('pagename' => dom_pagename($page['name'])));
	return;
    }

    if(!empty($args['lock_subpages'])) {
	$subpages = page_find($page['name'], array());
	foreach($subpages as &$subpage) {
	    if(page_lock($subpage, $username)) {
		ld(message_notice(), 'Lock $pagename$.',
		   array('pagename' => dom_pagename($subpage['name'])));
	    } else {
		ld(message_error(), 'Lock $pagename$ failed.',
		   array('pagename' => dom_pagename($subpage['name'])));
	    }
	}
    }

    return false;
}

function unlock_show(&$args, $dom) {
    $page = &main_page();
    $has_permission = auth_warn($page['name'], 'lock');
    title_set_pagename('Unlock $pagename$', $page['name']);

    $form = dom_append_page_form($page, $dom,
				 array('option' => 'unlock',
				       'action' => 'write',
				     ), 'unlock');

    $ul = dom_append_child($form, 'ul', 'form');

    if(($user = auth_get_user()) !== false)
	$username = $user['name'];
    else    
	$username = '';

    if(isset($page['meta']['lock_user']) && $page['meta']['lock_user'] != $username) {
	message_error(lf('This page was locked by $user$.', array('user' => $page['meta']['lock_user'])));
	if(!auth_has_permission($page['name'], 'admin'))
	    $has_permission = false;
    }

    $li = dom_append_child($ul, 'li');
    dom_append_text($li, lf('Unlock $pagename$', array('pagename' => $page['name'])));
    $submit = dom_append_child($li, 'input', '', array('type' => 'submit', 'value' => l('Run')));
    if(!$has_permission)
	dom_set_attribute($submit, 'disabled', 'disabled');

    $subpages = page_find($page['name'], array('is_pagename_only' => true));
    if(!empty($subpages)) {
	$li = dom_append_child($ul, 'li');
	$label = dom_append_child($li, 'label', 'unlock_subpages');
    
	$check = dom_append_child($label, 'input', '',
				  array(
				      'type' => 'checkbox',
				      'name' => 'unlock_subpages',
				      'value' => 'on',
				      'checked' => 'checked',
				      ));
	dom_append_text($label, l('Unlock follow subpages'));
	pagelist_append($li, $subpages, array('fields' => '[[relname]]', 'base_pagename' => $page['name']));
    }
    return false;
}

function unlock_write(&$args, $dom) {
    main_page();
    $page = &target_page($args);
    title_set_pagename('Unlock $pagename$', $page['name']);

    $username = '';
    if(($user = auth_get_user()) !== false)
	$username = $user['name'];
    
    if(page_unlock($page, $username)) {
	ld(message_notice(), 'Unlock $pagename$.',
	   array('pagename' => dom_pagename($page['name'])));
    } else {
	ld(message_error(), 'Unlock $pagename$ failed.',
	   array('pagename' => dom_pagename($page['name'])));
	return;
    }

    if(!empty($args['unlock_subpages'])) 
    {
	$subpages = page_find($page['name'], array());
	foreach($subpages as &$subpage) {
	    if(page_unlock($subpage, $username)) {
		ld(message_notice(), 'Unlock $pagename$.',
		   array('pagename' => dom_pagename($subpage['name'])));
	    } else {
		ld(message_error(), 'Unlock $pagename$ failed.',
		   array('pagename' => dom_pagename($subpage['name'])));
	    }
	}
    }

    return false;
}

?>
