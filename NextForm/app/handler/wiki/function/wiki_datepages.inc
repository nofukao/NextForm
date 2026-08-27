<?php
$WIKI_OPERATORS_FUNCTION['datepages'] = true;
function wiki_operator_datepages(&$context, $dom, $tree) {
    if(!empty($context['texts_only']))
	return;
    $pagename = $context['page']['name'];
    $list_options = array('context' => &$context, 'show' => 'contents', 'each' => 30);
    $find_condition = array(
	'is_ignore_system_page' => true,
	'name_regex' => '|/' . DATEPAGE_REGEX . '$|',
	'is_pagename_only' => true,
	'is_children_only' => true,
	'reverse' => true,
	);
    wiki_operator_pages_parse_parameters($context, $tree, $pagename, $find_condition, $list_options, $values);

    $template = template_get($context, wiki_get_target_string($tree));
    if($template !== '')
	$list_options['template'] = $template;

    $direction = 'past';
    $basedate = datepage_format(time());
    wiki_parse_parameters(
	$context, $tree,
	array(
	    'long_flags' => array(
		'all' => array(&$direction, 'all'),
		'past' => array(&$direction, 'past'),
		'future' => array(&$direction, 'future'),
		),
	    )
	);
    if($direction == 'future') {
	$find_condition['reverse'] = empty($find_condition['reverse']);
    }
    if(!empty($values)) {
	$date = array_pop($values);
	if(datepage_is_date($date))
	    $basedate = $date;
    }

    $find_pages = page_find($pagename, $find_condition);
    $list_pages = array();
    foreach($find_pages as &$page) {
	if(datepage_is_datepage($page['name'], $date)) {
	    if($direction == 'all' ||
	       ($direction == 'past' && $date <= $basedate) ||
	       ($direction == 'future' && $date >= $basedate)) {
		$list_pages[] = &$page;
	    }
	}
    }
    $list_options['base_pagename'] = $pagename;
    $section = pagelist_append($dom, $list_pages, $list_options, page_args($context['page']));
    if(!empty($section))
	dom_remove_empty($section);
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[カレンダーで作成したページ一覧:../../Function/datepages]]|&&datepages(&[&[ページ名&]&], past, future);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|list calendar pages|&&datepages(&[&[page name&]&], past, future);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/datepages

&title &&datepages

*概要
 &datepages([[ページ名]], past, future, all, reverse, each=表示ページ数, 年-月-日);
ページ名で指定されたページの子ページのうち日付の形式(YYYY-MM-DD)をしているページを一覧表示します．
[[&&calendar:../calendar]]を使用して作られた各日のページを時系列順に表示するのに適しています．

*パラメータ
:&[&[ページ名&]&]|一覧表示する子ページの親を指定します．指定がない場合は自分自身のページが親になります．
:past|現在から過去の日付のページを表示します．デフォルトで指定されています．
:future|現在から未来の日付のページを表示します．
:all|すべてのページを表示します．
:reverse|表示の順序を逆にします．
:each=表示ページ数|一度に表示する子ページの数を指定します．デフォルトは30です．
:年-月-日|pastとfutureの基準を現在ではなく指定した日付にします．

*対象
対象は指定しません．

*例
''予定''の子ページのうち2012-07-01から未来の子ページを20件づつ一覧表示します．
 &datepages([[予定]]], future, 20, 2012-07-01);
*/


/**
@manual $manualpage$/en/Wiki/Function/datepages

&title &&datepages

*Overview
 &datepages([[page name]], past, future, all, reverse, each=pages to show, year-month-day);
Lists the child pages of the page named whose names are in date form (YYYY-MM-DD).
It suits showing the pages for each day made with [[&&calendar:../calendar]]
in order of time.

*Parameters
:&[&[page name&]&]|the parent of the child pages to list. With nothing given, the page itself is the parent.
:past|show pages dated from now into the past. Set by default.
:future|show pages dated from now into the future.
:all|show every page.
:reverse|reverse the order they are shown in.
:each=pages to show|how many child pages to show at a time. The default is 30.
:year-month-day|take the date given, rather than now, as the point past and future are measured from.

*Target
No target is given.

*Example
Lists the child pages of ''Schedule'' dated from 2012-07-01 into the future, 20 at a time.
 &datepages([[Schedule]]], future, 20, 2012-07-01);
*/
?>
