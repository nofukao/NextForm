<?php
$WIKI_OPERATORS_FUNCTION['insert'] = true;
function wiki_operator_insert(&$context, $dom, $tree) {
    $target = wiki_get_target_string($tree);
    $page = &$context['page'];

    $mode = 'before';
    $hint = '';
    $button_label = false;
    $is_escape = false;
    $is_allow_url = true;
    $is_only_ja = false;
    wiki_parse_parameters(
	$context, $tree,
	array(
	    'long_flags' => array(
		'before' => array(&$mode, 'before'),
		'above' => array(&$mode, 'before'),
		'after' => array(&$mode, 'after'),
		'below' => array(&$mode, 'after'),
		'once' => array(&$mode, 'once'),
		'replace' => array(&$mode, 'once'),
		'escape' => &$is_escape,
		'url' => &$is_allow_url,
		'japanese' => &$is_only_ja
		),
	    'equalities' => array(
		'hint' => &$hint,
		'label' => &$button_label,
		)
	    )
	);

    $form = dom_append_page_form(
	$page, $dom,
	array('option' => 'wiki_insert',
	      'position' => $tree->position,
	      'length' => $tree->length,
	      'mode' => $mode,
	      'escape' => $is_escape ? 'true' : '',
	      'url' => $is_allow_url ? 'true' : '',
	      'japanese' => $is_only_ja ? 'true' : '',
	      'action' => 'write'), 'insert');
    dom_set_position($form, $tree);

    $default_value = trim($target);
    $attrs = array('type' => 'text', 'name' => 'value', 'value' => $default_value);
    if(!empty($hint))
	$attrs['placeholder'] = $hint;
    dom_append_child($form, 'input', 'insert', $attrs);
    if($button_label === false)
	$button_label = $mode == 'once' ? l('Change') : l('Add');
    dom_append_child($form, 'input', '',
		     array('type' => 'submit', 'value' => $button_label));
}

$OPTIONS['wiki_insert'] = array(
    'write' => 'wiki_option_insert_write',
    );

function wiki_option_insert_write(&$args, $dom) {
    return insert_write($args, $dom, 'insert', 'wiki_option_insert_write_value');
}

function wiki_option_insert_write_value(&$page, $page_dom, &$contents, $position, $length, $input_values, $args) {
    $value = default_value($args['value'], '');
    if(default_value($input_values['mode']) === 'after')
	$value = "\n" . $value;
    else 
	$value .= "\n";
    if(default_value($input_values['escape']) === 'true')
	$value = wiki_escape_source($value, true);

    if(!insert_write_check_value($value, $input_values))
	return false;

    return $value;
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/FunctionAdvanced
|[[テキスト挿入フォーム:../../Function/insert]]|&&insert(before, after, once, escape);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/FunctionAdvanced
|adding text form|&&insert(before, after, once, escape);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/insert
&title &&insert

*概要
 &insert(before, after, once, escape, url, japanese, hint=入力のヒント, label=ラベル);
ページにテキストを挿入するためのフォームを表示します．
ページに入力してほしい項目がある場合，フォームを表示することで入力を明示的に催促できます．

*パラメータ
:before|フォームの前にテキストを挿入します．デフォルトで指定されています．aboveと書いても同様です．
:after|フォームの後にテキストを挿入します．belowと書いても同様です．
:once|フォームの位置にテキストを挿入し，フォームは削除されます．replaceと書いても同様です．
:escape|フォームに入力されたテキストのWiki記法を無効化します．
:{url|URLの入力を許可します．デフォルトで指定されています．
無効にする場合は!urlと指定します．}
:japanese|日本語を含む入力のみ許可します．
:hint=入力のヒント|このフォームに何を入力するかのヒントを指定します．(placeholder属性を使用するためIEでは効果がありません．)
:label=ラベル|ボタンに表示される文字を指定します．

*対象
対象はフォームの初期値として解釈されます．

*例

&wikiexample{
&insert(after){ 初期値 }
}
*/

?>
