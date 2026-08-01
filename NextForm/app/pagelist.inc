<?php
function pagelist_append($dom, &$list_pages, $list_options = array(), $args = array()) {
    if(empty($list_pages))
	return;

    if(isset($list_options['template'])) {
	$placeholders = template_get_placeholders($list_options['template']);
    } else
	$placeholders = array();

    pagelist_apply_rules($list_pages, $list_options, $args, $placeholders);

    if(!isset($list_options['context']))
	$list_options['context'] = array();
    $context = $list_options['context'];
    $offset = default_value($args['pages'], 0);
    $each = default_value($list_options['each'], 0);
    $pagenum = count($list_pages);
    if($each > 0)
	$pages = array_slice($list_pages, $offset * $each, $each);
    else
	$pages = $list_pages;

    $section = dom_append_child($dom, 'section', 'pages');

    if(!empty($list_options['tags'])) {
	foreach($list_options['tags'] as $ts) {
	    foreach($ts as $t)
		$tags[] = $t;
	}
	$p = dom_append_child($section, 'p');
	dom_append_class($p, 'tag_notice');
	$a = dom_append_link($p, url_make(url_pagename($list_options['context']['page'])), '', 'button');
	dom_append_ref($a, 'times');
	dom_append_text($p,
			lf('List of $tag$ tagged pages', array('tag' => implode(' ', $tags))));
	dom_set_attribute($a, 'data-link-pagename', $list_options['context']['page']['name']);
    }

    $show = default_value($list_options['show'], 'list');
    if($show == 'table') {
	$fields = pagelist_parse_fields(
	    default_value($list_options['fields'], '[[title-pagename]] _'), $placeholders);
	pagelist_append_table($section, $pages, $list_options, $args, $fields);
    } else if($show == 'contents') {
	$fields = pagelist_parse_fields(default_value($list_options['fields'], '[[title]]'), $placeholders);
	pagelist_append_contents_list($section, $pages, $list_options, $args, $fields, $placeholders);
    } else if($show == 'date') {
	$fields = pagelist_parse_fields(
	    default_value($list_options['fields'], '[[title-pagename]]'), $placeholders);
	$current_date = '';
	$current_ul = null;
	$date_ul = dom_append_child($section, 'ul', 'pages_date');
	foreach($pages as &$listpage) {
	    $date = time_format($listpage['mtime'], 'date');
	    if($current_date != $date) {
		$li = dom_append_child($date_ul, 'li');
		dom_append_time($li, $listpage['mtime'], DATE_FORMAT);
		
		if(!is_null($current_ul))
		    dom_remove_empty($current_ul);
		$current_ul = dom_append_child($li, 'ul', 'pages');
		$current_date = $date;
	    }
	    $li = dom_append_child($current_ul, 'li', 'pagename');
	    pagelist_append_fields($li, $listpage, $fields, $list_options);
	    pagelist_set_page_attributes($li, $listpage);
	    dom_remove_empty($li);
	} 
	if(!is_null($current_ul))
	    dom_remove_empty($current_ul);
	dom_remove_empty($date_ul);
    } else if($show == 'tree') {
	$fields = pagelist_parse_fields(default_value($list_options['fields'], '[[basename]]'), $placeholders);
	$tree = array();
	$base_pagename = default_value($list_options['base_pagename']);
	if(isset($list_options['base_pagename']) && $list_options['base_pagename'] !== '') {
	    $base_pagename = $list_options['base_pagename'] . '/';
	} else {
	    $base_pagename = '';
	}
	$base_length = strlen($base_pagename);
	foreach($pages as &$listpage) {
	    $names = explode('/', substr($listpage['name'], $base_length));
	    $basename = array_pop($names);
	    $node =& $tree;
	    $pathname = $base_pagename;
	    foreach($names as $name) {
		$pathname .= $name . '/';
		if(!isset($node[$name]))
		    $node[$name] = array('' => rtrim($pathname, '/'));
		$node =& $node[$name];
	    }
	    $node[$basename][''] = &$listpage;
	}
	pagelist_append_tree_item($section, $tree, $fields, $list_options);
    } else {
	$fields = pagelist_parse_fields(default_value($list_options['fields'], '[[title-pagename]]'), $placeholders);
	$ul = dom_append_child($section, 'ul', 'pages');
	foreach($pages as &$listpage) {
	    $li = dom_append_child($ul, 'li');
	    pagelist_set_page_attributes($li, $listpage);
	    pagelist_append_fields($li, $listpage, $fields, $list_options);
	    dom_remove_empty($li);
	} 
	dom_remove_empty($ul);
    }

    if(isset($context['page'])) {
	$pagename = $context['page']['name'];
	$url_pagename = url_pagename($context['page']);
    } else {
	$pagename = '';
	$url_pagename = '';
    }
    if(!empty($list_options['showhidden'])) {
	if($list_options['hidden_num'] > 0) {
	    dom_append_link($section, url_make($url_pagename, array('pages_show_hidden' => 'true')),
			    lf('Show hidden $num$ pages', array('num' => $list_options['hidden_num'])));
	    //} else if(!empty($args['pages_show_hidden'])) {
	    //dom_append_link($section, url_make($pagename), l('Hide pages'));
	}
    }

    if($each > 0) {
	$nav = dom_append_child($section, 'nav');
	dom_append_class($nav, 'pages');
	$start = 0;
	$prev = max($offset - 1, 0);
	$next = $offset + 1;
	$end = max(floor(($pagenum - 1) / $each), 0);

	unset($args[$url_pagename]);
	if($start < $offset) {
	    unset($args['pages']);
	    $a = dom_append_link(
		$nav, url_make($url_pagename, $args), '|' . l('First'), 'first');
	    dom_set_attribute($a, 'data-link-pagename', $pagename);
	    dom_set_attribute($a, 'data-link-args', url_make_query($args));
	}
	if($prev < $offset) {
	    if($prev == 0)
		unset($args['pages']);
	    else
		$args['pages'] = $prev;
	    $a = dom_append_link(
		$nav, url_make($url_pagename, $args), '<' . l('Prev'), 'prev');
	    dom_set_attribute($a, 'data-link-pagename', $pagename);
	    dom_set_attribute($a, 'data-link-args', url_make_query($args));
	}
	if($next > $offset && $next <= $end) {
	    $args['pages'] = $next;
	    $a = dom_append_link(
		$nav, url_make($url_pagename, $args), l('Next'). '>', 'next');
	    dom_set_attribute($a, 'data-link-pagename', $pagename);
	    dom_set_attribute($a, 'data-link-args', url_make_query($args));
	}
	if($end > $offset) {
	    $args['pages'] = $end;
	    $a = dom_append_link(
		$nav, url_make($url_pagename, $args), l('Last') . '|', 'last');
	    dom_set_attribute($a, 'data-link-pagename', $pagename);
	    dom_set_attribute($a, 'data-link-args', url_make_query($args));
	}
	dom_remove_empty($nav);
    }

    return $section;
}

function pagelist_append_tree_item($li, &$node, $fields, $list_options, $pagename_prefix = '') {
    if($pagename_prefix !== '') {
	if(is_string($node[''])) {
	    dom_append_page_link($li, page_get_simplename($node['']), $node['']);
	} else {
	    pagelist_append_fields($li, $node[''], $fields, $list_options);
	    pagelist_set_page_attributes($li, $node['']);
	}
	$pagename_prefix .= '/';
    }
    unset($node['']);
    if(empty($node))
	return;
    $ul = dom_append_child($li, 'ul', 'pages');
    foreach($node as $name => $child) {
	$li = dom_append_child($ul, 'li');
	pagelist_append_tree_item($li, $child, $fields, $list_options, $pagename_prefix . $name);
	dom_remove_empty($li);
    }
}

function pagelist_apply_rules(&$pages, &$list_options, $args, &$placeholders) {
    global $pagelist_sort_rules;

    $display_rules = default_value($list_options['display_rules'], array());
    $sort_rules = default_value($list_options['sort_rules'], array());
    $hidden_num = 0;

    if(isset($list_options['template'])) {
	$pattern = template_get_regex_pattern($list_options['template'], '/');
    } else
	$pattern = false;

    $applied_pages = array();
    foreach($pages as &$page) {
	$child = array('pagename' => $page['name'], 'terms' => array());

	if(!isset($page['meta']) && empty($page['deleted'])) {
	    if(!auth_has_permission($page['name'], 'read'))
		continue;
	    $page = page_read($page['name']);
	}

	$values = array();
	if($pattern !== false && page_type_is_text_base(page_get_type($page))) {
	    $contents = page_get_contents($page);
	    if(preg_match('/' . $pattern . '/', $contents, $matches, PREG_OFFSET_CAPTURE)) {
		array_shift($matches);
		$values = $matches;
	    }
	}

	$page['terms']['tags']['value'] = list_to_string(tag_get_page_tags($page));
	$page['terms']['basename']['value'] = page_get_simplename($page['name']);
	$page['terms']['pagename']['value'] = $page['name'];
	if(isset($list_options['base_pagename'])) {
	    $page['terms']['relname']['value'] = page_relative_path($list_options['base_pagename'], $page['name']);
	} else {
	    $page['terms']['relname']['value'] = $page['name'];
	}
	if(isset($page['meta']['title']) && $page['meta']['title'] !== '') {
	    $page['terms']['title']['value'] = $page['meta']['title'];
	    $page['terms']['title-pagename']['value'] = $page['meta']['title'];
	    $page['terms']['title-basename']['value'] = $page['meta']['title'];
	    $page['terms']['title-relname']['value'] = $page['meta']['title'];
	} else {
	    $page['terms']['title']['value'] = $page['terms']['basename']['value'];
	    $page['terms']['title-pagename']['value'] = $page['terms']['pagename']['value'];
	    $page['terms']['title-basename']['value'] = $page['terms']['basename']['value'];
	    $page['terms']['title-relname']['value'] = $page['terms']['relname']['value'];
	}
	$page['terms']['mtime']['value'] = $page['mtime'];
	$page['terms']['mdate']['value'] = $page['mtime'];
	if(isset($page['meta']['btime'])) {
	    $page['terms']['btime']['value'] = $page['meta']['btime'];
	    $page['terms']['bdate']['value'] = $page['meta']['btime'];
	}
	if(isset($page['meta']['last_modify_user']))
	    $page['terms']['editor']['value'] = $page['meta']['last_modify_user'];
	if(isset($page['meta']['create_user']))
	    $page['terms']['creator']['value'] = $page['meta']['create_user'];
	if(isset($page['contents_size']))
	    $page['terms']['size']['value'] = $page['contents_size'];
	if(isset($page['meta']['type']))
	    $page['terms']['type']['value'] = $page['meta']['type'];
	if(isset($page['meta']['Content-type']))
	    $page['terms']['content_type']['value'] = $page['meta']['Content-type'];
	if(isset($page['pageview']))
	    $page['terms']['pageview']['value'] = $page['pageview'];

	$child_context = wiki_default_context($page);
	foreach($placeholders as &$placeholder) {
	    if($placeholder['type'] !== 'ignore') {
		$t = $placeholder['index'];
		$child_dom = dom_create_document();
		$td = dom_append_child($child_dom, 'td');
		if(isset($values[$t])) {
		    $child_context['position_offset'] = $values[$t][1];
		    if(isset($list_options['context']['page']['sentence']))
			$page['sentence'] = &$list_options['context']['page']['sentence'];
		    if(isset($placeholder['wiki_line_head']))  {
			wiki_convert_from_source($child_context, $td, $values[$t][0],
						 array('display' => 'block', 'append_line_head' => true));
		    } else {
			wiki_convert_from_source($child_context, $td, $values[$t][0]);
		    }
		    dom_set_attribute($td, 'data-twp', $values[$t][1]);
		    dom_set_attribute($td, 'data-twl', strlen($values[$t][0]));
		}
		dom_append_text($td, '');
		$page['terms'][$placeholder['term']] = array('dom' => $td, 'value' => dom_text($child_dom));
	    }
	}

	$is_append = true;
	foreach($display_rules as &$display_rule) {
	    if(isset($page['terms'][$display_rule['term']])) {
		$value = trim($page['terms'][$display_rule['term']]['value']);
		if(!empty($display_rule['numeric'])) {
		    $is_match = 
			(!empty($display_rule['equal']) && $value == $display_rule['value']) ||
			($display_rule['direction'] == '<' && $value < $display_rule['value']) ||
			($display_rule['direction'] == '>' && $value > $display_rule['value']);
		} else {
		    $cmp = strcmp($value, $display_rule['value']);
		    $is_match = 
			(!empty($display_rule['equal']) && $cmp == 0) ||
			($display_rule['direction'] == '<' && $cmp < 0) ||
			($display_rule['direction'] == '>' && $cmp > 0);
		}
		
		if($is_match) {
		    $page['display'] = $display_rule['display'];
		    if($display_rule['display'] === 'none') {
			if(empty($args['pages_show_hidden'])) {
			    $is_append = false;
			    break;
			}
		    } else
			break;
		}
	    }
	}
	if($is_append) {
	    $applied_pages[] = &$page;
	} else {
	    $hidden_num++;
	}
    }
    $pages = $applied_pages;

    if(!empty($list_options['sort_rules'])) {
	$pagelist_sort_rules = $list_options['sort_rules'];
	usort($pages, 'pagelist_page_compare');
    }

    if(!empty($list_options['limit'])) {
	$pages = array_slice($pages, 0, $list_options['limit']);
    }

    $list_options['hidden_num'] = $hidden_num;
}

function pagelist_page_compare(&$a_page, &$b_page) {
    global $pagelist_sort_rules;
    foreach($pagelist_sort_rules as &$sort) {
	$order = $sort['order'] == 'asc' ? 1 : -1;
	$a_set = isset($a_page['terms'][$sort['term']]);
	$b_set = isset($b_page['terms'][$sort['term']]);
	if(!$a_set && !$b_set) continue;
	if(!$a_set) return 1;
	if(!$b_set) return -1;
	$a_value = trim($a_page['terms'][$sort['term']]['value']);
	$b_value = trim($b_page['terms'][$sort['term']]['value']);
	if($a_value === '' && $b_value === '') continue;
	if($a_value === '') return 1;
	if($b_value === '') return -1;

	if(!empty($sort['map'])) {
	    if(isset($sort['map'][$a_value]))
		$a_value = $sort['map'][$a_value];
	    else
		$a_value = 0x7FFFFFFF;
	    if(isset($sort['map'][$b_value]))
		$b_value = $sort['map'][$b_value];
	    else
		$b_value = 0x7FFFFFFF;
	}
	if($a_value < $b_value)
	    return -$order;
	if($a_value > $b_value)
	    return $order;
    }
    return strcmp($a_page['name'], $b_page['name']);
}

function pagelist_append_table($dom, &$pages, &$list_options, $args, &$fields, &$placeholders = array()) {
    $context = $list_options['context'];
    $table = dom_append_child($dom, 'table', 'pages');
    $thead = dom_append_child($table, 'thead');
    $tbody = dom_append_child($table, 'tbody');

    $tr = dom_append_child($thead, 'tr');
    foreach($fields as $field) {
	$th = dom_append_child($tr, 'th');
	if(isset($field['display'])) {
	    dom_append_text($th, $field['display']);
	} else if(isset($field['placeholder'])) {
	    dom_append_text($th, $field['placeholder']['term']);
	} else if(isset($field['term'])) {
	    switch($field['term']) {
	    case 'title':
	    case 'title-pagename':
	    case 'title-relname':
		dom_append_text($th, l('Title'));
		break;
	    case 'basename':
	    case 'pagename':
	    case 'relname':
		dom_append_text($th, l('Page name'));
		break;
	    case 'mtime':
	    case 'mdate':
		dom_append_text($th, l('Last updated'));
		break;
	    case 'btime':
	    case 'bdate':
		dom_append_text($th, l('Birth time'));
		break;
	    case 'tags':
		dom_append_text($th, l('Tags'));
		break;
	    case 'editor':
		dom_append_text($th, l('Last updated by'));
		break;
	    case 'creator':
		dom_append_text($th, l('Creator'));
		break;
	    case 'type':
		dom_append_text($th, l('Type'));
		break;
	    case 'size':
		dom_append_text($th, l('Size'));
		break;
	    case 'content_type':
		dom_append_text($th, l('Content-type'));
		break;
	    case 'pageview':
		dom_append_text($th, l('Number of hits'));
		break;
	    case 'edit':
		dom_append_text($th, l('Edit'));
		break;
	    case 'delete':
		dom_append_text($th, l('Delete'));
		break;
	    default:
		dom_append_text($th, $field['term']);
		break;
	    }
	}
    }

    foreach($pages as &$page) {
	$tr = dom_append_child($tbody, 'tr');
	pagelist_set_page_attributes($tr, $page);
	if(!empty($page['display']) && color_is_valid($page['display'])) {
	    dom_set_attribute($tr, 'style', 'background-color: ' . $page['display']);
	}

	foreach($fields as $field) {
	    $td = dom_append_child($tr, 'td');
	    pagelist_append_field($td, $page, $field, $list_options);
	}
    }

    return $table;
}

function pagelist_set_page_attributes($dom, $page) {
    dom_set_attribute($dom, 'data-ref-pagename', $page['name']);
    dom_set_attribute($dom, 'data-ref-ticket', default_value($page['meta']['ticket'], ''));
    dom_set_attribute($dom, 'data-pagename', $page['name']);
    dom_set_attribute($dom, 'data-ticket', default_value($page['meta']['ticket'], ''));
}

function pagelist_parse_fields($show_fields, &$placeholders) {
    $tmp_fields = array();
    $used_placeholders = array();

    if(preg_match_all("/('.*?'|[^\s]+)/", $show_fields, $matches)) {
	foreach($matches[1] as $show_field) {
	    $show_field = trim($show_field, "'");
	    if($show_field == '')
		continue;
	    $field = array();
	    if(preg_match('/^\((.*)\)$/', $show_field, $matches)) {
		$field['brace'] = true;
		$show_field = $matches[1];
	    }
	    if(preg_match('/^\[\[(.*)\]\]$/', $show_field, $matches)) {
		$field['link'] = true;
		$show_field = $matches[1];
	    }
	    if(preg_match('/^(.*)@(.*)$/', $show_field, $matches)) {
		$field['display'] = $matches[2];
		$show_field = $matches[1];
	    }
	    foreach($placeholders as &$placeholder) {
		if($placeholder['type'] !== 'ignore' && $show_field == $placeholder['term']) {
		    $field['placeholder'] = &$placeholder;
		    $used_placeholders[$show_field] = true;
		    break;
		}
	    }
	    if(!isset($field['placeholder']))
		$field['term'] = $show_field;
	    $tmp_fields[] = $field;
	}
    }

    $fields = array();
    foreach($tmp_fields as $field) {
	if(isset($field['term']) && $field['term'] == '_') {
	    foreach($placeholders as &$placeholder) {
		if($placeholder['type'] !== 'ignore' && empty($used_placeholders[$placeholder['term']])) {
		    $fields[] = array('placeholder' => &$placeholder);
		}
	    }
	} else {
	    $fields[] = $field;
	}
    }
    return $fields;
}

function pagelist_append_fields($dom, &$page, $fields, &$list_options) {
    $is_first = true;
    foreach($fields as $field) {
	if($is_first)
	    $is_first = false;
	else
	    dom_append_text($dom, ' ');
	if(isset($field['display']) &&
	   isset($page['terms'][$field['term']]) &&
	   ($page['terms'][$field['term']]['value'] !== '' ||
	    isset($page['terms'][$field['term']]['dom'])))
	    dom_append_text($dom, $field['display'] . ': ');
	    
	pagelist_append_field($dom, $page, $field, $list_options);
    }
}

function pagelist_append_field($dom, &$page, $field, &$list_options) {
    global $main_pagename;
    $context = $list_options['context'];
    $tdom = $dom;
    if(!empty($field['link'])) {
	$tdom = dom_append_empty_page_link($dom, $page['name']);
	dom_append_text($tdom, '');
    }
    if(!empty($field['brace']) && $dom->tagName != 'td') {
	$tdom = dom_append_child($tdom, 'small');
	dom_append_text($tdom, '(');
    }
    if(isset($field['placeholder'])) {
	$term_dom = $page['terms'][$field['placeholder']['term']]['dom'];
	if($tdom->tagName !== 'td')
	    $tdom = dom_append_child($tdom, 'span');
	dom_append_dom_children($tdom, $term_dom);
	$twp = $term_dom->getAttribute('data-twp');
	$twl = $term_dom->getAttribute('data-twl');
	dom_set_attribute($tdom, 'data-twp', $twp);
	dom_set_attribute($tdom, 'data-twl', $twl);
	if($field['placeholder']['type'] === 'select') {
	    dom_set_attribute($tdom, 'data-option-value', $page['terms'][$field['placeholder']['term']]['value']);
	    dom_set_attribute($tdom, 'data-options', $field['placeholder']['options_text']);
	}
    } else if(isset($field['term'])) {
	switch($field['term']) {
	case 'pageview':
	case 'size':
	    if($dom->tagName == 'td')
		dom_append_class($dom, 'numeric');
	case 'content_type':
	case 'type':
	case 'pagename':
	case 'basename':
	case 'relname':
	case 'editor':
	case 'creator':
	    if(isset($page['terms'][$field['term']]['value']))
		dom_append_text($tdom, $page['terms'][$field['term']]['value']);
	    else
		dom_append_text($tdom, '');
	    break;
	case 'title':
	case 'title-pagename':
	case 'title-relname':
	    if(isset($page['terms']['title']['dom']))
		dom_append_dom_children($tdom, $page['terms']['title']['dom']);
	    else
		dom_append_text($tdom, $page['terms'][$field['term']]['value']);
	    if($page['terms'][$field['term']]['value'] !== $page['name'])
		dom_set_attribute($tdom, 'title', $page['name']);
	    break;
	case 'mtime':
	    dom_append_text($tdom, strftime(TIME_FORMAT, $page['terms']['mtime']['value']));
	    break;
	case 'mdate':
	    dom_append_text($tdom, strftime(DATE_FORMAT, $page['terms']['mtime']['value']));
	    break;
	case 'btime':
	    if(isset($page['terms']['btime']['value']))
		dom_append_text($tdom, strftime(TIME_FORMAT, $page['terms']['btime']['value']));
	    else
		dom_append_text($tdom, '');
	    break;
	case 'bdate':
	    if(isset($page['terms']['btime']['value']))
		dom_append_text($tdom, strftime(DATE_FORMAT, $page['terms']['btime']['value']));
	    else
		dom_append_text($tdom, '');
	    break;
	case 'tags':
	    $ul = tag_append_tag_ul($tdom, tag_get_page_tags($page), url_pagename($context['page']));
	    dom_remove_empty($ul);
	    break;
	case 'edit':
	    if(isset($list_options['template']) && $list_options['template'] !== '') {
		$hiddens = array(
		    'page' => $page['name'],
		    'pagename' => $page['name'],
		    'option' => 'templateedit',
		    'template' => $list_options['template'],
		    'redirect' => url_make($main_pagename)
		    );
		$form = dom_append_form($tdom, $hiddens);
		dom_append_child($form, 'input', '',
				 array('type' => 'submit', 'value' => l('Edit')));
	    } else {
		dom_append_link(
		    $tdom, url_make($page['name'], array('action' => 'edit')),
		    l('Edit'));
	    }
	    break;
	case 'delete':
	    $hiddens = array(
		'page' => $page['name'],
		'pagename' => $page['name'],
		'option' => 'delete',
		'action' => 'write',
		'back_page' => $main_pagename
		);
	    $form = dom_append_form($tdom, $hiddens);
	    dom_append_child($form, 'input', '',
			     array('type' => 'submit', 'value' => l('Delete')));
	    dom_set_attribute($form, 'onsubmit', "return confirm('".l('Are you OK?')."');");
	    break;
	default:
	    sentence_append($tdom, $field['term'], $list_options['context']['page']);
	    break;
	}
    }

    if(!empty($field['brace']) && $dom->tagName != 'td')
	dom_append_text($tdom, ')');

    return $tdom;
}

$PAGELIST_CONTENTS_DOM_CACHES = array();

function pagelist_append_contents_list($dom, &$pages, &$list_options, $args, $fields, $placeholders)
{
    global $PAGELIST_CONTENTS_DOM_CACHES;
    
    $is_export = isset($list_options['context']['page']['is_export']);

    $header_infos = string_to_list(default_value($list_options['header'], ''));
    $footer_infos = string_to_list(default_value($list_options['footer'], ''));

    $header_fields = pagelist_parse_fields(default_value($list_options['header'], ''), $placeholders);
    $footer_fields = pagelist_parse_fields(default_value($list_options['footer'], ''), $placeholders);

    foreach($pages as &$list_page) {
	$page = &page_read($list_page['name']);
	
	$dom_cache_key = md5(serialize(array($header_infos, $footer_infos, $page['name'])));

	if(isset($PAGELIST_CONTENTS_DOM_CACHES[$dom_cache_key])) {
	    $page_dom = $PAGELIST_CONTENTS_DOM_CACHES[$dom_cache_key];
	} else {
	    $page_dom = dom_create_document();
	    $article = dom_append_child($page_dom, 'article', 'page');

	    pagelist_set_page_attributes($article, $page);
	    dom_set_attribute($article, 'data-twp', 0);
	    dom_set_attribute($article, 'data-twl', -1);

	    $h = dom_append_child($article, 'h1', 'pages');

	    if(!empty($header_fields))
		$header = dom_append_child($article, 'header');

	    $page['abstract'] = true;
	    $page['in_pages_list_contents'] = true;
	    page_handle($page, $article);
	    if(isset($page['title_dom'])) {
		title_remove_actual($page['title_dom']);
		$list_page['terms']['title']['dom'] = $page['title_dom'];
	    }
	    pagelist_append_fields($h, $list_page, $fields, $list_options);
	    dom_remove_empty($h);

	    if(!empty($header_fields)) {
		pagelist_append_fields($header, $list_page, $header_fields, $list_options);
		dom_remove_empty($header);
	    }

	    if(!empty($footer_infos)) {
		$footer = dom_append_child($article, 'footer');
		pagelist_append_fields($footer, $list_page, $footer_fields, $list_options);
		dom_remove_empty($footer);
	    }

	    if($is_export)
		$PAGELIST_CONTENTS_DOM_CACHES[$dom_cache_key] = $page_dom;
	}
	dom_append_dom_children($dom, $page_dom);
    }
}
?>
