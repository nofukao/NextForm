<?php
/*
 * Markdown の [[ページ名]] 記法。
 *
 * 書き方は Obsidian や MediaWiki に合わせて **対象が先、表示名が後**:
 *
 *   [[ページ名]]              そのまま表示してリンクする
 *   [[ページ名|表示名]]        表示名でリンクする
 *   [[../隣のページ]]          相対指定 (wiki 記法と同じ解決)
 *   [[ページ名#見出し]]        ページ内の見出しへ
 *
 * wiki 記法の [[表示>ページ]] とは順が逆だが、Markdown を書く人が持って
 * くる習慣に合わせるほうが実利がある。
 *
 * **リンクの HTML は dom_append_page_link() に作らせる。**
 * 存在しないページの印 (a.not_exists)、未読、自分自身、親子関係の class、
 * data-link-pagename は、サイドの一覧や既存の CSS が使っている。
 * ここで属性を組み立て直すと、必ずどれかを取りこぼす。
 *
 * **このファイルは vendor/autoload.php を読んだあとに require すること。**
 */

class MarkdownWikiLink extends League\CommonMark\Node\Inline\AbstractInline {
    private $target;

    public function __construct(string $target) {
	parent::__construct();
	$this->target = $target;
    }

    public function getTarget(): string {
	return $this->target;
    }
}

class MarkdownWikiLinkParser implements League\CommonMark\Parser\Inline\InlineParserInterface {
    public function getMatchDefinition(): League\CommonMark\Parser\Inline\InlineParserMatch {
	/* 改行と入れ子は許さない。[[ の直後から ]] まで */
	return League\CommonMark\Parser\Inline\InlineParserMatch::regex('\[\[([^\[\]\r\n]+)\]\]');
    }

    public function parse(League\CommonMark\Parser\InlineParserContext $inlineContext): bool {
	$inlineContext->getCursor()->advanceBy($inlineContext->getFullMatchLength());
	$matches = $inlineContext->getSubMatches();
	$inlineContext->getContainer()->appendChild(new MarkdownWikiLink((string)$matches[0]));
	return true;
    }
}

class MarkdownWikiLinkRenderer implements League\CommonMark\Renderer\NodeRendererInterface {
    public function render(League\CommonMark\Node\Node $node,
			   League\CommonMark\Renderer\ChildNodeRendererInterface $childRenderer) {
	if(!($node instanceof MarkdownWikiLink))
	    return '';
	return markdown_wikilink_html($node->getTarget());
    }
}

/*
 * [[...]] の中身をページ名と表示名に分け、リンクの HTML を返す。
 * 相対指定の解決と見出しの id は wiki 記法 (wiki_link) と同じ関数を使う。
 */
function markdown_wikilink_html($raw) {
    global $MARKDOWN_CURRENT_PAGENAME;

    $label = '';
    $target = $raw;
    if(($position = strpos($raw, '|')) !== false) {
	$target = substr($raw, 0, $position);
	$label = trim(substr($raw, $position + 1));
    }
    $target = trim($target);
    if($label === '')
	$label = $target;

    $sectionname = '';
    if(($position = strpos($target, '#')) !== false) {
	$sectionname = substr($target, $position + 1);
	$target = substr($target, 0, $position);
	if($label === $raw || $label === trim($raw))
	    $label = $raw;
    }

    $base = isset($MARKDOWN_CURRENT_PAGENAME) ? $MARKDOWN_CURRENT_PAGENAME : '';
    /* [[#見出し]] のようにページ名が空なら、そのページ自身を指す */
    $pagename = ($target === '') ? $base : page_path_to_pagename($target, $base);

    $id = ($sectionname === '') ? '' : page_get_section_id($pagename, $sectionname);

    $document = dom_create_document();
    dom_append_page_link($document, $label, $pagename, $id);
    return dom_save_html($document);
}
?>
