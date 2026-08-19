<?php
/*
 * Markdown の ![説明](対象) を、この wiki の添付の仕組みに繋ぐ。
 *
 *   ![図](child.png)          子ページ (種別 file) を埋め込む
 *   ![図](../共通/図.png)      相対指定 (wiki 記法と同じ解決)
 *   ![図](https://…/a.png)    外部の画像はそのまま
 *
 * 埋め込みは wiki 記法の &include と同じ経路 (file ハンドラの embed) を通す。
 * 画像なら img、音声や動画ならそれぞれの再生画面になり、大きさなどの扱いも
 * 1 か所に集まったままになる。
 *
 * **このファイルは vendor/autoload.php を読んだあとに require すること。**
 */

class MarkdownImageRenderer implements League\CommonMark\Renderer\NodeRendererInterface {
    public function render(League\CommonMark\Node\Node $node,
			   League\CommonMark\Renderer\ChildNodeRendererInterface $childRenderer) {
	if(!($node instanceof League\CommonMark\Extension\CommonMark\Node\Inline\Image))
	    return '';
	/* 説明文は子要素として持っている。装飾は落として文字だけ使う */
	$alt = trim(strip_tags($childRenderer->renderNodes($node->children())));
	return markdown_image_html((string)$node->getUrl(), $alt, (string)$node->getTitle());
    }
}

/* スキーム付き (http: など) と // で始まるものは外部とみなす */
function markdown_image_is_external($url) {
    return preg_match('/^[a-zA-Z][a-zA-Z0-9+.-]*:/', $url) === 1 || substr($url, 0, 2) === '//';
}

function markdown_image_html($url, $alt, $title) {
    global $MARKDOWN_CURRENT_PAGENAME;

    if($url === '')
	return '';

    if(markdown_image_is_external($url)) {
	$document = dom_create_document();
	$attributes = array('src' => $url, 'alt' => $alt);
	if($title !== '')
	    $attributes['title'] = $title;
	dom_append_child($document, 'img', '', $attributes);
	return dom_save_html($document);
    }

    $base = isset($MARKDOWN_CURRENT_PAGENAME) ? $MARKDOWN_CURRENT_PAGENAME : '';
    $path = rawurldecode($url);

    /*
     * 添付は子ページなので、裸の名前は**まず子ページとして**探す。
     *
     * page_path_to_pagename() は ./ や ../ で始まらない名前を絶対 (トップ階層)
     * として扱う。Wiki 記法ではそれでよいが、Markdown を書く人は
     * ![図](image.png) と書いて隣に置いた添付を指すつもりでいる。
     * 子ページに無ければ、これまでどおり絶対として解決する。
     */
    $pagename = '';
    if($base !== '' && strpos($path, './') !== 0 && strpos($path, '../') !== 0) {
	$child = page_path_to_pagename('./' . $path, $base);
	if(page_is_exists($child))
	    $pagename = $child;
    }
    if($pagename === '')
	$pagename = page_path_to_pagename($path, $base);

    $document = dom_create_document();

    if(page_is_exists($pagename) && auth_has_permission($pagename, 'read')) {
	$page = page_read($pagename);
	if(page_get_type($page) === 'file') {
	    $content_type_handler = &file_get_content_type_handler($page);
	    if(!empty($content_type_handler['embed']) &&
	       function_exists($content_type_handler['embed'])) {
		$content_type_handler['embed']($page, $document, array());
		return dom_save_html($document);
	    }
	}
    }

    /*
     * 埋め込めないもの (まだ無い / 種別が file でない / 読む権限が無い) は
     * ページへのリンクにする。存在しなければ a.not_exists が付くので、
     * そこから作りに行ける。画像が出ないまま黙って壊れるよりよい。
     */
    dom_append_page_link($document, $alt !== '' ? $alt : $url, $pagename);
    return dom_save_html($document);
}
?>
