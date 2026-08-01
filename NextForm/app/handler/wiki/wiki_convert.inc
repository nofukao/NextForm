<?php
$WIKI_CONVERT_CACHE = array();

function wiki_convert(&$page, $dom, $source = null) {
    if(!empty($page['wiki_context'])) {
	$context = wiki_modify_context($page['wiki_context'], array(), 0);
	$context['page'] = &$page;
	unset($page['wiki_context']);
    } else
	$context = wiki_default_context($page);
    $context['root_dom'] = $dom;
    if(is_null($source))
	$source = page_get_contents($page);
    $result = wiki_convert_from_source($context, $dom, $source, array(), 0);
    $page['wiki_context_result'] = &$context;
    return $result;
}

function wiki_default_context(&$page) {
    global $WIKI_ALIASES;
    global $WIKI_TEMPLATES;

    $aliases = $WIKI_ALIASES;
    $templates = $WIKI_TEMPLATES;
    $redirect = array();
    $context = array(
	'page' => &$page,
	'depth' => 0,
	'display' => 'block',
	'append_line_head' => true,
	'ids' => array(),
	'aliases' => &$aliases,
	'templates' => &$templates,
	'redirect' => &$redirect,
	);
    return $context;
}

function wiki_modify_context(&$context, $context_modify_array = array(), $inc_depth = 1) {
    $new_context = $context;
    $new_context['depth'] += $inc_depth;
    $new_context['aliases'] = &$context['aliases'];
    $new_context['templates'] = &$context['templates'];
    $new_context['redirect'] = &$context['redirect'];
    
    foreach($context_modify_array as $key => $value) {
	$new_context[$key] = $value;
    }
    return $new_context;
}

function wiki_convert_from_source(&$arg_context, $node, &$source,
				  $context_modify_array = array('display' => 'inline'),
				  $inc_depth = 1) {
    $context = wiki_modify_context($arg_context, $context_modify_array, $inc_depth);
    $context['source'] = &$source;
    $tokens = &wiki_lex($context, $source);
    debug_watch('tokens');
    //if($context['depth'] == 0 && !empty($context['page']['args'])) {
    //pprint_r(wiki_show_tokens($tokens, "\n"));
    //}
    return wiki_convert_from_tokens($context, $node, $tokens, array(), 0);
}

function wiki_convert_from_tokens(&$arg_context, $node, &$tokens, 
				  $context_modify_array = array('display' => 'inline'),
				  $inc_depth = 1) {
    global $WIKI_CONVERT_CACHE;

    if(empty($tokens))
	return;
    $context = wiki_modify_context($arg_context, $context_modify_array, $inc_depth);
    if(!empty($context['append_line_head']) && ($context['display'] == 'block' || !ctype_space($tokens[0]->t)))
	array_unshift($tokens, new WikiLineHeadToken('', $tokens[0]->position, 0));

    $context['tokens'] = &$tokens;

    $subtrees = &wiki_parse_tokens_to_subtrees($context, $tokens);
    if($context['depth'] == 0)debug_watch('subtrees');//
    $blocks = &wiki_subtrees_to_blocks($subtrees);
    if($context['depth'] == 0)debug_watch('blocks');//
    if(count($blocks) >= 2 && empty($context['cell']))
	$context['display'] = 'block';
    $tree = &wiki_blocks_to_tree($blocks);
    if(empty($context['nocache']) && empty($WIKI_CONVERT_CACHE[$context['page']['name']]['tree']) &&
       $context['depth'] == 0)
	$WIKI_CONVERT_CACHE[$context['page']['name']]['tree'] = &$tree;
    if($context['depth'] == 0)debug_watch('tree');//
    if($context['depth'] == 0 && !empty($context['page']['args'])) {
	//pprint_r(wiki_show_tree($tree, '', $context));
    }

    if(!empty($context['phase']) && $context['phase'] == 'tree')
	return $tree;
    wiki_convert_from_tree($context, $node, $tree);
    if($context['depth'] == 0)debug_watch('dom');//
    return $node;
}

function wiki_convert_from_tree(&$context, $node, $tree) {
    global $WIKI_OPERATORS_FUNCTION;

    if($tree instanceof WikiTextTree) {
	wiki_convert_from_tokens_as_text($context, $node, $tree->target);
    } else if($tree instanceof WikiApplyTree) {
	if(!empty($context['aliases'][$tree->id])) {
	    wiki_alias_expand($context, $node, $tree);
	} else if(!empty($WIKI_OPERATORS_FUNCTION[$tree->id])) {
	    $fn = 'wiki_operator_' . $tree->id;
	    if(function_exists($fn)) {
		return $fn($context, $node, $tree);
	    } else
		wiki_convert_from_tokens($context, $node, $tree->target);
	} else {
	    $is_output = false;
	    $output_tokens = array();
	    foreach($context['tokens'] as &$token) {
		if($token === $tree->raw_token_start)
		    $is_output = true;
		if($token === $tree->raw_token_end)
		    break;
		if($is_output)
		    $output_tokens[] = $token;
	    }
	    if(isset($output_tokens[0])) {
		$output_tokens[0] = new WikiTextToken($output_tokens[0]->t, $output_tokens[0]->position, $output_tokens[0]->length);
	    }
	    wiki_convert_from_tokens($context, $node, $output_tokens);
	}
    } else if($tree instanceof WikiDivisionTree) {
	$section = dom_append_child_with_position($node, 'section', $tree);
	dom_append_class($section, 'division');
	$status = wiki_convert_from_trees($context, $section, $tree->children);
	if($section->hasChildNodes() && $section->childNodes->length == 1) {
	    dom_append_dom($node, $section->firstChild);
	}
	dom_remove_empty($section);
	return $status;
    } else if($tree instanceof WikiBlockTree) {
	if(!empty($tree->group)) {
	    $fn = 'wiki_operator_' . $tree->group;
	    if(function_exists($fn))
		$fn($context, $node, $tree);
	    else
		wiki_convert_from_tokens_as_text($context, $node, $tree->target);
	} else if($context['display'] === 'block'){
	    $p = dom_append_child_with_position($node, 'p', $tree);
	    $section = dom_append_child_with_position($node, 'section', $tree);
	    dom_append_class($section, 'block');
	    $tmp_p = dom_create('p');
	    $status = wiki_convert_from_trees($context, $tmp_p, $tree->children);
	    if(dom_has_no_phrasing_content($tmp_p, true))
		$target_element = $section;
	    else 
		$target_element = $p;
	    if($tmp_p->hasAttributes()) {
		foreach($tmp_p->attributes as $attribute) {
		    dom_set_attribute($target_element, $attribute->nodeName, $attribute->nodeValue);
		}
	    }
	    foreach($tmp_p->childNodes as $child_node) {
		dom_append_dom($target_element, $child_node);
	    }
	    dom_remove_empty($p);
	    dom_remove_empty($section);
	    return $status;
	} else {
	    return wiki_convert_from_trees($context, $node, $tree->children);
	}
    }
}

function wiki_convert_from_trees(&$context, $node, &$trees) {
    if(empty($trees))
	return;
    foreach($trees as &$tree) {
	$status = wiki_convert_from_tree($context, $node, $tree);
	if($status == 'abort') {
	    return $status;
	}
    }
}

function wiki_convert_from_tokens_as_text(&$context, $node, &$tokens) {
    $text_buffer = '';
    $n = count($tokens);
    for($i = 0;  $i < $n; $i++) {
	$token = &$tokens[$i];
	if(wiki_is_symbol($token, '~') && ($i + 1) < $n &&
	   $tokens[$i + 1] instanceof WikiLineTailToken)
	    $type = 'br';
	else if($token instanceof WikiURIToken && empty($context['in_link']))
	    $type = 'uri';
	else
	    $type = 'text';
	
	if($type != 'text' && $text_buffer !== '') {
	    sentence_append($node, $text_buffer, $context['page']);
	    $text_buffer = '';
	}

	switch($type) {
	case 'br':
	    dom_append_child($node, 'br');
	    break;
	case 'uri':
	    $a = dom_append_child($node, 'a', 'uri external', array('href' => $token->t));
	    dom_append_text($a, $token->t);
	    break;
	case 'text':
	    $text_buffer .= $token->t;
	}
    }

    if($text_buffer !== '') {
	sentence_append($node, $text_buffer, $context['page']);
    }
}

function wiki_summarize_tree(&$context, $node, $tree) {
    if($tree instanceof WikiApplyTree) {
	if($tree->id == 'section') {
	    //$node = dom_append_child_with_position($node, 'li', $tree);
	    $node = dom_append_child($node, 'li');
	    if(!empty($context['summarize_link'])) {
		$a = dom_append_child($node, 'a');
		$dom = dom_create_document();
		wiki_convert_from_tokens($context, $dom, $tree->target);
		$sectoinname = trim(dom_text($dom));
		dom_append_text($a, $sectoinname);

		$section_id = page_get_section_id($context['page']['name'], $sectoinname);
		$url = url_make($context['page']['name']);
		$url .= '#' . $section_id;
		dom_set_attribute($a, 'href', $url);
		dom_set_attribute($a, 'data-link-pagename', $context['page']['name']);
		dom_set_attribute($a, 'data-link-section', $section_id);
	    } else {
		if(!empty($tree->target)) {
		    wiki_convert_from_tokens($context, $node, $tree->target);
		}
	    }
	}
	if(!empty($tree->children)) {
	    //$ul = dom_append_child_with_position($node, 'ul', $tree);
	    $ul = dom_append_child($node, 'ul');
	    foreach($tree->children as &$child) {
		wiki_summarize_tree($context, $ul, $child);
	    }
	    dom_remove_empty($ul);
	}
    }
}

function wiki_get_position_data(&$pos) {
    if(empty($pos))
	return array();
    
    if(($pos instanceof WikiTree || $pos instanceof WikiToken)) {
	if(!isset($pos->position))
	    return array();
	$p = $pos->position;
	$l = $pos->length;
//    } else if(!empty($pos['position']) && !empty($pos['length'])) {
//	return $pos;
    } else if(is_array($pos)) {
	$p = $pos[0]->position;
	$l = $pos[count($pos) - 1]->position + $pos[count($pos) - 1]->length - $p;
    }
    return  array('position' => $p, 'length' => $l);
}

function wiki_operator_root(&$context, $node, $tree) {
    wiki_convert_from_trees($context, $node, $tree->children);
}
function wiki_operator_section(&$context, $node, &$tree) {
    $section = dom_append_child_with_position($node, 'section', $tree);
    dom_set_attribute($section, 'class', 'section');

    $is_optional = false;
    wiki_parse_parameters($context, $tree, array('long_flags' => array('optional' => &$is_optional)));
    if($is_optional) {
	dom_append_class($section, 'optional');
    }
    
    if(!empty($tree->target)) {
	$head = dom_append_child_with_position($section, 'h1', $tree->target);
	dom_set_attribute($head, 'data-twl', 
			  dom_get_attribute($head, 'data-twl') +
			  dom_get_attribute($head, 'data-twp') - $tree->position);
	dom_set_attribute($head, 'data-twp', $tree->position);
	
	wiki_convert_from_tokens($context, $head, $tree->target);
	dom_remove_empty($head);
	$sectoinname = trim(dom_text($head));
	$id = page_get_section_id($context['page']['name'], $sectoinname);
	if(!isset($context['ids'][$id]) && !empty($context['page']['is_main'])) {
	    dom_set_attribute($section, 'id', $id);
	    $context['ids'][$id] = true;
	}
    }
    wiki_convert_from_trees($context, $section, $tree->children);
    if(!$section->hasChildNodes())
	dom_append_text($section, '');
}

function wiki_operator_through(&$context, $node, $tree) {
    wiki_convert_from_tokens($context, $node, $tree->target, array('append_line_head' => false));
}

function wiki_operator_hr(&$context, $node, $tree) {
    dom_append_child($node, 'hr');
}

function wiki_operator_br(&$context, $node, $tree) {
    dom_append_child($node, 'br');
}

function wiki_operator_list(&$context, $node, $block) {
    $char_to_type = array('-' => 'ul', '+' => 'ol');
    $preindent = 0;
    $type = '';

    $section = dom_append_child_with_position($node, 'section', $block);
    dom_append_class($section, 'list');

    $item_stack = array($section);
    $list_stack = array();
    foreach($block->children as &$tree) {
	$opestring = $tree->symbol;
	$indent = strlen($opestring);
	$type = $char_to_type[substr($opestring, -1, 1)];
	
	$item_node = null;

	for($i = 0; $i < ($indent - $preindent); $i++) {
	    $list_node = dom_append_child($item_stack[0],
					  $char_to_type[substr($opestring, $preindent + $i, 1)]);
	    if($item_stack[0] !== $section)
		dom_append_class($item_stack[0], 'has_children');
	    array_unshift($list_stack, $list_node);

	    $item_node = dom_append_child($list_node, 'li');
	    array_unshift($item_stack, $item_node);
	}
	for($i = 0; $i < ($preindent - $indent); $i++) {
	    array_shift($item_stack);
	    $list_node = array_shift($list_stack);
	}

	if(empty($item_node)) {
	    if($list_stack[0]->nodeName != $type) {
		array_shift($list_stack);
		$list_node = dom_append_child($item_stack[1], $type);
		if($item_stack[1] !== $section)
		    dom_append_class($item_stack[1], 'has_children');
		array_unshift($list_stack, $list_node);
	    }

	    $item_node = dom_append_child($list_stack[0], 'li');
	    array_shift($item_stack);
	    array_unshift($item_stack, $item_node);
	}

	$is_optional = false;
	wiki_parse_parameters($context, $tree, array('long_flags' => array('optional' => &$is_optional)));
	if($is_optional) {
	    dom_append_class($item_node, 'optional');
	}

	wiki_convert_from_tokens($context, $item_node, $tree->target);
	dom_set_position($item_node, $tree);
	$li_pos = wiki_get_position_data($tree->target);
	if(!empty($li_pos)) {
	    dom_set_attribute($item_node, 'data-twlp', $li_pos['position']);
	    dom_set_attribute($item_node, 'data-twll', $li_pos['length']);
	}

	$preindent = $indent;
    }
}

function wiki_operator_deflist(&$context, $node, $block) {
    $dl = dom_append_child_with_position($node, 'dl', $block);
    foreach($block->children as &$tree) {
	list($term, $desc) = wiki_tokens_separate($tree->target, new WikiSymbolToken('|', 0, 0));
	$dt = dom_append_child_with_position($dl, 'dt', $term);
	$term = wiki_tokens_trim_space($term);
	wiki_convert_from_tokens($context, $dt, $term);
	$dd = dom_append_child_with_position($dl, 'dd', $desc);
	$desc = wiki_tokens_trim_space($desc);
	wiki_convert_from_tokens($context, $dd, $desc);
    }
    dom_remove_empty($dl);
}

function wiki_operator_blockquote(&$context, $node, $block) {
    $blockquote = dom_append_child_with_position($node, 'blockquote', $block);
    $tokens = array();
    foreach($block->children as &$tree) {
	if($tree->symbol == '>')
	    $tokens[] = new WikiLineHeadToken('', $tree->position, 0);
	$tokens = array_merge($tokens, $tree->target);
    }    

    $text = '';
    foreach($block->children as &$tree) {
	$text .= wiki_tokens_to_string_without_escape($tree->target);
    }
    sentence_append($blockquote, $text, $context['page'], array('excludes' => array('pagename')));
}

function wiki_operator_em(&$context, $node, $tree) {
    wiki_append_element($context, $node, $tree->target, 'em');
}

function wiki_operator_strong(&$context, $node, $tree) {
    wiki_append_element($context, $node, $tree->target, 'strong');
}

function wiki_operator_del(&$context, $node, $tree) {
    wiki_append_element($context, $node, $tree->target, 'del');
}

function wiki_operator_bracelink(&$context, $node, $tree) {
    if(!empty($context['in_link'])) {
	wiki_convert_from_tokens(
	    $context, $node, $tree->target,
	    array('display' => 'inline', 'append_line_head' => false, 'in_link' => true));
	return;
    }

    $separators = array(
	new WikiSymbolToken('|', 0, 0),
	new WikiSymbolToken('>', 0, 0),
	new WikiSymbolToken(':', 0, 0),
	);
    $name_tokens = $locate_tokens = null;
    $is_open = false;
    $max_score = 0;
    foreach($separators as $separator) {
	foreach(array('wiki_tokens_separate_from_right', 'wiki_tokens_separate') as $func) {
	    list($c_name_tokens, $c_locate_tokens) = $func($tree->target, $separator);
	    $c_is_open = false;
	    $score = empty($c_name_tokens) ? -1 : 1;
	    if(empty($c_locate_tokens))
		$score--;
	    else {
		$score++;
		if($separator->t == '>') {
		    if(wiki_is_symbol($c_locate_tokens[0], '>')) {
			$c_is_open = true;
			array_shift($c_locate_tokens);
		    } else if(!empty($c_name_tokens) &&
			      wiki_is_symbol($c_name_tokens[count($c_name_tokens) - 1], '>')) {
			$c_is_open = true;
			array_pop($c_name_tokens);
		    }
		}
		if(count($c_locate_tokens) === 1 && $c_locate_tokens[0] instanceof WikiURIToken)
		    $score++;
	    }

	    if($max_score < $score) {
		$max_score = $score;
		$name_tokens = $c_name_tokens;
		$locate_tokens = $c_locate_tokens;
		$is_open = $c_is_open;
	    }
	}
    }
    if($max_score <= 0) {
	$name_tokens = $tree->target;
	$locate_tokens = $tree->target;
    }
    
    if(count($locate_tokens) == 1 && $locate_tokens[0] instanceof WikiURIToken) {
	$uri = $locate_tokens[0]->t;
	$a = dom_append_child($node, 'a');
	dom_set_attribute($a, 'href', $uri);
	dom_append_class($a, 'external');
	if(empty($name_tokens))
	    dom_append_text($a, $uri);
    } else {
	if(wiki_is_symbol_in_line($locate_tokens, '#')) {
	    list($pagename_tokens, $section_tokens) = 
		wiki_tokens_separate($locate_tokens, new WikiSymbolToken('#', 0, 0));
	    $pagename = wiki_tokens_to_string($pagename_tokens);
	    $pagename = page_path_to_pagename($pagename, $context['page']['name']);
	    $sectionname = wiki_tokens_to_string($section_tokens);
	    $id = page_get_section_id($pagename, $sectionname);
	} else {
	    $pagename = wiki_tokens_to_string($locate_tokens);
	    $pagename = page_path_to_pagename($pagename, $context['page']['name']);
	    $id = '';
	}

	$a = dom_append_page_link($node, '', $pagename, $id);
    }
    if($is_open)
	dom_set_attribute($a, 'target', '_blank');
    wiki_convert_from_tokens($context, $a, $name_tokens,
			     array('display' => 'inline', 'append_line_head' => false, 'in_link' => true));
}
?>