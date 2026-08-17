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

	/*
	 * 全件を一度に出すと、ヒットが多いときに応答が巨大になる。
	 * 実測で 1077 件のとき 9.6MB あり、ブラウザ側が固まる。
	 * 照合は全件に対して行う (スコア順に並べるため件数が要る)。
	 * ここで絞るのは表示だけ。
	 */
	$result_num = count($search_results);
	$each = SEARCH_RESULTS_PER_PAGE;
	$offset = max(0, (int)default_value($args['pages'], 0));
	$last_offset = $each > 0 ? max(0, (int)ceil($result_num / $each) - 1) : 0;
	if($offset > $last_offset)
	    $offset = $last_offset;
	if($each > 0)
	    $search_results = array_slice($search_results, $offset * $each, $each, true);

	$p = dom_append_child($dom, 'p', 'search_summary');
	dom_append_text($p, lf('$num$ result(s)', array('num' => $result_num)));

	/*
	 * 検索結果は search_page_match() が読み取り権限で絞っている。
	 * 読むのに権限が要るサイトで未ログインだと、本文に確かにある語でも
	 * 「0 件」としか出ず、権限の問題だと分からない。
	 *
	 * 隠れている件数や有無は出さない。それを出すと、読めないページが
	 * 存在することを未ログインの相手に教えることになる。
	 * 0 件のときだけ、常に真である案内を添える。
	 */
	if($result_num == 0 && auth_get_user() === false) {
	    $notice = dom_append_child($dom, 'p', 'search_notice');
	    dom_append_text($notice, l('Logging in may widen what you can search.'));
	}
	if($each > 0 && $result_num > $each) {
	    dom_append_text($p, ' ');
	    dom_append_text($p, lf('($start$-$end$)',
				   array('start' => $offset * $each + 1,
					 'end' => $offset * $each + count($search_results))));
	}

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
	search_append_pagination($dom, $args, $offset, $last_offset);
    }
}

/*
 * 検索結果の前後移動。pagelist_append() の nav と同じ作法に合わせてある
 * (class="pages"、`pages` 引数で何ページ目かを渡す)。
 */
function search_append_pagination($dom, $args, $offset, $last_offset) {
    if($last_offset <= 0)
	return;
    $nav = dom_append_child($dom, 'nav', 'pages');
    unset($args['action']);
    $links = array();
    if($offset > 0) {
	$links[] = array(0, '|' . l('First'), 'first');
	$links[] = array($offset - 1, '<' . l('Prev'), 'prev');
    }
    if($offset < $last_offset) {
	$links[] = array($offset + 1, l('Next') . '>', 'next');
	$links[] = array($last_offset, l('Last') . '|', 'last');
    }
    foreach($links as $link) {
	if($link[0] == 0)
	    unset($args['pages']);
	else
	    $args['pages'] = $link[0];
	dom_append_link($nav, url_make('', $args), $link[1], $link[2]);
    }
}
?>
