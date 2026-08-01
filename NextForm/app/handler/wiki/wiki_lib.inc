<?php

function wiki_tokens_to_string(&$tokens) {
    if(empty($tokens))
	return '';
    $output = '';
    foreach($tokens as &$token) {
	$output .= $token->t;
    }
    return $output;
}

function wiki_tokens_to_string_without_escape(&$tokens) {
    if(empty($tokens))
	return '';
    $output = '';
    foreach($tokens as &$token) {
	if(!empty($token->pre_escape))
	    $output .= $token->pre_escape;
	else
	    $output .= $token->t;
    }
    return $output;
}


function wiki_tokens_trim_space(&$tokens) {
    $n = count($tokens);
    for($s = 0; $s < $n; $s++) {
	if(!ctype_space($tokens[$s]->t) &&
	   !ctype_cntrl($tokens[$s]->t))
	    break;
    }
    if($s == $n)
	return array();
    for($t = $n - 1; $t >= 0; $t--) {
	if(!ctype_space($tokens[$t]->t) &&
	   !ctype_cntrl($tokens[$t]->t))
	    break;
    }
    return array_slice($tokens, $s, $t - $s + 1);
}

function wiki_tokens_is_whitespace(&$tokens) {
    $n = count($tokens);
    for($s = 0; $s < $n; $s++) {
	if(!($tokens[$s]->t === '' || ctype_space($tokens[$s]->t) || ctype_cntrl($tokens[$s]->t)))
	    return false;
    }
    return true;
}

function wiki_get_parameters_string($tree) {
    if(empty($tree->parameters))
	return array();
    return array_map('wiki_tokens_to_string', $tree->parameters);
}

function wiki_get_target_string($tree) {
    if(empty($tree->target))
	return '';
    return wiki_tokens_to_string($tree->target);
}

function wiki_parse_parameters(&$context, $tree, $requires) {
    if(is_null($tree->parameters))
	return;
    foreach($tree->parameters as &$tokens) {
	$num = count($tokens);
	if($num <= 0)
	    continue;
	if($num == 1 && $tokens[0] instanceof WikiURIToken && array_key_exists('uri', $requires)) {
	    $requires['uri'] = $tokens[0]->t;
	    continue;
	}

	$param = wiki_tokens_to_string($tokens);
	if(preg_match('/^\[\[(.*)\]\]$/', $param, $maches)) {
	    $requires['pagename'] = page_path_to_pagename($maches[1], $context['page']['name'], false);
	} else if(preg_match('/^\s*([^=]+)\s*=\s*(.*)$/', $param, $maches) &&
		  array_key_exists('equalities', $requires) &&
		  array_key_exists($maches[1], $requires['equalities'])){
	    if(is_array($requires['equalities'][$maches[1]]))
		$requires['equalities'][$maches[1]][] = $maches[2];
	    else
		$requires['equalities'][$maches[1]] = $maches[2];
	} else if(preg_match('/^(\d+)$/', $param, $maches) && array_key_exists('number', $requires))  {
	    $requires['number'] = $maches[1];
	} else if(preg_match('/^!(.*)$/', $param, $maches) &&
		  array_key_exists('long_flags', $requires) &&
		  array_key_exists($maches[1], $requires['long_flags'])) {
	    if(is_array($requires['long_flags'][$maches[1]]) &&
	       array_key_exists(2, $requires['long_flags'][$maches[1]]))
		$requires['long_flags'][$maches[1]][0] = $requires['long_flags'][$maches[1]][2];
	    else if(is_array($requires['long_flags'][$maches[1]]))
		$requires['long_flags'][$maches[1]][0] = false;
	    else
		$requires['long_flags'][$maches[1]] = false;
	} else if(preg_match('/^\-(.*)$/', $param, $maches)) {
	    $length = strlen($maches[1]);
	    for($i = 0; $i < $length; $i++) {
		if(array_key_exists($maches[1][$i], $requires['short_flags']))
		    if(is_array($requires['short_flags'][$maches[1][$i]]))
			$requires['short_flags'][$maches[1][$i]][0] = $requires['short_flags'][$maches[1][$i]][1];
		    else
			$requires['short_flags'][$maches[1][$i]] = true;
	    }
	} else if(array_key_exists('long_flags', $requires) &&
		  array_key_exists($param, $requires['long_flags'])) {
	    if(is_array($requires['long_flags'][$param]))
		$requires['long_flags'][$param][0] = $requires['long_flags'][$param][1];
	    else
		$requires['long_flags'][$param] = true;
	} else if(array_key_exists('values', $requires)) {
	    if(!isset($requires['values']))
		$requires['values'] = array();
	    $requires['values'][] = $param;
	}
    }    
}

function wiki_append_element(&$context, $node, $target, $element_name, $attributes = array()) {
    $n = dom_append_child($node, $element_name, '', $attributes);
    wiki_convert_from_tokens($context, $n, $target, array('display' => 'inline', 'append_line_head' => false));
    return $n;
}

function wiki_escape_source($src, $is_line_head = true) {
    global $WIKI_TOKEN_SYMBOLS;
    global $WIKI_OPERATORS_LINE_HEAD;

    $regexes = array();
    foreach($WIKI_TOKEN_SYMBOLS as $symbol) {
	if($symbol !== '#' && $symbol !== ' ')
	    $regexes[] = preg_quote($symbol, '/');
    }
    if($is_line_head) {
	foreach($WIKI_OPERATORS_LINE_HEAD as $line_head) {
	    $regexes[] = '^' . $line_head['symbol'];
	}
    }
    $regex = '/(' . implode('|', $regexes) . ')/m';
    return preg_replace_callback($regex, 'wiki_escape_source_replace', $src);
}

function wiki_escape_source_replace($matches) {
    if($matches[0][0] == ' ')
	return '"" ""';
    $len = strlen($matches[0]);
    $encoded_symbol = '';
    for($i = 0; $i < $len; $i++) {
	$encoded_symbol .= '&' . $matches[0][$i];
    }
    return $encoded_symbol;
}
?>
