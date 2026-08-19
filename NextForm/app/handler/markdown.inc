<?php
/*
 * ページ種別 markdown。
 *
 * 変換は app/vendor/ に同梱した league/commonmark に任せる。
 * **ライブラリを知っているのは markdown_converter() と markdown_append_html()
 * だけ**にしてある。差し替えたくなったとき、あるいは同梱をやめたくなったときに
 * 触る場所を 2 つに閉じ込めておくため。
 *
 * 生 HTML は既定でエスケープする (MARKDOWN_ALLOW_HTML)。Markdown の標準は
 * 素通しだが、wiki 記法は素通ししないので、既定を合わせておかないと
 * 「書き込みを公開しているサイトでは Markdown ページだけ XSS の入口になる」
 * という状態になる。CSRF 対策と同じで、既定を安全側に倒す。
 */
$HANDLERS['markdown'] = array(
    'show' => 'markdown_show',
    'source' => 'markdown_source',
    'summary' => 'markdown_summary',
    'edit' => 'markdown_edit',
    'write' => 'markdown_write',
    'texts' => 'markdown_texts',
    'normalize' => 'markdown_normalize',
    );
$PAGE_TYPES_TEXTBASE[] = 'markdown';

define('MARKDOWN_VENDOR_AUTOLOAD', APP_DIR_PATH . '/vendor/autoload.php');

/*
 * 変換器を 1 リクエストに 1 つだけ作る。
 * Environment の組み立ては拡張の数だけコストがかかるので使い回す。
 */
function markdown_converter() {
    static $converter = null;
    if($converter !== null)
	return $converter;

    if(!is_file(MARKDOWN_VENDOR_AUTOLOAD))
	return false;
    require_once(MARKDOWN_VENDOR_AUTOLOAD);
    if(!class_exists('League\CommonMark\MarkdownConverter'))
	return false;
    // 同梱ライブラリのインターフェイスを実装するので、autoload のあとに読む
    require_once(APP_DIR_PATH . '/handler/markdown/front_matter.inc');

    $config = array(
	/*
	 * escape は生 HTML を文字として出す。strip は捨てる。
	 * allow_unsafe_links を false にすると javascript: などが落ちる。
	 */
	'html_input' => (defined('MARKDOWN_ALLOW_HTML') && MARKDOWN_ALLOW_HTML === 'true')
	    ? 'allow' : 'escape',
	'allow_unsafe_links' => false,
	);

    $environment = new League\CommonMark\Environment\Environment($config);
    $environment->addExtension(new League\CommonMark\Extension\CommonMark\CommonMarkCoreExtension());
    // 表・打ち消し線・タスクリスト・自動リンク
    $environment->addExtension(new League\CommonMark\Extension\GithubFlavoredMarkdownExtension());
    // 脚注は GFM の一式には入っていないので別に足す
    $environment->addExtension(new League\CommonMark\Extension\Footnote\FootnoteExtension());
    $environment->addExtension(
	new League\CommonMark\Extension\FrontMatter\FrontMatterExtension(new MarkdownFrontMatterParser()));

    $converter = new League\CommonMark\MarkdownConverter($environment);
    return $converter;
}

/*
 * Markdown を HTML にして $dom にぶら下げる。
 *
 * ライブラリが返すのは HTML の文字列なので、いったん DOMDocument に読み直して
 * から取り込む。この wiki の出力は DOM を組み立てて saveXML() で書き出す作りで、
 * 文字列を混ぜる口が無いため。読み直しには、壊れた HTML をそのまま外に出さない
 * という効き目もある。
 */
function markdown_append_html($dom, $html) {
    $document = new DOMDocument('1.0', 'UTF-8');
    /*
     * loadHTML() は charset の指定が無いと ISO-8859-1 とみなして日本語を壊すので、
     * meta を付けた最小の HTML として読ませ、body の中身だけを取り込む。
     * (mb_convert_encoding の HTML-ENTITIES は PHP 8.2 で非推奨になったため使わない。
     *  LIBXML_HTML_NOIMPLIED は根の要素が 1 つでないと残りを捨てるので使わない。)
     */
    $wrapped = '<html><head>'
	. '<meta http-equiv="Content-Type" content="text/html; charset=utf-8">'
	. '</head><body>' . $html . '</body></html>';

    $previous = libxml_use_internal_errors(true);
    $result = $document->loadHTML($wrapped, LIBXML_NONET);
    libxml_clear_errors();
    libxml_use_internal_errors($previous);
    if(!$result)
	return false;

    $body = $document->getElementsByTagName('body')->item(0);
    if(is_null($body))
	return false;
    dom_append_dom_children($dom, $body);
    return true;
}

/*
 * 本文を変換して $dom に入れる。ここが唯一の変換の入口。
 * ライブラリが無い (app/vendor/ が欠けている) ときは、原文をそのまま見せる。
 * 読めなくなるより、変換されないほうがましなため。
 */
function markdown_convert(&$page, $dom, $contents = null) {
    if(is_null($contents))
	$contents = page_get_contents($page);

    $converter = markdown_converter();
    if($converter === false) {
	message_error(l('Markdown library is missing. Showing the source instead.'));
	markdown_source_append($page, $dom, $contents);
	return false;
    }

    $section = dom_append_child($dom, 'section', 'markdown');
    return markdown_append_html($section, (string)$converter->convert($contents));
}

function markdown_show(&$page, $dom) {
    markdown_convert($page, $dom);
}

function markdown_source_append(&$page, $dom, $contents) {
    $pre = dom_append_child($dom, 'pre', 'text');
    dom_append_text($pre, $contents);
}

function markdown_source(&$page, $dom) {
    title_set_pagename('Source of $pagename$', $page['name']);
    markdown_source_append($page, $dom, page_get_contents($page));
}

function markdown_edit(&$page, $dom) {
    text_edit($page, $dom, 'markdown');
}

function markdown_write(&$page, $dom) {
    $contents = default_value($page['args']['contents'], '');
    $contents = markdown_normalize($page, $contents);
    $page['args']['contents'] = $contents;

    if(!empty($page['args']['preview'])) {
	markdown_convert($page, $dom, $contents);
	text_edit($page, $dom, 'markdown', $contents, default_value($page['args']['ticket'], ''));
    } else {
	text_write($page, $dom, 'markdown');
    }
}

/*
 * 検索インデックス (2-gram) に渡す本文。
 * 変換してから文字だけを取り出す。記号を正規表現で落とす方式にしないのは、
 * 表や脚注の中身まで漏れなく拾うため。wiki 側の wiki_texts() と同じ流儀。
 */
function markdown_texts(&$page) {
    $converter = markdown_converter();
    if($converter === false)
	return array('body' => explode("\n", page_get_contents($page)));

    $search_dom = dom_create_document();
    markdown_append_html($search_dom, (string)$converter->convert(page_get_contents($page)));
    $texts_info = array('body' => array());
    dom_texts($search_dom, $texts_info['body']);
    return $texts_info;
}

/*
 * 一覧などに出す要約。最初の段落の書き出しを使う。
 */
function markdown_summary(&$page, $dom) {
    $texts = markdown_texts($page);
    if(empty($texts['body']))
	return;
    dom_append_text($dom, $texts['body'][0]);
}

/*
 * 保存前の整形。Markdown では何もしない。
 *
 * wiki 記法の normalize は表の桁揃えなどを行うが、Markdown の原文は
 * 利用者が書いたとおりに残すべきで、整形すると別のツール (Obsidian など) と
 * 往復したときに差分が出る。この関数が無いと insert / replace / listedit /
 * templateedit の 4 画面が Markdown ページで動かないので、素通しで用意する。
 */
function markdown_normalize(&$page, $text) {
    return $text;
}
?>
