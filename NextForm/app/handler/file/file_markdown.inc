<?php
/*
 * 添付した Markdown (.md) を、開いたときに wiki の画面の中に描画する。
 *
 *   ![説明](x.md)       「説明」のリンク。押すと描画した画面 (action=view)
 *   ファイルのページ    「開く」(描画した画面) と「ダウンロード」(実体)
 *
 * **見分けは拡張子で行う。** ブラウザは .md の種類を決まった形で送ってこない
 * (開発検証サイトに添付された .md は application/octet-stream だった)。
 * 種類として登録 (FILE_CONTENT_TYPE_HANDLERS) しないのは、実体 (action=raw) を
 * これまでどおり application/octet-stream のダウンロードにしておくため。
 * 実体はページのクローン (clone.inc) とダウンロードが使うので、描画した HTML に
 * すり替えてはいけない。
 *
 * 描画は Markdown ページと同じ markdown_convert()。生 HTML の扱い
 * (MARKDOWN_ALLOW_HTML) も同じになる。添付できる人はページも編集できるので、
 * ここで入口が増えることはない。
 */
$FILE_CONTENT_TYPE_HANDLER_MARKDOWN = array(
    'embed' => 'file_markdown_embed',
    'view' => 'file_markdown_view',
    /* Markdown の ![説明](x.md) の説明を、リンクの表示として受け取る */
    'embed_label' => true,
    );

/* ブラウザが .md に付けてくることのある種類 */
$FILE_MARKDOWN_CONTENT_TYPES = array('text/markdown', 'text/x-markdown');

/*
 * md として扱うか。種類が md のもの、または種類が分からない
 * (空・octet-stream・text/plain) .md / .markdown。
 * 画像などの種類が付いたものは、名前が .md でもその種類のまま扱う。
 */
function file_is_markdown(&$page) {
    global $FILE_MARKDOWN_CONTENT_TYPES;
    $content_type = '';
    if(!empty($page['meta']['Content-type'])) {
	$parts = explode(';', $page['meta']['Content-type']);
	$content_type = strtolower(trim($parts[0]));
    }
    if(in_array($content_type, $FILE_MARKDOWN_CONTENT_TYPES))
	return true;
    if(!in_array($content_type, array('', 'application/octet-stream', 'text/plain')))
	return false;
    return preg_match('/\.(md|markdown)$/i', page_get_simplename($page['name'])) === 1;
}

/*
 * $params が null なのはファイルのページ (file_show) で、「開く」と
 * 「ダウンロード」を並べる。それ以外 (&include・Markdown の ![説明](x.md)) は
 * 描画した画面へのリンク 1 つ。表示の決め方は PDF などと同じ (file_embed_label)。
 *
 * 版の履歴から見ているとき (backuptime) は「開く」を出さない。
 * 描画した画面はいまの版しか出せない。
 */
function file_markdown_embed(&$page, $dom, $params = null) {
    if(!is_null($params))
	return file_markdown_append_open_link($page, $dom, file_embed_label($page, $params));

    if(!empty($page['backuptime']))
	return file_binary_embed($page, $dom);
    $a = file_markdown_append_open_link($page, $dom, l('Open'));
    dom_append_text($dom, ' ');
    file_binary_embed($page, $dom);
    return $a;
}

function file_markdown_append_open_link(&$page, $dom, $label) {
    $a = dom_append_link($dom, url_make($page['name'], array('action' => 'view')),
			 $label, 'open_markdown');
    dom_set_attribute($a, 'data-link-pagename', $page['name']);
    return $a;
}

/*
 * 本文を描画する。読むだけなので、部分編集の位置 (data-twp) は付けない。
 * 付けると nextform.js が部分編集の欄を出し、保存先がこのファイルのページになる。
 *
 * 相対の指定 (![図](図.png)・[資料](./資料.pdf)) は、**md を添付したページを
 * 起点に**探す。フォルダに md と画像を並べて書いた資料が、添付し直すだけで
 * そのまま読める。md 自身を起点にすると、md の子 (x.md/図.png) を探しに行く。
 *
 * UTF-8 でないものは描画しない (同梱ライブラリが受け付けない)。
 * ファイルのページを出して、ダウンロードはできるようにしておく。
 */
function file_markdown_view(&$page, $dom) {
    $contents = page_get_contents($page);
    if($contents === false)
	$contents = '';
    if(function_exists('mb_check_encoding') && !mb_check_encoding($contents, 'UTF-8')) {
	message_error(l('This file is not UTF-8, so it cannot be shown.'));
	file_show($page, $dom);
	return;
    }
    $page['markdown_base_pagename'] = page_path_to_pagename('..', $page['name'], false);
    markdown_convert($page, $dom, $contents, false);
}
?>
