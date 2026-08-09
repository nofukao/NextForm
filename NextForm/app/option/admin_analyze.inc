<?php
$OPTIONS['admin_analyze'] = array(
    'show' => 'admin_analyze_show',
    );

function admin_analyze_show(&$args, $dom) {
    auth_check(false, 'admin');
    title_set_l('Access analyzer');

    $now = time();
    $oneday = 60 * 60 * 24;

    $days = array(
	array(ANALYZE_PAGEVIEW_DATE_TOTAL, ANALYZE_PAGEVIEW_DATE_TOTAL, l('Aggregate total')),
	array($now, $now + $oneday, l('Today')),
	array($now - $oneday, $now, l('Yesterday')),
	);
    for($d = count($days) - 1; $d < ANALYZE_STORAGE_LIFE_DAYS; $d++) {
	$days[] = array($now - $d * $oneday, $now - ($d - 1) * $oneday,
			lf('$num$ days ago', array('num' => $d)));
    }
    
    $default_days_num = count($days);

    $counts = array();
    $day_totals = array();
    foreach($days as $day) {
	$day_counts = analyze_pageview_get_all($day[0], $day[1]);
	$day_totals[$day[0]] = $day_counts['allpages'];
	$day_totals2[$day[0]] = array_sum(array_values($day_counts['pages']));
	if($day[0] == ANALYZE_PAGEVIEW_DATE_TOTAL)
	    arsort($day_counts['pages']);
	foreach($day_counts['pages'] as $pagename => $num) {
	    $counts[$pagename][$day[0]] = $num;
	}
    }
    $days = array_reverse($days);

    $section = dom_append_child($dom, 'section');
    $h = dom_append_child($section, 'h1');
    dom_append_text($h, l('Number of hits per page'));
    
    $table = dom_append_child($section, 'table', 'analyze');
    $thead = dom_append_child($table, 'thead');
    $tbody = dom_append_child($table, 'tbody');
    $tr = dom_append_child($thead, 'tr');
    $th = dom_append_child($tr, 'th');
    dom_append_text($th, l('Page name'));
    foreach($days as $day) {
	$th = dom_append_child($tr, 'th');
	dom_append_text($th, $day[2]);
	if($day[0] != ANALYZE_PAGEVIEW_DATE_TOTAL) {
	    dom_set_attribute($th, 'title', nf_date(DATE_FORMAT, $day[0]));
	}
    }
    $tr = dom_append_child($thead, 'tr');
    $th = dom_append_child($tr, 'th', 'numeric');
    dom_append_text($th, l('Total'));
    foreach($days as $day) {
	$th = dom_append_child($tr, 'th', 'numeric');
	dom_append_text($th, $day_totals[$day[0]]);
    }

    foreach($counts as $pagename => $daycounts) {
	$tr = dom_append_child($tbody, 'tr');
	$td = dom_append_child($tr, 'td');
	dom_append_page_link($td, $pagename, $pagename);
	foreach($days as $day) {
	    $td = dom_append_child($tr, 'td', 'numeric');
	    $num = isset($daycounts[$day[0]]) ? $daycounts[$day[0]] : '';
	    dom_append_text($td, $num.'');
	}
    }
}
?>
