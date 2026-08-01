<?php
$OPTIONS['templateedit'] = array(
    'show' => 'templateedit_show',
    'write' => 'templateedit_write');

function templateedit_show(&$args, $dom) {
    $page = &target_page($args);
    title_set_pagename('Editing $pagename$', $page['name']);

    $redirect = default_value($args['redirect'], '');

    $form = templateedit_append_form(
	$dom, default_value($args['template'], ''), $page['name'],
	$redirect, false, false, false, 0, true, true, l('Save'));
}

function templateedit_write(&$args, $dom) {
    $is_newpage = !empty($args['newpage']);
    if($is_newpage)
	title_set_l('New page');
    else
	title_set_l('Edit');

    if($is_newpage) {
	$basepagename = default_value($args['target_pagename'], '');
	$sequence_digits = 4;
	if(isset($args['sequence_digits']))
	    $sequence_digits = max(1, min(9, $args['sequence_digits']));
	$sequence_format = '%0' . $sequence_digits . 'd';
	$target_pagename = templateedit_get_newpagename(
	    $basepagename, $args['child_pagename'], !empty($args['sequence']), $sequence_format);
	if($target_pagename == false)
	    return false;
    } else {
	if(!isset($args['target_pagename']) || $args['target_pagename'] == '') {
	    message_error(l('Invalid parameter.'));
	    return false;
	}
	$target_pagename = $args['target_pagename'];
	title_set_pagename('Editing $pagename$', $target_pagename);
    }

    $template = default_value($args['template']);
    $pattern = template_get_regex_pattern($template);
    $contents = template_replace_standard_variables($template);
    if(!empty($args['placeholder']))
	$contents = template_replace_placeholders_by_input($contents, $args);
    if($contents === false)
	return false;

    $target_page = page_read($target_pagename);

    if(!$is_newpage && page_type_is_text_base(page_get_type($target_page))) {
	$target_contents = page_get_contents($target_page);
	if(preg_match('/' . $pattern . '/', $target_contents, $matches, PREG_OFFSET_CAPTURE)) {
	    $offset = $matches[0][1];
	    $length = strlen($matches[0][0]);
	    $contents =
		substr($target_contents, 0, $offset) .
		$contents .
		substr($target_contents, $offset + $length);
	}
    }

    if(!page_type_is_text_base($target_page['meta']['type']))
	$target_page['meta']['type'] = 'wiki';

    if(($normalize_func = handler_function($target_page, 'normalize')) !== false) {
	$contents = $normalize_func($target_page, $contents);
    }

    if(!page_write($target_page, $contents, $args['ticket'], $error)) {
	message_error(l('Page write failed.'));
    } else {
	if(!empty($args['move'])) {
	    $location = url_make($target_page['name']);
	} else if(!empty($args['redirect'])) {
	    $location = $args['redirect'];
	} else {
	    $location = url_make($target_page['name']);
	}
	header('Location: ' . $location);
	exit(0);
    }
    return false;
}

function templateedit_append_form($dom, $template, $target_pagename,
				  $redirect, $is_move, $is_pagename, $is_sequence, $sequence_digits,
				  $is_ask, $is_placeholder, $button_label) {
    $is_newpage = $is_pagename || $is_sequence;

    if(!$is_newpage)
	$target_page = page_read($target_pagename);

    $hiddens = 
	array('option' => 'templateedit',
	      'action' => 'write',
	      'redirect' => $redirect,
	      'template' => $template,
	      'ticket' => default_value($target_page['meta']['ticket'], ''),
	      'target_pagename' => $target_pagename,
	      'newpage' => $is_newpage,
	      'sequence' => $is_sequence,
	      'sequence_digits' => $sequence_digits,
	      'placeholder' => $is_placeholder,
	      'move' => $is_move);

    $form = dom_append_form($dom, $hiddens, 'templateinput');

    if($is_pagename) {
	$label = dom_append_child($form, 'label');
	dom_append_text($label, l('Page name') . ' ');
	if($target_pagename !== false && $target_pagename != '')
	    dom_append_text($label, $target_pagename . '/');
	$attributes = array('type' => 'text', 'name' => 'child_pagename');
	if($is_sequence)
	    $attributes['placeholder'] = l('Subpage name (optional)');
	else
	    $attributes['required'] = 'required';
	dom_append_child($label, 'input', 'child_pagename', $attributes);
    }

    $pattern = template_get_regex_pattern($template);
    $template = template_replace_standard_variables($template);
    $placeholders = template_get_placeholders($template);

    if(!$is_newpage && page_type_is_text_base(page_get_type($target_page))) {
	$contents = page_get_contents($target_page);
	template_get_placeholder_values($pattern, $contents, $placeholders);
    }

    if($is_placeholder) {
	if($is_ask) {
	    template_append_placeholders_input($form, $placeholders);
	} else {
	    template_append_placeholders_hidden($form, $placeholders);
	}
    }

    if($button_label === false) {
	if($is_newpage)
	    $button_label = l('Create');
	else
	    $button_label = l('Save');
    }
    
    dom_append_child($form, 'input', '',
		     array('type' => 'submit', 'value' => $button_label));
    return $form;
}

function templateedit_get_newpagename($basepagename, &$child_pagename, $enable_sequence, $format) {
    if(isset($child_pagename) && $child_pagename !== '') {
	$pagename = page_path_to_pagename($basepagename . '/' . $child_pagename, '', true);
	if(page_is_exists($pagename)) {
	    ld(message_error(), '$pagename$ already exists.', array('pagename' => dom_pagename($pagename)));
	    return false;
	}
    } else if($enable_sequence) {
	for($n = 1; $n < 10000; $n++) {
	    $pagename = page_path_to_pagename($basepagename . '/' . sprintf($format, $n), '', true);
	    if(!page_is_exists($pagename)) {
		if(!page_is_deleted($pagename))
		    break;
	    }
	}
	if($n >= 10000) {
	    message_error(l('Reach maximum page number.'));
	    return false;
	}
    } else {
	message_error(l('Pagename required.'));
	return false;
    }
    return $pagename;
}

?>
