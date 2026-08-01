<?php

function template_get(&$context, $template, $name = 'LAST') {
    $template = template_trim($template);

    if(isset($context['templates']))
	$template = template_replace_variables($template, $context['templates']);
    if($template !== '')
	$context['templates'][$name] = $template;

    return $template;
}

function template_trim($template) {
    return trim($template, "\n");
}

function template_replace_variables($template, $variables) {
    $replace_from = array();
    $replace_to = array();
    foreach($variables as $from => $to) {
	$replace_from[] = '$' . $from . '$';
	$replace_to[] = $to;
    }
    return str_replace($replace_from, $replace_to, $template);
}

function template_replace_variables_by_dom($dom, $template, $variables) {
    $words = preg_split('/(\$[\w\d]+\$)/', $template, -1, PREG_SPLIT_DELIM_CAPTURE);
    foreach($words as $word) {
	if(preg_match('/^\$(.*)\$$/', $word, $matches) && isset($variables[$matches[1]])) {
	    $v = $variables[$matches[1]];
	    if(is_array($v)) {
		foreach($v as $d) {
		    dom_append_dom($dom, $d);
		}
	    } else if(is_string($v)){
		dom_append_text($dom, $v);
	    } else {
		dom_append_dom($dom, $v);
	    }
	} else {
	    dom_append_text($dom, $word);
	}
    }
}

function template_replace_standard_variables($template) {
    $user = auth_get_user();
    if(!empty($user))
	$username = $user['name'];
    else
	$username = 'anonymous';

    $variables = array(
	'user' => $username,
	'time' => time_format(time()),
	);

    return template_replace_variables($template, $variables);
}

function template_get_placeholders($template) {
    global $WIKI_OPERATORS_LINE_HEAD;

    $matches = array();
    preg_match_all('/(?:@([^=@\n]+)(?:=([^@]*))?@|`([^`]*)`)/', $template, $matches, PREG_SET_ORDER);
    $placeholders = array();
    $index = 0;
    foreach($matches as $match) {
	$placeholder = array('template' => $match[0], 'index' => $index);
	if($match[1] !== '') {
	    $term = $match[1];
	    if(preg_match('|^([^/]+)/(.*)$|', $term, $term_matches)) {
		$term = $term_matches[1];
		$options = options_parse($term_matches[2]);
		$placeholder['type'] = 'select';
		$placeholder['options'] = $options;
		$placeholder['options_text'] = $term_matches[2];
	    } else {
		$placeholder['type'] = 'text';
		if(preg_match('|^(.*?)([#\'\*]+)$|', $term, $term_matches)) {
		    $term = $term_matches[1];
		    if(strstr($term_matches[2], '#') !== false)
			$placeholder['type'] = 'textarea';
		    if(strstr($term_matches[2], '\'') !== false)
			$placeholder['escape'] = true;
		    if(strstr($term_matches[2], '*') !== false)
			$placeholder['required'] = true;
		}
	    }
	    if(isset($WIKI_OPERATORS_LINE_HEAD)) {
		foreach($WIKI_OPERATORS_LINE_HEAD as $opename => &$operator) {
		    if(preg_match('/^(' . $operator['symbol'] . ')(.*)$/', $term, $operator_matches)) {
			$placeholder['wiki_line_head'] = $operator_matches[1];
			$placeholder['wiki_operator'] = $opename;
			$term = $operator_matches[2];
			break;
		    }
		}
	    }
	    $term = trim($term);
	    if(isset($match[2]))
		$placeholder['value'] = trim($match[2]);
	    $placeholder['term'] = $term;
	} else {
	    $placeholder['type'] = 'ignore';
	}

	$placeholders[] = $placeholder;
	$index++;
    }
    return $placeholders;
}

function template_get_placeholder_values($pattern, $contents, &$placeholders = array()) {
    if(preg_match('/' . $pattern . '/', $contents, $matches, PREG_OFFSET_CAPTURE)) {
	array_shift($matches);
	$values = $matches;

	$i = 0;
	foreach($placeholders as &$placeholder) {
	    if(isset($values[$i])) {
		$placeholder['match'] = $values[$i][0];
		$placeholder['match_offset'] = $values[$i][1];
	    } else {
		unset($placeholder['match']);
		unset($placeholder['match_offset']);
	    }
	    $i++;
	}
	return $values;
    }
    return array();
}

function template_replace_placeholders_by_input($template, $args) {
    global $WIKI_OPERATORS_LINE_HEAD;

    $index = 0;
    $value = '';
    $placeholders = template_get_placeholders($template);
    $parts = preg_split('/(@[^@\n]+(?:=[^@]*)?@|`[^`]*`)/', $template, -1, PREG_SPLIT_DELIM_CAPTURE);
    $is_error = false;
    foreach($parts as $part) {
	if(preg_match('/^@([^@\n]+(?:=[^@]*)?)@$/', $part, $matches)) {
	    $placeholder = $placeholders[$index];
	    $replace_value = default_value($args['term' . $index], '');
	    $options = '';
	    if(!empty($placeholder['required']) && $replace_value === '') {
		message_error(lf('You must enter a $term$', array('term' => $placeholder['term'])));
		$is_error = true;
	    }
	    $linehead_symbol = false;
	    if(!empty($placeholder['wiki_operator']) && !empty($placeholder['wiki_line_head']) &&
	       $replace_value !== '') {
		$linehead_symbol = $placeholder['wiki_line_head'];
		if(in_array($placeholder['wiki_operator'], array('section', 'list')))
		    $linehead_symbol .= ' ';
	    }
	    if(!empty($placeholder['escape'])) 
		$replace_value = wiki_escape_source($replace_value, $linehead_symbol === false);
	    if($linehead_symbol !== false)
		$replace_value = preg_replace('/^/m', $linehead_symbol, $replace_value);

	    $value .= $replace_value;
	    $index++;
	} else if(preg_match('/^`([^`]*)`$/', $part, $matches)) {
	    if(isset($args['term' . $index]))
		$value .= $args['term' . $index];
	    else
		$value .= $matches[1];
	    $index++;
	} else {
	    $value .= $part;
	}
    }
    if($is_error)
	return false;

    return $value;
}

function template_append_placeholders_input($form, $placeholders) {
    $dl = dom_append_child($form, 'dl', 'templateinput');
    foreach($placeholders as &$placeholder) {
	$i = $placeholder['index'];
	$name = 'term' . $i;

	if($placeholder['type'] === 'ignore') {
	    if(isset($placeholder['match'])) {
		dom_append_child($form, 'input', '',
				 array('type' => 'hidden', 'name' => $name, 'value' => $placeholder['match']));
	    }
	} else {
	    $term = $placeholder['term'];
	    $dt = dom_append_child($dl, 'dt');
	    dom_append_text($dt, $term);
	    $dd = dom_append_child($dl, 'dd');

	    if(isset($placeholder['match'])) {
		$default_value = $placeholder['match'];
		if(isset($placeholder['wiki_line_head'])) {
		    $default_value = preg_replace(
			'/^' . preg_quote($placeholder['wiki_line_head'], '/') . '/m', '', $default_value);
		}
	    } else
		$default_value = default_value($placeholder['value'], '');

	    if($placeholder['type'] == 'textarea') {
		$input = dom_append_child($dd, 'textarea', '', array('name' => $name));
		dom_append_text($input, $default_value);
		if(!empty($placeholder['required']))
		    dom_set_attribute($input, 'required', 'required');
	    } else if($placeholder['type'] == 'select') {
		$input = dom_append_selector($dd, $name, $placeholder['options'], $default_value);
	    } else {
		$input = dom_append_child($dd, 'input', '',
					  array('type' => 'text', 'name' => $name, 'value' => $default_value));
		if(!empty($placeholder['required']))
		    dom_set_attribute($input, 'required', 'required');
	    }
	    dom_append_class($input, 'templateinput');
	    if(!empty($placeholder['escape']))
		dom_append_class($input, 'escape');
	}
    }
    dom_remove_empty($dl);
}

function template_append_placeholders_hidden($form, $placeholders) {
    $hiddens = array();
    $i = 0;
    foreach($placeholders as &$placeholder) {
	$term = $placeholder['term'];
	$name = 'term' . $i;
	$hiddens[$name] = default_value($placeholder['value'], '');
	$i++;
    }
    return dom_append_input_hidden($form, $hiddens);
}

function template_get_regex_pattern($template, $delimiter = '/') {
    global $WIKI_OPERATORS_LINE_HEAD;

    $patterns = '';
    $words = preg_split('/(@[^@\n]+(?:=[^@]*)?@|`[^`]*`|[ \t]+|\n+)/',
			$template, -1, PREG_SPLIT_DELIM_CAPTURE | PREG_SPLIT_NO_EMPTY);
    foreach($words as $word) {
	if($word == "\n")
	    $pattern = "\n";
	else if(preg_match("/^\n+$/", $word))
	    $pattern = "\n*";
	else if(preg_match('/^[ \t]+$/', $word))
	    $pattern = '[ \t]+';
	else if(preg_match('/^`[^`]*`$/', $word, $matches)) {
	    $pattern = '((?s).*?)';
	} else if(preg_match('/^@(.+)@$/s', $word, $matches)) {
	    $symbol = false;
	    if(isset($WIKI_OPERATORS_LINE_HEAD)) {
		foreach($WIKI_OPERATORS_LINE_HEAD as $opename => &$operator) {
		    if(preg_match('/^(' . $operator['symbol'] . ')/', $matches[1], $symbol_matches)) {
			$symbol = $symbol_matches[1];
			break;
		    }
		}
	    }
	    if($symbol !== false)
		$pattern = '((?:(?m)^' . preg_quote($symbol, $delimiter) . '.*\n?)*)';
	    else {
		if(preg_match('/(#[\'\*]*)$/', $matches[1]) || 
		   preg_match('/(#[\'\*]*)=/', $matches[1])) {
		    $pattern = '((?s).*)';
		} else
		    $pattern = '(.*)';
	    }
	} else {
	    $pattern = preg_quote($word, $delimiter);
	}
	$patterns .= $pattern;
    }
    //pprint_r($patterns);

    return $patterns;
}
?>
