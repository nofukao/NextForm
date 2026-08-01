<?php
$TOOLS['site']['alltag'] = array(
    'order' => 40,
    'show' => 'alltag_tool'
    );

function alltag_tool(&$page, $dom) {
    tools_add_link(
	$dom,
	array('name' => l('All tags'),
	      'seo_follow' => true,
	      'uri' => url_make('', array('option' => 'alltag'))));
}

$OPTIONS['alltag'] = array(
    'show' => 'alltag_show',
    );

function alltag_show(&$args, $dom) {
    title_set_l('All tags');
    $alltags = tag_alltags_get();
    ksort($alltags, SORT_STRING);

    $table = dom_append_child($dom, 'table', 'tags');
    $thead = dom_append_child($table, 'thead');
    $tbody = dom_append_child($table, 'tbody');

    $tr = dom_append_child($thead, 'tr');
    $th = dom_append_child($tr, 'th', 'tags');
    dom_append_text($th, l('Tags'));
    $th = dom_append_child($tr, 'th', 'numeric');
    dom_append_text($th, l('Number of pages'));

    foreach($alltags as $tag => $num) {
	$tr = dom_append_child($tbody, 'tr');
	$td = dom_append_child($tr, 'td', 'tags');
	dom_append_link($td, url_make('', array('option' => 'allpage', 'tag' => $tag)), $tag);
	$td = dom_append_child($tr, 'td', 'numeric');
	dom_append_text($td, $num);
    }

    return false;
}
?>
