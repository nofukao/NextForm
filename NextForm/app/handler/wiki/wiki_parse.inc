<?php
class WikiToken {
    var $t = '';
    var $position;
    var $length;
    function __construct($t, $p, $l) {
	$this->t = $t;
	$this->position = $p;
	$this->length = $l;
    }
};
class WikiLineHeadToken extends WikiToken{};
class WikiLineTailToken extends WikiToken{};
class WikiTextToken extends WikiToken{ var $pre_escape = null; };
class WikiURIToken extends WikiToken{};
class WikiSymbolToken extends WikiToken {};

class WikiTree {
    var $target;
    var $position = -1;
    var $length = -1;
    function __construct($t) {$this->target = $t;}
};
class WikiApplyTree extends WikiTree {
    var $id;
    var $symbol;
    var $parameters;
    var $target;
    var $children = array();
    var $raw_token_start = null;
    var $raw_token_end = null;
    function __construct($id, $symbol, $parameters, $target) {
	$this->id = $id;
	$this->symbol = $symbol;
	$this->parameters = $parameters;
	$this->target = $target;
    }
};
class WikiTextTree extends WikiTree {};
class WikiEmptyLineTree extends WikiTree {};
class WikiBlockTree extends WikiTree { 
    var $group;
    var $children = array();
    function __construct($c, $g) {$this->children = $c; $this->group = $g;}
};
class WikiDivisionTree extends WikiTree {
    var $children = array();
    function __construct($c) {$this->children = $c;}
};

function wiki_lex_make_hash(&$values) {
    $hash = array();
    foreach($values as $value) {
	$head = $value[0];
	$hash[$head][] = array('symbol' => $value, 'length' => strlen($value));
    }
    return $hash;
}

$WIKI_TOKEN_SYMBOLS_HASH = wiki_lex_make_hash($WIKI_TOKEN_SYMBOLS);

function &wiki_lex(&$context, &$src) {
    global $WIKI_TOKEN_SYMBOLS_HASH;
    global $WIKI_TOKEN_REGEX;
    global $WIKI_TOKEN_REGEX_HEAD;

    $tokens = array();
    $lines = explode("\n", $src);
    $pos = default_value($context['position_offset'], 0);

    $lines_count = 0;
    $lines_num = count($lines);
    foreach($lines as &$line) {
	$length = strlen($line);
	$offset = 0;
	$text_offset = 0;
	while($offset < $length) {
	    $head = $line[$offset];
	    $add_token = null;

	    if($head === '&' && $offset + 1 < $length && ctype_space($line[$offset + 1])) {
		$add_token = new WikiTextToken($head, $pos + $offset, 1);
		$new_offset = $offset + 1;
	    } else if($head === '&' && $offset + 1 < $length && ctype_punct($line[$offset + 1]) && $line[$offset + 1] != '#') {
		$add_token = new WikiTextToken($line[$offset + 1], $pos + $offset, 2);
		$add_token->pre_escape = substr($line, $offset, 2);
		$new_offset = $offset + 2;
	    } else if(isset($WIKI_TOKEN_SYMBOLS_HASH[$head])) {
		foreach($WIKI_TOKEN_SYMBOLS_HASH[$head] as &$symbol) {
		    if(substr($line, $offset, $symbol['length']) === $symbol['symbol']) {
			$new_offset = $offset + $symbol['length'];
			$add_token = new WikiSymbolToken($symbol['symbol'], $pos + $offset, $symbol['length']);
			break;
		    }
		}
	    }

	    if(empty($add_token) && isset($WIKI_TOKEN_REGEX_HEAD[$head])) {
		if(($token_type = preg_match_table($WIKI_TOKEN_REGEX, '/^', '/', substr($line, $offset), $matches))) {
		    $l = strlen($matches[1]);
		    $new_offset = $offset + $l;
		    $add_token = new $token_type($matches[1], $pos + $offset, $l);
		}
	    } 

	    if(!empty($add_token)) {
		if($text_offset < $offset) {
		    $tokens[] = new WikiTextToken(substr($line, $text_offset, $offset - $text_offset), $pos + $text_offset, $offset - $text_offset);
		}
		$text_offset = $new_offset;
		$offset = $new_offset;
		$tokens[] = $add_token;
	    } else {
		$offset++;
	    } 
	}
	if($text_offset < $length) {
	    $tokens[] = new WikiTextToken(substr($line, $text_offset), $pos + $text_offset, $length - $text_offset);
	}

	if(++$lines_count != $lines_num) {
	    $pos += $length;
	    $tokens[] = new WikiLineTailToken("\n", $pos, 1);
	    $pos ++;
	    $tokens[] = new WikiLineHeadToken('', $pos, 0);
	}
    }

    if(!empty($context['source_position'])) {
	foreach($tokens as &$token) {
	    $token->position = $context['source_position']->position;
	    $token->length = $context['source_position']->length;
	}
    }

    return $tokens;
}

function wiki_is_symbol(&$token, $symbol = '') {
    return $token instanceof WikiSymbolToken && (empty($symbol) || $token->t === $symbol);
}

function wiki_is_symbol_in_line(&$tokens, $symbol, $offset = 0) {
    $foward = 0;
    for(; $offset > 0; $offset--) {
	if(nextstop($tokens) === false) {
	    for(; $foward > 0; $foward--, prev($tokens));
	    return false;
	}
	$foward++;
    }

    while(($token = current($tokens)) !== false) {
	if(wiki_is_symbol($token, $symbol)) {
	    for(; $foward > 0; $foward--, prev($tokens));
	    return true;
	}
	if($token instanceof WikiLineTailToken) {
	    for(; $foward > 0; $foward--, prev($tokens));
	    return false;
	}

	if(nextstop($tokens) === false) {
	    for(; $foward > 0; $foward--, prev($tokens));
	    return false;
	}
	$foward++;
    }

    for(; $foward > 0; $foward--, prev($tokens));
    return false;
}

function wiki_take_tokens_brace(&$tokens, $open_symbol, $close_symbol,
				$is_multi_line = true, $is_remove_tail_space = true) {
    $count = 1;
    $result = array();
    while(($token = current($tokens)) !== false) {
	next($tokens);
	if(!empty($close_symbol) && wiki_is_symbol($token, $close_symbol)) {
	    $count--;
	    if($count == 0)
		break;
	    else
		$result[] = $token;
	} else {
	    if(!empty($open_symbol) && wiki_is_symbol($token, $open_symbol))
		$count++;
	    $result[] = $token;
	}

	if(!$is_multi_line && $token instanceof WikiLineTailToken)
	    break;
    }
    if($is_remove_tail_space) {
	while(($token = current($tokens)) !== false &&
	      (($token instanceof WikiSymbolToken && ctype_space($token->t)) ||
	       ($token instanceof WikiLineTailToken)) ) {
	    next($tokens);
	}
    }

    return $result;
}

function wiki_take_tokens_brace_target(&$tokens, $brace_num, $is_remove_tail_space) {
    $open_brace_count = 0;
    $close_brace_count = 0;
    $count = 1;
    $result = array();
    while(($token = current($tokens)) !== false) {
	next($tokens);

	if(wiki_is_symbol($token, '{')) {
	    $open_brace_count++;
	    $close_brace_count = 0;
	} else if(wiki_is_symbol($token, '}')) {
	    $close_brace_count++;
	    $open_brace_count = 0;
	} else {
	    $open_brace_count = 0;
	    $close_brace_count = 0;
	}

	if($close_brace_count == $brace_num) {
	    $count--;
	    $open_brace_count = 0;
	    $close_brace_count = 0;
	    if($count == 0) {
		for(; $brace_num > 1; $brace_num--)
		    array_pop($result);

		/* right side match */
		while(($token = current($tokens)) !== false && wiki_is_symbol($token, '}')) {
		    array_push($result, $token);
		    next($tokens);
		}
		break;
	    } else
		$result[] = $token;
	} else {
	    if($open_brace_count == $brace_num) {
		$count++;
		$open_brace_count = 0;
		$close_brace_count = 0;
	    }
	    $result[] = $token;
	}
    }

    if($is_remove_tail_space) {
	while(($token = current($tokens)) !== false &&
	      (($token instanceof WikiSymbolToken && ctype_space($token->t)) ||
	       ($token instanceof WikiLineTailToken)) ) {
	    next($tokens);
	}
    }
    return $result;
}

function wiki_search_symbol(&$tokens, $symbol, $is_remove = false, $is_skip_line = true) {
    $i = 0;
    if(($token = current($tokens)) === false)
	return false;
    while(ctype_space($token->t) && ($is_skip_line || $token->t != "\n")) {
	if(($token = next($tokens)) === false) {
	    for(; $i > 0; $i--) prev($tokens);
	    return false;
	}
	$i++;
    }
    
    if($token instanceof WikiSymbolToken && $token->t === $symbol) {
	if(!$is_remove) {
	    for(; $i > 0; $i--) prev($tokens);
	}
	return true;
    }
    for(; $i > 0; $i--) prev($tokens);
    return false;
}

function wiki_take_parameters(&$tokens) {
    $count = 1;
    $result = array();
    $arg = array();
    
    if(($token = current($tokens)) === false)
	return $result;
    if(!wiki_is_symbol($token, '('))
	return $result;

    while(($token = next($tokens)) !== false) {
	if(wiki_is_symbol($token, ',')) {
	    $result[] = $arg;
	    $arg = array();
	} else if(wiki_is_symbol($token, ')')) {
	    $count--;
	    if($count == 0) {
		next($tokens);
		break;
	    } else
		$arg[] = $token;
	} else {
	    if(wiki_is_symbol($token, '('))
		$count++;
	    $arg[] = $token;
	}
    }
    if(!empty($arg))
	$result[] = $arg;

    foreach($result as &$arg) {
	$arg = wiki_tokens_trim_space($arg);
    }

    return $result;
}

function wiki_take_target_line(&$tokens, $is_semicolon = true) {
    $result = array();
    while(($token = current($tokens)) !== false) {
	next($tokens);
	if($is_semicolon && wiki_is_symbol($token, ';'))
	    break;
	$result[] = $token;
	if($token instanceof WikiLineTailToken)
	    break;
    }
    return $result;
}

function wiki_take_target(&$tokens, $is_multi_line = true, $is_semicolon = true,
			  $is_remove_tail_space = true, $open_brace_num = 0) {
    if($open_brace_num > 0) {
	$result = wiki_take_tokens_brace_target($tokens, $open_brace_num, $is_remove_tail_space);
	if(wiki_search_symbol($tokens, ';', true, false))
	    next($tokens);
	return $result;
    } else if($is_multi_line && wiki_search_symbol($tokens, '{', true, false)) {
	$open_brace_num = 0;
	while(($token = current($tokens)) !== false && wiki_is_symbol($token, '{')) {
	    $open_brace_num++;
	    next($tokens);
	}
	$result = wiki_take_tokens_brace_target($tokens, $open_brace_num, $is_remove_tail_space);
	if(wiki_search_symbol($tokens, ';', true, false))
	    next($tokens);
	return $result;
    } else {
	if($is_semicolon)
	    wiki_search_symbol($tokens, ';', true, false);
	return wiki_take_target_line($tokens, $is_semicolon);
    }
}

$wiki_parse_text_tokens = array();
$wiki_parse_subtrees = array();

function wiki_parse_flush_text_tokens() {
    global $wiki_parse_text_tokens;
    global $wiki_parse_subtrees;
    if(!empty($wiki_parse_text_tokens)) {
	$text_tree = new WikiTextTree($wiki_parse_text_tokens);
	$wiki_parse_subtrees[] = $text_tree;

	$first_token = &$wiki_parse_text_tokens[0];
	$last_token = &$wiki_parse_text_tokens[count($wiki_parse_text_tokens) - 1];
	$text_tree->position = $first_token->position;
	$text_tree->length = $last_token->position + $last_token->length - $first_token->position;
	$wiki_parse_text_tokens = array();
    }
}

function &wiki_parse_tokens_to_subtrees(&$context, &$tokens) {
    global $WIKI_OPERATORS_LINE_HEAD;
    global $WIKI_OPERATORS_INLINE;
    global $WIKI_OPERATORS_FUNCTION;
    global $WIKI_ENTITY_REFERENCES;
    global $wiki_parse_text_tokens;
    global $wiki_parse_subtrees;

    $wiki_parse_text_tokens = array();
    $wiki_parse_subtrees = array();
    $add_tree = null;

    $last_token = end($tokens);
    if($last_token !== false)
	$last_position = $last_token->position + $last_token->length;

    reset($tokens);

    while(($token = current($tokens)) !== false) {
	$position = $token->position;

	$is_match = false;
	if($token instanceof WikiLineHeadToken) {
	    $lh = $token;

	    if(next($tokens) === false)
		end($tokens);
	    else {
		$open_brace_num = 0;
		if(($token = current($tokens)) !== false) {
		    if($token instanceof WikiLineTailToken) {
			next($tokens);
			$add_tree = new WikiEmptyLineTree(array($lh, $token));
			$is_match = true;
		    } else if($token instanceof WikiSymbolToken) {
			while(wiki_is_symbol($token, '{')) {
			    $open_brace_num++;
			    $token = next($tokens);
			}
			if($token !== false) {
			    foreach($WIKI_OPERATORS_LINE_HEAD as $opename => &$operator) {
				if(preg_match('/^' . $operator['symbol'] . '$/', $token->t)) {
				    $symbol_token = $token;
				    next($tokens);
				    $parameters = wiki_take_parameters($tokens);
				    $target = wiki_take_target($tokens,
							       $operator['symbol'] != ' ',
							       false, true, $open_brace_num);
				    $add_tree = new WikiApplyTree($opename,
								  $symbol_token->t, $parameters, $target);
				    $is_match = true;
				    break;
				}
			    }
			} else {
			    $open_brace_num--;
			    end($tokens);
			}
		    }
		}

		if(!$is_match) {
		    prev($tokens);
		    for(; $open_brace_num > 0; $open_brace_num--)
			prev($tokens);
		}
	    }
	    
	} else if(wiki_is_symbol($token, '/*')) {
	    next($tokens);
	    $target = wiki_take_tokens_brace($tokens, '/*', '*/', true, true);
	    //$add_tree = new WikiApplyTree('comment', '/*', null, $target);
	    if(!empty($context['texts_only']))
		$add_tree = new WikiTextTree($target);
	    $is_match = true;
	} else if(wiki_is_symbol($token, '//')) {
	    next($tokens);
	    $target = wiki_take_tokens_brace($tokens, '//', '', false, false);
	    //$add_tree = new WikiApplyTree('comment', '//', null, $target);
	    if(!empty($context['texts_only']))
		$add_tree = new WikiTextTree($target);
	    $is_match = true;
	} else if(wiki_is_symbol($token, '&')) {
	    $raw_token_start = $token;
	    if(($token = nextstop($tokens)) !== false) {
		if(!empty($WIKI_ENTITY_REFERENCES[$token->t])) {
		    next($tokens);
		    if(wiki_search_symbol($tokens, ';', true, false))
			next($tokens);
		    $add_tree = new WikiApplyTree('entity', $token->t, array(), array());
		    $is_match = true;
		} else if($token->t === '#') {
		    $token = next($tokens);
		    if($token !== false) {
			$add_tree = new WikiApplyTree('entity_number', $token->t, array(), array());
			next($tokens);
			$is_match = true;
			if(wiki_search_symbol($tokens, ';', true, false))
			    next($tokens);
		    }
		} else {
		    next($tokens);
		    $parameters = wiki_take_parameters($tokens);
		    $target = wiki_take_target($tokens, true, true, false);
		    $add_tree = new WikiApplyTree($token->t, '&', $parameters, $target);
		    $add_tree->raw_token_start = $raw_token_start;
		    $add_tree->raw_token_end = current($tokens);
		    $is_match = true;
		}
	    }
	} else {
	    foreach($WIKI_OPERATORS_INLINE as $opename => &$operator) {
		if(wiki_is_symbol($token, $operator['open'])) {
		    if(wiki_is_symbol_in_line($tokens, $operator['close'], 1)) {
			next($tokens);
			$taget_tokens = wiki_take_tokens_brace(
			    $tokens,
			    $operator['open'], $operator['close'], false, false);
			$add_tree = new WikiApplyTree($opename, $token->t, null, $taget_tokens);
			$is_match = true;
		    }
		    break;
		}
	    }
	}

	if(!$is_match) {
	    $token = current($tokens);
	    $wiki_parse_text_tokens[] = $token;
	    next($tokens);
	}

	if($is_match && !empty($add_tree)) {
	    wiki_parse_flush_text_tokens();

	    if(!empty($context['source_position'])) {
		$add_tree->position = $context['source_position']->position;
		$add_tree->length = $context['source_position']->length;
	    } else {
		$add_tree->position = $position;
		if(($token = current($tokens)) !== false)
		    $add_tree->length = $token->position - $position;
		else
		    $add_tree->length = $last_position - $position;
	    }

	    $wiki_parse_subtrees[] = $add_tree;
	    $add_tree = null;
	}
    }

    wiki_parse_flush_text_tokens();

    return $wiki_parse_subtrees;
}

function &wiki_subtrees_to_blocks(&$subtrees) {
    global $WIKI_OPERATORS_LINE_HEAD;
    global $WIKI_OPERATORS_INLINE;
    global $WIKI_OPERATORS_FUNCTION;

    $blocks = array();
    $block = array();
    $group = '';
    foreach($subtrees as &$tree) {
	if(wiki_tree_is_empty($tree))
	    continue;
	if($tree instanceof WikiApplyTree && !empty($WIKI_OPERATORS_LINE_HEAD[$tree->id])) {
	    $operator = &$WIKI_OPERATORS_LINE_HEAD[$tree->id];

	    if(!empty($operator['group'])) {
		if($group === $tree->id) {
		    $block[] = &$tree;
		} else {
		    if(!empty($block)) {
			$blocks[] = new WikiBlockTree($block, $group);
			$block = array();
		    }
		    $block = array(&$tree);
		    $group =  $tree->id;
		}
	    } else {
		if(!empty($block)) {
		    $blocks[] = new WikiBlockTree($block, $group);
		    $block = array();
		}
		$blocks[] = &$tree;
	    }
	} else if($tree instanceof WikiEmptyLineTree) {
	    if(!empty($block)) {
		$blocks[] = new WikiBlockTree($block, $group);
		$block = array();
	    }
	    $group = '';
	    $blocks[] = $tree;
	} else {
	    if(!empty($group)) {
		if(!empty($block)) {
		    $blocks[] = new WikiBlockTree($block, $group);
		    $block = array();
		}
		$group = '';
	    }
	    $block[] = &$tree;
	}
    }
    if(!empty($block))
	$blocks[] = new WikiBlockTree($block, $group);

    return $blocks;
}

function wiki_tree_is_empty($tree) {
    if(empty($tree))
	return true;
    if($tree instanceof WikiTextTree) {
	foreach($tree->target as &$token) {
	    if($token->t !== '')
		return false;
	}
    } else if($tree instanceof WikiBlockTree) {
	if(empty($tree->children))
	    return true;
	foreach($tree->children as &$child) {
	    if(!wiki_tree_is_empty($child))
		return false;
	}
    } else {
	return false;
    }
    return true;
}

function &wiki_blocks_to_tree(&$blocks) {
    $root = new WikiApplyTree('root', '', null, array());

    $parent_stack = array(&$root);
    $preindent = $indent = 0;

    $section_trees = array(&$root);

    foreach($blocks as &$block) {
	if($block instanceof WikiApplyTree && $block->id === 'section') {
	    $indent = strlen($block->symbol);
	    if(!empty($block->parameters) &&
	       wiki_tokens_to_string($block->parameters[0]) === 'end') {
		if($indent <= $preindent) {
		    $indent_diff = $preindent - $indent;
		    for($i = 0; $i <= $indent_diff; $i++) {
			array_shift($parent_stack);
		    }
		    $preindent -= $indent_diff + 1;
		}
	    } else {
		if($indent > $preindent) {
		    for($i = 0; $i < ($indent - $preindent) - 1; $i++) {
			$n = new WikiApplyTree('section', str_repeat('*', $preindent + $i + 1), null, array());
			$parent_stack[0]->children[] = $n;
			array_unshift($parent_stack, $n);
			$section_trees[] = $n;
		    }
		    $parent_stack[0]->children[] = $block;
		    array_unshift($parent_stack, $block);
		} else if($indent <= $preindent) {
		    for($i = 0; $i <= ($preindent - $indent); $i++) {
			array_shift($parent_stack);
		    }
		    $parent_stack[0]->children[] = $block;
		    array_unshift($parent_stack, $block);
		}
		$preindent = $indent;
	    }
	    $section_trees[] = $block;
	} else {
	    $parent_stack[0]->children[] = $block;
	}
    }
    wiki_set_tree_position($root);

    foreach($section_trees as $section_tree) {
	$block = array();
	$new_children = array(&$block);
	foreach($section_tree->children as $child_tree) {
	    if($child_tree instanceof WikiApplyTree && ($child_tree->id === 'section' || $child_tree->id === 'hr')) {
		unset($block);
		$new_children[] = $child_tree;
		$block = array();
		$new_children[] =& $block;
	    } else if($child_tree instanceof WikiEmptyLineTree) {
		unset($block);
		$block = array();
		$new_children[] =& $block;
	    } else
		$block[] = $child_tree;
	}
	unset($block);
	$section_tree->children = array();
	foreach($new_children as $block) {
	    if($block instanceof WikiApplyTree) {
		$section_tree->children[] = $block;
	    } else if(!empty($block)){
		if(count($block) > 1) {
		    $block_tree = new WikiDivisionTree($block);
		    wiki_set_tree_position($block_tree);
		    $section_tree->children[] = $block_tree;
		} else {
		    $section_tree->children[] = $block[0];
		}
	    }
	}
    }
    
    return $root;
}

function wiki_set_tree_position($tree) {
    if(!empty($tree->children)) {
	array_walk($tree->children, 'wiki_set_tree_position');
	$first_child = &$tree->children[0];
	$last_child = &$tree->children[count($tree->children) - 1];

	$new_position = $tree->position;
	$new_length = $tree->length;
	if($tree->position < 0 || $first_child->position < $tree->position)
	    $new_position = $first_child->position;
	if($tree->length < 0 || $tree->position + $tree->length < $last_child->position + $last_child->length)
	    $new_length = $last_child->position + $last_child->length - $new_position;
	$tree->position = $new_position;
	$tree->length = $new_length;
    }
}

function wiki_show_token(&$token) {
    $pos = '|' . $token->position . '-' . $token->length . '|'; 
    if($token instanceof WikiTextToken)
	return '[Text]' . $token->t  . $pos;
    else if($token instanceof WikiURIToken)
	return '[URI]' . $token->t  . $pos;
    else if($token instanceof WikiSymbolToken)
	return '[Symbol]' . $token->t  . $pos;
    else if($token instanceof WikiLineHeadToken)
	return '[LH]'  . $pos;
    else if($token instanceof WikiLineTailToken)
	return '[LT]'  . $pos;
    else
	return '[?]' . print_r($token, true)  . $pos;
}

function wiki_show_tokens(&$tokens, $delim = '') {
    $output = array();
    foreach($tokens as &$token) {
	$output[] = wiki_show_token($token);
    }
    return implode($delim, $output) . $delim;
}

function wiki_show_tree($tree, $indent = '', &$context = null) {
    $output = $indent;
    $pos = '|' . $tree->position . '-' . $tree->length . '|';
    if(!empty($context['source']))
	$pos .= '['.str_replace("\n", '\n', substr($context['source'], $tree->position, $tree->length)).']';

    if($tree instanceof WikiApplyTree) {
	if(empty($tree->parameters))
	    $parameters = '';
	else {
	    $parameters = array();
	    foreach($tree->parameters as $a) {
		$parameters[] = wiki_show_tokens($a);
	    }
	    $parameters = implode(',', $parameters);
	}
	$output .= '(Apply ' . $tree->id  . ' ' .
	    $tree->symbol . ' ( ' .
	    $parameters . ' ) { ' .
	    wiki_show_tokens($tree->target) . " }) $pos\n";
	if(!empty($tree->children)) {
	    foreach($tree->children as $child) {
		$output .= wiki_show_tree($child, $indent . '    ', $context);
	    }
	}

    } else if($tree instanceof WikiTextTree){
	$output .= '(Text ' . wiki_show_tokens($tree->target) . ") $pos\n";
    } else if($tree instanceof WikiEmptyLineTree){
	$output .= '(EmptyLine ' . wiki_show_tokens($tree->target) . ") $pos\n";
    } else if($tree instanceof WikiDivisionTree){
	$output .= "Division $pos\n";
	foreach($tree->children as $subtree) {
	    $output .= wiki_show_tree($subtree, $indent . '    ', $context);
	}
    } else if($tree instanceof WikiBlockTree){
	$output .= 'Block [' . $tree->group . "] $pos\n";
	foreach($tree->children as $subtree) {
	    $output .= wiki_show_tree($subtree, $indent . '    ', $context);
	}
    } else
	$output .= '(? ' . print_r($tree, true) . ") $pos\n";
    return $output;
}

function wiki_tokens_separate(&$tokens, $separator_token) {
    $before = array();
    $after = array();
    $target = &$before;
    $is_found = false;
    foreach($tokens as &$token) {
	if(!$is_found &&
	   $token instanceof $separator_token && $token->t === $separator_token->t) {
	    $target = &$after;
	    $is_found = true;
	} else {
	    $target[] = $token;
	}
    }
    return array($before, $after);
}

function wiki_tokens_separate_from_right(&$tokens, $separator_token) {
    $before = array();
    $after = array();
    $target = &$after;
    $is_found = false;
    for($i = count($tokens) - 1; $i >= 0; $i--) {
	$token =& $tokens[$i]; 
	if(!$is_found &&
	   $token instanceof $separator_token && $token->t === $separator_token->t) {
	    $target = &$before;
	    $is_found = true;
	} else {
	    array_unshift($target, $token);
	}
    }
    return array($before, $after);
}

function wiki_tokens_split(&$tokens, $separator_token) {
    $fields = array();
    $field = array();
    foreach($tokens as &$token) {
	if($token instanceof $separator_token && $token->t === $separator_token->t) {
	    $fields[] = $field;
	    $field = array();
	} else {
	    $field[] = $token;
	}
    }
    $fields[] = $field;
    return $fields;
}

function wiki_show_trees(&$trees, $delim = '') {
    $output = array();
    foreach($trees as &$tree) {
	$output[] = wiki_show_tree($tree);
    }
    return implode($delim, $output);
}

?>
