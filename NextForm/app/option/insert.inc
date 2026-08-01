<?php
function insert_write(&$args, $dom, $class, $value_function = false) {
    global $main_pagename;
    
    if(!isset($args['position']) || !isset($args['length']) || $args['length'] < 0) {
	message_error(l('Invalid parameter.'));
	return false;
    }

    $page = &target_page($args);
    if(empty($page['is_exists'])) {
	message_error(lf('$pagename$ is not exist.', array('pagename' => $page['name'])));
	return false;
    }
    if(!page_type_is_text_base(page_get_type($page)) || $page['contents_size'] > CONTENTS_READ_MAX_SIZE) {
	message_error(l('Invalid page type.'));
	return false;
    }

    $contents = page_get_contents($page);
    $search_dom = dom_create_document();
    wiki_convert($page, $search_dom, $contents);
    
    $form = dom_find_near_node($search_dom, $args['position'], $args['length'], false, 'form', $class);
    if($form === false) {
	message_error(l('Confliction detected.'));
	message_error(l('Page write failed.'));
	return true;
    }

    $is_conflict = false;
    if(default_value($args['ticket'], '') !== $page['meta']['ticket']) {
	message_error(l('Confliction detected.'));
	$is_conflict = true;
    }

    $input_values = dom_get_form_input_values($form);

    $position = dom_get_attribute($form, 'data-twp');
    $length = dom_get_attribute($form, 'data-twl');

    if($input_values['mode'] === 'once') {
    } else if($input_values['mode'] === 'before') {
	$length = 0;
    } else {
	$position += $length;
	$length = 0;
    }

    $value = default_value($args['value'], '');
    if($value_function !== false) {
	$value = $value_function($page, $search_dom, $contents, $position, $length, $input_values, $args);
	if($value === false)
	    return true;
    }

    $new_contents = substr($contents, 0, $position) .
	$value . substr($contents, $position + $length);

    //pprint_r($new_contents);
    //return true;

    $page['insert'] = true;

    if(($normalize_func = handler_function($page, 'normalize')) !== false) {
	$new_contents = $normalize_func($page, $new_contents);
    }

    if(!page_write($page, $new_contents, $page['meta']['ticket'], $error)) {
	message_error(l('Page write failed.'));
    } else if($is_conflict) {
	message_notice(l('Your text was merged.'));
	return true;
    } else  {
	header('Location: ' . url_make($main_pagename));
	exit(0);
    }
    return true;
}

function insert_write_check_value($text, $input_values) {
    $unescape_text = preg_replace('/&([[:punct:]])/', '$1', $text);
    if(empty($input_values['url']) &&
       (preg_match('/' . WIKI_URI_REGEX . '/', $text) || preg_match('/' . WIKI_URI_REGEX . '/', $unescape_text))) {
	message_error(l('URL is not allowed.'));
	return false;
    }
    if(!empty($input_values['japanese']) && !preg_match('/[一-龠ぁ-んァ-ヴー]+/u', $text)) {
	message_error(l('Posted text is not include Japanese.'));
	return false;
    }
    return true;
}

?>
