<?php
$WIKI_OPERATORS_FUNCTION['inserttemplate'] = true;
function wiki_operator_inserttemplate(&$context, $dom, $tree) {
    if(!empty($context['texts_only']))
	return;

    $template = template_get($context, wiki_get_target_string($tree));

    $page = &$context['page'];

    $mode = 'before';
    $is_allow_url = true;
    $is_only_ja = false;
    $button_label = false;
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
		'url' => &$is_allow_url,
		'japanese' => &$is_only_ja
		),
	    'equalities' => array(
		'label' => &$button_label,
		)
	    )
	);

    $form = dom_append_page_form(
	$page, $dom,
	array('option' => 'wiki_inserttemplate',
	      'position' => $tree->position,
	      'length' => $tree->length,
	      'mode' => $mode,
	      'url' => $is_allow_url ? 'true' : '',
	      'japanese' => $is_only_ja ? 'true' : '',
	      'template' => $template,
	      'action' => 'write'), 'templateinput');
    dom_set_position($form, $tree);

    $template = template_replace_standard_variables($template);
    $placeholders = template_get_placeholders($template);

    template_append_placeholders_input($form, $placeholders);

    if($button_label === false)
	$button_label = $mode == 'once' ? l('Change') : l('Add');
    
    dom_append_child($form, 'input', '',
		     array('type' => 'submit', 'value' => $button_label));
}

$OPTIONS['wiki_inserttemplate'] = array(
    'write' => 'wiki_option_inserttemplate_write');

function wiki_option_inserttemplate_write(&$args, $dom) {
    return insert_write($args, $dom, 'templateinput', 'wiki_option_inserttemplate_write_value');
}

function wiki_option_inserttemplate_write_value(&$page, $page_dom, &$contents, $position, $length, $input_values, $args) {
    if($args['template'] !== $input_values['template']) {
	message_error(l('Confliction detected.'));
	message_error(l('Page write failed.'));
	return false;
    }
       
    $value = template_replace_standard_variables($args['template']);
    $value = template_replace_placeholders_by_input($value, $args);
    if($value === false)
	return false;

    if(default_value($input_values['mode']) === 'after')
	$value = "\n" . $value;
    else 
	$value .= "\n";

    $index = 0;
    $input_text = '';
    while(isset($args['term' . $index])) {
	$input_text .= $args['term' . $index];
	$index++;
    }
    if(!insert_write_check_value($input_text, $input_values))
	return false;

    return $value;
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/FunctionAdvanced
|[[テンプレートの挿入:../../Function/inserttemplate]]|&&inserttemplate(before, after, once){テンプレート}|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/FunctionAdvanced
|insert template|&&inserttemplate(before, after, once){template}|
*/
/**
@manual $manualpage$/ja/Wiki/Function/inserttemplate
&title &&inserttemplate

*概要
 &inserttemplate(before, after, once, url, japanese, label=ラベル){テンプレート}
テンプレートを定義し，そのテンプレートをページに挿入するフォームを作成します．
このファンクションを利用してお好みの形式の掲示板を作成することもできます．

*パラメータ
:before|フォームの前にテンプレートを挿入します．デフォルトで指定されています．aboveと書いても同様です．
:after|フォームの後にテンプレートを挿入します．belowと書いても同様です．
:once|フォームの位置にテンプレートを挿入し，フォームは削除されます．replaceと書いても同様です．
:{url|URLの入力を許可します．デフォルトで指定されています．
無効にする場合は!urlと指定します．}
:japanese|日本語を含む入力のみ許可します．
:label=ラベル|ボタンに表示される文字を指定します．

*対象
対象にはテンプレートを書きます．
テンプレートにはフォームによって挿入したい内容をそのままWiki記法で書きます．
テンプレートの中で
 @項目名@
という形式で書くと，フォームの入力項目になり，テンプレートを挿入する際に入力された文字に置き換わります．
 @項目名=初期値@
という形式も使用でき，この場合はフォームに指定された初期値が埋め込まれます．
 @項目名#@
のように項目名の最後に#記号をつけるとその項目は複数行の内容を入力することができるようになります．
 @項目名*@
のように項目名の最後に*記号をつけると必須項目になります．
 @項目名'@
のように項目名の最後に'記号をつけるとその項目に入力されたWiki記法は無視されるようになります．
 @項目名/選択肢1/選択肢2/...@
のように項目名の最後に/記号をつけ，
続けて選択肢を列挙するとその項目はプルダウンメニューなり指定した選択肢から選ぶようになります．
 @-項目名@
のように項目名の前に[[行頭記法:../../LineHead]]の記号を書くとフォームに入力された内容の行頭に指定した記号が追加されます．
 @-項目名#'*=初期値@
のように組み合わせて使用することもできます．

また，テンプレート内で&$user&$と書いた場合はフォームで投稿したユーザ名，
&$time&$と書いた場合はフォームで投稿を行った日時に置き換わります．

テンプレートに&$LAST&$と指定すると直前に使用したテンプレートを使用することができます．

*例
 //掲示板風テンプレート
 //(実際にSandBox等のどこかのページにコピペして使用してみると楽に理解できるはずです)
 
 &inserttemplate() {
 *@タイトル@
 :投稿日時|&time(new){$time$}
 :投稿者|@名前=$user$@
 @ 本文#=ここに本文@
 }
*/

$WIKI_ALIASES['bbs'] = 
'&inserttemplate(label='.l('Post').',<PARAMS>){
+{'.l('Poster').': @'.l('Poster').'=$user$@ '.l('Posted').': &time{$time$}
@ '.l('Text').'#*@
}}';
/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[掲示板:../../Function/bbs]]|&&bbs;|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|bulletin board|&&bbs;|
*/
/**
@manual $manualpage$/ja/Wiki/Function/bbs
&title &&bbs

*概要
 &bbs(url, japanese);
掲示板の投稿フォームを表示します．

*パラメータ
:{url|URLの入力を許可します．デフォルトで指定されています．
無効にする場合は!urlと指定します．}
:japanese|日本語を含む入力のみ許可します．

*対象
対象は指定しません．

*例

&wikiexample{
&bbs;
}

*エイリアス
&&bbsは以下のように定義されたエイリアスです．
&pre{`$WIKI_ALIASES['bbs']`}

*/
?>
