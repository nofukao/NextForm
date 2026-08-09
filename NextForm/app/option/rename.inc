<?php
$TOOLS['page']['rename'] = array(
    'order' => 80,
    'show' => 'rename_tool'
    );

function rename_tool(&$page, $dom) {
    $pagename = empty($page) ? '' : $page['name'];
    tools_add_link(
	$dom,
	array('name' => l('Rename'),
	      'disable' => empty($page),
	      'uri' => url_make($pagename, array('option' => 'rename'))));
}

$OPTIONS['rename'] = array(
    'show' => 'rename_show',
    'write' => 'rename_write',
    );

function rename_show(&$args, $dom) {
    $page = &main_page();
    $has_permission = auth_warn($page['name'], 'write') && lock_warn($page);
    title_set_pagename('Rename $pagename$', $page['name']);

    $form = dom_append_page_form($page, $dom,
				 array('option' => 'rename',
				       'action' => 'write',
				     ));

    $ul = dom_append_child($form, 'ul', 'form');
    $li = dom_append_child($ul, 'li');
    $label = dom_append_child($li, 'label', 'rename');
    dom_append_text($label, l('Page name'));

    $text = dom_append_child($label, 'input', '', array('type' => 'text', 'name' => 'newpage'));
    dom_set_attribute($text, 'value', $page['name']);

    $submit = dom_append_child($li, 'input', '', array('type' => 'submit', 'value' => l('Run')));
    if(!$has_permission)
	dom_set_attribute($submit, 'disabled', 'disabled');

    $subpages = page_find($page['name'], array('is_pagename_only' => true));
    if(!empty($subpages)) {
	$li = dom_append_child($ul, 'li');
	$label = dom_append_child($li, 'label', 'rename_subpages');
    
	$check = dom_append_child($label, 'input', '',
				  array(
				      'type' => 'checkbox',
				      'name' => 'rename_subpages',
				      'value' => 'on',
				      'checked' => 'checked',
				      ));
	dom_append_text($label, l('Rename follow subpages'));

	pagelist_append($li, $subpages, array('fields' => '[[pagename]]'));
    }
    return false;
}

function rename_write(&$args, $dom) {
    $page = &target_page($args);
    title_set_pagename('Rename $pagename$', $page['name']);
    page_rename($page['name'], $args['newpage'], !empty($args['rename_subpages']));
    return false;
}
?>
