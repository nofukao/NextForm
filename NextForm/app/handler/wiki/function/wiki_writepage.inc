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


/**
@manual $manualpage$/en/Wiki/Function/writepage
&title &&writepage

*Overview
 &writepage([[page name]], move, label=label, ask){template}
Shows a form for editing the fields pulled out of the page named, using a template.
When the page named does not match the template, its contents are rewritten into
the template.

*Parameters
:&[&[page name&]&]|edit each field of the page named.
:{move|go to the edited page once the editing is done.
Set by default. Write !move to turn it off.}
:label=label|the text shown on the button.
:{ask|show a form for entering the fields.
Set by default. Write !ask to turn it off.
With !ask, the page is saved using the default values of the template.}
:{placeholder|enable pulling fields out of the template in the form @field@ (see below).
Set by default. Write !placeholder to turn it off.}

*Target
Write the template as the target.
Write the contents of the page to be edited into the template as they are.
The parts of those contents that you want to edit, though, are written as follows.
Writing them in the form
 @field@
makes them entries in the form, replaced by the text entered when the page is created.
The form
 @field=default@
can also be used, and the default given is put into the form.
Ending the field name with a # symbol, as in
 @field#@
lets several lines be entered for that field.
Ending the field name with a * symbol, as in
 @field*@
makes it a required field.
Ending the field name with a ' symbol, as in
 @field'@
makes Wiki notation entered into that field be ignored.
Ending the field name with a / symbol and listing choices after it, as in
 @field/choice1/choice2/...@
turns that field into a pull down menu to choose from the choices given.
Writing a symbol of [[line head notation:../../LineHead]] before the field name, as in
 @-field@
adds that symbol to the start of the lines entered into the form.
They can be combined, as in
 @-field#'*=default@

Writing &$user&$ as the default is replaced by the name of the user who posted the
form, and &$time&$ by the time the form was posted.

Writing &$LAST&$ in the template uses the template used just before.

*Example
 &writepage([[./Articles]], !move) {
 *@title@
 :author|@name=$user$@
 @ body#=the body goes here@
 }
*/

/**
@manual $manualpage$/en/Wiki/Function/newpage
&title &&newpage

*Overview
 &newpage([[page name]], move, pagename, sequence, label=label, ask){template}
Shows a form for creating a new child page.
Given a template, a page can be created with contents based on it.

*Parameters
:&[&[page name&]&]|create a child page of the page named. Left out, the current page is used.
:{move|go to the edited page once the editing is done.
Set by default. Write !move to turn it off.}
:{pagename|show a form for entering the page name.
Set by default. Write !pagename to turn it off.}
:{sequence|when the name of the new child page is left out, use a running number as the
page name.
Set by default. Write !sequence to turn it off.
Giving a number, as in sequence=3, sets how many digits the page name has, but take
care: 001 and 0001 are different pages.
}
:label=label|the text shown on the button.
:{ask|show a form for entering the fields.
Set by default. Write !ask to turn it off.
With !ask, the page is created using the default values of the template.}
:{placeholder|enable pulling fields out of the template in the form @field@ (see below).
Set by default. Write !placeholder to turn it off.}

*Target
Write the template as the target.
Write the contents of the page to be created into the template in Wiki notation,
as they are.
Writing, in the template, the form
 @field@
makes it an entry in the form, replaced by the text entered when the page is created.
The form
 @field=default@
can also be used, and the default given is put into the form.
Ending the field name with a # symbol, as in
 @field#@
lets several lines be entered for that field.
Ending the field name with a * symbol, as in
 @field*@
makes it a required field.
Ending the field name with a ' symbol, as in
 @field'@
makes Wiki notation entered into that field be ignored.
Ending the field name with a / symbol and listing choices after it, as in
 @field/choice1/choice2/...@
turns that field into a pull down menu to choose from the choices given.
Writing a symbol of [[line head notation:../../LineHead]] before the field name, as in
 @-field@
adds that symbol to the start of the lines entered into the form.
They can be combined, as in
 @-field#'*=default@

Writing &$user&$ in the template is replaced by the name of the user who posted the
form, and &$time&$ by the time the form was posted.

*Example
 &newpage() {
 *@title@
 :author|@name=$user$@
 @ body#=the body goes here@
 }

*Alias
&&newpage is an alias defined as follows.
 `$WIKI_ALIASES['newpage']`
*/
?>
