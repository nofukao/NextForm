<?php
$TOOLS['page']['compare'] = array(
    'order' => 40,
    'show' => 'compare_tool'
    );

function compare_tool(&$page, $dom) {
    $tool = array('name' => l('Modification'), 'disable' => empty($page));
    $backuptimes = page_get_backup_times($page);
    if(page_is_exists($page['name']) && !empty($backuptimes)) {
	$backuptime = max($backuptimes);

	$modification_mode = 'lastchange';
	
	if(USER_HISTORY_ENABLE == 'true') {
	    $lastview_time = user_history_get_page_view_preload($page['name']);
	    $candidate_time = 0;
	    foreach($backuptimes as $time) {
		if($time < $lastview_time)
		    $candidate_time = $time;
	    }
	    if($candidate_time != $backuptime) {
		$modification_mode = 'lastview';
		$backuptime = $candidate_time;
	    }
	}
	
	$tool['uri'] = url_make($page['name'],
				array('option' => 'compare', 
				      'compare1time' => $backuptime,
				      'compare2time' => 'now',
				      'modification' => $modification_mode));
    } else {
	$tool['disable'] = true;
    }
    tools_add_link($dom, $tool);
}

$OPTIONS[ 'compare'] = array(
    'show' => 'compare_show',
    );

function compare_show(&$args, $dom) {
    $page = &main_page();
    $comparable_types = array('wiki', 'text');

    $pagename1 = $page['name'];
    $pagename2 = $page['name'];
    $compare1time = default_value($args['compare1time'], 'now');
    $compare2time = default_value($args['compare2time'], 'now');

    if($pagename1 == $pagename2 && $compare1time > $compare2time) {
	$t = $compare1time;
	$compare1time = $compare2time;
	$compare2time = $t;
    } 

    $page1 = page_read_backup($pagename1, $compare1time);
    $page2 = page_read_backup($pagename2, $compare2time);
    if(empty($page1['is_exists']) || empty($page2['is_exists']))
	return false;

    if(!empty($args['modification'])) {
	if($args['modification'] == 'lastview') {
	    title_set_ld('Compare $pagename$ with the last view',
			 array('pagename' => dom_pagename($pagename1)));
	} else {
	    title_set_ld('Newest modification of $pagename$',
			 array('pagename' => dom_pagename($pagename1)));
	}
    } else {
	$link1 = dom_create('a');
	dom_set_attribute($link1, 'href', url_make($pagename1,
						   array('option' => 'history',
							 'action' => 'page',
							 'backuptime' => $compare1time)));
	ld($link1, '$pagename$ (at $time$)', 
	   array('pagename' => $pagename1,
		 'time' => dom_append_time(null, $compare1time, TIME_FORMAT)));

	$link2 = dom_create('a');
	dom_set_attribute($link2, 'href', url_make($pagename2,
						   array('option' => 'history',
							 'action' => 'page',
							 'backuptime' => $compare2time)));
	ld($link2, '$pagename$ (at $time$)', 
	   array('pagename' => $pagename2,
		 'time' => dom_append_time(null, $compare2time, TIME_FORMAT)));

	title_set_ld('Difference between $pagename1$ and $pagename2$',
		     array('pagename1' => $link1,  'pagename2' => $link2));
    }

    if(page_type_is_text_base($page1['meta']['type']) &&
       page_type_is_text_base($page2['meta']['type'])) {
	$contents1 = page_get_contents($page1);
	$contents2 = page_get_contents($page2);

	if(empty($args['compare_source']) &&
	   $page1['meta']['type'] == 'wiki' &&
	   $page2['meta']['type'] == 'wiki' &&
	   $page1['contents_size'] <= CONTENTS_READ_MAX_SIZE &&
	   $page2['contents_size'] <= CONTENTS_READ_MAX_SIZE) {

	    $ul = dom_append_child($dom, 'ul', 'compare part legend');
	    $li = dom_append_child($ul, 'li', 'delete');
	    dom_append_text($li, l('Deleted part'));
	    $li = dom_append_child($ul, 'li', 'add');
	    dom_append_text($li, l('Added part'));
	    $li = dom_append_child($ul, 'li', 'source');
	    $new_args = $args;
	    $new_args['compare_source'] = 'true';
	    dom_append_link($li, url_make($page['name'], $new_args), l('Compare source'));

	    wiki_compare($dom, $page1, $page2, $contents1, $contents2);
	} else {
	    dom_append_compare_line($dom, $contents1, $contents2);
	}
    } else {
	$article = dom_append_child($dom, 'article', 'compare1');
	handle($page1, $article);
	$article = dom_append_child($dom, 'article', 'compare2');
	handle($page2, $article);
    }

    $keys = $page1['meta'] + $page2['meta'];
    $keys = array_keys($keys);
    $ignore_keys = array('ticket');
    $meta_compares = array();
    foreach($keys as $key) {
	if(!in_array($key, $ignore_keys)) {
	    $v1 = default_value($page1['meta'][$key]);
	    $v2 = default_value($page2['meta'][$key]);
	    if($v1 != $v2) {
		$meta_compares[$key] = array($v1, $v2);
	    }
	}
    }

    $meta_showes = array(
	'type' => array('name' => 'Type'),
	'title' => array('name' => 'Title'),
	'description' => array('name' => 'Description'),
	'type' => array('name' => 'Type'),
	'btime' => array('name' => 'Birth time', 'time' => true),
	'last_modify_user' => array('name' => 'Last updated by'),
	'create_user' => array('name' => 'Creator'),
	'tag' => array('name' => 'Tags'),
	'lock' => array('name' => 'Lock'),
	'lock_user' => array('name' => 'Locked by'),
	'Content-type' => array('name' => 'File type'),
	'image_height' => array('name' => 'Image height'),
	'image_width' => array('name' => 'Image width'),
	'original_filename' => array('name' => 'Original file name'),
	'video_duration' => array('name' => 'Video duration'),
	'video_height' => array('name' => 'Video width'),
	'video_width' => array('name' => 'Video height'),
	'audio_duration' => array('name' => 'Audio duration'),
	);

    if(!empty($meta_compares)) {
	$table = dom_append_child($dom, 'table', 'meta_compare');
	$tbody = dom_append_child($table, 'tbody');
	$caption = dom_insert_child($table, 'caption');
	dom_append_text($caption, l('Meta info difference'));
	foreach($meta_compares as $key => &$meta_compare) {
	    $tr = dom_append_child($tbody, 'tr');
	    $td = dom_append_child($tr, 'td', 'name');
	    dom_append_text($td, l(default_value($meta_showes[$key]['name'], $key)));
	    $td = dom_append_child($tr, 'td', 'value1');
	    if(!empty($meta_showes[$key]['time']) && !empty($meta_compare[0]))
		dom_append_text($td, time_format($meta_compare[0]));
	    else
		dom_append_text($td, $meta_compare[0]);
	    dom_append_ref($td, 'nbsp');
	    $td = dom_append_child($tr, 'td', 'value2');
	    if(!empty($meta_showes[$key]['time']) && !empty($meta_compare[1]))
		dom_append_text($td, time_format($meta_compare[1]));
	    else
		dom_append_text($td, $meta_compare[1]);
	    dom_append_ref($td, 'nbsp');
	}
    }

    return false;
}
?>
