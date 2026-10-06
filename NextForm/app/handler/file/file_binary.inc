<?php
$FILE_CONTENT_TYPE_HANDLER_BINARY = array(
    'embed' => 'file_binary_embed',
    /* Markdown の ![説明](x.pdf) の説明を、リンクの表示として受け取る */
    'embed_label' => true,
    );

$FILE_CONTENT_TYPE_HANDLERS['text/plain'] = &$FILE_CONTENT_TYPE_HANDLER_BINARY;
$FILE_CONTENT_TYPE_HANDLERS['application/pdf'] = &$FILE_CONTENT_TYPE_HANDLER_BINARY;

/*
 * $params が null なのはファイルのページ (file_show) で、「ダウンロード」と出す。
 * それ以外 (&include・Markdown の ![説明](x.pdf)) は file_embed_label() の表示。
 */
function file_binary_embed(&$page, $dom, $params = null) {
    $queries = array('action' => 'raw');
    if(!empty($page['backuptime']))
       $queries['backuptime'] = $page['backuptime'];

    if(is_null($params))
	$name = l('Download');
    else
	$name = file_embed_label($page, $params);

    $a = dom_append_link($dom, url_make($page['name'], $queries), $name, 'download');
    dom_set_attribute($a, 'data-link-pagename', $page['name']);
    return $a;
}

/*
 * 埋め込みをリンクで出す種類の、リンクの表示。
 *
 *   $params['label']    Markdown の ![説明](x.pdf) の説明
 *   並びの引数の最後    &include([[x.pdf]],表示名) の表示名
 *   どちらも無ければ    ファイル名 (x.pdf)
 *
 * ページ名の全体 (親/x.pdf) は出さない。ページの中では、隣に添付した
 * ファイルを指していることが分かれば足りる。
 */
function file_embed_label(&$page, $params) {
    if(isset($params['label']) && $params['label'] !== '')
	return $params['label'];
    $label = page_get_simplename($page['name']);
    foreach($params as $key => $param) {
	if(is_int($key))
	    $label = $param;
    }
    return $label;
}

?>
