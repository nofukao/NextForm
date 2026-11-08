<?php
$WIKI_OPERATORS_FUNCTION['search'] = true;
function wiki_operator_search(&$context, $dom, $tree) {
    if(!empty($context['texts_only'])) {
	return;
    }
    $args = args_get();
    dom_append_search($dom, default_value($args['query'], ''));
}
/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[検索フォーム:../../Function/search]]|&&search;|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|search form|&&search;|
*/
/**
@manual $manualpage$/ja/Wiki/Function/search
&title &&search

*概要
 &search;
サイト内のページを検索するフォームを表示します．

*パラメータ
パラメータは指定しません．

サイトの検索の画面と同じ書き方(ORや-語，title:など)で探せます．マニュアルは含めずに探します
([[検索:../../../SiteMenu#検索]])．

*対象
対象は指定しません．

*例

&wikiexample{
&search;
}
*/
