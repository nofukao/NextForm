<?php
$TOOLS['site']['search'] = array(
    'order' => 70,
    'show' => 'search_site_tool'
    );
$TOOLS['search']['search'] = array(
    'order' => 10,
    'show' => 'search_tool'
    );

function search_site_tool(&$page, $dom) {
    tools_add_link(
	$dom,
	array('name' => l('Search'),
	      'class' => 'search',
	      'uri' => url_make('', array('option' => 'search'))));
}

function search_tool(&$page, $dom) {
    $li = dom_append_child($dom, 'li');
    $args = args_get();
    dom_append_search($li, default_value($args['query'], ''));
}

$OPTIONS['search'] = array(
    'show' => 'search_show',
    );

function search_show(&$args, $dom) {
    title_set_l('Search');
    $query_text = default_value($args['query'], '');

    $form = dom_append_search($dom, $query_text);

    if(page_is_exists(MANUAL_PAGENAME)) {
	$label = dom_append_child($form, 'label');
	$attributes = array(
	    'type' => 'checkbox',
	    'name' => 'ignore_system_page',
	    'value' => 'on',
	    );
	if(!empty($args['ignore_system_page']))
	    $attributes['checked'] = 'checked';
	$check = dom_append_child($label, 'input', '', $attributes);
	dom_append_text($label, l('Ignore manual pages'));
    }
    
    $order = default_value($args['order'], SEARCH_DEFAULT_ORDER);
    $options = array(
	'score' => l('Frequency'),
	'mtime' => l('Last updated'),
	);
    $label = dom_append_child($form, 'label');
    dom_append_text($label, l('Sort'));
    dom_append_selector($label, 'order', $options, $order);

    if(isset($args['query']) && $args['query'] !== '') {
	$query = search_parse_query($query_text);
	$search_results = search($query,
				 array(
				     'is_ignore_system_page' => !empty($args['ignore_system_page']),
				     'order' => $order
				     ));

	if(!empty($args['top']) && !empty($search_results)) {
	    $pagename = array_key_first($search_results);
	    header('Location: ' . url_make($pagename, array('query' => $args['query'])));
	    exit(0);
	}

	$p = dom_append_child($dom, 'p', 'search_summary');
	dom_append_text($p, lf('$num$ result(s)', array('num' => count($search_results))));

	$ul = dom_append_child($dom, 'ul', 'search_result');
	foreach($search_results as $pagename => &$search_result) {
	    $li = dom_append_child($ul, 'li');

	    if(isset($search_result['title']))
		$name = $search_result['title'] . ' (' . $pagename . ')';
	    else
		$name = $pagename;
	    $a = dom_append_page_link($li, $name, $pagename, '', url_make($pagename, array('query' => $args['query'])));

	    $span = dom_append_child($li, 'span', 'mtime');
	    dom_append_text($span, l('Last updated').': ');
	    dom_append_time($span, $search_result['mtime'], TIME_FORMAT); 

	    $span = dom_append_child($li, 'span', 'score');
	    dom_append_text($span, '(' . lf('score: $score$', array('score' => round($search_result['score'], 2))) . ')');

	    
	    if(isset($search_result['snippets']) && $search_result['snippets'] !== '') {
		$p = dom_append_child($li, 'pre');
		dom_append_hightlighted_text($p, implode("\n", $search_result['snippets']), $query);
	    }
	}
    }
}
?>
