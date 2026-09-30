<?php
$WIKI_OPERATORS_FUNCTION['link'] = true;

function wiki_operator_link(&$context, $dom, $tree) {
    global $wiki_operator_link_url_encode_replace_charset;
    global $wiki_operator_link_url_encode_replace_need_convert;

    $values = array();
    $pagename = $context['page']['name'];
    $is_open_new = false;
    $uri = false;
    $charset = 'UTF-8';
    wiki_parse_parameters($context, $tree, array(
			      'pagename' => &$pagename,
			      'uri' => &$uri,
			      'long_flags' => array(
				  'open' => &$is_open_new
				  ),
			      'equalities' => array(
				  'charset' => &$charset,
				  ),
			      'values' => &$values,
			      ));
    $page_action = array(
	'edit' => array(
	    'name' => 'Editing $pagename$',
	    'args' => array('action' => 'edit'),
	    ),
	'raw' => array(
	    'name' => '$pagename$',
	    'args' => array('action' => 'raw'),
	    ),
	'tag' => array('name' => 'Tags for $pagename$'),
	'history' => array('name' => 'History of $pagename$'),
	'lock' => array('name' => 'Lock $pagename$'),
	'unlock' => array('name' => 'Unlock $pagename$'),
	'delete' => array('name' => 'Delete $pagename$'),
	'rename' => array('name' => 'Rename $pagename$'),
	'attach' => array('name' => 'Attach files in $pagename$'),
	'meta' => array('name' => 'Meta info of $pagename$'),
	'debuginfo' => array('name' => 'Debug info of $pagename$'),
	);
    $site_action = array(
	'newpage' => array('name' => 'New page'),
	'allpage' => array('name' => 'All pages'),
	'alltag' => array('name' => 'All tags'),
	'deletedpage' => array('name' => 'Deleted pages'),
	'config' => array('name' => 'Preferences'),
	);

    $mode = '';

    $wiki_operator_link_url_encode_replace_charset = 'UTF-8';
    $wiki_operator_link_url_encode_replace_need_convert = false;
    if(function_exists('mb_convert_encoding') && function_exists('mb_list_encodings') &&
       in_array($charset, mb_list_encodings()) && $charset !== 'UTF-8') {
	$wiki_operator_link_url_encode_replace_charset = $charset;
	$wiki_operator_link_url_encode_replace_need_convert = true;
    }

    /*
     * 値の**全体**が URL のときだけ URL として扱う。
     *
     * WIKI_URI_REGEX はアンカーされていない。この定数は字句解析の
     * トークン定義 (handler/wiki.inc) と、投稿に URL が含まれるかの検査
     * (option/insert.inc) でも使われており、どちらも「文中のどこかに
     * URL があるか」を見るものなので、**定数自体をアンカーしてはいけない**。
     * ここは「この値は URL か」を判定する場所なので、この 1 箇所で挟む。
     *
     * 挟まないと、一致した部分ではなく値全体が href になる。
     * &link(abchttp://example.com/); が href="abchttp://example.com/" に
     * なっていた。javascript: は : がパラメータ解析で落ちるため実際には
     * 注入できなかったが、スキームを組み立てられる形を残す理由が無い。
     * 挟むことで、通るスキームは定数が並べている http/https/ftp/file だけになる。
     */
    foreach($values as $value) {
	$encoded_value = preg_replace_callback('/([\x7f-\xff]+)/', 'wiki_operator_link_url_encode_replace', $value);
	if(preg_match('/\A' . WIKI_URI_REGEX . '\z/', $encoded_value)){
	    $uri = $encoded_value;
	}
    }

    if($uri !== false) {
	$a = dom_append_link($dom, $uri);
	dom_append_class($a, 'uri external');
    } else {
	if(!empty($values))
	    $mode = array_pop($values);
	if(isset($page_action[$mode])) {
	    $action = &$page_action[$mode];
	} else if(isset($site_action[$mode])) {
	    $action = &$site_action[$mode];
	    $pagename = '';
	}
	$args = array();
	if(!empty($action['args']))
	    $args = $action['args'];
	else if(isset($action))
	    $args = array('option' => $mode);

	if($pagename == '') {
	    $a = dom_append_link($dom, url_make($pagename, $args));
	} else {
	    $a = dom_append_empty_page_link($dom, $pagename, '', url_make($pagename, $args));
	}
    }
    
    if($is_open_new)
	dom_set_attribute($a, 'target', '_blank');

    if(wiki_tokens_is_whitespace($tree->target)) {
	$name = $uri !== false ? $uri : $pagename;
	if(isset($action['name']))
	    $name = lf($action['name'], array('pagename' => $pagename));
	dom_append_text($a, $name);
    } else {
	wiki_convert_from_tokens($context, $a, $tree->target);
    }
}

function wiki_operator_link_url_encode_replace($matches) {
    global $wiki_operator_link_url_encode_replace_charset;
    global $wiki_operator_link_url_encode_replace_need_convert;
    if($wiki_operator_link_url_encode_replace_need_convert)
	return urlencode(mb_convert_encoding(
			     $matches[1],
			     $wiki_operator_link_url_encode_replace_charset,
			     'UTF-8'));
    else
	return urlencode($matches[1]);
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[リンク(詳細設定可能):../../Function/link]]|&&link(&[&[ページ名&]&], edit); or &&link(URL, open);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|functional link|&&link(&[&[page name&]&], edit){link words}|
*/
/**
@manual $manualpage$/ja/Wiki/Function/link

&title &&link

*概要
 &link([[ページ名]], edit, open){ リンクの表示 }
 &link(URL ,open){ リンクの表示 }
いろいろなリンクを作成します．
例えば，指定したページやURLを新しいウィンドウ(タブ)で開くリンクや，
ページの編集画面へのリンクを作成できます．

*パラメータ
:&[&[ページ名&]&]|指定したページ名へのリンクになります．省略した場合は現在のページになります．
:open|新しいウィンドウ(タブ)で開くリンクになります．
:{URL|パラメータにURLを記述した場合はそのURLへのリンクになります
(下記のedit,tag,..の指定は無効になります)．
''URLに続いて,(カンマ)を書く場合はスペースを入れて下さい．そうしないとカンマもURLとして認識されます．''
このファンクションに限り，英数字以外の文字をURLとして使用できます．
}
:edit|編集画面へのリンクにします．
:tag|タグ設定画面へのリンクにします．
:history|履歴へのリンクにします．
:lock|ロック画面へのリンクにします．
:unlock|アンロック画面へのリンクにします．
:delete|削除画面へのリンクにします．
:rename|リネーム画面へのリンクにします．
:attach|添付画面へのリンクにします．
:meta|メタ情報へのリンクにします．
:debuginfo|デバッグ情報画面へのリンクにします．
:newpage|新規ページ作成画面へのリンクにします．
:allpage|全ページ一覧へのリンクにします
:alltag|タグ一覧へのリンクにします．
:deletedpage|削除されたページ一覧へのリンクにします．
:config|個人設定へのリンクにします．
:{charset=文字コード|
英数字以外の文字をURLにする場合に使用する文字コードを指定します．
指定されない場合はUTF-8が使用されます．SJISやEUC-JP等が指定できます．
}

*対象
リンクの表示を書きます．省略した場合は自動で設定されます．

*例

&wikiexample{
&link(edit){ここ}をクリックしてこのページを編集

&link([[Side]],edit,open){新しいタブでメニューバーを編集する}

&link(http://www.google.com, open);
}
*/

$WIKI_ALIASES['direct'] = '&link(raw,<PARAMS>){<TARGET>}';

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[画像やファイルへ直接リンク:../../Function/direct]]|&&direct(&[&[ページ名&]&]){リンクの表示}|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|direct link to image or file|&&direct(&[&[page name&]&]){link words}|
*/
/**
@manual $manualpage$/ja/Wiki/Function/direct

&title &&direct

*概要
 &direct([[ページ名]]){ リンクの表示 }
画像やファイルへ直接リンクを張ります．通常の&[&[と&]&]によるリンクの場合でも
画像やファイルを表示する画面へのリンクになりますが，
この画面には編集や削除等の操作ができるよう様々な情報が同時に表示されます．
このような画面ではなく画像だけまたはファイルのダウンロードに直接リンクを張りたい場合に&&directを使用します．


*パラメータ
:&[&[ページ名&]&]|リンク先になるページ名を指定します．

*対象
リンクの表示を書きます．省略した場合はページ名になります．

*例
 [[./some-image.png]] //通常のリンク
 &direct([[./some-image.png]]) //画像へ直接リンク
 &direct([[./some-archive.zip]]) //直接ダウンロードのできるリンク
 &direct([[./game.html]]){遊ぶ} //添付したHTMLを動かすリンク(サイトの設定が要る)

添付したHTMLを動かす設定は[[HTMLファイルの直接実行:../../../HtmlFile]]にあります．

*エイリアス
&&directは以下のように定義されたエイリアスです．
 `$WIKI_ALIASES['direct']`

*/

/**
@manual $manualpage$/en/Wiki/Function/direct

&title &&direct

*Overview
 &direct([[page name]]){ what the link shows }
Makes a link straight to an image or a file. A normal link with &[&[ and &]&] also
leads to a screen that shows the image or file, but that screen shows various
information at the same time, so that it can be edited, deleted and so on.
Use &&direct when you want to link straight to the image alone, or to the download
of the file, instead of that screen.

*Parameters
:&[&[page name&]&]|the name of the page the link points at.

*Target
Write what the link shows. Left out, it is the page name.

*Example
 [[./some-image.png]] //a normal link
 &direct([[./some-image.png]]) //a link straight to the image
 &direct([[./some-archive.zip]]) //a link that downloads the file directly
 &direct([[./game.html]]){Play} //a link that runs an attached HTML file (needs a site setting)

The setting for running attached HTML files is described in
[[Running HTML files directly:../../../HtmlFile]].

*Alias
&&direct is an alias defined as follows.
 `$WIKI_ALIASES['direct']`

*/

/**
@manual $manualpage$/en/Wiki/Function/link

&title &&link

*Overview
 &link([[page name]], edit, open){ what the link shows }
 &link(URL ,open){ what the link shows }
Builds links of various kinds.
For example, a link that opens the page or URL named in a new window (tab), or a link
to the editing screen of a page.

*Parameters
:&[&[page name&]&]|a link to the page name given. Left out, it is the current page.
:open|a link that opens in a new window (tab).
:{URL|writing a URL as the parameter makes it a link to that URL
(the edit, tag and other settings below then have no effect).
''When you write a , (comma) after a URL, put a space before it.
Otherwise the comma is read as part of the URL.''
In this function alone, characters other than letters and digits can be used in
the URL.
}
:edit|make it a link to the editing screen.
:tag|make it a link to the tag setting screen.
:history|make it a link to the history.
:lock|make it a link to the lock screen.
:unlock|make it a link to the unlock screen.
:delete|make it a link to the delete screen.
:rename|make it a link to the rename screen.
:attach|make it a link to the attach screen.
:meta|make it a link to the metadata.
:debuginfo|make it a link to the debug information screen.
:newpage|make it a link to the new page screen.
:allpage|make it a link to the list of all pages.
:alltag|make it a link to the list of tags.
:deletedpage|make it a link to the list of deleted pages.
:config|make it a link to the personal settings.
:{charset=character encoding|
set the character encoding used when characters other than letters and digits go into
the URL. Left out, UTF-8 is used. SJIS, EUC-JP and others can be given.
}

*Target
Write what the link shows. Left out, it is set automatically.

*Example

&wikiexample{
Click &link(edit){here} to edit this page

&link([[Side]],edit,open){edit the menu bar in a new tab}

&link(http://www.google.com, open);
}
*/
?>
