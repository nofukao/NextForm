<?php
$HANDLERS = array();
require_once(APP_DIR_PATH . '/handler/wiki.inc');
require_once(APP_DIR_PATH . '/handler/text.inc');
require_once(APP_DIR_PATH . '/handler/file.inc');

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
    $handle_function = default_value($handler[$action], $handler['show']);

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
