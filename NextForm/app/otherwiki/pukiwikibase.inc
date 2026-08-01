<?php
global $LANGUAGE;
$LANGUAGE['ja']['Recently updated pages'] = '最近更新されたページ';
$LANGUAGE['ja']['Popular pages'] = '人気のページ';

function pukiwikibase_convert(&$page, $source) {
    $page['meta']['type'] = 'wiki';

    $output = array();
    
    $lines = explode("\n", $source);
    $lines[] = '';
    $table_format = null;
    $list_start_line_no = null;
    $line_no = 0;
    foreach($lines as $line) {
	$is_list_start = false;
	$is_list_finish = false;
	if(preg_match('/^(\*+)(.*?)\s*\[\#.*\]\s*$/', $line, $matches)) {
	    $output[] = $matches[1] . pukiwikibase_convet_inline($matches[2]);
	    $is_list_finish = true;
	} else if(preg_match('/^(\*+)(.*)$/', $line, $matches)) {
	    $output[] = $matches[1] . pukiwikibase_convet_inline($matches[2]);
	    $is_list_finish = true;
	} else if(preg_match('/^ (.*)$/', $line, $matches)) {
	    $output[] = ' ' . $matches[1];
	} else if(preg_match('/^\|(.*)\|\s*c\s*$/', $line, $matches)) {
	    $fields = explode('|', $matches[1]);
	    $format = '';	    
	    foreach($fields as $field) {
		$field = strtoupper(trim($field));
		switch($field) {
		case 'LEFT:': $format .= 'l'; break;
		case 'RIGHT:': $format .= 'r'; break;
		case 'CENTER:': $format .= 'c'; break;
		default: $format .= 'n'; break;
		}
	    }
	    $table_format = $format;
	} else if(preg_match('/^\|(.*)$/', $line, $matches)) {
	    $output_line = pukiwikibase_convet_inline($line);
	    if(!is_null($table_format)) {
		$output_line .= $table_format;
		$table_format = null;
	    }
	    $output[] = $output_line;
	} else if(preg_match('/^\-{4,}\s*$/', $line, $matches)) {
	    $output[] = '====';
	} else if(preg_match('/^([-+]{1,3})(.*)$/', $line, $matches)) {
	    $output[] = $matches[1] . pukiwikibase_convet_inline($matches[2]);
	    $is_list_start = true;
	    $is_list_finish = true;
	} else if(preg_match('/^#(\w+)(?:\((.*?)\))?/', $line, $matches)) {
	    $output[] = pukiwikibase_convet_plugin($matches, 'block');
	} else if($line === ''){
	    $output[] = '';
	    $is_list_finish = true;
	} else {
	    $output[] = pukiwikibase_convet_inline($line);
	}

	if($is_list_finish) {
	    if(!is_null($list_start_line_no) && $line_no - 1 > $list_start_line_no) {
		if(preg_match('/^([-+]{1,3})(.*)/', $output[$list_start_line_no], $matches)) {
		    $output[$list_start_line_no] =  $matches[1] . '{' . $matches[2];
		    if(!isset($output[$line_no - 1])) {
			debug_log($page['name'] . ' line:' . $line);
		    }
		    $output[$line_no - 1] .= "\n}";
		}
	    }
	    $list_start_line_no = null;
	}
	if($is_list_start)
	    $list_start_line_no = $line_no;
	
	$line_no++;
    }
    array_pop($output);
    
    return implode("\n", $output);
}

function pukiwikibase_convet_inline($source) {
    $source = preg_replace_callback('/\&(\w+)(?:\((.*?)\))?(?:\{(.*)\})?;/', 'pukiwikibase_convet_plugin', $source);
    return $source;
}

function pukiwikibase_convet_plugin($matches, $type = 'inline') {
    global $IMPORT_OTHERWIKI_CURRENT_PAGENAME;
    //global $plugins;

    $name = strtolower($matches[1]);
    if(isset($matches[2]))
	$args = explode(',', $matches[2]);
    else
	$args = array();
    $target = default_value($matches[3]);

    //$plugins[$name]['uses'][] = sprintf('[%s] %s', $current_pagename, $matches[0]);

    switch($name) {
    case 'ref':
    case 'media':
	$pagename = array_shift($args);
	if(strpos($pagename, '/') === false)
	    $pagename = './' . $pagename;
	if(pukiwikibase_arg_search($args, 'noimg')) {
	    return '[[' . $pagename . ']]';
	} else {
	    $new_args = array('[['. $pagename .']]');
	    foreach($args as $arg) {
		if(preg_match('/^\d+%$/', trim($arg)))
		    $new_args[] = $arg;
	    }
	    return '&include(' . implode(',', $new_args) . ');';
	}
    case 'video':
	return '&include(' . implode(',', $args) . ');';

    case 'color':
    case 'aname':
    case 'p':
    case 'paint':
    case 'amp':
    case 'heart':
    case 'smile':
    case 'bigsmile':
    case 'huh':
    case 'oh':
    case 'wink':
    case 'sad':
    case 'worried':
	return $matches[0];

    case 'sum':
	return '={sum(up)}';

    case 'math':
	return '&math{' . implode(',', $args) . '}';

    case 'include':
	return '&include([[' . $args[0] . ']]);';

    case 'recent':
	return '*'.l('Recently updated pages')."\n".'&recent('. implode(',', $args) .');';

    case 'new':
	$target = preg_replace('/\(.*\) */', '', $target);
	return '&time(new){' . $target . '}';
	
    case 'di':
	if($type == 'inline') {
	    return pukiwikibase_convet_inline($target);
	}
	$args[] = 'noname';
	$args[] = 'nodate';
    case 'comment':
	$new_args = array();
	if(!pukiwikibase_arg_search($args, 'noname'))
	    $new_args[] = 'name';
	if(!pukiwikibase_arg_search($args, 'nodate'))
	    $new_args[] = 'date';
	if(pukiwikibase_arg_search($args, 'below'))
	    $new_args[] = 'below';
	if(pukiwikibase_arg_search($args, 'above'))
	    $new_args[] = 'above';
	
	if(!empty($new_args[0]) && $new_args[0] == 'name' &&
	   !empty($new_args[1]) && $new_args[1] == 'date') {
	    array_shift($new_args);
	    array_shift($new_args);
	    return '&comment(' . implode(',', $new_args) . ');';
	} else {
	    return '&listadd(' . implode(',', $new_args) . ');';
	}

    case 'cal':
    case 'calendar':
    case 'terus_cal':
	$new_args = array();
	if(!empty($args[0]))
	    $new_args[] = '[[' . $args[0] . ']]';
	return '&calendar(' . implode(',', $new_args) . ');';

    case 'calendar_viewer':
	$pagename = otherwiki_trim_pagename(default_value($args[0]));
	$new_args = array('[[' . $pagename . ']]');
	if(pukiwikibase_arg_search($args, 'past'))
	    $new_args[] = 'past';
	if(pukiwikibase_arg_search($args, 'future'))
	    $new_args[] = 'future';
	return '&datepages(' . implode(',', $new_args) . ');';

    case 'contents':
	return '&index;';

    case 'ls':
	return '&pages;';
    case 'ls2':
	$new_args = array();
	$pagename = otherwiki_trim_pagename(default_value($args[0]));
	if($pagename !== '')
	    $new_args[] = '[[' . $pagename . ']]';
	return '&pages(' . implode(',', $new_args) . ');';
    case 'norelated':
	return '';

    case 'counter':
	$new_args = array();
	if(($pagename = pukiwikibase_arg_search_regex($args, 'page\s*=\s*(.*)\s*')) !== false)
	    $new_args[] = '[[' . $pagename . ']]';
	    
	if($type == 'block') {
	    $new_args_string = implode(',', $new_args);
	    if(!empty($new_args))
		$new_args_string = ',' . $new_args_string;
	    return 'Counter: &counter(total' . $new_args_string . ');' .
		', today: &counter(today' . $new_args_string . ');' . 
		', yesterday: &counter(yesterday' . $new_args_string . ');~';
	} else {
	    if(pukiwikibase_arg_search($args, 'today'))
		$new_args[] = 'today';
	    if(pukiwikibase_arg_search($args, 'yesterday'))
		$new_args[] = 'yesterday';
	    if(pukiwikibase_arg_search($args, 'total'))
		$new_args[] = 'total';
	    if(pukiwikibase_arg_search($args, 'total'))
		$new_args[] = 'total';
	    return '&counter(' . implode(',', $new_args) . ');';
	}
	
    case 'popular':
	$new_args = array();
	if(pukiwikibase_arg_search($args, 'true'))
	    $new_args[] = 'today';
	if(pukiwikibase_arg_search($args, 'false'))
	    $new_args[] = 'total';
	if(($num = pukiwikibase_arg_search_regex($args, '\s*([0-9]+)\s*')) !== false)
	    $new_args[] = $num;

	if(($pagenames = pukiwikibase_arg_search_regex_multiple($args, 'ignore\s*=\s*(.*)\s*')) !== false) {
	    foreach($pagenames as $pagename) {
		$new_args[] = 'name=!' . $pagename;
	    }
	}
	return '*'.l('Popular pages')."\n".'&popularpages('. implode(',', $new_args) .');';
    }
    debug_log('unkown pukiwiki plugin [[' . $IMPORT_OTHERWIKI_CURRENT_PAGENAME . ']] ' . $matches[0]);
    
    return $matches[0];
}

function pukiwikibase_arg_search(&$args, $needle) {
    foreach($args as $arg) {
	if(trim($arg) == $needle)
	    return true;
    }
    return false;
}

function pukiwikibase_arg_search_regex(&$args, $needle) {
    foreach($args as $arg) {
	if(preg_match('/^' . $needle . '$/', trim($arg), $matches))
	    return $matches[1];
    }
    return false;
}

function pukiwikibase_arg_search_regex_multiple(&$args, $needle) {
    $results = array();
    foreach($args as $arg) {
	if(preg_match('/^' . $needle . '$/', trim($arg), $matches))
	    $results[] = $matches[1];
    }
    return $results;
}

?>