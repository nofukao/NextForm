<?php
require_once(APP_DIR_PATH . '/version.inc');
require_once(APP_DIR_PATH . '/setup.inc');
require_once(APP_DIR_PATH . '/util.inc');
require_once(APP_DIR_PATH . '/dom.inc');
require_once(APP_DIR_PATH . '/template.inc');
require_once(APP_DIR_PATH . '/language.inc');
require_once(APP_DIR_PATH . '/message.inc');
require_once(APP_DIR_PATH . '/page.inc');
require_once(APP_DIR_PATH . '/cache.inc');
require_once(APP_DIR_PATH . '/storage_file.inc');
require_once(APP_DIR_PATH . '/auth.inc');
require_once(APP_DIR_PATH . '/title.inc');
require_once(APP_DIR_PATH . '/tool.inc');
require_once(APP_DIR_PATH . '/diff.inc');
require_once(APP_DIR_PATH . '/option.inc');
require_once(APP_DIR_PATH . '/sentence.inc');
require_once(APP_DIR_PATH . '/user_history.inc');
require_once(APP_DIR_PATH . '/pagelist.inc');
require_once(APP_DIR_PATH . '/tag.inc');
require_once(APP_DIR_PATH . '/lock.inc');
require_once(APP_DIR_PATH . '/search.inc');
require_once(APP_DIR_PATH . '/export.inc');
require_once(APP_DIR_PATH . '/manual.inc');
require_once(APP_DIR_PATH . '/theme.inc');
require_once(APP_DIR_PATH . '/csrf.inc');
require_once(APP_DIR_PATH . '/analyze.inc');

setup_local();
auth_setup();
theme_setup();

require_once(APP_DIR_PATH . '/handle.inc');
foreach(glob_array(APP_DIR_PATH . '/plugin/*.inc') as $file) require_once($file);
foreach(glob_array(PLUGIN_DIR_PATH . '/*.inc') as $file) require_once($file);

date_default_timezone_set(DEFAULT_TIMEZONE);
if(function_exists('mb_internal_encoding'))
    mb_internal_encoding('UTF-8');

$main_page = null;
$main_pagename = '';
$main_raw_pagename = '';
$main_page_output = '';
$page_stop_processing = false;
$head_tags = false;
$head_tags_added = array();

function main() {
    global $main_page;
    global $main_pagename, $main_raw_pagename;
    global $main_page_output;
    global $title_dom;
    global $head_tags;

    debug_watch('start', 'system');
    debug_watch('start');

    $args = args_get();
    message_init();
    head_tags_init();

    if(USER_HISTORY_ENABLE == 'true')
	user_history_load();

    $main_pagename = '';

    if(strpos($_SERVER['REQUEST_URI'], '?&') !== false &&
       preg_match('|^&(' . PAGE_SHORTID_KEY_REGEX . ')$|', $_SERVER['QUERY_STRING'], $matches)) {
	$args['option'] = 'shorturl';
	$args['key'] = $matches[1];
    }
    
    if(isset($args['page']) && $args['page'] !== '') {
	$main_raw_pagename = $args['page'];
    } else {
	$fields = explode('&', $_SERVER['QUERY_STRING']);
	if(isset($fields[0]) && strpos($fields[0], '=') === false) {
	    $main_raw_pagename = urldecode($fields[0]);
	}
    }
    $main_pagename = page_path_to_pagename($main_raw_pagename);
    title_set_pagename('$pagename$', $main_pagename);

    register_shutdown_function('main_shutdown', getcwd());

    $is_functional_page = true;
    $main_page_output = option($args, '', $is_continue);
    if($is_continue) {
	dom_set_self_link_pagename($main_pagename);
	$main_page_output .= page($main_pagename, $main_page, $args, true);
	if(isset($main_page['title_dom'])) {
	    $title_dom =  $main_page['title_dom'];
	}
	if(!empty($main_page['wiki_context_result']['redirect']) && 
	   (!isset($args['noredirect']) || $args['noredirect'] !== 'true')) {
	    $redirect = $main_page['wiki_context_result']['redirect'];

	    $url = url_make($redirect['pagename'], array('redirectfrom' => $main_page['name']));
	    $script = head_tags_add_javascript();
	    $script_text = sprintf(
		"\n".'redirectStart("%s", %d);'."\n//",
		$url, $redirect['timeout'] * 1000);
	    dom_append_comment($script, $script_text);
	    
	    $noscript = dom_append_child($head_tags, 'noscript');
	    $meta = dom_append_child($noscript, 'meta', '', 
				     array('id' => 'redirect',
					   'http-equiv' => 'refresh',
					   'content' => $redirect['timeout'] . '; url=' . $url));
	}
	if(isset($args['redirectfrom'])) {
	    $ldom = dom_create_document();
	    $a = dom_append_link($ldom, url_make($args['redirectfrom'], array('noredirect' => 'true')), $args['redirectfrom']);
	    ld(message_info(), 'Redirected from $pagename$', array('pagename' => $a));
	}

	if(isset($main_page['meta']['description']) && $main_page['meta']['description'] !== '')
	    head_tags_add_meta(array('name' => 'description', 'content' => $main_page['meta']['description']));
	else if(SITE_DESCRIPTION !== '')
	    head_tags_add_meta(array('name' => 'description', 'content' => SITE_DESCRIPTION));
	$page_tags = tag_get_page_tags($main_page);
	if(!empty($page_tags))
	    head_tags_add_meta(array('name' => 'keywords', 'content' => implode(',', $page_tags)));

	if($main_page['done_action'] === 'show')
	    $is_functional_page = false;
    }
    if($is_functional_page) {
	head_tags_add_meta(array('name' => 'robots', 'content' => 'noindex,nofollow'));
    }

    title_set_disable();

    debug_watch('end');
    debug_watch('main end', 'system');
    if(!empty($args['debug']) && DEBUG_MODE == 'true') {
	debug_watch_show();
	debug_watch_show('system');
    }
}

function main_shutdown($cwd) {
    chdir($cwd);
    if(USER_HISTORY_ENABLE == 'true')
	user_history_save();
}

function &main_page() {
    global $main_page;
    global $main_pagename;
    if(empty($main_page))
	$main_page = page_read($main_pagename);
    return $main_page;
}

function url_pagename(&$page) {
    global $main_pagename;
    if(isset($page['is_main']) && $page['is_main'] === false) // side page
	return $main_pagename;
    else
	return $page['name'];
}

function &target_page(&$args) {
    if(!isset($args['pagename'])) {
	return main_page();
    }
    $pagename = page_path_to_pagename($args['pagename']);
    $page = page_read($pagename);
    $page['args'] = $args;
    return $page;
}

function head_tags_init() {
    global $head_tags;
    global $head_tags_added;
    $head_tags = dom_create_document();
    $head_tags_added = array();

    dom_append_child($head_tags, 'meta', '', array('charset' => 'UTF-8'));

    dom_append_child($head_tags, 'link', '', array(
			 'rel' => 'alternate',
			 'type' => 'application/atom+xml',
			 'title'=> 'site feed',
			 'href' => '?option=feed'
			 ));
    dom_append_child($head_tags, 'meta', '', array(
			 'name' => 'application-name',
			 'content' => 'NextForm'
			 ));
    head_tags_add_javascript('resource/script/prototype.js');
    head_tags_add_javascript('resource/script/toratora.js');
}

function head_tags_add_javascript($src = '') {
    global $head_tags;
    global $head_tags_added;
    
    $attributes = array('type' => 'text/javascript');
    if($src !== '') {
	if(!empty($head_tags_added[$src]))
	    return;
	$head_tags_added[$src] = true;
	$attributes['src'] = $src;
	$attributes['charset'] = 'UTF-8';
    }
    $script = dom_append_child($head_tags, 'script', '', $attributes);
    dom_append_text($script, '');
    return $script;
}

function head_tags_add_css($src, $add_attributes = array()) {
    global $head_tags;
    global $head_tags_added;
    
    $attributes = array('rel' => 'stylesheet', 'type' => 'text/css');
    if($src !== '') {
	if(!empty($head_tags_added[$src]))
	    return;
	$head_tags_added[$src] = true;
	$attributes['href'] = $src;
    }
    $attributes += $add_attributes;
    $link = dom_append_child($head_tags, 'link', '', $attributes);
    return $link;
}

function head_tags_add_meta($attributes = array()) {
    global $head_tags;
    dom_append_child($head_tags, 'meta', '', $attributes);
}


function head_tags() {
    global $head_tags;
    return dom_save_html($head_tags);
}

$TOOLS['site']['sidepage'] = array(
    'order' => 10,
    'show' => 'sideapge_tool'
    );

function sideapge_tool(&$page, $dom) {
    if(SIDE_PAGENAME === '')
	return;
    tools_add_link(
	$dom,
	array('name' => SIDE_PAGENAME,
	      'class' => 'sidepage',
	      'uri' => url_make(SIDE_PAGENAME)));
}
?>
