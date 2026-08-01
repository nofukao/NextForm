<?php
$TOOLS['page']['subpage'] = array(
    'order' => 90,
    'show' => 'subpage_tool'
    );

function subpage_tool(&$page, $dom) {
    if(page_get_type($page) != 'file') {
	tools_add_link(
	    $dom,
	    array('name' => l('Subpage'),
		  'disable' => empty($page),
		  'seo_follow' => true,
		  'uri' => url_make($page['name'], array('option' => 'subpage'))));
    }
}

$OPTIONS['subpage'] = array(
    'show' => 'subpage_show',
    );

function subpage_show(&$args, $dom) {
    $page = &main_page();
    title_set_pagename('Subpages of $pagename$', $page['name']);

    $section = dom_append_child($dom, 'section', 'page');
    dom_append_class($section, 'page');

    $ul = dom_append_child($section, 'ul', 'subpage');

    $li = dom_append_child($ul, 'li');
    if(isset($page['name']) && $page['name'] !== '' && page_get_type($page) != 'file')
	$default_name = $page['name'] . '/';
    else
	$default_name = '';
    newpage_dom_append_form($li, $default_name);

    $subpages = page_find($page['name'], array('type' => '!file'));
    pagelist_append($section, $subpages, array('fields' => '[[relname]]', 'base_pagename' => $page['name']));
    $file_subpages = page_find($page['name'], array('type' => 'file'));
    pagelist_append($section, $file_subpages, array('fields' => '[[relname]]', 'base_pagename' => $page['name']));
    if(empty($subpages) && empty($file_subpages))
	message_notice(l('No subpage'));

    return false;
}
?>
