<?php
$OPTIONS['replace'] = array(
    'write' => 'replace_write',
    );

function replace_write(&$args, $dom) {
    global $main_pagename;
    
    if(!isset($args['position']) || !isset($args['length'])) {
	message_error(l('Invalid parameter.'));
	return;
    }

    $page = &target_page($args);
    if(!page_type_is_text_base(page_get_type($page)) || (isset($page['contents_size']) && $page['contents_size'] > CONTENTS_READ_MAX_SIZE)) {
	message_error(l('Invalid page type.'));
	return;
    }

    $position = $args['position'];
    $length = $args['length'];
    
    $value = default_value($args['value'], '');

    $contents = page_get_contents($page);

    if($length < 0) {
	$new_contents = $value;
    } else {
	$new_contents = substr($contents, 0, $position) .
	    $value . substr($contents, $position + $length);
    }

    if(($normalize_func = handler_function($page, 'normalize')) !== false) {
	$new_contents = $normalize_func($page, $new_contents);
    }

    $is_error = false;
    if(empty($new_contents)) {
	if(page_is_exists($page['name']) && !page_delete($page)) {
	    message_notice(l('Page delete failed.'));
	    $is_error = true;
	}
    } else {
	if(!page_write($page, $new_contents, $args['ticket'], $error)) {
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
