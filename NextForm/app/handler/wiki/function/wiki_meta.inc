<?php
$WIKI_OPERATORS_FUNCTION['meta'] = true;
function wiki_operator_meta(&$context, $dom, $tree) {
    $meta_names = array();
    $pagename = $context['page']['name'];
    $separator = ', ';
    wiki_parse_parameters(
	$context, $tree, array(
	    'pagename' => &$pagename,
	    'values' => &$meta_names,
	    'equalities' => array(
		'separator' => &$separator,
		)));
    $meta_name = array_pop($meta_names);

    $fields = array('type', 'title', 'description', 'tag', 'create_user', 'last_modify_user',
		    'Content-type', 'original_filename',
		    'image_width', 'image_height', 
		    'audio_duration',
		    'video_duration', 'video_width', 'video_height',
		    );

    if(empty($meta_name) || !in_array($meta_name, $fields) ||
       !auth_has_permission($pagename, 'read')) {
	return;
    }

    $page = page_read($pagename);

    if($meta_name === 'tag') {
	$separator = str_replace(array('space', 'comma'), array(' ', ','), $separator);
	$meta_value = implode($separator, tag_get_page_tags($page));
    } else 
	$meta_value = default_value($page['meta'][$meta_name], '');
    sentence_append($dom, $meta_value, $context['page']);
}

$WIKI_ALIASES['description'] = '&meta(description, <PARAMS>);';
/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[メタ情報:../../Function/meta]]|&&meta(&[&[ページ名&]&], title, description);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|meta information|&&meta(&[&[page name&]&], title, description);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/meta

&title &&meta

*概要
 &meta([[ページ名]], title, description, tag);
ページのタイトルや説明，タグなどのメタ情報を表示します．

*パラメータ
:&[&[ページ名&]&]|指定したページの情報を表示します．省略された場合は現在のページの情報になります．
:title|指定したページのタイトルを表示します．
:description|指定したページの説明を表示します．
:tag|指定したページのタグを表示します．
:{separator=記号|複数のタグを列挙するときに使用する区切り記号を指定します．
スペースはspace, カンマはcommaと指定して下さい．
}

*対象
対象は指定しません．

*例
 *このページの説明
 &meta(description);
 *あのページのタイトル
 &meta([[あのページ]],title);
 *このページのタグ
 &meta(tag, separator=spaceとspace);
*/
?>
