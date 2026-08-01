<?php
define('ANALYZE_PAGEVIEW_KEY_PREFIX', 'apv-');
define('ANALYZE_PAGEVIEW_KEY_TOTAL', ANALYZE_PAGEVIEW_KEY_PREFIX . 'total');
define('ANALYZE_PAGEVIEW_KEY_DATE_FORMAT', ANALYZE_PAGEVIEW_KEY_PREFIX . '%Y%m%d');
define('ANALYZE_PAGEVIEW_DATE_TOTAL', 0);

function analyze_pageview_get($pagename, $start, $end = 0) {
    $keys = analyze_pageview_get_keys($start, $end);
    $count = 0;
    foreach($keys as $key) {
	$count += (int)analyze_pageview_get_value($pagename, $key);
    }
    return $count;
}

function analyze_pageview_get_all($start, $end = 0) {
    $keys = analyze_pageview_get_keys($start, $end);

    $counts = array('pages' => array(), 'allpages' => 0);
    foreach($keys as $key) {
	$key_length = strlen($key) + 1;
	$cachekeys = cache_get_keys_prefix('', $key . '-');
	foreach($cachekeys as $cachekey) {
	    if(($data = cache_get('', $cachekey)) !== false) {
		$pagename = substr($cachekey, $key_length);
		$counts['pages'][$pagename] = $data;
	    }
	}
	if(($data = cache_get('', $key)) !== false)
	    $counts['allpages'] = $data;
    }
    return $counts;
}

function analyze_pageview_get_keys($start, $end = 0) {
    $keys = array();
    if($start == ANALYZE_PAGEVIEW_DATE_TOTAL) {
	$keys[] = ANALYZE_PAGEVIEW_KEY_TOTAL;
    } else {
	for($t = $start; $t < $end; $t += 60 * 60 * 24) {
	    $keys[] = strftime(ANALYZE_PAGEVIEW_KEY_DATE_FORMAT, $t);
	}
    }
    return $keys;
}

function analyze_pageview_delete($start, $end = 0) {
    $keys = analyze_pageview_get_keys($start, $end);
    foreach($keys as $key) {
	cache_delete('', $key);
    }
}

function analyze_pageview_delete_all() {
    storage_cache_delete('', ANALYZE_PAGEVIEW_KEY_PREFIX);
}

function analyze_pageview_get_value($pagename, $date) {
    if($pagename === false || $pagename === '')
	$cachekey = $date;
    else
	$cachekey = $date . '-'. $pagename;
    if(($data = cache_get('', $cachekey)) === false) {
	return false;
    }
    return (int)$data;
}

function analyze_quantize_time($time) {
    if($time == ANALYZE_PAGEVIEW_DATE_TOTAL)
	return $time;
    $l = localtime($time, true);
    return mktime(0, 0, 0, $l['tm_mon'] + 1, $l['tm_mday'], $l['tm_year'] + 1900);
}

?>
