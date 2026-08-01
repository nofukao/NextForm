<?php
$TOOLS = array();

function tools($category, &$page = null) {
    $dom = tools_convert($category, $page);
    if($dom === false)
	return '';
    return dom_save_html($dom);
}

function tools_convert($category, &$page = null) {
    global $TOOLS;
    if(!isset($TOOLS[$category]))
	return false;
    $tools =& $TOOLS[$category];

    uasort($tools, 'order_compare');

    $dom = dom_create_document();
    $ul = dom_append_child($dom, 'ul', $category . '_tools');

    foreach($tools as $name => &$tool) {
	if(function_exists($tool['show'])) {
	    //$_SERVER['tools_order'] = $tool['order'];
	    $tool['show']($page, $ul);
	}
    }

    return $dom;
}

function tools_add_link($dom, $tool_data) {
    $li = dom_append_child($dom, 'li');

    if(isset($tool_data['class']))
	dom_append_class($li, $tool_data['class']);
	
    if(empty($tool_data['disable'])) {
	$a = dom_append_child($li, 'a', '', array('href' => $tool_data['uri']));
	//dom_append_text($a, $_SERVER['tools_order'] .' '. $tool_data['name']);
	dom_append_text($a, $tool_data['name']);
	if(isset($tool_data['target']))
	    dom_set_attribute($a, 'target', $tool_data['target']);
	if(!empty($tool_data['accesskey'])) {
	    dom_set_attribute($a, 'accesskey', $tool_data['accesskey']);
	    dom_append_text($a, '(' . strtoupper($tool_data['accesskey']) . ')' );
	}
	if(empty($tool_data['seo_follow']))
	    dom_set_attribute($a, 'rel', 'nofollow');
	else if(isset($tool_data['rel']))
	    dom_set_attribute($a, 'rel', $tool_data['rel']);
    } else {
	dom_append_text($li, $tool_data['name']);
	dom_set_attribute($li, 'class', 'disabled');
	if(!empty($tool_data['accesskey'])) {
	    dom_append_text($li, '(' . strtoupper($tool_data['accesskey']) . ')' );
	}
    }
    return $li;
}

function tool_page_tags(&$page) {
    $dom = dom_create_document();
    if(!is_null($page)) {
	$tags = tag_get_page_tags($page);
	$dl = dom_append_child($dom, 'dl', 'page_tags');
	$dt = dom_append_child($dl, 'dt');
	$dd = dom_append_child($dl, 'dd');
	dom_append_text($dt, l('Tags'));
	dom_append_text($dd, '');

	$ul = tag_append_tag_ul($dd, $tags, '', array('option' => 'allpage'));
	dom_set_attribute($ul, 'data-tags', list_to_string($tags));
	$li = dom_append_child($ul, 'li', 'edit_tag');
	$a = dom_append_link($li, url_make($page['name'], array('option' => 'tag')), l('Edit'));
	dom_remove_empty($ul);
    }
    dom_remove_empty($dom);
    return dom_save_html($dom);
}

function tool_page_info(&$page) {
    $dom = dom_create_document();

    $dl = dom_append_child($dom, 'dl', 'page_info');

    $type = '';
    if(empty($page['is_exists']) && !empty($page['args']['newpage_type']))
	$type = $page['args']['newpage_type'];
    else if(!empty($page['is_exists']) && !empty($page['meta']['type']))
	$type = $page['meta']['type'];
    if($type !== '') {
	$dt = dom_append_child($dl, 'dt');
	$dd = dom_append_child($dl, 'dd', 'type');
	dom_append_text($dt, l('Page type'));
	dom_append_text($dd, $type);
    }

    if(!empty($page['mtime'])) {
	$dt = dom_append_child($dl, 'dt');
	$dd = dom_append_child($dl, 'dd', 'mtime');
	dom_append_text($dt, l('Last modified'));
	dom_append_time($dd, $page['mtime'], TIME_FORMAT);
    }

    if(isset($page['meta']['last_modify_user'])) {
	$dt = dom_append_child($dl, 'dt');
	$dd = dom_append_child($dl, 'dd', 'last_modify_user');
	dom_append_text($dt, l('Last updated by'));
	dom_append_text($dd, $page['meta']['last_modify_user']);
    }

    if(!empty($page['meta']['btime'])) {
	$dt = dom_append_child($dl, 'dt');
	$dd = dom_append_child($dl, 'dd', 'btime');
	dom_append_text($dt, l('Birth time'));
	dom_append_time($dd, $page['meta']['btime'], TIME_FORMAT);
    }

    if(!empty($page['meta']['create_user'])) {
	$dt = dom_append_child($dl, 'dt');
	$dd = dom_append_child($dl, 'dd', 'create_user');
	dom_append_text($dt, l('Creator'));
	dom_append_text($dd, $page['meta']['create_user']);
    }

    if(!empty($page['name']) && !empty($page['is_exists'])) {
	$dt = dom_append_child($dl, 'dt');
	$dd = dom_append_child($dl, 'dd');
	$plain_url = url_make($page['name']);
	$shorten_url = shorturl_get_url($page['name']);
	if(strlen($shorten_url) < strlen($plain_url)) {
	    $text = l('Page shorten URL');
	    $url = $shorten_url;
	} else {
	    $text = l('Page URL');
	    $url = $plain_url;
	}
	dom_append_text($dt, $text);
	dom_append_link($dd, $url, $url, 'page_url');
    }

    if(!empty($page['handle_time']) && DEBUG_MODE == 'true') {
	$dt = dom_append_child($dl, 'dt');
	$dd = dom_append_child($dl, 'dd', 'handle_time');
	dom_append_text($dt, l('Page creation time'));
	dom_append_text($dd, $page['handle_time']);
    }

    if(isset($page['name'])) {
	$pages = page_find($page['name'], array('type' => 'file', 'is_children_only' => true));
	if(!empty($pages)) {
	    $dl = dom_append_child($dom, 'dl', 'attach');
	    $dt = dom_append_child($dl, 'dt');
	    dom_append_text($dt, l('Attach'));
	    $dd = dom_append_child($dl, 'dd');
	    $ul = dom_append_child($dd, 'ul', 'attach');
	    foreach($pages as &$listpage) {
		$li = dom_append_child($ul, 'li');
		dom_append_page_link($li, './' . page_get_simplename($listpage['name']), $listpage['name']);
	    }
	}
    }
    dom_remove_empty($dl);
    
    return dom_save_html($dom);
}

?>