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

?>
