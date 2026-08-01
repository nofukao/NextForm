<?php
$USER_HISTORY_PAGE_VIEW = array('basetime' => 0, 'pages' => array());
$USER_HISTORY_PAGE_VIEW_PRELOAD = array();
$USER_HISTORY_IS_LOADED = false;

function user_history_load() {
    global $USER_HISTORY_PAGE_VIEW;
    global $USER_HISTORY_IS_LOADED;
    global $USER_HISTORY_PAGE_VIEW_PRELOAD;
    $USER_HISTORY_PAGE_VIEW = user_history_load_data($USER_HISTORY_IS_LOADED);
    $USER_HISTORY_PAGE_VIEW_PRELOAD = $USER_HISTORY_PAGE_VIEW;
}

function user_history_save() {
    global $USER_HISTORY_PAGE_VIEW;
    user_history_save_data($USER_HISTORY_PAGE_VIEW);
}

function user_history_clear() {
    global $USER_HISTORY_PAGE_VIEW;
    user_history_clear_data();
    $USER_HISTORY_PAGE_VIEW = array('basetime' => 0, 'pages' => array());
}

function user_history_add_page_view($pagename) {
    global $USER_HISTORY_PAGE_VIEW;
    $USER_HISTORY_PAGE_VIEW['pages'][$pagename] = time();
}

function user_history_get_page_view($pagename) {
    global $USER_HISTORY_PAGE_VIEW;
    if(!empty($USER_HISTORY_PAGE_VIEW['pages'][$pagename]))
	return $USER_HISTORY_PAGE_VIEW['pages'][$pagename];
    if(!empty($USER_HISTORY_PAGE_VIEW['basetime']))
	return $USER_HISTORY_PAGE_VIEW['basetime'];
    return false;
}

function user_history_get_page_view_preload($pagename) {
    global $USER_HISTORY_PAGE_VIEW_PRELOAD;
    if(!empty($USER_HISTORY_PAGE_VIEW_PRELOAD['pages'][$pagename]))
	return $USER_HISTORY_PAGE_VIEW_PRELOAD['pages'][$pagename];
    if(!empty($USER_HISTORY_PAGE_VIEW_PRELOAD['basetime']))
	return $USER_HISTORY_PAGE_VIEW_PRELOAD['basetime'];
    return false;
}

function user_history_get_basetime() {
    global $USER_HISTORY_PAGE_VIEW;
    if(!empty($USER_HISTORY_PAGE_VIEW['basetime']))
	return $USER_HISTORY_PAGE_VIEW['basetime'];
    return 0;
}

function user_history_set_basetime($time) {
    global $USER_HISTORY_PAGE_VIEW;
    $USER_HISTORY_PAGE_VIEW['basetime'] = $time;
    $delete_pagenames = array();
    foreach($USER_HISTORY_PAGE_VIEW['pages'] as $pagename => $ptime) {
	if($ptime < $time)
	    $delete_pagenames[] = $pagename;
    }
    foreach($delete_pagenames as $delete_pagename) {
	unset($USER_HISTORY_PAGE_VIEW['pages'][$delete_pagename]);
    }
}

function user_history_load_data(&$is_success) {
    if(($user = auth_get_user()) !== false) {
	$is_success = true;
	$cachekey = 'user-history-' . $user['name'];
	$data = cache_serialized_get('', $cachekey);
	if($data !== false)
	    return $data;
    }
    return array();
}

function user_history_save_data(&$history) {
    if(($user = auth_get_user()) !== false) {
	$cachekey = 'user-history-' . $user['name'];

	$is_success = false;
	$new_history = user_history_load_data($is_success);
	foreach($history as $key => $value) {
	    $new_history[$key] = $value;
	}
	$data = serialize($new_history);
	cache_create('', $cachekey, $data);
    }
}

function user_history_clear_data() {
    if(($user = auth_get_user()) !== false) {
	$cachekey = 'user-history-' . $user['name'];
	cache_delete('', $cachekey);
    }
}


?>
