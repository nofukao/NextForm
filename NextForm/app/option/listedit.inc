<?php
$OPTIONS['listedit'] = array(
    'write' => 'listedit_write',
    'insert' => 'listedit_insert',
    );

function listedit_write(&$args, $dom) {
    global $main_pagename;
    global $WIKI_OPERATORS_LINE_HEAD;
    
    if(!isset($args['targets']) || !isset($args['mode'])) {
	message_error(l('Invalid parameter.'));
	return;
    }
    
    $page = &target_page($args);
    if(!page_type_is_text_base(page_get_type($page))) {
	message_error(l('Invalid page type.'));
	return;
    }

    $mode = $args['mode'];
    $contents = page_get_contents($page);
    $original_contents = $contents;
    $targets = array();
    $fields = array_unique(explode(',', $args['targets']));
    $offset = 0;
    foreach($fields as $field) {
	$v = explode(':', $field);
	if(count($v) == 2) {
	    $position = $v[0];
	    $length = $v[1];
	    $position += $offset;
	    if($mode == 'delete') {
		$contents = substr($contents, 0, $position) . substr($contents, $position + $length);
		$offset -= $length;
	    } else if($mode == 'strike') {
		if(substr($contents, $position + $length - 1, 1) == "\n")
		    $length --;
		$target_string = substr($contents, $position, $length);
		if(preg_match('/^%%\s*(.*?)\s*%%$/', $target_string , $matches) ||
		   preg_match('/^\&del{ *(.*?) *}$/s', $target_string , $matches)) {
		    $contents = substr($contents, 0, $position) .
			$matches[1] .
			substr($contents, $position + $length);
		    $offset -= strlen($matches[0]) - strlen($matches[1]);
		} else {
		    if(strstr($target_string, "\n") !== false) {
			$contents = substr($contents, 0, $position) .
			    '&del{' . $target_string . '}' .
			    substr($contents, $position + $length);
			$offset += 6;
		    } else {
			$contents = substr($contents, 0, $position) .
			    '%% ' . $target_string . ' %%' .
			    substr($contents, $position + $length);
			$offset += 6;
		    }
		}
	    } else if(($mode == 'insert_before' || $mode == 'insert_after') && !empty($args['value'])) {
		if(isset($args['parent_twp']) && isset($args['parent_twl'])) {
		    $item = substr($contents, (int)$args['parent_twp'], (int)$args['parent_twl']);
		} else
		    $item = substr($contents, $position, $length);
		
		if(preg_match('/^('. $WIKI_OPERATORS_LINE_HEAD['list']['symbol'] . ')/', $item, $matches)) {
		    $symbols = $matches[1];
		    if(isset($args['parent_twp']))
			$symbols .= '-';
		} else
		    $symbols = '-';
		$value = $symbols . ' ' . $args['value'] . "\n";

		if($mode == 'insert_before')
		    $length = 0;
		else if(substr($contents, $position + $length - 1, 1) != "\n")
		    $value = "\n" . $value;

		$contents = substr($contents, 0, $position + $length) .
		    $value .
		    substr($contents, $position + $length);
		$offset += strlen($value);
	    }
	}
    }

    if(($normalize_func = handler_function($page, 'normalize')) !== false) {
	$contents = $normalize_func($page, $contents);
    }

    $is_error = false;
    if($original_contents != $contents) {
	if(!page_write($page, $contents, $args['ticket'], $error)) {
	    message_error(l('Page write failed.'));
	    $is_error = true;
	}
    }
    if(!$is_error && !empty($args['redirect'])) {
	header('Location: ' . $args['redirect']);
	exit(0);
    }

    return true;
}
?>
