<?php
$WIKI_OPERATORS_FUNCTION['listadd'] = true;
function wiki_operator_listadd(&$context, $dom, $tree) {
    $target = wiki_get_target_string($tree);
    $page = &$context['page'];

    $mode = 'before';
    $is_name = false;
    $is_date = false;
    $is_nest = true;
    $is_escape = false;
    $is_allow_url = true;
    $is_only_ja = false;
    $is_ordered = false;
    $button_label = l('Add');

    wiki_parse_parameters(
	$context, $tree,
	array(
	    'equalities' => array(
		'label' => &$button_label,
		),
	    'long_flags' => array(
		'before' => array(&$mode, 'before'),
		'above' => array(&$mode, 'before'),
		'after' => array(&$mode, 'after'),
		'below' => array(&$mode, 'after'),
		'name' => &$is_name,
		'date' => &$is_date,
		'nest' => &$is_nest,
		'escape' => &$is_escape,
		'url' => &$is_allow_url,
		'japanese' => &$is_only_ja,
		'ordered' => &$is_ordered
		),
	    )
	);
    
    $form = dom_append_page_form($page, $dom,
				 array('option' => 'wiki_listadd',
				       'position' => $tree->position,
				       'length' => $tree->length,
				       'mode' => $mode,
				       'name' => $is_name ? 'true' : '',
				       'date' => $is_date ? 'true' : '',
				       'escape' => $is_escape ? 'true' : '',
				       'url' => $is_allow_url ? 'true' : '',
				       'japanese' => $is_only_ja ? 'true' : '',
				       'ordered' => $is_ordered ? 'true' : '',
				       'nest' => $is_nest ? 'true' : '',
				       'action' => 'write'), 'listadd');
    if($is_nest)
	dom_append_class($form, 'nest');
    dom_set_position($form, $tree);
				 
    $default_value = trim($target);
    dom_append_child($form, 'input', 'listadd_text',
		     array(
			 'name' => 'listadd_text',
			 'type' => 'text',
			 'value' => $default_value,
			 'required' => 'required',
			 'placeholder' => l('Content'),
			 ));
    
    if($is_name) {
	$user = auth_get_user();
	if(!empty($user))
	    $username = $user['name'];
	else
	    $username = '';

	dom_append_child($form, 'input', 'listadd_name',
			 array(
			     'name' => 'listadd_name',
			     'type' => 'text',
			     'value' => $username,
			     'placeholder' => l('Name'),
			     ));
    }

    $input = dom_append_child($form, 'input', '',
			      array('type' => 'submit', 'value' => $button_label));
    
}

$OPTIONS['wiki_listadd'] = array(
    'write' => 'wiki_option_listadd_write',
    );

function wiki_option_listadd_write(&$args, $dom) {
    return insert_write($args, $dom, 'listadd', 'wiki_option_listadd_write_value');
}

function wiki_option_listadd_write_value(&$page, $page_dom, &$contents, &$position, &$length, $input_values, $args) {
    global $WIKI_OPERATORS_LINE_HEAD;

    $value = default_value($args['listadd_text'], '');
    if(default_value($input_values['escape']) === 'true')
	$value = wiki_escape_source($value, true);

    if(!insert_write_check_value($value, $input_values))
	return false;

    if(!empty($input_values['name']) && isset($args['listadd_name']) && $args['listadd_name'] !== '') {
	$value .= ' -- ' . $args['listadd_name'];
	if(!empty($input_values['date']))
	    $value .= ' &time(new){' . time_format(time()) . '}';
    } else if(!empty($input_values['date']))
	$value .= ' -- &time(new){' . time_format(time()) . '}';

    if(isset($args['ordered']) && $args['ordered'] == 'true')
	$add_symbol = '+ ';
    else
	$add_symbol = '- ';

    if(!empty($input_values['nest']) &&
       isset($args['parent_twp']) && isset($args['parent_twl']) &&
       isset($args['parent_text']) && $args['parent_text'] !== '') {
	$parent_twp = (int)$args['parent_twp'];
	$parent_twl = (int)$args['parent_twl'];
	$li = dom_find_near_node($page_dom, $parent_twp, $parent_twl, false, 'li', '', array(), 
				    'wiki_option_listadd_write_value_start_text', $args['parent_text']);
	if($li === false) {
	    message_error(l('Page write failed.'));
	    return false;
	}
	$parent_position = dom_get_attribute($li, 'data-twp');
	$parent_length = dom_get_attribute($li, 'data-twl');

	$parent_text = substr($contents, $parent_position, $parent_length);
	$parent_text = preg_replace('/^{/', '', $parent_text);

	if(preg_match('/^(' . $WIKI_OPERATORS_LINE_HEAD['list']['symbol'] . ')/', $parent_text, $matches))
	    $value = $matches[1] . $add_symbol . $value;
	else
	    $value = $add_symbol . $value;

	$position = $parent_position + $parent_length;
	if($input_values['mode'] !== 'after') {
	    $sections = dom_get_nodes_by_xpath($li, './/section[@class="list"]');
	    foreach($sections as $section) {
		$section->parentNode->removeChild($section);
	    }
	    $last_li = dom_get_node_by_xpath($li, './/li[@data-twp and @data-twl][last()]');
	    if(!empty($last_li)) {
		$position = (int)dom_get_attribute($last_li, 'data-twp') +
		    (int)dom_get_attribute($last_li, 'data-twl');
	    }
	}
    } else {
	$value = $add_symbol . $value;
    }

    $value .= "\n";
    if($position > 0 && $position - 1 <  $page['contents_size'] &&
       $contents[$position - 1] !== "\n") {
	$value = "\n" . $value;
	if($position < $page['contents_size'] &&
	   $contents[$position] === "\n")
	    $value = substr($value, 0, -1);
    }
    
    return $value;
}

function wiki_option_listadd_write_value_start_text($element, $text) {
    $a = str_replace(array("\r","\n"), '', $element->textContent);
    $b = str_replace(array("\r","\n"), '', $text);
    return strpos($a, $b) === 0;
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[リスト挿入フォーム:../../Function/listadd]]|&&listadd(name, date, before, after, escape);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|adding list item form|&&listadd(name, date, before, after, escape);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/listadd
&title &&listadd

*概要
 &listadd(name, date, before, after, escape, url, japanese, nest, ordered, label=ラベル);
ページに箇条書きの項目を追加するためのフォームを表示します．

*パラメータ
:name|名前を入力する欄も表示します．
:date|日時もいっしょに追加します．
:before|フォームの前に項目を追加します．デフォルトで指定されています．aboveと書いても同様です．
:after|フォームの後に項目を追加します．belowと書いても同様です．
:escape|フォームに入力された項目のWiki記法を無効化します．
:{url|URLの入力を許可します．デフォルトで指定されています．
無効にする場合は!urlと指定します．}
:japanese|日本語を含む入力のみ許可します．
:{nest|親を指定して入れ子で項目を追加できるようにします．デフォルトで指定されています．
無効にする場合は!nestと指定します．}
:ordered|順序付き(番号付き)の箇条書きにします．
:label=ラベル|ボタンに表示される文字を指定します．
*対象
対象はフォームの初期値として解釈されます．

*例

&wikiexample{
-listaddの使い方が分かりました． -- taro &time(new){2012-07-28 15:59:31}
&listadd(name, date, before);
}
*/

$WIKI_ALIASES['comment'] = '&listadd(name,date,<PARAMS>){<TARGET>}';
/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[コメント:../../Function/comment]]|&&comment(before, after, escape);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|comment form|&&comment(before, after, escape);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/comment
&title &&comment

*概要
 &comment(before, after, escape, url, japanese, nest, ordered);
ページに簡単なコメントをしてもらうフォームを表示します．

*パラメータ
:before|フォームの前にコメントを追加します．デフォルトで指定されています．aboveと書いても同様です．
:after|フォームの後にコメントを追加します．belowと書いても同様です．
:escape|フォームに入力されたコメントのWiki記法を無効化します．
:{url|URLの入力を許可します．デフォルトで指定されています．
無効にする場合は!urlと指定します．}
:japanese|日本語を含む入力のみ許可します．
:{nest|既存のコメントに返信できるようにします．デフォルトで指定されています．
無効にする場合は!nestと指定します．}
:ordered|順序付き(番号付き)の箇条書きにします．

*対象
対象はフォームの初期値として解釈されます．

*例

&wikiexample{
-commentの使い方が分かりました． -- toro &time(new){2012-07-28 15:59:31}
&comment;
}

*エイリアス
&&commentは以下のように定義されたエイリアスです．
 `$WIKI_ALIASES['comment']`

*/

?>
