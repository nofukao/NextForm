<?php
$WIKI_OPERATORS_FUNCTION['pages'] = true;
function wiki_operator_pages(&$context, $dom, $tree) {
    if(!empty($context['texts_only']))
	return;

    $list_options = array('context' => &$context, 'showhidden' => true);
    $find_condition = array('is_ignore_system_page' => true);
    $pagename = $context['page']['name'];
    wiki_operator_pages_parse_parameters($context, $tree, $pagename, $find_condition, $list_options, $values);

    $template = template_get($context, wiki_get_target_string($tree));
    if($template !== '')
	$list_options['template'] = $template;

    if(isset($list_options['show']) && $list_options['show'] === 'contents' &&
       isset($context['page']['in_pages_list_contents'])) {
	//dom_append_error($dom, l('Cannot use &pages(contents) in other &pages(contents).'));
	return;
    }
    $context['page']['depend_pages_unknown'] = true;

    $pages = page_find($pagename, $find_condition);
    $list_options['base_pagename'] = $pagename;
    $section = pagelist_append($dom, $pages, $list_options, page_args($context['page']));
    if(!empty($section))
	dom_remove_empty($section);
}

function wiki_operator_pages_parse_parameters(&$context, $tree, &$pagename, &$find_condition, &$list_options, &$arg_values) {
    $find_condition['is_pagename_only'] = true;
    $name_condition = array();
    $type_condition = array();
    $tag_condition = array();
    $values = array();
    wiki_parse_parameters(
	$context, $tree,
	array(
	    'pagename' => &$pagename,
	    'number' => &$list_options['limit'],
	    'values' => &$values,
	    'equalities' => array(
		'name' => &$name_condition,
		'type' => &$type_condition,
		'tag' => &$tag_condition,
		'tags' => &$tag_condition, // deprecated

		'sort' => &$find_condition['sort'],
		'limit' => &$list_options['limit'],
		'each' => &$list_options['each'],
		'fields' => &$list_options['fields'],
		'header' => &$list_options['header'],
		'footer' => &$list_options['footer'],
		'show' => &$list_options['show'],
		),
	    'long_flags' => array(
		'showhidden' => &$list_options['showhidden'],
		'reverse' => &$find_condition['reverse'],
		'children' => &$find_condition['is_children_only'],
		'withsystem' => array(&$find_condition['is_ignore_system_page'], false, true),
		),
	    'short_flags' => array(
		'l' => &$list_options['long'],
		'S' => array(&$find_condition['sort'], 'size'),
		'r' => &$find_condition['reverse'],
		'd' => &$find_condition['is_children_only'],
		't' => array(&$find_condition['sort'], 'mtime'),
		'a' => array(&$find_condition['is_ignore_system_page'], false, true),
		),
	    )
	);
    if(!empty($name_condition))
	$find_condition['names'] = array_map('string_to_list', $name_condition);
    if(!empty($type_condition))
	$find_condition['types'] = array_map('string_to_list', $type_condition);
    if(!empty($tag_condition))
	$find_condition['tags'] = array_map('string_to_list', $tag_condition);

    $args = page_args($list_options['context']['page']);
    if(isset($args['tag'])) {
	$find_condition['tags'] = array(string_to_list($args['tag']));
	$list_options['tags'] = $find_condition['tags'];
    }

    $display_rules = array();
    $sort_rules = array();
    foreach($values as $value) {
	if(preg_match('/^(.*?)(:|;)(<*|>*)([^<>].*?)\|(.*)$/', trim($value), $matches)) {
	    $display_rule = array(
		'term' => trim($matches[1]),
		'numeric' => $matches[2] === ';',
		'operator' => trim($matches[3]),
		'value' => trim($matches[4]),
		'display' => trim($matches[5]),
		);
	    if($display_rule['operator'] === '' || strlen($display_rule['operator']) == 2)
		$display_rule['equal'] = true;

	    if(strpos($display_rule['operator'], '>') !== false)
		$display_rule['direction'] = '>';
	    else if(strpos($display_rule['operator'], '<') !== false)
		$display_rule['direction'] = '<';
	    else
		$display_rule['direction'] = '';
	    $display_rules[] = $display_rule;
	} else if(preg_match('/^([^#]*)#(.*)?$/', $value, $matches)) {
	    $sort = array('term' => trim($matches[1]), 'order' => 'asc');
	    if(isset($matches[2])) {
		if(strpos($matches[2], '/') !== false) {
		    $sort['map'] = array_flip(array_map('trim', explode('/', $matches[2])));
		} else {
		    $order = strtolower(trim($matches[2]));
		    if($order == 'asc' || $order == 'desc') {
			$sort['order'] = $order;
		    }
		}
	    }
	    $sort_rules[] = $sort;
	} else {
	    $arg_values[] = $value;
	}
    }
    if(!empty($display_rules))
	$list_options['display_rules'] = $display_rules;
    if(!empty($sort_rules))
	$list_options['sort_rules'] = $sort_rules;
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/FunctionAdvanced
|[[ページ一覧(詳細設定可能):../../Function/pages]]|&&pages(&[&[ページ名&]&], 件数);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/FunctionAdvanced
|list of pages (customizable)|&&pages(&[&[page name&]&], number of pages);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/pages
&title &&pages

*概要
 &pages([[ページ名]], 
        show=表示形式,
        limit=件数,
        each=件数,
        name=ページ名,
        type=種別,
        tag=タグ,
        sort=順序,
        reverse,
        children,
        withsystem,
        fields=項目名1 項目名2 ...,
        header=項目名1 項目名2 ...,
        footer=項目名1 項目名2 ...,
        showhidden,
        表示条件...,
        表示順序...);
指定されたページの子ページを一覧表示します．条件や表示方法を詳細に設定できます．
また子ページの内容から項目を抜き出すこともできます．

*パラメータ
:{&[&[ページ名&]&]|指定したページの子ページを一覧表示します．省略した場合は現在のページの子ページが一覧表示されます．
すべてのページを対象としたい場合は&[&[&]&]と書きます．}
:{show=表示形式|表示する形式を指定します．以下の指定ができます．
:show=list|箇条書きでページを列挙します．デフォルトで指定されています．
:show=tree|子ページを入れ子にしたツリー状の箇条書きでページを列挙します．
:show=table|表でページ一覧を表示します．fieldsパラメータで表示したい項目を指定します．
:show=contents|ページ名だけでなくページの内容も表示します．
}
:limit=件数|指定した件数のみ表示します．
:each=件数|一度に指定した件数だけ表示します．超えた分は次のページに表示されるようになります．
:{name=ページ名|上記の&[&[ページ名&]&]で指定したページの子ページのうち，
さらにここで指定したページ名のページだけを表示します．
&*や?などのワイルドカードが使用できます．
name=!ページ名 のように書くとこのページ名以外のページを表示します．
name=ページ名1 ページ名2 ...のように複数書くといずれか(or)の条件になります．
name=ページ名1, name=ページ名2, name=..のように書くとすべてのページ名にマッチする(and)条件になります．
}
:{type=種別|指定した種別のページのみを表示します．
type=wikiやtype=fileのように指定します．
&*や?などのワイルドカードが使用できます．
type=!種別 のように書くとその種別を持たないページを表示します．
type=種別1 種別2 ...のように複数書くといずれか(or)の条件になります．
}
:{tag=タグ|指定したタグを持つページを表示します．
&*や?などのワイルドカードも使用できます．
tag=!タグ1 のように書くとタグ1を持たないページを表示します．
tag=タグ1 タグ2 ...のように複数書くといずれか(or)の条件になります．
tag=タグ1, tag=タグ2, tag=..のように書くとすべてのタグを持つ(and)条件になります．
}
:{sort=順序|sort=mtimeの場合更新日時の順に, sort=btimeの場合作成日時の順に，
sort=sizeの場合ページの容量の順に並び替えをします．
指定がない場合はページ名の順に表示されます．}
:reverse|逆順に表示します．
:children|直接の子ページのみを表示します．すなわち孫ページは表示されません．
:withsystem|マニュアルページ等のシステムで使用するページも表示します．
:{fields=項目名1 項目名2 ...|表示する項目名を空白で区切って指定します．項目名には以下の指定ができます．
:pagename|ページ名
:basename|省略されたページ名
:relname|指定したページから見た相対ページ名
:title|ページのタイトル．タイトルがない場合はbasenameとみなされます．
:title-pagename|ページのタイトル．タイトルがない場合はpagenameとみなされます．
:title-relname|ページのタイトル．タイトルがない場合はrelnameとみなされます．
:tags|ページのタグ
:mtime|ページの更新日時
:mdate|ページの更新日
:btime|ページの作成日時
:bdate|ページの作成日
:editor|最終更新者
:creator|作成者
:edit|編集ボタン
:delete|削除ボタン
:テンプレートで抽出した項目名|テンプレート(後述)で抽出した項目名
:_ (アンダーバーのみ)|テンプレートで抽出した項目のうち指定していない残りの項目

以下のように項目名を&[&[と&]&]で囲むことによってその項目をリンクにすることができます．
 [[項目名]]
以下のように書くことで指定した項目の説明を指定できます．説明は表の列の見出し等に使用されます．
 項目名@説明
}
:{header=項目名1 項目名2 ...|show=contentsを指定した際に，各ページの内容の前に表示する項目を指定します．
指定方法はfieldsパラメータと同様です．}
:{footer=項目名1 項目名2 ...|show=contentsを指定した際に，各ページの内容の後に表示する項目を指定します．
指定方法はfieldsパラメータと同様です．}
:{showhidden|非表示のページ(後述)も一覧に表示するためのリンクを表示します．
デフォルトで指定されています．無効にする場合は!showhiddenと指定します．}
:{項目名:項目の値&|背景色|
一覧表示の際に指定した項目が指定した項目の値のときに，指定した背景色で表示します(show=table時のみ有効)．
背景色は[[&&color:../color]]で使用できるものと同等です．
背景色の代わりにnoneと指定すると，その項目を非表示にできます．
このパラメータは複数指定することができますが，先に書いた順に比較がされ，
最初に一致したものを背景色として使用します．
&:の代わりに:>や:<と書くと一致ではなく大小の比較を条件にできます．}
:{項目名;項目の値&|背景色|上記と同様ですが，数値として比較を行います．}
:{項目名#表示順序|
一覧表示を指定した項目で並び替えます．表示順序には昇順の場合はasc，降順の場合はdescと書きます．
asc, descの他にも半角/(スラッシュ)で区切った項目の値を列挙することで列挙の順に並び替えます．
複数指定された場合は先に書いたものから優先して並び替えを行います．
}

*対象
対象は基本的には不要ですが，テンプレートを書くこともできます．
テンプレートを書くことで一覧表示の際に表示したい項目を子ページの内容から取ってくることができます．
子ページが同じ(または似たような)形式で書かれている場合に便利です．

テンプレートには基本的には子ページの内容(種別wikiの場合はソースの内容)をそのまま書きます．
テンプレートの中でそれぞれの子ページで内容が異なっている部分を，
 @項目名@
のように好きな項目名を付け，記号@で囲んで書くと，それぞれの子ページの該当部分を項目として抽出し，
fields, header, footerパラメータで使用できるようになります．

テンプレート書き方は[[&&newpage:../newpage]]や[[&&writepage:../writepage]]のものと同様です．
これらと&&pagesを組み合わせれば，好きな形式のページを作成，一覧表示ができます．

テンプレートに&$LAST&$と指定すると直前に使用したテンプレートを使用することができます．

*例

&wikiexample{
*最近の更新3件
&pages([[]], sort=mtime, limit=3);

*兄弟ページ一覧(容量順)
&pages([[..]], show=table, fields=[[basename]] size@容量 type, sort=size, withsystem, limit=5);
}

*(optional)高度な例
//バグトラック風テンプレート
//(実際にSandBox等のどこかのページにコピペして使用してみると楽に理解できるはずです)

 &pages(
 状態:保留|yellow,
 状態:着手|#aaf,
 状態:完了|none,                   //状態が完了のタスクは表示しない,
 状態#提案/保留/着手/完了,         //状態が提案, 保留, 着手, 完了の順番で並び替え,
 優先度#高/中/低,                  //状態が同じ場合は優先度で並び替え,
 更新日時#desc,                    //さらに優先度が同じ場合は更新日時で並び替え,
 ) {
 &title @タイトル=&insert(once) ここにタイトル@
 :カテゴリ|&select($bq$本体機能/プラグイン/デザイン$bq$)@カテゴリ@
 :状態    |&select($bq$提案/保留/着手/完了$bq$)@状態@
 :優先度  |&select($bq$高/中/低$bq$)@優先度@
 :期限    |@期限=&insert(once)@
 :投稿者  |@投稿者=$user$@
 }
*/

$WIKI_ALIASES['listpages'] = '&pages(show=list, <PARAMS>){<TARGET>}';
/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[ページ一覧:../../Function/listpages]]|&&listpages(&[&[ページ名&]&], 件数);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|list of pages|&&listpages(&[&[page name&]&], number of pages);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/listpages
&title &&listpages

*概要
 &listpages([[ページ名]], fields=[[basename]], sort=順序, reverse, withsystem, 件数);
指定されたページの子ページ(孫ページも含む)を一覧表示します．より複雑な一覧表示を行いたい場合は[[&&pages:../pages]]を使用して下さい．

*パラメータ
:&[&[ページ名&]&]|指定したページの子ページを一覧表示します．省略した場合は現在のページの子ページが一覧表示されます．
:件数|表示する件数を指定します．省略した場合はすべて表示します．
:fields=&[&[basename&]&]|ページ名を省略した形で表示します．
:{sort=順序|sort=mtimeの場合更新日時の順に, sort=btimeの場合作成日時の順に，sort=sizeの場合ページの容量の順に並び替えをします．
指定がない場合はページ名の順に表示されます．}
:reverse|逆順に表示します．
:withsystem|マニュアルページ等のシステムで使用するページも表示します．
[[&&pages:../pages]]のパラメータも使用できます．

*対象
対象は指定しません．

*例
 &listpages;

*エイリアス
&&listpagesは以下のように定義されたエイリアスです．
&pre{`$WIKI_ALIASES['listpages']`}
*/

$WIKI_ALIASES['childpages'] = '&pages(show=list, children, fields=[[title]], <PARAMS>){<TARGET>}';
/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[子ページのみ一覧:../../Function/childpages]]|&&childpages(&[&[ページ名&]&], 件数);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|list of child pages|&&childpages(&[&[page name&]&], number of pages);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/childpages
&title &&childpages

*概要
 &childpages([[ページ名]], sort=順序, reverse, withsystem, 件数);
指定されたページの子ページのみ(孫ページは含まず)を一覧表示します．より複雑な一覧表示を行いたい場合は[[&&pages:../pages]]を使用して下さい．

*パラメータ
:&[&[ページ名&]&]|指定したページの子ページを一覧表示します．省略した場合は現在のページの子ページが一覧表示されます．
:件数|表示する件数を指定します．省略した場合はすべて表示します．
:{sort=順序|sort=mtimeの場合更新日時の順に, sort=btimeの場合作成日時の順に，sort=sizeの場合ページの容量の順に並び替えをします．
指定がない場合はページ名の順に表示されます．}
:reverse|逆順に表示します．
:withsystem|マニュアルページ等のシステムで使用するページも表示します．
[[&&pages:../pages]]のパラメータも使用できます．

*対象
対象は指定しません．

*例
 &childpages;

*エイリアス
&&listpagesは以下のように定義されたエイリアスです．
&pre{`$WIKI_ALIASES['childpages']`}
*/

$WIKI_ALIASES['treepages'] = '&pages(show=tree, <PARAMS>){<TARGET>}';
/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[ページ一覧(ツリー表示):../../Function/treepages]]|&&treepages(&[&[ページ名&]&]);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|list of pages (tree)|&&treepages(&[&[page name&]&], number of pages);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/treepages
&title &&treepages

*概要
 &treepages([[ページ名]], sort=順序, reverse, withsystem);
指定されたページの子ページ(孫ページも含む)を入れ子になったツリー状で一覧表示します．
より複雑な一覧表示を行いたい場合は[[&&pages:../pages]]を使用して下さい．

*パラメータ
:&[&[ページ名&]&]|指定したページの子ページを一覧表示します．省略した場合は現在のページの子ページが一覧表示されます．
:{sort=順序|sort=mtimeの場合更新日時の順に, sort=btimeの場合作成日時の順に，sort=sizeの場合ページの容量の順に並び替えをします．
指定がない場合はページ名の順に表示されます．}
:reverse|逆順に表示します．
:withsystem|マニュアルページ等のシステムで使用するページも表示します．
[[&&pages:../pages]]のパラメータも使用できます．

*対象
対象は指定しません．

*例
 &treepages;
 &treepages([[]]);

*エイリアス
&&treepagesは以下のように定義されたエイリアスです．
&pre{`$WIKI_ALIASES['treepages']`}
*/

$WIKI_ALIASES['recent'] = '&pages([[]], show=date, sort=mtime, 30, <PARAMS>){<TARGET>}';
/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[更新ページ一覧:../../Function/recent]]|&&recent(件数);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|list of recently updated pages|&&recent(number of pages);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/recent
&title &&recent

*概要
 &recent(件数);
最近更新されたページの一覧を表示します．

*パラメータ
:件数|表示する件数を指定します．省略した場合は30件になります．
[[&&pages:../pages]]のパラメータも使用できます．
*対象
対象は指定しません．

*例

&wikiexample{
&recent(5);
}

*エイリアス
&&recentは以下のように定義されたエイリアスです．
&pre{`$WIKI_ALIASES['recent']`}
*/

$WIKI_ALIASES['templatepages'] = '&newpage(!move,<PARAMS>){<TARGET>}
&pages(show=table, fields=[[basename]] _, <PARAMS>){$LAST$}
';
/**
@manual $manualpage$/ja/Wiki/CheatSheet/FunctionAdvanced
|[[テンプレートによるページの作成と一覧:../../Function/templatepages]]|&&templatepages(&[&[ページ名&]&]){テンプレート}|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/FunctionAdvanced
|create and list formatted pages|&&templatepages(&[&[page name&]&]){template}|
*/
/**
@manual $manualpage$/ja/Wiki/Function/templatepages
&title &&templatepages

*概要
 &templatepages([[ページ名]]){テンプレート}
テンプレートを定義し，そのテンプレートから子ページを新規作成するボタンを表示します．
また子ページの一覧を表示し，テンプレートを用いてページ内の項目を取得し一覧に併せて表示します．

*パラメータ
:&[&[ページ名&]&]|新しいページを指定したページの子ページとして作成します．

[[&&pages:../pages]]と[[&&newpage:../newpage]]のパラメータも使用できます．

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
 //名簿を作る(実際にこれをどこかにコピーして試してみると分かり易いです)
 &templatepages{
 :氏名|@氏名@
 :性別|@性別/男性/女性@
 :生年月日|@生年月日@
 }

*エイリアス
&&templatepagesは以下のように定義されたエイリアスです．
&pre{`$WIKI_ALIASES['templatepages']`}
*/

$WIKI_ALIASES['blogpages'] = 
    '&pages(show=contents, reverse, children, '.
    'footer=\'tags@'.l('Tags').'\' \'mtime@'.l('Last updated').'\', each=10, <PARAMS>){<TARGET>}';
/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[ブログ記事一覧:../../Function/blogpages]]|&&blogpages(&[&[ページ名&]&], 件数);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|list of blog articles|&&blogpages(&[&[page name&]&], number of articles);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/blogpages
&title &&blogpages

*概要
 &blogpages([[ページ名]], each=件数);
ブログの記事一覧を表示します．
子ページを記事とみなし，抜粋部分([[&&more:../more]]より前の部分)を表示します．

*パラメータ
:&[&[ページ名&]&]|指定したページの子ページを記事として表示します．省略した場合は現在のページになります．
:each=件数|一度に表示する件数を指定します．省略した場合は10件になります．

[[&&pages:../pages]]のパラメータも使用できます．
*対象
対象は指定しません．

*例
 &blogpages(each=20);

*エイリアス
&&blogpagesは以下のように定義されたエイリアスです．
&pre{`$WIKI_ALIASES['blogpages']`}
*/

$LANGUAGE['ja']['Create article'] = '記事作成';

$WIKI_ALIASES['blogadmin'] = 
'&newpage(!move, !pagename, label='.l('Create article').',<PARAMS>) {
&title @'.l('タイトル').'@
@'.l('Text').'#@
}
&pages(show=table, children, fields=edit [[basename]] [[タイトル]] tags mtime,<PARAMS>,
pagename#desc
){$LAST$}';
/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[ブログ管理画面:../../Function/blogadmin]]|&&blogadmin(&[&[ページ名&]&]);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|blog administration page|&&blogadmin(&[&[page name&]&]);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/blogadmin
&title &&blogadmin

*概要
 &blogadmin([[ページ名]]);
ブログの記事作成と記事一覧，編集が行える画面を表示します．
指定したページの子ページを記事とみなし，管理します．
管理ではなく閲覧用に記事を一覧表示したい場合は[[&&blogpages:../blogpages]]を使用してください．

*パラメータ
:&[&[ページ名&]&]|指定したページの子ページを記事として管理します．省略した場合は現在のページになります．

*対象
対象は指定しません．

*例
 &blogadmin([[ブログ記事]]);

*エイリアス
&&blogadminは以下のように定義されたエイリアスです．
&pre{`$WIKI_ALIASES['blogadmin']`}
*/

$LANGUAGE['ja']['Status'] = '状態';
$LANGUAGE['ja']['propose'] = '提案';
$LANGUAGE['ja']['pending'] = '保留';
$LANGUAGE['ja']['assigned'] = '着手';
$LANGUAGE['ja']['confirmed'] = '確認';
$LANGUAGE['ja']['completed'] = '完了';
$LANGUAGE['ja']['rejected'] = '却下';
$LANGUAGE['ja']['Priority'] = '優先度';
$LANGUAGE['ja']['critical'] = '高';
$LANGUAGE['ja']['major'] = '中';
$LANGUAGE['ja']['minor'] = '低';
$LANGUAGE['ja']['Category'] = 'カテゴリ';
$LANGUAGE['ja']['new feature'] = '新機能';
$LANGUAGE['ja']['other'] = 'その他';
$LANGUAGE['ja']['bug'] = 'バグ';
$LANGUAGE['ja']['Deadline'] = '期限';

$wiki_alias_tracker_status = 
    array(l('propose'), l('pending'), l('assigned'), l('confirmed'), l('completed'), l('rejected'));
$wiki_alias_tracker_priority = array(l('critical'), l('major'), l('minor'));

$WIKI_ALIASES['tracker'] = 
'&newpage(!pagename,!move,<PARAMS>) {
&title @'.l('Title')."'".'@
:'.l('Category').' |@'.l('Category').'/<category='.implode('/', array(l('new feature'),l('bug'),l('other'))).'>@
:'.l('Status').' |@'.l('Status').'/<status='.implode('/',$wiki_alias_tracker_status).'>@
:'.l('Priority').' |@'.l('Priority').'/<priority='.implode('/',$wiki_alias_tracker_priority).'='.l('major').'>@
:'.l('Poster').' |@'.l('Poster').'=$user$@
<TARGET=`
====
&bbs;
`>
}
&pages(show=table, children, fields=[[basename]] [[タイトル]] _ btime,
<PARAMS>,
'.l('Status').':'.l('pending').'|#a5f980,
'.l('Status').':'.l('assigned').'|#b1fae3,
'.l('Status').':'.l('confirmed').'|#ffe875,
'.l('Status').':'.l('completed').'|none,
'.l('Status').':'.l('rejected').'|none,
'.l('Status').'#<status='.implode('/',$wiki_alias_tracker_status).'>,
'.l('Priority').'#<priority='.implode('/',$wiki_alias_tracker_priority).'>,
mtime#desc,
) {$LAST$}';

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[バグトラック:../../Function/tracker]]|&&tracker(&[&[ページ名&]&]);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|bug tracker|&&tracker(&[&[page name&]&]);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/tracker
&title &&tracker

*概要
 &tracker([[ページ名]], category=カテゴリ1/カテゴリ2/.., status=状態1/状態2/.., priority=優先度1/優先度2/..);
バグトラック機能を使用できるようにし，バグ報告一覧を表示します．
個々のバグ報告は決められたテンプレートに従った子ページとして作成，管理されます．

より細かくカスタマイズしたバグトラックを使用したい場合は[[以下の定義:.#エイリアス]]を参考に
[[&&newpage:../newpage]]と[[&&pages:../pages]]を使用して下さい．

*パラメータ
:{&[&[ページ名&]&]|
指定したページの子ページを対象としてページの新規作成，一覧を行います．
省略した場合は現在のページになります．}
:{category=カテゴリ1/カテゴリ2/..|カテゴリの選択肢を半角/(スラッシュ)で区切って指定します．省略可能です．}
:{status=状態1/状態2/..|状態の選択肢を半角/(スラッシュ)で区切って指定します．省略可能です．}
:{priority=優先度1/優先度2/..|優先度の選択肢を半角/(スラッシュ)で区切って指定します．省略可能です．}

また[[&&pages:../pages#パラメータ]]のパラメータをそのまま使用できます．

*対象
対象には追加のテンプレートを記述できます．
子ページと一致している必要がない部分は半角`(バッククォート)で囲みます．

*例

&wikiexample{
//デフォルトのまま使用
&tracker;
}

&wikiexample{
//各項目をカスタマイズ
&tracker(
category=仕事/遊び,
status=まだ/おわった,
priority=高い/低い,
優先度:高い|#ffaaaa,
優先度:低い|skyblue
){`
*概要

*症状

`};
}

*エイリアス
&&trackerは以下のように定義されたエイリアスです．
&pre{`$WIKI_ALIASES['tracker']`}

*/
?>
