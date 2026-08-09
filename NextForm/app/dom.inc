<?php
require_once(APP_DIR_PATH . '/dom_model.inc');

function dom_create_document() {
    $dom = new DOMDocument('1.0','UTF-8');
    $dom->preserveWhiteSpace = true;
    $dom->formatOutput = false;
    return dom_append_child($dom, 'div');
}

function dom_owner($dom) {
    if($dom instanceof DOMDocument)
	return $dom;
    else
	return $dom->ownerDocument;
}

function dom_set_attribute($dom, $name, $value) {
    $dom->setAttribute($name, $value);
}

function dom_get_attribute($dom, $name) {
    return $dom->getAttribute($name);
}

function dom_create_child(&$dom, $name, $class = '', $attributes = array()) {
    if(is_null($dom))
	$dom = dom_create_document();
    $n = dom_owner($dom)->createElement($name);
    if($class !== '')
	$attributes['class'] = $class;
    foreach($attributes as $name => $value) {
	dom_set_attribute($n, $name, $value);
    }
    return $n;
}

function dom_append_child($dom, $name, $class = '', $attributes = array()) {
    $n = dom_create_child($dom, $name, $class, $attributes);
    $dom->appendChild($n);
    return $n;
}

function dom_insert_child($dom, $name, $class = '', $attributes = array()) {
    $n = dom_create_child($dom, $name, $class, $attributes);
    $dom->insertBefore($n, $dom->firstChild);
    return $n;
}

function dom_set_position($dom, &$pos = null) {
    if(!empty($pos)) {
	$p = wiki_get_position_data($pos);
	if(!empty($p)) {
	    dom_set_attribute($dom, 'data-twp', $p['position']);
	    dom_set_attribute($dom, 'data-twl', $p['length']);
	}
    }
}

function dom_remove_position($dom) {
    if($dom instanceof DOMElement) {
	$dom->removeAttribute('data-twl');
	$dom->removeAttribute('data-twp');
	foreach($dom->childNodes as $child) {
	    dom_remove_position($child);
	}
    }
}

function dom_append_child_with_position($dom, $name, &$pos = null) {
    $n = dom_append_child($dom, $name);
    dom_set_position($n, $pos);
    return $n;
}

function dom_insert_child_with_position($dom, $name, &$pos = null) {
    $n = dom_insert_child($dom, $name);
    dom_set_position($n, $pos);
    return $n;
}

function dom_append_text($dom, $text) {
    $n = dom_owner($dom)->createTextNode($text);
    $dom->appendChild($n);
    return $n;
}

function dom_append_comment($dom, $text) {
    $n = dom_owner($dom)->createComment($text);
    $dom->appendChild($n);
    return $n;
}

function dom_insert_text($dom, $text) {
    $n = dom_owner($dom)->createTextNode($text);
    $dom->insertBefore($n, $dom->firstChild);
    return $n;
}

function dom_append_ref($dom, $name) {
    $n = dom_owner($dom)->createEntityReference($name);
    $dom->appendChild($n);
}

function dom_remove_empty($dom) {
    if(!$dom->hasChildNodes()) {
	$dom->parentNode->removeChild($dom);
	return false;
    }
    return $dom;
}

function dom_append_input_hidden($dom, $hiddens) {
    foreach($hiddens as $name => $value) {
	$hidden = dom_append_child($dom, 'input', '',
				   array(
				       'type' => 'hidden',
				       'name' => $name,
				       'value' => $value,
				       ));
    }
}

function dom_create($name, $class = '', $attributes = array()) {
    $doc = dom_create_document();
    return dom_create_child($doc, $name, $class, $attributes);
}

function dom_create_text($text) {
    $doc = new DOMDocument('1.0','UTF-8');
    return $doc->createTextNode($text);
}

function dom_append_dom($dom, $adom) {
    $adom = dom_owner($dom)->importNode($adom, true);
    $dom->appendChild($adom);
    return $adom;
}

function dom_insert_dom($dom, $adom) {
    $adom = dom_owner($dom)->importNode($adom, true);
    $dom->insertBefore($adom, $dom->firstChild);
}

function dom_append_dom_children($dom, $adom) {
    foreach($adom->childNodes as $child) {
	dom_append_dom($dom, $child);
    }
}

function dom_append_class($dom, $add_class) {
    $class = dom_get_attribute($dom, 'class');
    if($class === '')
	$class = $add_class;
    else
	$class = $class . ' ' . $add_class;
    dom_set_attribute($dom, 'class', $class);
}

function dom_has_class($dom, $class) {
    return in_array($class, explode(' ', dom_get_attribute($dom, 'class')));
}

function dom_save_html($dom) {
    $owner = dom_owner($dom);
    $output = '';
    foreach($dom->childNodes as $child) {
	$output .= $owner->saveXML($child);
    }
    return $output;
}

function dom_append_link($dom, $uri, $name = '', $class = '', $style = '') {
    $a = dom_append_child($dom, 'a', '', array('href' => $uri));
    if($name !== '')
	dom_append_text($a, $name);
    if($class !== '')
	dom_set_attribute($a, 'class', $class);
    if($style !== '')
	dom_set_attribute($a, 'style', $style);
    return $a;
}

function dom_set_self_link_pagename($pagename = null) {
    global $dom_self_link_pagename;
    $dom_self_link_pagename = $pagename;
}

function dom_append_empty_page_link($dom, $pagename, $id = '', $uri = '') {
    global $USER_HISTORY_IS_LOADED;
    global $dom_self_link_pagename;

    $a = dom_append_child($dom, 'a', 'page');
    dom_set_attribute($a, 'data-link-pagename', $pagename);
    if(isset($dom_self_link_pagename)) {
	$relations = page_get_relations($dom_self_link_pagename, $pagename);
	foreach($relations as $relation) {
	    dom_append_class($a, $relation);
	}
    }

    if(!page_is_exists($pagename)) {
	dom_append_class($a, 'not_exists');
	dom_set_attribute($a, 'rel', 'nofollow');
    } else if(USER_HISTORY_ENABLE == 'true' && USER_HISTORY_UNREAD == 'true' &&
	      $USER_HISTORY_IS_LOADED &&
	      auth_has_permission($pagename, 'read')) {
	$page = page_read($pagename);

	if(!empty($page['mtime'])) {
	    if(($time = user_history_get_page_view($pagename)) === false ||
		   $page['mtime'] > $time) {
		    dom_append_class($a, 'unread');		    
	    }	
	}
    }
    if($uri === '') {
	$uri = url_make($pagename);
	if($id !== '') {
	    $uri .= '#' . $id;
	    dom_set_attribute($a, 'data-link-section', $id);
	}
    }
    dom_set_attribute($a, 'href', $uri);
    return $a;
}

function dom_append_page_link($dom, $name, $pagename, $id = '', $uri = '') {
    $a = dom_append_empty_page_link($dom, $pagename, $id, $uri);
    if($name !== '')
	dom_append_text($a, $name);
    return $a;
}

function dom_append_error($dom, $message) {
    $small = dom_append_child($dom, 'small', 'error');
    dom_append_text($small, $message);
    return $small;
}

function dom_pagename($pagename) {
    $names = explode('/', $pagename);
    $pagename = '';
    $doms = array();
    $n = count($names);
    for($i = 0; $i < $n; $i++) {
	$name = $names[$i];
	$pagename .= $name;
	$a = dom_create('a');
	dom_set_attribute($a, 'href', url_make($pagename));
	dom_append_text($a, $name);
	if(!page_is_exists($pagename))
	    dom_set_attribute($a, 'rel', 'nofollow');
	$doms[] = $a;
	if($i == $n - 1)
	    break;
	$doms[] = dom_create_text('/');
	$pagename .= '/';
    }
    return $doms;
}

function dom_append_form($dom, $hiddens = array(), $class = '', $method = 'POST') {
    global $main_pagename;
    $action = BASE_URI . '?';
    $form = dom_append_child($dom, 'form', $class,
			     array('method' => $method, 'action' => $action));
    dom_append_input_hidden($form, $hiddens);

    return $form;
}

function dom_append_page_form($page, $dom, $hiddens = array(), $class = '', $method = 'POST') {
    global $main_pagename;
    $hiddens['page'] = $main_pagename;
    $hiddens['pagename'] = $page['name'];
    if(!empty($page['meta']['ticket']) && empty($hiddens['ticket']))
	$hiddens['ticket'] = $page['meta']['ticket'];
    $form = dom_append_child($dom, 'form', $class,
			     array('method' => $method, 'action' => url_make($main_pagename)));
    dom_append_input_hidden($form, $hiddens);

    return $form;
}

function dom_append_selector($dom, $name, $options, $default_value = '', $is_empty_option_add = false) {
    $select = dom_append_child($dom, 'select', '', array('name' => $name));
    
    foreach($options as $value => $label) {
	$attributes = array('value' => $value);
	if($default_value == $value) {
	    $attributes['selected'] = 'selected';
	    $is_empty_option_add = false;
	}
	$option = dom_append_child($select, 'option', '', $attributes);
	dom_append_text($option, $label);
	if($label == '')
	    dom_set_attribute($option, 'label', ' ');
    }
    if($is_empty_option_add) {
	$option = dom_append_child($select, 'option', '',
				   array('value' => '', 'selected' => 'selected'));
	dom_append_text($option, '');
	dom_set_attribute($option, 'label', ' ');
    }
    return $select;
}
function dom_append_time($dom, $time, $format = '') {
    if($time == 'now' || $time == 'Now') {
	$time = time();
	$format = l('Now');
    }
    $t = dom_append_child($dom, 'time', '',
			  array('datetime' => time_xmlformat($time)));
    if($format !== '')
	dom_append_text($t, nf_date($format, $time));
    return $t;
}

function dom_append_compare_line($dom, &$text1, &$text2) {
    $lines1 = explode("\n", $text1);
    $lines2 = explode("\n", $text2);
    return dom_append_diff($dom, $lines1, $lines2);
}

function dom_append_diff($dom, &$tokens1, &$tokens2) {
    $diff_result = diff($tokens1, $tokens2);

    $ul = dom_append_child($dom, 'ul', 'compare legend');
    $li = dom_append_child($ul, 'li', 'delete');
    dom_append_text($li, l('Deleted line'));
    $li = dom_append_child($ul, 'li', 'add');
    dom_append_text($li, l('Added line'));
    
    $ul = dom_append_child($dom, 'ul', 'compare');

    foreach($diff_result as &$diff_line) {
	$li = dom_append_child($ul, 'li', $diff_line['change']);
	if($diff_line['change'] == 'add')
	    $e = dom_append_child($li, 'ins');
	else if($diff_line['change'] == 'delete')
	    $e = dom_append_child($li, 'del');
	else
	    $e = $li;
	dom_append_text($e, $diff_line['string']);
    }
}

function dom_equals($dom1, $dom2) {
    if(get_class($dom1) !== get_class($dom2))
	return false;
    if($dom1 instanceof DOMText) {
	return $dom1->wholeText === $dom2->wholeText;
    } else {
	return $dom1->tagName === $dom2->tagName &&
	    dom_get_attribute($dom1, 'class') === dom_get_attribute($dom2, 'class');
    }
}

function dom_in_tag_name($dom, $tag_name) {
    $cdom = $dom;
    while(true) {
	if($cdom->tagName === $tag_name) {
	    return true;
	}
	if(!isset($cdom->parentNode) || $cdom->parentNode instanceof DOMDocument)
	    break;
	$cdom = $cdom->parentNode;
    }
    return false;
}

function dom_equals_recursive($dom1, $dom2) {
    if(!dom_equals($dom1, $dom2))
	return false;
    if($dom1 instanceof DOMText)
	return true;
    if($dom1->childNodes->length !== $dom2->childNodes->length)
	return false;
    for($i = 0; $i < $dom1->childNodes->length; $i++) {
	if(!dom_equals_recursive($dom1->childNodes->item($i), $dom2->childNodes->item($i)))
	    return false;
    }
    return true;
}

function dom_append_search($dom, $query_text) {
    $form = dom_append_form($dom, array('option' => 'search'), 'search', 'GET');
    $submit = dom_append_child($form, 'input', '', 
			       array('type' => 'search',
				     'name' => 'query',
				     'value' => $query_text,
				     'placeholder' => l('Search keywords')));
    $submit = dom_append_child($form, 'input', '',
			       array('type' => 'submit', 'value' => l('Search')));
    return $form;
}

function dom_append_hightlighted_text($dom, $text, $highlights) {
    $page = array('query' => $highlights);
    sentence_init($page);
    sentence_append($dom, $text, $page, array('only' => 'search', 'nonests' => true));
}

function dom_text($dom) {
    return $dom->textContent;
}

function dom_texts($dom, &$texts = array()) {
    if($dom instanceof DOMText) {
	$text = trim($dom->textContent);
	if($text !== '')
	    $texts[] = $text;
    } else {
	if(!empty($dom->childNodes)) {
	    $buf = '';
	    foreach($dom->childNodes as $childNode) {
		if($childNode instanceof DOMText || 
		   $childNode instanceof DOMEntityReference) {
		    $buf .= trim($childNode->textContent);
		} else {
		    if($buf !== '') {
			$texts[] = $buf;
			$buf = '';
		    }
		    dom_texts($childNode, $texts);
		}
	    }
	    if($buf !== '')
		$texts[] = $buf;
	}
    }
}

function dom_get_nodes_by_xpath($dom, $xpath) {
    $dom_xpath = new DOMXPath(dom_owner($dom));
    $ns = $dom_xpath->query($xpath, $dom);
    $result = array();
    if(!is_null($ns)) {
	foreach($ns as $n) {
	    $result[] = $n;
	}
    }
    return $result;
}

function dom_get_node_by_xpath($dom, $xpath) {
    $result = dom_get_nodes_by_xpath($dom, $xpath);
    if(isset($result[0]))
	return $result[0];
    return null;
}

function dom_find_near_node($target_dom, $target_position, $target_length, $need_include, $tag_name, $class,
			       $attributes = array(), $predicate = false, $predicate_arg = false) {
    $xpath = './/' . $tag_name . '[@data-twp and @data-twl and not(ancestor-or-self::*[@data-pagename])]';
    $elements = dom_get_nodes_by_xpath($target_dom, $xpath);
    if(is_null($elements))
	return false;
    $min_distance = null;
    $min_element = null;
    foreach($elements as $element) {
	if($class !== '' && !dom_has_class($element, $class))
	    continue;
	$is_match = true;
	foreach($attributes as $attribute_name => $attribute_value) {
	    if(dom_get_attribute($element, $attribute_name) !== $attribute_value) {
		$is_match = false;
		break;
	    }
	}
	if(!$is_match)
	    continue;
	if($predicate !== false) {
	    if(!$predicate($element, $predicate_arg))
		continue;
	}
	
	$candidate_position = dom_get_attribute($element, 'data-twp');
	$candidate_length = dom_get_attribute($element, 'data-twl');

	if($need_include &&
	   !($candidate_position <= $target_position &&
	     $target_position + $target_length <= $candidate_position + $candidate_length))
	    continue;

	$distance = abs($candidate_position - $target_position) + abs(($candidate_position + $candidate_length) - ($target_position + $target_length));
	if(is_null($min_distance) || $distance < $min_distance) {
	    $min_distance = $distance;
	    $min_element = $element;
	}
    }
    if(is_null($min_element))
	return false;
    return $min_element;
}

function dom_remove_other_page($dom) {
    if(!($dom instanceof DOMElement))
	return;
    if(dom_get_attribute($dom, 'data-pagename') !== '') {
	$dom->parentNode->removeChild($dom);
	return;
    }
    for($i = 0; $i < $dom->childNodes->length; $i++) {
	dom_remove_other_page($dom->childNodes->item($i));
    }
}

function dom_get_form_input_values($form) {
    $inputs = dom_get_nodes_by_xpath($form, './/input');
    $values = array();
    if(is_null($inputs))
	return $values;
    foreach($inputs as $input) {
	$name = dom_get_attribute($input, 'name');
	$value = dom_get_attribute($input, 'value');
	if($name !== false)
	    $values[$name] = $value;
    }
    return $values;
}

?>
