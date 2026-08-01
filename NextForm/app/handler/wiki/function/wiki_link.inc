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

    foreach($values as $value) {
	$encoded_value = preg_replace_callback('/([\x7f-\xff]+)/', 'wiki_operator_link_url_encode_replace', $value);
	if(preg_match('/' . WIKI_URI_REGEX . '/', $encoded_value)){
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

*エイリアス
&&directは以下のように定義されたエイリアスです．
 `$WIKI_ALIASES['direct']`

*/
?>
