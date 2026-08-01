<?php
$WIKI_OPERATORS_FUNCTION['select'] = true;
function wiki_operator_select(&$context, $dom, $tree) {
    global $main_pagename;

    if(!empty($context['texts_only'])) {
	wiki_convert_from_tokens($context, $dom, $tree->target);
	return;
    }

    $is_once = false;
    $options = array();
    wiki_parse_parameters($context, $tree, 
			  array('values' => &$options,
				'long_flags' => array(
				    'once' => &$is_once,
				    'replace' => &$is_once,
				    )));
    $options = implode('/', $options);
    $target = wiki_get_target_string($tree);
    $page = &$context['page'];

    if(empty($options)) {
	dom_append_error($dom, l('&select no parameter'));
	return;
    }

    if($is_once) {
	$position = $tree->position;
	$length = $tree->length;
    } else {
	$target_tokens = wiki_tokens_trim_space($tree->target);
	if(empty($target_tokens)) {
	    $position = $tree->position + $tree->length - 1;
	    $contents = page_get_contents($page);
	    while($position >= 0 && $position < $page['contents_size'] &&
		  (in_array($contents[$position], array(';', '}', "\n"))))
		$position--;
	    $position++;
	    $length = 0;
	} else {
	    $last_token = end($target_tokens);
	    $first_token = reset($target_tokens);
	    $position = $first_token->position;
	    $length = $last_token->position + $last_token->length - $position;
	}
    }

    $form = dom_append_page_form(
	$page, $dom,
	    array('option' => 'replace',
		  'position' => $position,
		  'length' => $length,
		  'redirect' => url_make($main_pagename),
		  'action' => 'write'), 'select');
    dom_set_position($form, $tree);
    $option_elements = options_parse($options);
    $default_value = trim($target);
    dom_append_selector($form, 'value', $option_elements, $default_value, true);
    dom_append_child($form, 'input', 'redundant',
		     array('type' => 'submit', 'value' => l('Change')));
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/FunctionAdvanced
|[[選択フォーム:../../Function/select]]|&&select(once, A/B/C...);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/FunctionAdvanced
|selector form|&&select(once, A/B/C...);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/select
&title &&select

*概要
 &select(once, 選択肢1/選択肢2/...){ 現在の選択 }
選択肢を表示します．どの選択肢を選択しているかは対象の部分(上記の例で現在の選択の部分)に保存されます．
選択肢を変更すると対象の部分が新しい選択肢に置き換わります．

*パラメータ
:once|一度選択を行うと選択肢はなくなり，選択した値だけに置き換えるようにします．
:{選択肢1/選択肢2/...|選択肢を半角/(スラッシュ)記号で区切って書きます．
個々の選択肢をさらに半角:(コロン)記号で区切ると選択肢の表示と値を別々に設定できます．
}

*対象
対象は現在の選択肢の値が保存されています．省略した場合はどれも選択していないことになります．

*例

&wikiexample{
&select(和食/洋食/中華/イタリアン){洋食};

&select(once, 和食:A/洋食:B/中華:C/イタリアン:D){A}
}
*/

?>
