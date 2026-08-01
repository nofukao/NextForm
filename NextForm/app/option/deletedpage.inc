<?php
$TOOLS['site']['deletedpage'] = array(
    'order' => 60,
    'show' => 'deletedpage_tool'
    );

function deletedpage_tool(&$page, $dom) {
    tools_add_link(
	$dom,
	array('name' => l('Deleted pages'),
	      'uri' => url_make('', array('option' => 'deletedpage'))));
}

$OPTIONS['deletedpage'] = array(
    'show' => 'deletedpage_show',
    );

function deletedpage_show(&$args, $dom) {
    title_set_l('Deleted pages');
    $deleted_pages = page_find_deleted();
    $list_options = array(
	'show' => 'table',
	'fields' => '[[pagename]] \'mtime@' . l('Deleted at') . '\'',
	);
    pagelist_append($dom, $deleted_pages, $list_options);
    return false;
}
?>
