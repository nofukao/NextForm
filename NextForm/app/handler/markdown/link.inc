<?php
/*
 * Markdown の [表示名](対象) を、この wiki のページに繋ぐ。
 *
 *   [図](child.png)            子ページ (添付) へのリンク
 *   [隣](../隣のページ)         相対指定 (wiki 記法と同じ解決)
 *   [節](../ページ#見出し)       ページの見出しへ
 *   [外](https://…)            外部はそのまま
 *
 * 行き先の解決は画像 (image.inc) と同じ markdown_resolve_pagename()。
 * ![図](x.png) で埋め込めるものは、[図](x.png) でリンクできる。
 *
 * **ページとして扱わない URL は null を返し、ライブラリの描画に任せる**
 * (同梱ライブラリは、描画器が null を返すと次の描画器を試す)。
 * javascript: などを落とす判定もそちらにあるので、ここで二重に持たない。
 *
 * **リンクの属性は dom_append_empty_page_link() に作らせる。** [[...]] と同じ理由
 * (wikilink.inc)。中身はライブラリに描かせて、強調などの装飾を残す。
 *
 * **このファイルは vendor/autoload.php を読んだあとに require すること。**
 */

class MarkdownLinkRenderer implements League\CommonMark\Renderer\NodeRendererInterface {
    public function render(League\CommonMark\Node\Node $node,
			   League\CommonMark\Renderer\ChildNodeRendererInterface $childRenderer) {
	if(!($node instanceof League\CommonMark\Extension\CommonMark\Node\Inline\Link))
	    return null;
	$url = (string)$node->getUrl();
	if(!markdown_link_is_page_path($url))
	    return null;

	$attributes = markdown_link_attributes($url);
	$title = (string)$node->getTitle();
	if($title !== '')
	    $attributes['title'] = $title;
	return new League\CommonMark\Util\HtmlElement('a', $attributes,
						      $childRenderer->renderNodes($node->children()));
    }
}

/*
 * ページへの行き先として読むか。
 *
 * 外部 (スキーム付き・//) のほか、次のものは URL としてそのまま残す。
 *   /…   サイトの根からの URL
 *   ?…   この wiki の URL (?ページ名)
 *   #…   ページの中の位置
 *   ? を含むもの  index.php?Top のように、いまも動いている相対 URL
 * 残りは、書いたまま出しても /nextform/x.png のような存在しない場所を指す
 * (.htaccess が index.php 以外を拒む) ので、ページ名として読んで失うものが無い。
 */
function markdown_link_is_page_path($url) {
    if($url === '' || markdown_image_is_external($url))
	return false;
    if($url[0] === '/' || $url[0] === '#' || strpos($url, '?') !== false)
	return false;
    return true;
}

/*
 * 行き先のページへのリンクの属性 (href・class・data-link-pagename など)。
 * #見出し は [[ページ#見出し]] と同じく page_get_section_id() で id にする。
 */
function markdown_link_attributes($url) {
    $sectionname = '';
    if(($position = strpos($url, '#')) !== false) {
	$sectionname = rawurldecode(substr($url, $position + 1));
	$url = substr($url, 0, $position);
    }
    $pagename = markdown_resolve_pagename(rawurldecode($url));
    $id = ($sectionname === '') ? '' : page_get_section_id($pagename, $sectionname);

    $document = dom_create_document();
    $a = dom_append_empty_page_link($document, $pagename, $id);
    $attributes = array();
    foreach($a->attributes as $attribute)
	$attributes[$attribute->name] = $attribute->value;
    return $attributes;
}
?>
