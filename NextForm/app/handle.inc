<?php
$HANDLERS = array();
require_once(APP_DIR_PATH . '/handler/wiki.inc');
require_once(APP_DIR_PATH . '/handler/text.inc');
require_once(APP_DIR_PATH . '/handler/markdown.inc');
require_once(APP_DIR_PATH . '/handler/file.inc');

/*
 * ハンドラの登録表には 2 種類が混ざっている。
 *
 *   1. 画面を出す action   … show / edit / write など。$handler[$action]($page, $dom)
 *   2. 内部から使う部品     … texts / normalize。handler_function() で引いて直接呼ぶ
 *
 * 2 は URL から呼ぶものではない。引数の数も型も違うため、呼ぶと壊れる。
 * 実際 ?action=normalize は wiki_normalize(&$page, $text) に DOM を渡して
 * Fatal error になり、?action=texts は本文が空のページを返していた。
 *
 * 登録自体は消せない。handler_function($page, 'normalize') が
 * insert / replace / listedit / templateedit から使われており、
 * texts は検索が使っている。**消すとこの 5 つが壊れる。**
 * そこで登録は残したまま、ディスパッチの対象から外す。
 */
$HANDLER_INTERNAL_FUNCTIONS = array(
    'texts'     => true,
    'normalize' => true,
    );

/*
 * $action を URL から呼んでよいか。
 *
 * 実体が無い登録も弾く。上流には $OPTIONS['listedit']['insert'] のように、
 * 存在しない関数を指したままの登録が残っている。呼べば Fatal error になる。
 * 名前を数え上げて塞ぐのではなく、**呼ぶ前に実体を確かめる**ことで、
 * 同じ書き損じが今後増えても 500 にはならないようにする。
 */
function handle_is_dispatchable(&$registry, $action) {
    global $HANDLER_INTERNAL_FUNCTIONS;
    if(empty($registry[$action]))
	return false;
    if(!empty($HANDLER_INTERNAL_FUNCTIONS[$action]))
	return false;
    return function_exists($registry[$action]);
}

function handle(&$page, $dom, $action = '') {
    global $HANDLERS;
    global $POST_HANDLERS;

    $time_start = microtime(true);

    if(empty($page['args']))
	$args = array();
    else {
	$args = &$page['args'];
    }

    if(empty($action)) {
	if(isset($args['action']))
	    $action = $args['action'];
	else
	    $action = 'show';
    }

    if($page['is_exists']) {
	$type = default_value($page['meta']['type'], DEFAULT_PAGE_TYPE);
	if(!empty($args['newpage_type']))
	    message_notice(l('Page already exists.'));

	if($action == 'show' && isset($args['query'])) {
	    $query = search_parse_query($args['query']);
	    $page['query'] = &$query;
	    search_notice_query($page, $dom);
	}

	user_history_add_page_view($page['name']);
    } else {
	$type = default_value($args['newpage_type'], DEFAULT_PAGE_TYPE);
	if($action == 'show') {
	    if(!empty($page['is_main'])) {
		$action = 'edit';

		$form = dom_append_form($dom, array('page' => $page['name']), 'newpage_type', 'GET');
		$label = dom_append_child($form, 'label');
		dom_append_text($label, l('Page type'));
		$options = array();
		foreach($HANDLERS as $htype => &$handler) {
		    $options[$htype] = $htype;
		}
		dom_append_selector($label, 'newpage_type', $options, $type);
		$submit = dom_append_child($form, 'input', 'redundant',
					   array('type' => 'submit', 'value' => l('Change')));
	    } else {
		ld(dom_append_child($dom, 'p'), 'Create page $pagename$',
		   array('pagename' => dom_pagename($page['name'])));
	    }
	}
    }

    if(empty($HANDLERS[$type]))
	$handler = &$HANDLERS[DEFAULT_PAGE_TYPE];
    else
	$handler = &$HANDLERS[$type];
    /*
     * 呼べる action だけをディスパッチする。呼べないものは昔から
     * 「知らない action」と同じ扱いで show に落ちる。$action は書き換えない。
     * 書き換えると sentence_init() が走るかどうかが変わり、
     * 知らない action を渡したときの出力が今までと違ってしまう。
     */
    if(handle_is_dispatchable($handler, $action)) {
	/* 状態を変える action は POST かつ同一オリジンのときだけ通す */
	csrf_check($action);
	$handle_function = $handler[$action];
    } else {
	$handle_function = $handler['show'];
    }

    if($action == 'show')
	sentence_init($page);
    $handle_function($page, $dom);
    if($action == 'show')
	sentence_term($page);

    $page['done_action'] = $action;

    uasort($POST_HANDLERS, 'order_compare');
    foreach($POST_HANDLERS as $name => &$post_handler) {
	$post_handler['handle']($page, $dom);
    }

    $time_end = microtime(true);

    $page['handle_time'] = $time_end - $time_start;
}

function handler(&$page) {
    global $HANDLERS;
    if(empty($page['meta']['type']))
       return false;
    $type = $page['meta']['type'];
    if(empty($HANDLERS[$type]))
	return false;
    return $HANDLERS[$type];
}

function handler_function(&$page, $action) {
    if(($handler = handler($page)) !== false) {
	if(!empty($handler[$action]))
	    return $handler[$action];
    }
    return false;
}

$TOOLS['page']['edit'] = array(
    'order' => 10,
    'show' => 'handle_edit_tool'
    );

function handle_edit_tool(&$page, $dom) {
    $pagename = empty($page) ? '' : $page['name'];
    tools_add_link(
	$dom,
	array('name' => l('Edit'),
	      'accesskey' => 'e',
	      'disable' => empty($page),
	      'uri' => url_make($pagename, array('action' => 'edit'))));
}
?>
