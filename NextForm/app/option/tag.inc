<?php
$TOOLS['page']['tag'] = array(
    'order' => 20,
    'show' => 'tag_tool'
    );

function tag_tool(&$page, $dom) {
    tools_add_link(
	$dom,
	array('name' => l('Tags'),
	      'disable' => empty($page),
	      'uri' => url_make($page['name'], array('option' => 'tag'))));
}

$OPTIONS['tag'] = array(
    'show' => 'tag_show',
    'write' => 'tag_write',
    );

function tag_show(&$args, $dom) {
    $page = &main_page();
    title_set_pagename('Tags for $pagename$', $page['name']);
    $has_permission = auth_warn($page['name'], 'write') && lock_warn($page);

    $form = dom_append_page_form($page, $dom,
				 array('option' => 'tag',
				       'action' => 'write',
				     ), 'tags');

    $ul = dom_append_child($form, 'ul', 'form');
    $li = dom_append_child($ul, 'li');
    $label = dom_append_child($li, 'label', 'tags');
    dom_append_text($label, l('Space separated tags'));

    $tag_string = list_to_string(tag_get_page_tags($page));
    $text = dom_append_child($label, 'input', '', array('type' => 'text', 'name' => 'tag_string'));
    dom_set_attribute($text, 'value', $tag_string);

    $submit = dom_append_child($li, 'input', '', array('type' => 'submit', 'value' => l('Change')));
    if(!$has_permission)
	dom_set_attribute($submit, 'disabled', 'disabled');

    $subpages = page_find($page['name'], array('is_pagename_only' => true));
    if(!empty($subpages)) {
	$li = dom_append_child($ul, 'li');
	$label = dom_append_child($li, 'label', 'tag_subpages');
    
	$check = dom_append_child($label, 'input', '',
				  array(
				      'type' => 'checkbox',
				      'name' => 'tag_subpages',
				      'value' => 'on',
				      ));
	dom_append_text($label, l('Tag follow subpages'));
	pagelist_append($li, $subpages, array('fields' => '[[relname]]', 'base_pagename' => $page['name']));
    }
    return false;
}

function tag_write(&$args, $dom) {
    $page = &target_page($args);
    title_set_pagename('Tags for $pagename$', $page['name']);

    $tags = string_to_list(default_value($args['tag_string'], ''));

    $pages = array($page);
    if(!empty($args['tag_subpages'])) 
	$pages = array_merge($pages, page_find($page['name'], array()));
    
    $is_error = false;
    foreach($pages as &$dpage) {
	$dpage['keep_mtime'] = true;
	if(tag_set_page_tags($dpage, $tags)) {
	    ld(message_notice(), 'Tagged $pagename$.',
	       array('pagename' => dom_pagename($dpage['name'])));
	} else {
	    ld(message_error(), 'Tag $pagename$ failed.',
	       array('pagename' => dom_pagename($dpage['name'])));
	    $is_error = true;
	}
    }

    if(!$is_error && !empty($args['redirect'])) {
	header('Location: ' . $args['redirect']);
	exit(0);
    }
    
    return false;
}

?>
