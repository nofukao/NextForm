<?php
$WIKI_OPERATORS_FUNCTION['counter'] = true;
function wiki_operator_counter(&$context, $dom, $tree) {
    if(!empty($context['texts_only'])) {
	wiki_convert_from_tokens($context, $dom, $tree->target);
	return;
    }

    $analyze_condition = array();
    wiki_operator_analyze_parse_parameters($context, $tree, $analyze_condition);
    if(!empty($analyze_condition['allpages'])) {
	$count = analyze_pageview_get(false, $analyze_condition['start'], $analyze_condition['end']);
    } else
	$count = analyze_pageview_get($analyze_condition['pagename'],
				      $analyze_condition['start'], $analyze_condition['end']);
    dom_append_text($dom, (int)$count);
    wiki_convert_from_tokens($context, $dom, $tree->target);
}

$WIKI_OPERATORS_FUNCTION['popularpages'] = true;
function wiki_operator_popularpages(&$context, $dom, $tree) {
    if(!empty($context['texts_only'])) {
	wiki_convert_from_tokens($context, $dom, $tree->target);
	return;
    }

    $pagename = '';
    $list_options = array(
	'is_ignore_system_page' => true,
	'limit' => 10,
	'fields' => "[[title-pagename]] (pageview)",
	'display_rules' => array(
	    array(
		'term' => 'pageview',
		'numeric' => true,
		'equal' => true,
		'direction' => '<',
		'value' => 0,
		'display' => 'none'
		)
	    )
	);
    wiki_operator_analyze_parse_parameters($context, $tree, $analyze_condition);
    wiki_operator_pages_parse_parameters($context, $tree, $pagename, $find_condition, $list_options, $values);
    $find_condition['sort'] = 'pageview';
    $find_condition['analyze'] = $analyze_condition;
    $find_pages = page_find($pagename, $find_condition);
    $list_options['base_pagename'] = $pagename;
    $section = pagelist_append($dom, $find_pages, $list_options);
    if(!empty($section))
	dom_remove_empty($section);
}

function wiki_operator_analyze_parse_parameters(&$context, $tree, &$condition) {
    $now = time();

    $pagename = $context['page']['name'];
    $is_allpages = false;
    $start = ANALYZE_PAGEVIEW_DATE_TOTAL;
    $days = false;
    $day = false;

    wiki_parse_parameters(
	$context, $tree,
	array(
	    'pagename' => &$pagename,
	    'long_flags' => array(
		'today' => array(&$start, $now),
		'yesterday' => array(&$start, $now - 60 * 60 * 24),
		'total' => array(&$start, ANALYZE_PAGEVIEW_DATE_TOTAL),
		'allpages' => &$is_allpages,
		),
	    'equalities' => array(
		'day' => &$day,
		'days' => &$days,
		),
	    )
	);
    if($days !== false) {
	$start = $now - $days * 60 * 60 * 24;
	$end = $now + 60 * 60 * 24;
    } else if($day !== false) {
	$start = $now - $day * 60 * 60 * 24;
	$end = $start + 60 * 60 * 24;
    } else {
	$end = $start + 60 * 60 * 24;
    }
    if($is_allpages)
	$condition['allpages'] = true;
    else
	$condition['pagename'] = $pagename;

    $condition['start'] = analyze_quantize_time($start);

    if($start == ANALYZE_PAGEVIEW_DATE_TOTAL)
	$condition['end'] = ANALYZE_PAGEVIEW_DATE_TOTAL;
    else
	$condition['end'] = analyze_quantize_time($end);

    return $condition;
}


/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[カウンター:../../Function/counter]]|&&counter(total, today, yesterday, allpages);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|counter|&&counter(total, today, yesterday, allpages);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/counter
&title &&counter

*概要
 &counter([[ページ名]],  total, today, yesterday, day=N, days=N, allpages);
アクセスカウンターを表示します．
集計対象の期間とページを指定できます．
表示されるのは数値のみなので前後に好きなように説明を入れて使用します．
アクセスカウンターの値はアクセス後すぐに増えるのではなく一定の間隔で更新されます．
また同一のユーザーの同一ページへの短時間でのアクセスはカウントされません．

カウンターの値は，サイトの管理者であれば''管理ツール'' -> ''アクセス解析''からでも確認できます．

*注意
自分でNextFormを設置した場合，
アクセスカウンターを使用するにはWebサーバーの管理者がアクセス解析の設定をしていなければいけません．
詳細は[[アクセス解析の設定:../../../Administration/Analyzer]]を参照して下さい．

*パラメータ
:{&[&[ページ名&]&]|アクセス数を知りたいページを指定します．
ページ名が省略された場合は現在のページが対象になります．}
:total|総計のアクセス数を表示します．デフォルトで指定されています．
:today|本日のアクセス数を表示します．
:yesterday|昨日のアクセス数を表示します．
:{day=N|Nには日数をいれます．N日前のアクセス数を表示します．
何日前まで集計のデータが残っているかはサイトの設定によります．
このサイトでは`ANALYZE_STORAGE_LIFE_DAYS`日間です．
day=0はtodayと，day=1はyesterdayと同じ効果になります．
}
:{days=N|Nには日数をいれます．N日前から現在までのアクセス数の合計を表示します．
何日前まで集計のデータが残っているかはサイトの設定によります．
このサイトでは`ANALYZE_STORAGE_LIFE_DAYS`日間です．}
:allpages|ひとつのページではなくサイトのすべてのページの合計を表示します．&[&[&]&]と指定しても同じ効果です．

*対象
対象は指定しません．

*例
 -このページのアクセス数: &counter;
 -このページの本日のアクセス数: &counter(today);
 -このページの昨日のアクセス数: &counter(yesterday);
 -Topページの3日前からの合計アクセス数: &counter([[Top]], days=3);
 -このサイトの累計のアクセス数: &counter(allpages);
 -このサイトの本日のアクセス数: &counter(allpages, today);
 -このサイトの4日前のアクセス数: &counter(allpages, day=4);
*/

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[人気のページ一覧:../../Function/popularpages]]|&&popularpages(total, today, yesterday, allpages, 件数);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|list of popular pages|&&popularpages(total, today, yesterday, allpages, number of pages);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/popularpages
&title &&popularpages

*概要
 &popularpages(total, today, yesterday, day=N, days=N, 件数);
人気のページ(アクセス数の多いページ)をアクセスの多い順に一覧表示します．

アクセス数は，サイトの管理者であれば''管理ツール'' -> ''アクセス解析''からでも確認できます．

*注意
自分でNextFormを設置した場合，
人気のページ一覧を使用するにはWebサーバーの管理者がアクセス解析の設定をしていなければいけません．
詳細は[[アクセス解析の設定:../../../Administration/Analyzer]]を参照して下さい．

*パラメータ
:&[&[ページ名&]&]|一覧の対象を指定したページの子ページに限定します．
:total|総計のアクセス数を元に一覧表示します．デフォルトで指定されています．
:today|本日のアクセス数を元に一覧表示します．
:yesterday|昨日のアクセス数を元に一覧表示します．
:{day=N|Nには日数をいれます．N日前のアクセス数を元に一覧表示します．
何日前まで集計のデータが残っているかはサイトの設定によります．
このサイトでは`ANALYZE_STORAGE_LIFE_DAYS`日間です．
day=0はtodayと，day=1はyesterdayと同じ効果になります．
}
:{days=N|Nには日数をいれます．N日前から現在までのアクセス数元に一覧表示します．
何日前まで集計のデータが残っているかはサイトの設定によります．
このサイトでは`ANALYZE_STORAGE_LIFE_DAYS`日間です．}
:件数|表示する件数を指定します．省略した場合は10件になります．

[[&&pages:../pages]]のパラメータも使用できます．
*対象
対象は指定しません．

*例
 &popularpages; //人気のページ10件を表示
 &popularpages(show=table, today, name=!Top, name=!Side, 5); //今日の人気ページを表で5件，ただしTopとSideは除外
 &popularpages(fields=[[pagename]]); //ページ名だけ表示(アクセス件数は非表示)
*/


/**
@manual $manualpage$/en/Wiki/Function/counter
&title &&counter

*Overview
 &counter([[page name]],  total, today, yesterday, day=N, days=N, allpages);
Shows an access counter.
The period and the page to total up can be set.
Only the number is shown, so put whatever wording you like around it.
The value of the access counter does not rise the moment a page is accessed;
it is updated at a regular interval.
Repeated accesses to the same page by the same user in a short time are not counted.

The administrator of the site can also see the value of the counter at
''Admin tools'' -> ''Access analysis''.

*Note
If you installed NextForm yourself, the administrator of the web server has to set up
access analysis before the access counter can be used.
See [[Access analysis settings:../../../Administration/Analyzer]] for details.

*Parameters
:{&[&[page name&]&]|the page whose number of accesses you want.
Left out, the current page is used.}
:total|show the total number of accesses. Set by default.
:today|show the number of accesses today.
:yesterday|show the number of accesses yesterday.
:{day=N|put a number of days in N. Shows the number of accesses N days ago.
How many days of data are kept depends on the settings of the site.
On this site it is `ANALYZE_STORAGE_LIFE_DAYS` days.
day=0 has the same effect as today, and day=1 the same as yesterday.
}
:{days=N|put a number of days in N. Shows the total number of accesses from N days ago
until now.
How many days of data are kept depends on the settings of the site.
On this site it is `ANALYZE_STORAGE_LIFE_DAYS` days.}
:allpages|show the total over every page of the site rather than one page. Writing &[&[&]&] has the same effect.

*Target
No target is given.

*Example
 -accesses to this page: &counter;
 -accesses to this page today: &counter(today);
 -accesses to this page yesterday: &counter(yesterday);
 -accesses to the Top page from 3 days ago: &counter([[Top]], days=3);
 -accesses to this site in total: &counter(allpages);
 -accesses to this site today: &counter(allpages, today);
 -accesses to this site 4 days ago: &counter(allpages, day=4);
*/

/**
@manual $manualpage$/en/Wiki/Function/popularpages
&title &&popularpages

*Overview
 &popularpages(total, today, yesterday, day=N, days=N, number of pages);
Lists the popular pages (the pages with the most accesses), most accessed first.

The administrator of the site can also see the numbers of accesses at
''Admin tools'' -> ''Access analysis''.

*Note
If you installed NextForm yourself, the administrator of the web server has to set up
access analysis before the list of popular pages can be used.
See [[Access analysis settings:../../../Administration/Analyzer]] for details.

*Parameters
:&[&[page name&]&]|limit the list to the child pages of the page named.
:total|list them by the total number of accesses. Set by default.
:today|list them by the number of accesses today.
:yesterday|list them by the number of accesses yesterday.
:{day=N|put a number of days in N. Lists them by the number of accesses N days ago.
How many days of data are kept depends on the settings of the site.
On this site it is `ANALYZE_STORAGE_LIFE_DAYS` days.
day=0 has the same effect as today, and day=1 the same as yesterday.
}
:{days=N|put a number of days in N. Lists them by the number of accesses from N days
ago until now.
How many days of data are kept depends on the settings of the site.
On this site it is `ANALYZE_STORAGE_LIFE_DAYS` days.}
:number of pages|how many to show. Left out, 10 are shown.

The parameters of [[&&pages:../pages]] can be used as well.
*Target
No target is given.

*Example
 &popularpages; //show the 10 most popular pages
 &popularpages(show=table, today, name=!Top, name=!Side, 5); //today's 5 most popular in a table, leaving out Top and Side
 &popularpages(fields=[[pagename]]); //show only the page names (not the access counts)
*/
?>
