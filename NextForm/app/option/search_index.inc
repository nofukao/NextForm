<?php
$OPTIONS['search_index'] = array(
    'show' => 'search_index_show',
    'check' => 'search_index_check_show',
    'repair' => 'search_index_repair_write',
    'write' => 'search_index_write',
    );

/*
 * 検査の結果を画面に出す。$is_deep が偽なら集合比較だけの速い検査
 * (実測 967 ページで 0.13 秒) なので、この画面を開くたびに走らせてよい。
 * 真なら全ページの本文を読み直す (同 0.77 秒)。
 */
function search_index_append_result($dom, &$check) {
    $ul = dom_append_child($dom, 'ul', 'search_index_stat');
    $stats = array(
	lf('Index files: $num$ ($size$)',
	   array('num' => $check['file_num'],
		 'size' => encode_prefixed_number($check['bytes']) . 'B')),
	lf('Indexed pages: $num1$ / Actual pages: $num2$',
	   array('num1' => count($check['indexed_pages']), 'num2' => $check['page_num'])),
	);
    foreach($stats as $stat) {
	$li = dom_append_child($ul, 'li');
	dom_append_text($li, $stat);
    }

    $problems = array(
	'broken'  => 'Unreadable index files: $num$',
	'missing' => 'Pages not in the index: $num$',
	'ghosts'  => 'Index entries for pages that no longer exist: $num$',
	'partial' => 'Pages missing some of their index entries: $num$',
	'stale'   => 'Pages with index entries no longer in their text: $num$',
	);
    foreach($problems as $key => $message) {
	if(empty($check[$key]))
	    continue;
	$p = dom_append_child($dom, 'p', 'search_index_problem');
	dom_append_text($p, lf($message, array('num' => count($check[$key]))));

	$ul = dom_append_child($dom, 'ul');
	foreach($check[$key] as $pagename => $count) {
	    $li = dom_append_child($ul, 'li');
	    if($key === 'broken')
		dom_append_text($li, bin2hex($pagename));
	    else
		dom_append_page_link($li, $pagename, $pagename);
	    /*
	     * 欠けているページは、全体を再構築しなくても 1 件ずつ直せる。
	     * 保存時の索引更新は旧本文と新本文の差分しか当てないため、
	     * 一度失われた登録は編集を繰り返しても戻らない。
	     */
	    if($key === 'partial') {
		dom_append_text($li, ' ');
		$form = dom_append_form($li,
					array('option' => 'search_index',
					      'action' => 'repair',
					      'pagename' => $pagename),
					'search_index_repair');
		dom_append_child($form, 'input', '',
				 array('type' => 'submit', 'value' => l('Repair')));
	    }
	}
    }

    $p = dom_append_child($dom, 'p', 'search_index_conclusion');
    if($check['is_ok'])
	dom_append_text($p, l('The index matches the pages.'));
    else
	dom_append_text($p, l('Rebuild the search index to fix this.'));

    if(!$check['is_deep']) {
	$p = dom_append_child($dom, 'p', 'search_index_note');
	dom_append_text($p, l('This check cannot see entries missing from a page that is otherwise indexed.'));
    }
}

function search_index_append_buttons($dom, $is_check_shown) {
    if(!$is_check_shown) {
	$form = dom_append_form($dom,
				array('option' => 'search_index', 'action' => 'check'),
				'search_index_check');
	dom_append_child($form, 'input', '',
			 array('type' => 'submit', 'value' => l('Check in detail')));
    }
    $form = dom_append_form($dom,
			    array('option' => 'search_index', 'action' => 'write'),
			    'search_index');
    dom_append_text($form, l('Rebuild search index'));
    dom_append_child($form, 'input', 'run',
		     array('type' => 'submit', 'value' => l('Run')));
}

function search_index_show(&$args, $dom) {
    /*
     * auth_check ではなく auth_warn なのは上流からの踏襲 (画面自体は開ける)。
     * ただし検査は索引を 1 回読むので、権限が無い相手には走らせない。
     */
    $has_permission = auth_warn(false, 'admin');
    title_set_l('Search index');
    if($has_permission && SEARCH_ENGINE === 'builtin') {
	$check = search_index_check(false);
	search_index_append_result($dom, $check);
    }
    search_index_append_buttons($dom, false);
}

function search_index_check_show(&$args, $dom) {
    auth_check(false, 'admin');
    title_set_l('Search index');
    if(SEARCH_ENGINE === 'builtin') {
	$check = search_index_check(true);
	search_index_append_result($dom, $check);
    }
    search_index_append_buttons($dom, true);
    return false;
}

function search_index_repair_write(&$args, $dom) {
    auth_check(false, 'admin');
    title_set_l('Search index');
    $pagename = default_value($args['pagename'], '');
    if($pagename === '' || !search_index_repair($pagename))
	ld(message_error(), '$pagename$ is not exist.',
	   array('pagename' => dom_pagename($pagename)));
    else
	ld(message_notice(), 'Repaired $pagename$.',
	   array('pagename' => dom_pagename($pagename)));

    /* 直した結果をそのまま見せたいので、もう一度詳しく検査する */
    return search_index_check_show($args, $dom);
}

function search_index_write(&$args, $dom) {
    auth_check(false, 'admin');
    title_set_l('Rebuilding search index..');
    $allpages = page_find('', array('is_pagename_only' => true));

    $show_options = array(
	'progress' => '$num$ / $totalnum$ pages ($percent$%)',
	'no_progress' => 'Rebuilding search index..'
	);

    $alltags = tag_alltags_get();
    run_queue($args, $dom, $show_options,
	      $allpages,
	      'search_index_processor',
	      'search_index_initializer',
	      'search_index_finalizer', $alltags);
    tag_alltags_set($alltags);
}

function search_index_initializer($args, $dom, &$alltags) {
    if(SEARCH_ENGINE === 'builtin')
	cache_delete('', 'search_index_');
    else
	search_engine_index_clear();
    $alltags = array();
    tag_alltags_set($alltags);
    return true;
}

function search_index_processor($args, $dom, &$alltags, &$queue_page) {
    $page = page_read($queue_page['name']);
    $tags = tag_get_page_tags($page);
    foreach($tags as $tag) {
	if(isset($alltags[$tag]))
	    $alltags[$tag]++;
	else
	    $alltags[$tag] = 1;
    }
    if(SEARCH_ENGINE === 'builtin')
	search_page_index_add($page);
    else
	search_engine_index_update($page);
}

function search_index_finalizer($args, $dom, &$alltags) {
    $p = dom_append_child($dom, 'p');
    dom_append_text($p, l('Done.'));
    $p = dom_append_child($dom, 'p');
    dom_append_link($p, url_make('', array('option' => 'admin')), l('Return'));
    return true;
}
?>
