<?php
define('WIKI_CALENDAR_ONE_DAY', 60 * 60 * 24);
$WIKI_CALENDAR_WEEK_NAME = array('Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat');
$WIKI_CALENDAR_WEEK_NAME_TO_DAYOFWEEK = array(
    'sun' => 0, l('Sun') => 0,
    'mon' => 1, l('Mon') => 1,
    'tue' => 2, l('Tue') => 2,
    'wed' => 3, l('Wed') => 3,
    'thu' => 4, l('Thu') => 4,
    'fri' => 5, l('Fri') => 5,
    'sat' => 6, l('Sat') => 6,
    );
$WIKI_CALENDAR_EXTERNAL_INFO = array();

$WIKI_OPERATORS_FUNCTION['calendar'] = true;
function wiki_operator_calendar(&$context, $dom, $tree) {
    global $WIKI_CALENDAR_WEEK_NAME;
    global $WIKI_CALENDAR_EXTERNAL_INFO;

    if(!empty($context['texts_only']))
	return;

    $target = wiki_get_target_string($tree);

    $now = time();
    $now_date = datepage_format($now);

    $calendar_mode = 'month';
    $behind = 1;
    $ahead = 3;
    $base_time = $now;

    $args = page_args($context['page']);

    if(!empty($args['calendar'])) {
	$d = $args['calendar'];
	$d = trim($d, ' - /');
	$d = str_replace('/', '-', $d);
	if(preg_match('/\d{4}/', $d)) {
	    $d = $d . '-01-01';
	} else if(preg_match('/\d{4}\-\d{2}/', $d)) {
	    $d = $d . '-01';
	}
	$base_time = strtotime($d);
	if($base_time == 0)
	    $base_time = $now;
    }

    $basepagename = $context['page']['name'];
    wiki_parse_parameters(
	$context, $tree,
	array(
	    'pagename' => &$basepagename,
	    'equalities' => array(
		'behind' => &$behind,
		'ahead' => &$ahead,
		),
	    'long_flags' => array(
		'month' => array(&$calendar_mode, 'month'),
		'weeks' => array(&$calendar_mode, 'weeks'),
		),
	    )
	);
    if(!empty($args['calendar_mode'])) {
	if($args['calendar_mode'] == 'month')
	    $calendar_mode = 'month';
	else
	    $calendar_mode = 'weeks';
    }

    if($calendar_mode == 'month') {
	$behind = 5;
	$ahead = 5;
    } else {
	if(!empty($args['calendar_behind'])) 
	    $behind = (int)$args['calendar_behind'];
	if(!empty($args['calendar_ahead'])) 
	    $ahead = (int)$args['calendar_ahead'];
    }

    $dayrules = wiki_operator_calendar_get_dayrules(WIKI_CALENDAR_RULES_DEFAULT . "\n" . $target);
    if(!isset($WIKI_CALENDAR_EXTERNAL_INFO['basepagename'])) {
	$WIKI_CALENDAR_EXTERNAL_INFO['basepagename'] =& $basepagename;
	$WIKI_CALENDAR_EXTERNAL_INFO['dayrules'] =& $dayrules;
    }

    $start_time = $base_time - WIKI_CALENDAR_ONE_DAY * (7 * ($behind + 1) - 1);
    $end_time = $base_time + WIKI_CALENDAR_ONE_DAY * (7 * ($ahead + 1));
    $base_month = strftime('%m', $base_time);

    $weeks = array();
    $week = array();
    $w = strftime('%w', $start_time);
    for($t = $start_time; $t <= $end_time; $t += WIKI_CALENDAR_ONE_DAY, $w = ($w + 1) % 7) {
	if($w == 0) {
	    if(count($week) == 7) {
		if($calendar_mode == 'month') {
		    foreach($week as $d) {
			if($d['month'] == $base_month) {
			    $weeks[] = $week;
			    break;
			}
		    }
		} else
		    $weeks[] = $week;
	    }
	    $week = array();
	}
	$name = datepage_format($t);
	$day = array('time' => $t,
		     'day' => strftime('%d', $t),
		     'month' => strftime('%m', $t),
		     'year' => strftime('%Y', $t),
		     'name' => $name,
		     'pagename' => $basepagename . '/' . $name,
	    );
	$week[$w] = $day;
    }

    $table = dom_append_child($dom, 'table', 'calendar');
    dom_append_class($table, 'nosort');
    $thead = dom_append_child($table, 'thead');
    $tbody = dom_append_child($table, 'tbody');
    //$tfoot = dom_append_child($table, 'tfoot');

    $tr = dom_append_child($thead, 'tr');
    for($i = 0; $i < 7; $i++) {
	$th = dom_append_child($tr, 'th');
	dom_append_text($th, l($WIKI_CALENDAR_WEEK_NAME[$i]));
    }
    $weeks_num = count($weeks);
    $preday = null;
    if($calendar_mode == 'weeks') {
	$th = dom_append_child($tr, 'th');
	dom_append_text($th, '');
    }

    for($w = 0; $w < $weeks_num; $w++) {
	$tr = dom_append_child($tbody, 'tr');
	for($d = 0; $d < 7; $d++) {
	    $day = &$weeks[$w][$d];
	    $classes = array();
	    if(!empty($weeks[$w - 1][$d]) && $weeks[$w - 1][$d]['month'] != $day['month'])
		$classes[] = 'month_border border_top';
	    if(!empty($weeks[$w + 1][$d]) && $weeks[$w + 1][$d]['month'] != $day['month'])
		$classes[] = 'month_border border_bottom';
	    if(!empty($weeks[$w][$d - 1]) && $weeks[$w][$d - 1]['month'] != $day['month'])
		$classes[] = 'month_border border_left';
	    if(!empty($weeks[$w][$d + 1]) && $weeks[$w][$d + 1]['month'] != $day['month'])
		$classes[] = 'month_border border_right';
	    if($day['month'] != $base_month && $calendar_mode == 'month')
		$classes[] = 'other_month';
	    if($day['name'] == $now_date)
		$classes[] = 'today';
	    if(page_is_exists($day['pagename']))
		$classes[] = 'exists';

	    $classes[] = 'week_' . $d;

	    $dayinfos = wiki_operator_calendar_get_dayinfos(
		$dayrules, $day['year'], $day['month'], $day['day'], $d);
	    $event_names = array();
	    foreach($dayinfos as &$dayinfo) {
		$event_names[] = $dayinfo['name'];
		if(!empty($dayinfo['class']))
		    $classes[] = $dayinfo['class'];
	    }

	    $td = dom_append_child($tr, 'td', implode(' ', $classes));
	    dom_set_attribute($td, 'data-ref-pagename', $day['pagename']);
	    $dl = dom_append_child($td, 'dl');
	    $dt = dom_append_child($dl, 'dt', 'day');

	    $uri = url_make($day['pagename']);
	    $a = dom_append_link($dt, $uri, ''); 
	    if(empty($preday) || $preday['month'] != $day['month']) {
		$detail_day = substr($day['name'], 0, strlen($day['name']) - strlen($day['day']));
		$span = dom_append_child($a, 'span', 'day_detail');
		dom_append_text($span, $detail_day);
	    }
	    dom_append_text($a, $day['day']);
	    if(page_is_exists($day['pagename']))
		dom_set_attribute($a, 'data-link-pagename', $day['pagename']);
	    
	    if(!page_is_exists($day['pagename']))
		dom_set_attribute($a, 'rel', 'nofollow');

	    foreach($event_names as $event_name) {
		$dt = dom_append_child($dl, 'dt', 'event_name');
		dom_append_text($dt, $event_name);
	    }

	    $dd = dom_append_child($dl, 'dd');
	    if(page_is_exists($day['pagename'])) {
		$page = page_read($day['pagename']);
		if(!empty($page['meta']['ticket']))
		    dom_set_attribute($td, 'data-ref-ticket', $page['meta']['ticket']);
		handle($page, $dd, 'summary');
	    } else {
		dom_append_text($dd, '');
	    }
	    $preday = &$day;
	}
	if($w == 0 && $calendar_mode == 'weeks') {
	    $th = dom_append_child($tr, 'th');
	    dom_set_attribute($th, 'rowspan', $weeks_num);
	    dom_append_class($th, 'navigation');
	}
    }

    $caption = dom_insert_child($table, 'caption');

    $new_args = array('calendar' => datepage_format($base_time),
		      'calendar_mode' => $calendar_mode,
		      'calendar_behind' => $behind,
		      'calendar_ahead' => $ahead);

    if($calendar_mode == 'weeks') {
	$caption_text_start = strftime(l('%b %Y'), $weeks[0][0]['time']);
	$caption_text_end = strftime(l('%b %Y'), $weeks[$weeks_num - 1][6]['time']);
	if($caption_text_start == $caption_text_end)
	    $caption_text = $caption_text_start;
	else
	    $caption_text = $caption_text_start . ' - ' . $caption_text_end;
	$form_value = datepage_format($base_time);
	$h = dom_append_child($caption, 'h1');
	dom_append_text($h, $caption_text);
	
	$next_args = $new_args;
	$next_args['calendar_behind'] += 2;
	$a = dom_append_link($th, url_make(url_pagename($context['page']), $next_args));
	dom_append_text($a, '▲');
	dom_append_class($a, 'button previous');
	dom_set_attribute($a, 'rel', 'nofollow');
	$next_args = $new_args;
	$next_args['calendar_ahead'] += 2;
	$a = dom_append_link($th, url_make(url_pagename($context['page']), $next_args));
	dom_append_text($a, '▼');
	dom_append_class($a, 'button next');
	dom_set_attribute($a, 'rel', 'nofollow');
    } else {
	$caption_text = strftime(l('%b %Y'), $base_time);
	$form_value = strftime('%Y-%m', $base_time);

	$blt = localtime($base_time, true);
	$prev_time = mktime($blt['tm_hour'], $blt['tm_min'], $blt['tm_sec'],
			    $blt['tm_mon'] + 1 - 1, 1, $blt['tm_year'] + 1900);
	$next_time = mktime($blt['tm_hour'], $blt['tm_min'], $blt['tm_sec'],
			    $blt['tm_mon'] + 1 + 1, 1, $blt['tm_year'] + 1900);
	$prev_date = datepage_format($prev_time);
	$next_date = datepage_format($next_time);

	$next_args = $new_args;
	$next_args['calendar'] = $prev_date;
	unset($next_args['calendar_ahead']);
	unset($next_args['calendar_behind']);
	$a = dom_append_link($caption, url_make(url_pagename($context['page']), $next_args));
	dom_append_text($a, '◀');
	dom_append_class($a, 'button previous');
	dom_set_attribute($a, 'rel', 'nofollow');
	$h = dom_append_child($caption, 'h1');
	dom_append_text($h, $caption_text);
	$next_args = $new_args;
	$next_args['calendar'] = $next_date;
	unset($next_args['calendar_ahead']);
	unset($next_args['calendar_behind']);
	$a = dom_append_link($caption, url_make(url_pagename($context['page']), $next_args));
	dom_append_text($a, '▶');
	dom_append_class($a, 'button next');
	dom_set_attribute($a, 'rel', 'nofollow');
    }

    $hiddens = $new_args;
    $hiddens['page'] = url_pagename($context['page']);
    unset($hiddens['calendar']);
    $form = dom_append_form($caption, $hiddens, 'calendar', 'GET');
    $input = dom_append_child($form, 'input', '',
			      array('type' => 'text',
				    'name' => 'calendar',
				    'value' => $form_value));
    $input = dom_append_child($form, 'input', '',
			      array('type' => 'submit', 'value' => l('Move')));
    if($calendar_mode == 'month') {
	$next_args = $new_args;
	$next_args['calendar_mode'] = 'weeks';
	unset($next_args['calendar_ahead']);
	unset($next_args['calendar_behind']);
	$a = dom_append_link($form, url_make(url_pagename($context['page']), $next_args), l('Weekly'), 'button');
	dom_set_attribute($a, 'rel', 'nofollow');
    } else {
	$next_args = $new_args;
	$next_args['calendar_mode'] = 'month';
	$a = dom_append_link($form, url_make(url_pagename($context['page']), $next_args), l('Monthly'), 'button');
	dom_set_attribute($a, 'rel', 'nofollow');
    }
    $a = dom_append_link($form, url_make(url_pagename($context['page'])), l('Back'), 'button');
    //if(!isset($args['calendar']))
    //dom_append_class($a, 'disabled');
}

function wiki_operator_calendar_get_dayrules($rules_text) {
    $dayrules = array('remotes' => array(), 'patterns' => array(), 'immediates' => array(), 'loaded' => array());
    $lines = explode("\n", $rules_text);
    foreach($lines as &$line) {
	$line = trim($line);
	if(preg_match('|^([\d\*][\d\*/,]*) +([\d\*/,]+) +([\d\*/,]+) +([\d\-\*/,]+) +([^\s]+) +([^\s]+?)( +\w*)?$|', $line, $matches)) {
	    $pattern = array(
		'year' => $matches[1],
		'month' => $matches[2],
		'day' => $matches[3],
		'weekofmonth' => $matches[4],
		'dayofweek' => $matches[5]);
	    $pattern['name'] = $matches[6];
	    if(!empty($matches[7]))
		$pattern['class'] = trim($matches[7]);
	    $dayrules['patterns'][] = $pattern;
	} else if(preg_match('/^([\w\d]+[\w\d\._-]*@[\w\d_-]+[\w\d\._-]+)( +\w*)?( +[\d\.]+)?$/', $line, $matches)) {
	    $remote = array('type' => 'google', 'id' => $matches[1], 'cacheday' => 90);
	    if(!empty($matches[2]))
		$remote['class'] = trim($matches[2]);
	    if(!empty($matches[3]))
		$remote['cacheday'] = trim($matches[3]);
	    $dayrules['remotes'][] = $remote;
	}
    }
    return $dayrules;
}

function wiki_operator_calendar_get_dayinfos(&$dayrules, $year, $month, $day, $dayofweek) {
    global $WIKI_CALENDAR_WEEK_NAME_TO_DAYOFWEEK;
    $year = (int)$year;
    $month = (int)$month;
    $day = (int)$day;
    $dayofweek = (int)$dayofweek;

    $now = time();
    foreach($dayrules['remotes'] as &$remote) {
	$key = $remote['id'] . ' ' . $year;
	if(empty($dayrules['loaded'][$key])) {
	    $cachekey = 'calendar-' . $key;
	    $immediates = cache_serialized_get('', $cachekey, $now - WIKI_CALENDAR_ONE_DAY * $remote['cacheday']);
	    if($immediates === false) {
		if($remote['type'] == 'google') {
		    $immediates = wiki_operator_calendar_get_google_calender(
			$remote['id'], $year . '-01-01', $year . '-12-31');
		    cache_serialized_create('', $cachekey, $immediates);
		} else {
		    $immediates = array();
		}
	    }
	    foreach($immediates as $date => &$im) {
		if(!isset($dayrules['immediates'][$date]))
		    $dayrules['immediates'][$date] = array();
		foreach($im as &$m) {
		    $m['class'] = default_value($remote['class']);
		    $dayrules['immediates'][$date][] = $m;
		}
	    }
	    $dayrules['loaded'][$key] = true;
	}
    }

    $dayinfos = array();
    $date = sprintf('%04d-%02d-%02d', $year, $month, $day);
    if(!empty($dayrules['immediates'][$date])) {
	$dayinfos = array_merge($dayinfos, $dayrules['immediates'][$date]);
    }

    foreach($dayrules['patterns'] as &$pattern) {
	$is_match = true;
	//pprint_r($pattern);
	
	$is_match = $is_match && wiki_operator_calendar_pattern_match($pattern['year'], $year);
	$is_match = $is_match && wiki_operator_calendar_pattern_match($pattern['month'], $month);
	$is_match = $is_match && wiki_operator_calendar_pattern_match($pattern['day'], $day);

	if($is_match) { 
	    $weekofmonth = floor(($day - 1)/ 7) + 1;
	    $daysofmonth = (int)strftime('%d', mktime(0, 0, 0, $month + 1, 0, $year));
	    $weekofmonth_rev = -(floor(($daysofmonth - $day) / 7) + 1);
	    $is_match = 
		(wiki_operator_calendar_pattern_match($pattern['weekofmonth'], $weekofmonth) ||
		 wiki_operator_calendar_pattern_match($pattern['weekofmonth'], $weekofmonth_rev));
	    //$is_match = $is_match && wiki_operator_calendar_pattern_match($pattern['weekofmonth'], $weekofmonth_rev);
	}

	if($is_match) {
	    $k = strtolower($pattern['dayofweek']);
	    if(isset($WIKI_CALENDAR_WEEK_NAME_TO_DAYOFWEEK[$k]))
		$pattern_dayofweek = $WIKI_CALENDAR_WEEK_NAME_TO_DAYOFWEEK[$k];
	    else
		$pattern_dayofweek = $pattern['dayofweek'];
	    $is_match = wiki_operator_calendar_pattern_match($pattern_dayofweek, $dayofweek);
	}
	
	if($is_match) {
	    $dayinfos[] = $pattern;
	}
    }

    return $dayinfos;
}

function wiki_operator_calendar_pattern_match($patterns_string, $value) {
    $patterns = explode(',', $patterns_string);
    foreach($patterns as &$pattern) {
	if($pattern === '*')
	    return true;
	if(preg_match('|\*/(\d+)|', $pattern, $matches)) {
	    $mod = (int)$matches[1];
	    if($mod != 0 && ($value % $mod) == 0)
		return true;
	} else if(((int)$pattern) == $value)
	    return true;
    }
}

function wiki_operator_calendar_get_google_calender($id, $start, $end) {
    if(GOOGLE_API_KEY === '')
	return array();
    
    $uri = 'https://www.googleapis.com/calendar/v3/calendars/' .
	urlencode($id) .
	'/events?' .
	'key=' . urlencode(GOOGLE_API_KEY) .
	'&singleEvents=true'.
	'&timeMin=' . urlencode($start) . 'T00:00:00Z' .
	'&timeMax=' . urlencode($end) . 'T00:00:00Z' .
	'&maxResults=100' .
	'&orderBy=startTime';

    $data = array();

    $file_context = stream_context_create(array('http' => array('ignore_errors' => true)));
    $json_text = @file_get_contents($uri, false, $file_context);
    if(!empty($json_text)) {
	$json = json_decode($json_text, true);
	if(!empty($json['items'])) {
	    foreach ($json['items'] as &$item) {
		$date  = datepage_format(strtotime($item['start']['date']));
		$title = $item['summary'];
		if(preg_match('|^(.*)/ .+ / .+$|', $title, $matches))
		    $title = $matches[1];
		$d = array('name' => trim($title));
		$data[$date][] = $d;
	    }
	}
    }
    return $data;
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[カレンダー:../../Function/calendar]]|&&calendar(&[&[ページ名&]&], month, weeks, behind=N, ahead=N);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|calendar|&&calendar(&[&[page name&]&], month, weeks, behind=N, ahead=N);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/calendar

&title &&calendar

*概要
 &calendar([[ページ名]], month, weeks, behind=N, ahead=N){
 イベント情報
 }
カレンダーを表示します．さらにカレンダー上の1日を1ページに対応させて簡単なスケジュール帳としても利用できます．
1日に対応するページは現在のページの子ページになり ''現在のページ名/年-月-日'' というページ名になります．
カレンダー上の各マスにはその日のページの要約が表示されます．
カレンダーのマスをダブルクリックすることですぐに子ページを編集することができます．
[[&&datepages:../datepages]]を使うと子ページを内容とともに一覧表示することもできます．

*パラメータ
:&[&[ページ名&]&]|カレンダーで作成する子ページの親を指定します．指定がない場合は自分自身のページが親になります．
:month|一般的なカレンダーのように月を表示します．デフォルトで指定されています．
:weeks|今週を基準に前後に数週間分表示します．表示する週はbehind,aheadパラメータで指定できます．
:behind=N|weeksが指定された際にN週分過去の週を表示します(Nは整数)．
:ahead=N|weeksが指定された際にN週分未来の週を表示します(Nは整数)．

*対象
対象には祝祭日等のイベントの情報が書けます．
特になにも書かなくてもGoogleカレンダーを利用して通常の祝祭日が表示されるようになっていますが，
Googleカレンダーを利用するにはCalendar APIへのアクセスが許可されたGoogle APIキーを取得し，
サイトメニュー > 管理ツール > サイト設定 の Google API key に設定する必要があります．
またSSL経由でAPIを利用するためphpの設定に""extension=openssl.so""が書かれている必要があります．

1行にひとつずつルールを書き，ルールは以下の2つが使用できます．
-GoogleカレンダーのカレンダーID(APIキーの設定が必要)
-{crontab風表記
 年 月 日 週 曜日 イベントの名前
という形式でルールを書きます．
年 月 日 週 曜日は数値か&*が書けます．&*は何でもいいという意味です．
週はその月で何回目の曜日かを指定します．例えば
 * * * 3 火 資源ごみの日
と書けば毎月第3火曜日のことになります．負の値の場合は月の最後からカウントします．}

どちらもルールのあとにスペースをいれてholidayと書くと休日扱いになります．

*例

以下のような設定を例にあげます．
-hogehogefoobar@group.calendar.google.comというGoogleカレンダーの予定を表示
-毎年6月20日は創立記念日で休日
-2012年5月21日は金環日食
-毎年12月の最終火曜日は大掃除

&wikiexample{
&calendar(weeks,ahead=2,behind=1) {
hogehogefoobar@group.calendar.google.com
* 6 20 * * 創立記念日 holiday
2012 5 21 * * 金環日食
* 12 * -1 火 大掃除
}
}
*/

function wiki_operator_calendar_get_day_page_info_for_external_tool($basepagename, $date) {
    global $WIKI_CALENDAR_EXTERNAL_INFO;

    $page = page_read($basepagename);
    $dom = dom_create_document();
    wiki_convert($page, $dom);
    $time = strtotime($date);
    $year = strftime('%Y', $time);
    $month = strftime('%m', $time);
    $day = strftime('%d', $time);
    $d = strftime('%w', $time);

    $dayinfos = wiki_operator_calendar_get_dayinfos(
	$WIKI_CALENDAR_EXTERNAL_INFO['dayrules'], $year, $month, $day, $d);

    $classes = array('week_' . $d);
    $event_names = array();
    foreach($dayinfos as &$dayinfo) {
	$event_names[] = $dayinfo['name'];
	if(!empty($dayinfo['class']))
	    $classes[] = $dayinfo['class'];
    }

    $pagename = $WIKI_CALENDAR_EXTERNAL_INFO['basepagename'] . '/' . $date;
    $contents = '';
    //$dom = dom_create_document();
    if(page_is_exists($pagename)) {
	$page = page_read($pagename);
	if(page_type_is_text_base($page['meta']['type']))
	    $contents = page_get_contents($page);
	else
	    $contents = '';
	//handle($page, $dom, 'summary');
    }
    return array(
	'classes' => &$classes,
	'event_names' => &$event_names,
	'contents' => $contents,
	//'summary' => $dom
	);
}
?>
