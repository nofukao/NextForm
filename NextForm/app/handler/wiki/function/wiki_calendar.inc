<?php
define('WIKI_CALENDAR_ONE_DAY', 60 * 60 * 24);
/*
 * 週表示で URL (calendar_behind / calendar_ahead) から広げられる週の数の上限 (前後それぞれ)。
 * URL は誰でも書き換えられるので、上限が無いと誰でも長い範囲を描かせ、日のページの数だけ
 * 目次の変換を走らせられた。本文に書いた behind= / ahead= は書き手の値なので収めず、
 * 本文でこれより広く書いてあれば、URL の上限もその値にする。
 */
define('WIKI_CALENDAR_URL_WEEKS_MAX', 12);
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

    /*
     * マスは日のページの有無とタイトルで変わり、本文は変わらない。差分の書き出しが
     * このページを飛ばして、あとから作った日のページを書き出さないように、
     * &pages と同じく「何に依るか分からない (毎回書き直す)」の印を立てる。
     */
    $context['page']['depend_pages_unknown'] = true;

    $target = wiki_get_target_string($tree);

    $now = time();
    $now_date = datepage_format($now);

    $calendar_mode = 'month';
    $behind = 1;
    $ahead = 3;
    $base_time = $now;

    $args = page_args($context['page']);

    /* URL の値。calendar[]=.. のような並びは文字ではないので使わない (trim() が TypeError になる) */
    if(!empty($args['calendar']) && is_string($args['calendar'])) {
	$d = $args['calendar'];
	$d = trim($d, ' - /');
	$d = str_replace('/', '-', $d);
	/*
	 * 年だけ・年と月だけのときは、その 1 日にする。値の全体で比べる。以前は 4 桁の
	 * 数字があればいつも -01-01 を足し、2026-08-06 を 2026-08-06-01-01 にしていた
	 * (strtotime は最後の -01 を UTC-1 時間と読み、日本より西の時間帯では前の日になる)。
	 */
	if(preg_match('/^\d{4}$/', $d)) {
	    $d = $d . '-01-01';
	} else if(preg_match('/^\d{4}\-\d{1,2}$/', $d)) {
	    $d = $d . '-01';
	}
	$base_time = strtotime($d);
	if($base_time === false)
	    $base_time = $now;
    }
    /*
     * 表のマスは起点から 24 時間ずつ足して作る。夏時間のある時間帯で起点が 0 時台や
     * 23 時台だと、切り替えをまたいだところで日付が重なったり抜けたりするので、
     * 起点はその日の正午にそろえる (1 時間ずれても日付が変わらない)。
     */
    $base_time = mktime(12, 0, 0, (int)nf_date('%m', $base_time),
			(int)nf_date('%d', $base_time), (int)nf_date('%Y', $base_time));

    $basepagename = $context['page']['name'];
    $show = '';
    $maxlength_value = '';
    wiki_parse_parameters(
	$context, $tree,
	array(
	    'pagename' => &$basepagename,
	    'equalities' => array(
		'behind' => &$behind,
		'ahead' => &$ahead,
		'show' => &$show,
		'maxlength' => &$maxlength_value,
		),
	    'long_flags' => array(
		'month' => array(&$calendar_mode, 'month'),
		'weeks' => array(&$calendar_mode, 'weeks'),
		),
	    )
	);
    /*
     * マスに出すもの。既定はタイトル (show=title。同じ日の複数のページも並べる)。
     * 今までの目次の表示は show=toc。知らない値は既定にする。
     */
    $is_title_mode = (trim($show) !== 'toc');
    /* マスの文字数の上限。1 以上の整数 (数字だけ) のときだけ効く。ほかは上限なし */
    $maxlength = (is_string($maxlength_value) && ctype_digit(trim($maxlength_value))) ?
	(int)trim($maxlength_value) : 0;

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
	/* 本文の behind= / ahead= は書き手の値。数字でなければ 0 (足し算が TypeError になる) */
	$behind = max((int)$behind, 0);
	$ahead = max((int)$ahead, 0);
	/*
	 * URL の値は上限に収める。本文でそれより広く書いてあれば、本文の値まで
	 * (▲▼や「移動」は今の範囲を URL に写すので、本文の値より狭めない)。
	 */
	if(!empty($args['calendar_behind'])) 
	    $behind = min(max((int)$args['calendar_behind'], 0),
			  max(WIKI_CALENDAR_URL_WEEKS_MAX, $behind));
	if(!empty($args['calendar_ahead'])) 
	    $ahead = min(max((int)$args['calendar_ahead'], 0),
			 max(WIKI_CALENDAR_URL_WEEKS_MAX, $ahead));
    }

    $dayrules = wiki_operator_calendar_get_dayrules(WIKI_CALENDAR_RULES_DEFAULT . "\n" . $target);
    if(!isset($WIKI_CALENDAR_EXTERNAL_INFO['basepagename'])) {
	$WIKI_CALENDAR_EXTERNAL_INFO['basepagename'] =& $basepagename;
	$WIKI_CALENDAR_EXTERNAL_INFO['dayrules'] =& $dayrules;
    }

    $start_time = $base_time - WIKI_CALENDAR_ONE_DAY * (7 * ($behind + 1) - 1);
    $end_time = $base_time + WIKI_CALENDAR_ONE_DAY * (7 * ($ahead + 1));
    $base_month = nf_date('%m', $base_time);

    $weeks = array();
    $week = array();
    $w = nf_date('%w', $start_time);
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
		     'day' => nf_date('%d', $t),
		     'month' => nf_date('%m', $t),
		     'year' => nf_date('%Y', $t),
		     'name' => $name,
		     'pagename' => $basepagename . '/' . $name,
	    );
	$week[$w] = $day;
    }

    /* show=title では、表に出る日のページ (2 つ目からを含む) を先に 1 度だけ探す */
    $day_pages = array();
    $day_numbers_max = array();
    $deleted_numbers_max = null;	/* 「＋」を出すときに初めて探す */
    if($is_title_mode)
	wiki_operator_calendar_find_day_pages($basepagename, $weeks, $day_pages, $day_numbers_max);

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
	    /*
	     * 読めない日のページは、無いページと同じに描く (&pages が一覧から落とすのと同じ)。
	     * page_read() は読めないページで止まる (403 かログインの要求) ので、1 つ読めない
	     * だけでカレンダーのあるページ全体が開けなくなっていた。
	     */
	    if($is_title_mode) {
		$pages_of_day = isset($day_pages[$day['name']]) ? $day_pages[$day['name']] : array();
		$is_day_page = !empty($pages_of_day);
	    } else {
		$is_day_page = page_is_exists($day['pagename']) &&
		    auth_has_permission($day['pagename'], 'read');
	    }
	    if($is_day_page)
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

	    /*
	     * show=title でページのある日の日付はリンクにしない (行き先が 1 つに決まらない。
	     * ページへはマスのタイトルから行く)。小さい画面とサイドでは、マスを押すと中身が
	     * 浮いて出る。日付のリンクが無くなってもキーボードで開けるよう、nextform.js の
	     * calendarHelperSetup() が、中身が隠れているときだけ日付を Tab で止まり Enter で
	     * 開けるものにする (広い画面に、押しても何も起きない止まり場所を作らない)。
	     * ページの無い日の日付は今までどおり、1 つ目を作るリンク。
	     */
	    $is_date_text = $is_title_mode && $is_day_page;
	    if($is_date_text) {
		$day_label = $dt;
	    } else {
		$uri = url_make($day['pagename']);
		$day_label = dom_append_link($dt, $uri, '');
	    }
	    /* span.day_detail は日付の中に置く (JavaScript と CSS が小さい画面の見分けに使う) */
	    if(empty($preday) || $preday['month'] != $day['month']) {
		$detail_day = substr($day['name'], 0, strlen($day['name']) - strlen($day['day']));
		$span = dom_append_child($day_label, 'span', 'day_detail');
		dom_append_text($span, $detail_day);
	    }
	    dom_append_text($day_label, $day['day']);
	    if(!$is_date_text) {
		if($is_day_page)
		    dom_set_attribute($day_label, 'data-link-pagename', $day['pagename']);
		else
		    dom_set_attribute($day_label, 'rel', 'nofollow');
	    }

	    foreach($event_names as $event_name) {
		$dt = dom_append_child($dl, 'dt', 'event_name');
		dom_append_text($dt, $event_name);
	    }

	    $dd = dom_append_child($dl, 'dd');
	    if($is_day_page && $is_title_mode) {
		wiki_operator_calendar_append_day_pages($dd, $td, $pages_of_day, $maxlength);
		/*
		 * 「＋」: 次の番号のページを作るリンク。書ける人にだけ出す。番号は、読めない
		 * ページと消したページも含めたいちばん大きい番号の次 (消したページの名前を
		 * 使い回すと、新しいページの履歴に消したページの古い版が出る)。消したページを
		 * 探すのは、その日に書ける人に出すときだけ。静的な書き出しには出さない
		 * (無いページへのリンクになる)。data-link-pagename も付けない (付けると
		 * 書き出しが無いページを書き出そうとしてエラーになる)。
		 */
		if(empty($context['page']['is_export']) &&
		   auth_has_permission($day['pagename'], 'write')) {
		    if(is_null($deleted_numbers_max))
			$deleted_numbers_max = wiki_operator_calendar_deleted_numbers_max($basepagename);
		    $next = max(default_value($day_numbers_max[$day['name']], 1),
				default_value($deleted_numbers_max[$day['name']], 0)) + 1;
		    $next_pagename = $day['pagename'] . '_' . $next;
		    if($next <= DATEPAGE_NUMBER_MAX && auth_has_permission($next_pagename, 'write')) {
			$a = dom_append_link($dd, url_make($next_pagename), '＋', 'create');
			dom_set_attribute($a, 'rel', 'nofollow');
			dom_set_attribute($a, 'title', l('Add a page for this day'));
			dom_set_attribute($a, 'aria-label', l('Add a page for this day'));
		    }
		}
	    } else if($is_day_page) {
		$page = page_read($day['pagename']);
		if(!empty($page['meta']['ticket']))
		    dom_set_attribute($td, 'data-ref-ticket', $page['meta']['ticket']);
		handle($page, $dd, 'summary');
		if($maxlength > 0)
		    wiki_operator_calendar_cut_toc($dd, $dd, $maxlength);
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
	$caption_text_start = nf_date(l('%b %Y'), $weeks[0][0]['time']);
	$caption_text_end = nf_date(l('%b %Y'), $weeks[$weeks_num - 1][6]['time']);
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
	$caption_text = nf_date(l('%b %Y'), $base_time);
	$form_value = nf_date('%Y-%m', $base_time);

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

/*
 * show=title のときにセルに出す文字。短いタイトル → タイトル の順に選ぶ。
 * どちらも無ければ空文字で、呼び出し側は今までどおり目次を出す (ページ名を出すと
 * 日付と重なるため)。meta を読むだけで、本文は描かない。
 */
function wiki_operator_calendar_cell_title(&$page) {
    foreach(array('short_title', 'title') as $key) {
	if(isset($page['meta'][$key]) && trim($page['meta'][$key]) !== '')
	    return trim($page['meta'][$key]);
    }
    return '';
}

/*
 * show=title のときの日のページ。親の子ページを 1 度だけ探し、表に出る日の
 * YYYY-MM-DD と YYYY-MM-DD_N (util.inc の DATEPAGE_NUMBER_REGEX) を拾う。
 *   $day_pages[日付][番号]  ページ名。読めるものだけ、番号順
 *   $numbers_max[日付]      いちばん大きい番号。読めないページも含める (「＋」の番号)
 * page_find() は名前だけを見て、読む権限は確かめない。キャッシュに消したページが
 * 残っていることがあるので、表に出る日のものは page_is_exists() で確かめる。
 * 親が空のときは探さない (サイト全体を探すことになる)。1 つ目だけを見る。
 */
function wiki_operator_calendar_find_day_pages($basepagename, &$weeks, &$day_pages, &$numbers_max) {
    $dates = array();
    foreach($weeks as $week) {
	foreach($week as $day)
	    $dates[$day['name']] = true;
    }

    $names = array();
    if($basepagename === '') {
	foreach($dates as $date => $dummy)
	    $names[] = '/' . $date;
    } else {
	$found = page_find($basepagename, wiki_operator_calendar_day_pages_condition($basepagename));
	foreach($found as $found_page)
	    $names[] = $found_page['name'];
    }

    foreach($names as $name) {
	$parsed = datepage_parse($name);
	if($parsed === false || !isset($dates[$parsed['date']]) || !page_is_exists($name))
	    continue;
	$date = $parsed['date'];
	if(!isset($numbers_max[$date]) || $numbers_max[$date] < $parsed['number'])
	    $numbers_max[$date] = $parsed['number'];
	if(auth_has_permission($name, 'read'))
	    $day_pages[$date][$parsed['number']] = $name;
    }
    foreach($day_pages as &$pages)
	ksort($pages);
}

/* 日のページを探す条件 (親の子ページのうち、日付のページの名前のもの) */
function wiki_operator_calendar_day_pages_condition($basepagename) {
    return array(
	'is_pagename_only' => true,
	'is_children_only' => true,
	'name_regex' => '|^' . preg_quote($basepagename . '/', '|') .
			DATEPAGE_REGEX . DATEPAGE_NUMBER_REGEX . '$|',
	);
}

/* 消した日のページのいちばん大きい番号 (日付ごと)。「＋」が消したページの名前を使わないように */
function wiki_operator_calendar_deleted_numbers_max($basepagename) {
    $numbers_max = array();
    if($basepagename === '')
	return $numbers_max;
    $found = page_find_deleted($basepagename, wiki_operator_calendar_day_pages_condition($basepagename));
    foreach($found as $found_page) {
	$parsed = datepage_parse($found_page['name']);
	if($parsed === false)
	    continue;
	if(!isset($numbers_max[$parsed['date']]) || $numbers_max[$parsed['date']] < $parsed['number'])
	    $numbers_max[$parsed['date']] = $parsed['number'];
    }
    return $numbers_max;
}

/*
 * show=title のページのある日のマスの中身。ページを番号順に箇条書き (ul.day_pages) に並べる。
 * 項目ごとにそのページの data-ref-pagename と data-ref-ticket を付ける (一覧 &pages の項目と
 * 同じ。その項目の上でダブルクリックすると、そのページを編集する)。マス (td) の ticket は
 * 1 つ目のもので、1 つ目が無ければ付けない (マスのダブルクリックは 1 つ目を編集する)。
 * 各ページは、短いタイトル → タイトル → (どちらも無ければ) 目次。
 */
function wiki_operator_calendar_append_day_pages($dd, $td, &$pages_of_day, $maxlength) {
    $ul = dom_append_child($dd, 'ul', 'day_pages');
    foreach($pages_of_day as $number => $pagename) {
	$page = page_read($pagename);
	$ticket = default_value($page['meta']['ticket'], '');
	if($number == 1 && $ticket !== '')
	    dom_set_attribute($td, 'data-ref-ticket', $ticket);

	$li = dom_append_child($ul, 'li');
	dom_set_attribute($li, 'data-ref-pagename', $pagename);
	dom_set_attribute($li, 'data-ref-ticket', $ticket);

	$title = wiki_operator_calendar_cell_title($page);
	if($title !== '') {
	    $text = wiki_operator_calendar_cut_string($title, $maxlength);
	    $a = dom_append_page_link($li, $text, $pagename);
	    dom_append_class($a, 'title');
	    if($text !== $title)
		dom_set_attribute($a, 'title', $title);
	} else {
	    wiki_operator_calendar_append_toc($li, $page);
	    if($maxlength > 0)
		wiki_operator_calendar_cut_toc($li, $li, $maxlength);
	}
    }
}

/*
 * タイトルの無いページの目次 (show=title のとき)。種別の summary を直に呼ぶ。handle() を
 * 通すと、カレンダーを開いただけでそのページを「読んだ」ことになる (一覧の目次と同じ考え)。
 * 見出しはリンクにする (ツールの「目次」と同じ)。
 * 日付がリンクでないので、そのページへのリンク (data-link-pagename) が 1 本も無ければ
 * (見出しの無いページ・text・file・本文が空)、中身をそのページへのリンク 1 本に置き換える。
 * リンクの文字は中身の文字 (書き出し)、文字も無ければページ名の最後の部分。
 */
function wiki_operator_calendar_append_toc($li, &$page) {
    $summary = handler_function($page, 'summary');
    if($summary !== false && function_exists($summary)) {
	$toc_page = $page;
	$summary($toc_page, $li, true);
    }

    $xpath = new DOMXPath(dom_owner($li));
    foreach($xpath->query('.//a[@data-link-pagename]', $li) as $a) {
	if($a->getAttribute('data-link-pagename') === $page['name'])
	    return;
    }
    $text = trim($li->textContent);
    /* Wiki 記法の目次は、文字の無いページでページ名の全体を出す。マスでは最後の部分にする */
    if($text === '' || $text === $page['name'])
	$text = page_get_simplename($page['name']);
    while($li->firstChild)
	$li->removeChild($li->firstChild);
    dom_append_page_link($li, $text, $page['name']);
}

/* 見た目の 1 文字 (書記素) ずつに分ける。不正な UTF-8 などで分けられなければ false */
function wiki_operator_calendar_graphemes($text) {
    if(preg_match_all('/\X/u', $text, $matches) === false)
	return false;
    return $matches[0];
}

/* $maxlength 文字 (見た目の 1 文字を 1) を超えたら切って「…」を付ける。0 は上限なし */
function wiki_operator_calendar_cut_string($text, $maxlength) {
    if($maxlength <= 0)
	return $text;
    $chars = wiki_operator_calendar_graphemes($text);
    if($chars === false || count($chars) <= $maxlength)
	return $text;
    return implode('', array_slice($chars, 0, $maxlength)) . '…';
}

/*
 * 目次 ($node の中) の文字を出てくる順に数え、$maxlength 文字に来たところで切って「…」を
 * 付ける (先頭の見出しの途中で切れることもある)。後ろの文字は消し、空になった項目や入れ子の
 * 箇条書きも消す。残った見出しはリンクのまま。文字のかたまりの前後の空白は数えない。
 * 切ったときは、$title_holder の title 属性に見出しの全文を 1 行に 1 つずつ入れる
 * (マウスを載せると出る)。数えられない文字があれば切らない。
 */
function wiki_operator_calendar_cut_toc($node, $title_holder, $maxlength) {
    $xpath = new DOMXPath(dom_owner($node));
    $texts = array();
    $total = 0;
    foreach($xpath->query('.//text()', $node) as $text) {
	$chars = wiki_operator_calendar_graphemes(trim($text->data));
	if($chars === false)
	    return false;
	$texts[] = array('node' => $text, 'chars' => $chars);
	$total += count($chars);
    }
    if($total <= $maxlength)
	return false;

    $lines = wiki_operator_calendar_toc_lines($node, $xpath);

    /* 1 段目: 上限に来た文字を切り、その後ろの文字を消す (要素はまだ消さない) */
    $left = $maxlength;
    $is_cut = false;
    $parents = array();
    foreach($texts as $text) {
	if($is_cut) {
	    $parents[] = $text['node']->parentNode;
	    $text['node']->parentNode->removeChild($text['node']);
	    continue;
	}
	$count = count($text['chars']);
	if($count == 0)
	    continue;
	if($count < $left) {
	    $left -= $count;
	    continue;
	}
	$text['node']->data = implode('', array_slice($text['chars'], 0, $left)) . '…';
	$is_cut = true;
    }

    /*
     * 2 段目: 空になった項目・リンク・箇条書きを、$node の手前まで消す。
     * 先に消した要素の中にある要素は、もう $node につながっていないので飛ばす
     * (上へたどると、親の無い要素に行き着く)。
     */
    foreach($parents as $parent) {
	if(!wiki_operator_calendar_is_inside($parent, $node))
	    continue;
	while($parent !== $node && trim($parent->textContent) === '' &&
	      $parent->getElementsByTagName('img')->length == 0) {
	    $grandparent = $parent->parentNode;
	    $grandparent->removeChild($parent);
	    $parent = $grandparent;
	}
    }
    dom_set_attribute($title_holder, 'title', implode("\n", $lines));
    return true;
}

/* $element が $node の中に (まだ) つながっているか */
function wiki_operator_calendar_is_inside($element, $node) {
    for($e = $element; !is_null($e); $e = $e->parentNode) {
	if($e === $node)
	    return true;
    }
    return false;
}

/* 目次の見出しの文字 (項目ごとに 1 つ。入れ子の箇条書きは除く)。項目が無ければ中身の文字 */
function wiki_operator_calendar_toc_lines($node, $xpath) {
    $items = $xpath->query('.//li', $node);
    if($items->length == 0)
	return array(trim($node->textContent));
    $lines = array();
    foreach($items as $item) {
	$line = '';
	foreach($item->childNodes as $child) {
	    if(!($child instanceof DOMElement && strtolower($child->nodeName) === 'ul'))
		$line .= $child->textContent;
	}
	$line = trim($line);
	if($line !== '')
	    $lines[] = $line;
    }
    return $lines;
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
	    $daysofmonth = (int)nf_date('%d', mktime(0, 0, 0, $month + 1, 0, $year));
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

    /*
     * ページの描画を止めて取りに行くので、待ち時間に上限を掛ける。
     * 掛けないと default_socket_timeout (既定 60 秒) 任せになる。
     * 取れなければ空で返し、その結果もキャッシュされる (呼び出し側を参照)。
     */
    $file_context = stream_context_create(
	array('http' => array('ignore_errors' => true,
			      'timeout' => EXTERNAL_FETCH_TIMEOUT)));
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
|[[カレンダー:../../Function/calendar]]|&&calendar(&[&[ページ名&]&], month, weeks, behind=N, ahead=N, show=toc, maxlength=N);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|calendar|&&calendar(&[&[page name&]&], month, weeks, behind=N, ahead=N, show=toc, maxlength=N);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/calendar

&title &&calendar

*概要
 &calendar([[ページ名]], month, weeks, behind=N, ahead=N, show=toc, maxlength=N){
 イベント情報
 }
カレンダーを表示します．さらにカレンダー上の1日を1ページに対応させて簡単なスケジュール帳としても利用できます．
1日に対応するページは現在のページの子ページになり ''現在のページ名/年-月-日'' というページ名になります．
同じ日に2つ以上のページを作ることもできます(下の「同じ日のページ」)．
カレンダー上の各マスにはその日のページのタイトルが表示されます(下の「マスに出るもの」)．
カレンダーのマスをダブルクリックすることですぐに子ページを編集することができます．
[[&&datepages:../datepages]]を使うと子ページを内容とともに一覧表示することもできます．

*パラメータ
:&[&[ページ名&]&]|カレンダーで作成する子ページの親を指定します．指定がない場合は自分自身のページが親になります．
:month|一般的なカレンダーのように月を表示します．デフォルトで指定されています．
:weeks|今週を基準に前後に数週間分表示します．表示する週はbehind,aheadパラメータで指定できます．
:behind=N|weeksが指定された際にN週分過去の週を表示します(Nは整数)．
:ahead=N|weeksが指定された際にN週分未来の週を表示します(Nは整数)．画面の▲▼で広げられるのは，前後それぞれ12週までです．
:{show=toc|各マスに，タイトルの代わりにその日のページの要約(見出しの一覧)を出します(NextForm 0.21 までの表示です)．
日付はその日のページへのリンクになり，同じ日の2つ目からのページは出ません．
書かなければタイトルを出します(""show=title""と書いても同じです)．}
:{maxlength=N|各マスに出す文字の数の上限です(Nは1以上の整数)．ページごとに数え，全角も半角も1文字と数えます．
超えたところで切って「…」を付けます．切ったタイトルは，マウスを載せると全文が出ます．
要約は見出しを順に数え，上限に来たところで切ります(先頭の見出しの途中で切れることもあります)．
書かなければ切りません．}

*マスに出るもの
各マスには，その日のページの''短いタイトル''を出します．
短いタイトルが無いページはタイトルを，どちらも無いページは要約(見出しの一覧)を出します．
短いタイトルは，タイトルから日付を除いたものなど，マスの場所で分かることを除いて書きます
(Wiki記法の[[&&title(short):../title]]，Markdown記法のフロントマターの ""short_title:""，
メタ情報の画面)．
マスのタイトルと要約の見出しは，そのページへのリンクになります．
見出しの無いページは書き出しの文字が，文字も無いページはページ名がリンクになります．
狭い画面とサイドバーでは，ページのある日のマスに「...」が出て，押すと中身が出ます．

*同じ日のページ
同じ日に2つ以上のページを作れます．ページ名は ''年-月-日''(1つ目)，''年-月-日_2''，''年-月-日_3''… です
(番号は9999まで)．
-ページのある日は，日付がリンクにならず，その日のページを番号の順に並べます．
-書ける人には，最後に小さく「＋」が出ます．押すと，次の番号のページを作れます．
番号は，いちばん大きい番号(消したページを含む)の次です．
-ページの無い日は，日付を押すと1つ目のページを作れます．
-ページのタイトル(や要約)の上でダブルクリックするとそのページを，日付や余白の上でダブルクリックすると1つ目のページを編集します．
-2つ目からのページは1つ目の子ページではなく兄弟です．1つ目の名前を変えたり，消したり，クローンしたりしても，一緒には動きません．
-""show=toc""のときは，今までどおり1つ目のページだけを出します．

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
    $year = nf_date('%Y', $time);
    $month = nf_date('%m', $time);
    $day = nf_date('%d', $time);
    $d = nf_date('%w', $time);

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

/**
@manual $manualpage$/en/Wiki/Function/calendar

&title &&calendar

*Overview
 &calendar([[page name]], month, weeks, behind=N, ahead=N, show=toc, maxlength=N){
 event data
 }
Shows a calendar. One day on the calendar can also be tied to one page, making a simple
scheduler. The page for a day is a child page of the current page, named
''current page name/year-month-day''.
A day can also have two or more pages (see "Pages of the same day" below).
Each box of the calendar shows the title of the page for that day (see "What a box shows" below).
Double clicking a box of the calendar edits the child page straight away.
[[&&datepages:../datepages]] lists the child pages with their contents.

*Parameters
:&[&[page name&]&]|the parent of the child pages the calendar creates. With nothing given, the page itself is the parent.
:month|show a month, like an ordinary calendar. Set by default.
:weeks|show a few weeks around this week. Which weeks are shown is set by the behind and ahead parameters.
:behind=N|with weeks set, show N weeks into the past (N is a whole number).
:ahead=N|with weeks set, show N weeks into the future (N is a whole number). The ▲ and ▼ buttons on the screen widen it up to 12 weeks each way.
:{show=toc|show the summary (the list of headings) of the page for the day in each box,
instead of its title (how NextForm 0.21 and before showed it).
The date is a link to the page for the day, and the second and later pages of a day are not shown.
With nothing given, the title is shown (the same as writing ""show=title"").}
:{maxlength=N|the most characters shown in a box (N is a whole number, 1 or more). Each page is
counted on its own, and every character counts as one, whether full width or half width.
Where it goes over, the text is cut and "…" is added. Hovering the mouse over a cut title shows it whole.
A summary is counted heading by heading, in order, and cut where it reaches the limit
(it can be cut in the middle of the first heading). With nothing given, nothing is cut.}

*What a box shows
Each box shows the ''short title'' of the page for the day.
A page without a short title shows its title, and a page with neither shows its summary
(the list of headings).
Write the short title without what the place of the box already tells, such as the date
in the title ([[&&title(short):../title]] in Wiki notation, ""short_title:"" in the front
matter in Markdown notation, or the meta information screen).
The titles and the headings of a summary in a box are links to the page.
For a page without headings the first words are the link, and for a page with no words, its page name.
On a narrow screen and in the side bar, a box of a day with pages shows "...", and tapping it shows what is inside.

*Pages of the same day
A day can have two or more pages. They are named ''year-month-day'' (the first),
''year-month-day_2'', ''year-month-day_3'' and so on (up to 9999).
-For a day with pages, the date is not a link, and the pages of the day are listed in the order of their numbers.
-Anyone who can write sees a small "＋" at the end. Pressing it makes the page with the next number.
The number is the one after the largest number, deleted pages included.
-For a day without pages, pressing the date makes the first page.
-Double clicking on the title (or the summary) of a page edits that page, and double clicking on the date or a blank part edits the first page.
-The second and later pages are not child pages of the first but its siblings. Renaming, deleting or cloning the first page does not move them with it.
-With ""show=toc"", only the first page is shown, as before.

*Target
Data about events such as public holidays can be written as the target.
Ordinary public holidays are shown through Google Calendar even with nothing written,
but using Google Calendar means getting a Google API key allowed to reach the Calendar
API and setting it in site menu > Admin tools > Site preferences, under Google API key.
The php settings also have to contain ""extension=openssl.so"", so that the API can be
reached over SSL.

Write one rule per line. Two kinds of rule can be used.
-the calendar ID of a Google Calendar (the API key has to be set)
-{crontab style notation
 year month day week weekday name of the event
Write the rule in that form.
Year, month, day, week and weekday can be a number or &*. &* means anything will do.
Week sets which occurrence of that weekday in the month it is. For example
 * * * 3 Tue collection of recyclables
means the third Tuesday of every month. A negative value counts from the end of
the month.}

In both cases, writing holiday after the rule, with a space before it, treats it as
a holiday.

*Example

Take the following setup as an example.
-show the events of the Google Calendar hogehogefoobar@group.calendar.google.com
-the 20th of June every year is Founder's Day, a holiday
-21 May 2012 is the annular solar eclipse
-the last Tuesday of December every year is the big clean

&wikiexample{
&calendar(weeks,ahead=2,behind=1) {
hogehogefoobar@group.calendar.google.com
* 6 20 * * Founder's Day holiday
2012 5 21 * * annular solar eclipse
* 12 * -1 Tue the big clean
}
}
*/
?>
