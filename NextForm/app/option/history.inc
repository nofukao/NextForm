<?php
$TOOLS['page']['history'] = array(
    'order' => 50,
    'show' => 'history_tool'
    );

function history_tool(&$page, $dom) {
    if(empty($page)) {
	tools_add_link($dom, array('name' => l('History'), 'disable' => true));
	return;
    }
    tools_add_link(
	$dom,
	array('name' => l('History'),
	      'disable' => count(page_get_backup_times($page)) < 1,
	      'uri' => url_make($page['name'], array('option' => 'history'))));
}

$OPTIONS['history'] = array(
    'show' => 'history_show',
    'page' => 'history_page',
    'diff' => 'history_diff',
    'raw' => 'history_raw',
    );

function history_show(&$args, $dom) {
    $page = &main_page();
    title_set_pagename('History of $pagename$', $page['name']);
    $backup_pages = page_get_backup_pages($page);
    if($page['is_exists'])
	$backup_pages['now'] = $page;

    $backup_pages = array_reverse($backup_pages, true);

    $form = dom_append_form($dom, 
			    array('page' => $page['name'],
				  'option' => 'compare',
				), 'compare', 'GET');

    $table = dom_append_child($form, 'table', 'history');
    dom_append_class($table, 'nosort');
    $thead = dom_append_child($table, 'thead');
    $tbody = dom_append_child($table, 'tbody');

    $tr = dom_append_child($thead, 'tr');
    foreach(array(l('Modification time'), l('Last updated by'), l('Type'), '', '') as $term) {
	$td = dom_append_child($tr, 'td');
	dom_append_text($td, $term);
    }
    $td = dom_append_child($tr, 'td', '', array('colspan' => '2'));
    $submit = dom_append_child($td, 'input', '', array('type' => 'submit', 'value' => l('Compare')));

    foreach($backup_pages as $time => &$backup_page) {
	$tr = dom_append_child($tbody, 'tr');

	$td = dom_append_child($tr, 'td');
	if($time == 'now') {
	    dom_append_time($td, time(), l('Now'));
	    $uri_show = url_make($backup_page['name']);
	    $uri_source = url_make($backup_page['name'], array('action' => 'source'));
	} else {
	    dom_append_time($td, $time, TIME_FORMAT);
	    $uri_show = url_make($page['name'],
				 array('option' => 'history',
				       'action' => 'page',
				       'backuptime' => $time,
				));
	    $uri_source = url_make($page['name'],
				   array('option' => 'history',
					 'action' => 'page',
					 'internal_action' => 'source',
					 'backuptime' => $time,
				       ));
	}

	$td = dom_append_child($tr, 'td');
	dom_append_text($td, default_value($backup_page['meta']['last_modify_user']));

	$td = dom_append_child($tr, 'td');
	dom_append_text($td, $backup_page['meta']['type']);

	$td = dom_append_child($tr, 'td');
	dom_append_link($td, $uri_show, l('Show'));

	$td = dom_append_child($tr, 'td');
	if($backup_page['meta']['type'] != 'file')
	    dom_append_link($td, $uri_source, l('Source'));

	$td = dom_append_child($tr, 'td');
	$radio = dom_append_child($td, 'input', '',
				  array(
				      'type' => 'radio',
				      'name' => 'compare1time',
				      'value' => $time,
				      ));
	$td = dom_append_child($tr, 'td');
	$radio = dom_append_child($td, 'input', '',
				  array(
				      'type' => 'radio',
				      'name' => 'compare2time',
				      'value' => $time,
				      ));
    }
    return false;
}

function history_page(&$args, $dom) {
    global $main_page;

    $page = &target_page($args);
    if(empty($args['backuptime'])) {
	message_error(l('Backup not found.'));
	return;
    }
    $backup_page = page_read_backup($page['name'], $args['backuptime']);
    if(empty($backup_page['is_exists'])) {
	message_error(l('Backup not found.'));
	return;
    }

    if(!empty($args['internal_action']))
	page_handle($backup_page, $dom, $args['internal_action']);
    else
	page_handle($backup_page, $dom);

    if($backup_page['done_action'] == 'source') {
	title_set_ld('Source of old version of $pagename$ at $time$',  
		     array('pagename' => dom_pagename($page['name']),
			   'time' => dom_append_time(null, $args['backuptime'], TIME_FORMAT)));
    } else {
	title_set_ld('Old version of $pagename$ at $time$',  
		     array('pagename' => dom_pagename($page['name']),
			   'time' => dom_append_time(null, $args['backuptime'], TIME_FORMAT)));
    }
    $main_page = $backup_page;
    return false;
}

function history_raw(&$args, $dom) {
    $page = &target_page($args);
    if(empty($args['backuptime'])) {
	message_error(l('Backup not found.'));
	return;
    }
    $backup_page = page_read_backup($page['name'], $args['backuptime']);
    if(empty($backup_page['is_exists'])) {
	message_error(l('Backup not found.'));
	return;
    }

    handle($backup_page, $dom, 'raw');
}

?>
