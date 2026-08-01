<?php
$OPTIONS['export'] = array(
    'write' => 'export_write',
    );

function export_write(&$args, $dom) {
    global $main_pagename;
    auth_check(false, 'export');
    title_set_l('Export result');

    if(EXPORT_ENABLE !== 'true') {
	message_error(l('Export function disabled.'));
	return false;
    }

    if(!isset($args['page']) || $args['page'] === '') {
	message_error(l('Invalid parameter.'));
	return false;
    }

    $export_directory = default_value($args['directory'], '');
    if(strpos($export_directory, '..') !== false) {
	message_error(lf('Invalid directory name $filename$.', array('filename' => $export_directory)));
	return false;
    }

    $time_start = microtime(true);
    $export_data = export_pages($args['page'], $export_directory, !empty($args['force']));
    dom_set_self_link_pagename();
    $convert_time = microtime(true) - $time_start;

    $back_page = default_value($args['back_page'], '');
    dom_append_link($dom, url_make($back_page), l('Return'));

    if(DEBUG_MODE == 'true') {
	$p = dom_append_child($dom, 'p');
	dom_append_text($p, l('Export time') . ': ' . $convert_time . ' ' . count($export_data) . ' pages');
    }

    message_info(l('Export finished.'));

    if(!empty($args['quiet'])) {
	return;
    }

    $common_paths = array();
    $common_count = array();
    foreach($export_data as $key => &$export_datum) {
	$pagename = $export_datum['pagename'];
	$targets = array('pagename' => $pagename);
	foreach($export_datum['outputs'] as &$output) {
	    $targets['filepath'] = $output['filepath'];
	    if(isset($output['template']))
		$targets['template'] = $output['template'];
	}
	foreach($targets as $key => $path) {
	    $paths = explode('/', $path);
	    if(isset($common_paths[$key])) {
		for($i = 0;
		    isset($common_paths[$key][$i]) &&
			isset($paths[$i]) &&
			$common_paths[$key][$i] == $paths[$i];
		    $i++);
		array_splice($common_paths[$key], $i);
		$common_count[$key]++;
	    } else {
		$common_paths[$key] = $paths;
		$common_count[$key] = 1;
	    }
	}
    }
    $common_offsets = array();
    foreach(array('pagename', 'filepath', 'template') as $key) {
	if(isset($common_paths[$key]) && default_value($common_count[$key], 0) > 1) {
	    $offset = array_sum(array_map('strlen', $common_paths[$key])) + count($common_paths[$key]);
	    $common_offsets[$key] = $offset;
	} else
	    $common_offsets[$key] = 0;
    }

    $table = dom_append_child($dom, 'table', 'export_result');
    $thead = dom_append_child($table, 'thead');
    $tbody = dom_append_child($table, 'tbody');

    $tr = dom_append_child($thead, 'tr');
    $th = dom_append_child($tr, 'th');
    dom_append_text($th, l('Page name'));
    $th = dom_append_child($tr, 'th');
    dom_append_text($th, l('File name'));
    $th = dom_append_child($tr, 'th');
    dom_append_text($th, l('Template'));
    $th = dom_append_child($tr, 'th');
    dom_append_text($th, l('Result'));
    if(DEBUG_MODE == 'true') {
	$th = dom_append_child($tr, 'th');
	dom_append_text($th, l('Page creation time'));
    }

    foreach($export_data as $key => &$export_datum) {
	$pagename = $export_datum['pagename'];
	$argsstring = $export_datum['argsstring'];
	foreach($export_datum['outputs'] as &$output) {
	    $tr = dom_append_child($tbody, 'tr');
	    if(default_value($export_datum['status']) == 'error')
		$status = 'error';
	    else
		$status = default_value($output['status']);
	    dom_append_class($tr, $status);

	    $td = dom_append_child($tr, 'td');

	    if($argsstring != '') {
		$export_args = url_parse_query($argsstring);
		$fields = array();
		foreach($export_args as $k => $v) {
		    $fields[] = $k . "=" . $v;
		}
		dom_append_page_link(
		    $td, substr($pagename, $common_offsets['pagename']) . '?' . implode('&', $fields), $pagename,
		    '', url_make($pagename, $export_args));
	    } else 
		dom_append_page_link($td, substr($pagename, $common_offsets['pagename']), $pagename);


	    $td = dom_append_child($tr, 'td');
	    if(isset($args['url']) && $args['url'] != '') {
		dom_append_link($td, 
				rtrim($args['url'], '/') .'/'. url_path_encode($output['filepath']),
				substr($output['filepath'], $common_offsets['filepath']));
	    } else {
		dom_append_text($td, substr($output['filepath'], $common_offsets['filepath']));
	    }

	    $td = dom_append_child($tr, 'td');
	    if(isset($output['template'])) {
		dom_append_page_link(
		    $td, substr($output['template'], $common_offsets['template']), $output['template']);
	    } else {
		dom_append_text($td, '-');
	    }
	    
	    $td = dom_append_child($tr, 'td');
	    if(isset($output['status'])) {
		dom_append_text($td, l($output['status']));
	    } else {
		dom_append_text($td, '-');
	    }

	    if(DEBUG_MODE == 'true') {
		$td = dom_append_child($tr, 'td');
		if(isset($export_datum['convert_time'])) {
		    dom_append_text($td, sprintf('%.4f', $export_datum['convert_time']));
		} else {
		    dom_append_text($td, '-');
		}
	    }

	}
    }
    

    return false;
}
?>
