<?php
$WIKI_OPERATORS_FUNCTION['writepage'] = true;
function wiki_operator_writepage(&$context, $dom, $tree) {
    if(!empty($context['texts_only']))
	return;

    $template = template_get($context, wiki_get_target_string($tree));

    $is_pagename = false;
    $is_sequence = false;
    $sequence_digits = 4;
    $is_move = true;
    $is_ask = true;
    $is_placeholder = true;
    $target_pagename = false;
    $button_label = false;
    wiki_parse_parameters(
	$context, $tree,
	array(
	    'pagename' => &$target_pagename,
	    'equalities' => array(
		'label' => &$button_label,
		'sequence' => &$sequence_digits,
		),
	    'long_flags' => array(
		'pagename' => &$is_pagename,
		'sequence' => &$is_sequence,
		'move' => &$is_move,
		'ask' => &$is_ask,
		'placeholder' => &$is_placeholder,
		),
	    )
	);
    if($target_pagename === false) {
	if($is_pagename || $is_sequence) {
	    $target_pagename = $context['page']['name'];
	} else {
	    dom_append_error($dom, l('&writepage pagename not specified'));
	    return;
	}
    }

    if($is_move)
	$redirect = url_make($target_pagename);
    else
	$redirect = url_make($context['page']['name']);

    $form = templateedit_append_form(
	$dom, $template, $target_pagename,
	$redirect, $is_move, $is_pagename, $is_sequence, $sequence_digits,
	$is_ask, $is_placeholder, $button_label);
    dom_set_position($form, $tree);
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/FunctionAdvanced
|[[ページ編集フォーム:../../Function/writepage]]|&&writepage(&[&[ページ名&]&], move){テンプレート}|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/FunctionAdvanced
|page editing form|&&writepage(&[&[page name&]&], move){template}|
*/
/**
@manual $manualpage$/ja/Wiki/Function/writepage
&title &&writepage

*概要
 &writepage([[ページ名]], move, label=ラベル, ask){テンプレート}
指定したページからテンプレートを使って抜き出した項目を編集するフォームを表示します．
指定したページがテンプレートと一致しない場合は指定したページの内容をテンプレートに書き換えます．

*パラメータ
:&[&[ページ名&]&]|指定したページの各項目に対する編集を行います．
:{move|編集が完了した後に編集したページへ移動します．
デフォルトで指定されています．無効にするには!moveと指定して下さい．}
:label=ラベル|ボタンに表示される文字を指定します．
:{ask|項目を入力するためのフォームを表示します．
デフォルトで指定されています．無効にするには!askと指定して下さい．
!askを指定すると，テンプレートのデフォルト値を使用してページの保存を行います．}
:{placeholder|テンプレートで@項目名@のような形式で項目を抽出する機能(下記参照)を有効にします．
デフォルトで指定されています．無効にするには!placeholderと指定して下さい．}

*対象
対象にはテンプレートを書きます．
テンプレートには編集対象となるページの内容をそのまま書きます．
ただし，ページの内容の中で編集したい項目に相当する部分は以下のように書きます．
 @項目名@
という形式で書くと，フォームの入力項目になり，ページを作成する際に入力された文字に置き換わります．
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

また，初期値で&$user&$と書いた場合はフォームで投稿したユーザ名，
&$time&$と書いた場合はフォームで投稿を行った日時に置き換わります．

テンプレートに&$LAST&$と指定すると直前に使用したテンプレートを使用することができます．

*例
 &writepage([[./記事]], !move) {
 *@タイトル@
 :投稿者|@名前=$user$@
 @ 本文#=ここに本文@
 }
*/

$WIKI_ALIASES['newpage'] = '&writepage(pagename, sequence, <PARAMS>){<TARGET>}';
/**
@manual $manualpage$/ja/Wiki/CheatSheet/FunctionAdvanced
|[[新規ページ作成フォーム:../../Function/newpage]]|&&newpage(&[&[ページ名&]&], pagename, sequence){テンプレート}|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/FunctionAdvanced
|create new page form|&&newpage(&[&[page name&]&], pagename, sequence){template}|
*/
/**
@manual $manualpage$/ja/Wiki/Function/newpage
&title &&newpage

*概要
 &newpage([[ページ名]], move, pagename, sequence, label=ラベル, ask){テンプレート}
新しい子ページを作成するフォームを表示します．
テンプレートを指定するとテンプレートをもとにした内容のページが作成できます．

*パラメータ
:&[&[ページ名&]&]|指定したページの子ページを作成します．省略した場合は現在のページになります．
:{move|編集が完了した後に編集したページへ移動します．
デフォルトで指定されています．無効にするには!moveと指定して下さい．}
:{pagename|ページ名を入力するフォームを表示します．
デフォルトで指定されています．無効にするには!pagenameと指定して下さい．}
:{sequence|新規子ページ名が省略された際にページ名を連番の数字にします．
デフォルトで指定されています．無効にするには!sequenceと指定して下さい．
sequence=3のように数字を指定するとページ名の桁数を指定できますが，
例えば001と0001は異なるページになりますので注意して使用して下さい．
}
:label=ラベル|ボタンに表示される文字を指定します．
:{ask|項目を入力するためのフォームを表示します．
デフォルトで指定されています．無効にするには!askと指定して下さい．
!askを指定すると，テンプレートのデフォルト値を使用してページの作成を行います．}
:{placeholder|テンプレートで@項目名@のような形式で項目を抽出する機能(下記参照)を有効にします．
デフォルトで指定されています．無効にするには!placeholderと指定して下さい．}

*対象
対象にはテンプレートを書きます．
テンプレートには新しく作成するページの内容をそのままWiki記法で書きます．
テンプレートの中で
 @項目名@
という形式で書くと，フォームの入力項目になり，ページを作成する際に入力された文字に置き換わります．
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

*例
 &newpage() {
 *@タイトル@
 :投稿者|@名前=$user$@
 @ 本文#=ここに本文@
 }

*エイリアス
&&newpageは以下のように定義されたエイリアスです．
 `$WIKI_ALIASES['newpage']`
*/

?>
