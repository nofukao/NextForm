<?php
$WIKI_OPERATORS_FUNCTION['time'] = true;
function wiki_operator_time(&$context, $dom, $tree) {
    $formats = array();
    $mode = 'normal';
    $is_show = true;
    $new_time = 0;
    wiki_parse_parameters(
	$context, $tree,
	array(
	    'values' => &$formats,
	    'long_flags' => array(
		'elapsed' => array(&$mode, 'elapsed'),
		'left' => array(&$mode, 'left'),
		'new' => array(&$new_time, WIKI_TIME_DEFAULT_NEW_HOUR),
		'show' => &$is_show,
		),
	    'equalities' => array(
		'new' => &$new_time,
		),
	    )
	);
    $format = array_pop($formats);
    if(empty($format))
	$format = TIME_FORMAT;

    if($mode != 'normal' && !empty($context['texts_only']))
	return;

    $target_dom = dom_create_document();
    wiki_convert_from_tokens($context, $target_dom, $tree->target);
    $text = dom_text($target_dom);
    $text = preg_replace('/\(.*?\)/', ' ', $text);
    $time = strtotime($text);
    if($time === false || $time === -1) {
	dom_append_error($dom, l('&time unkown time format'));
	return;
    }

    $diff = time() - $time;

    if($is_show) {
	switch($mode) {
	case 'normal':
	    $output = nf_date($format, $time);
	    break;
	case 'left':
	    $diff = -$diff;
	case 'elapsed':
	    $remain = max(0, $diff);
	    $output = '';
	    $secs = array((60 * 60 * 24), (60 * 60), 60, 1);
	    $unit = array('days', 'hours', 'minutes', 'seconds');
	    for($i = 0; $i < 4; $i++) {
		$v = floor($remain / $secs[$i]);
		if($v > 0 || $i == 3) {
		    $output = $v . l($unit[$i]);
		    break;
		}
		$remain -= $v * $secs[$i];
	    }
	    break;
	}
	
    } else
	$output = '';

    
    $te = dom_append_time($dom, $time);

    sentence_append($te, $output, $context['page']);

    if($new_time > 0 && $diff < $new_time * 60 * 60 && empty($context['texts_only'])) {
	if(!empty($output))
	    dom_append_text($te, ' ');
	$strong = dom_append_child($te, 'strong', 'new');
	dom_append_text($strong, 'NEW!');
    }
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[日時の表示:../../Function/time]]|&&time(new, left, elapsed){日時}|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|time and date|&&time(new, left, elapsed){time and date}|
*/
/**
@manual $manualpage$/ja/Wiki/Function/time
&title &&time

*概要
 &time(new, left, elapsed, show, フォーマット){日時}
日時を指定したフォーマットにして表示します．
新しい日時であることを強調したり，目標の日時までの残り時間を表示することもできます．

*パラメータ
:{new|指定した日時が新しい場合，それを強調するNEW!という文字を加えます．
新しい日時は標準では`WIKI_TIME_DEFAULT_NEW_HOUR`時間以内になります．
new=時間の形式で指定するとNEW!を表示する時間を指定できます．}
:left|指定した時間までの残り時間を表示します．
:elapsed|指定した時間からの経過時間を表示します．表示はleftと同様です．
:{show|時刻を表示します．デフォルトで指定されています．
NEW!のみを表示して日時の表示が不要なときに!showと指定します．}
:{フォーマット|指定したフォーマットに従って日時を表示します．
フォーマットは[[strftimeのフォーマット:http://php.net/manual/ja/function.strftime.php]]に従います．
デフォルトは`TIME_FORMAT`です．}

*対象
対象には日時を書きます．&&mtime;等の他の記法が含まれていても構いません．

*例

&wikiexample{
2013年のお正月まであと&time(left){2013-1-1}です．

このページが作成された日は&time(%Y/%m/%d){ &mtime; }です．
}
*/

?>
