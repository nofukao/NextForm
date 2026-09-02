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
$PAGE_TYPES_PREVIEWABLE[] = 'markdown';
$PAGE_TYPE_LABELS['markdown'] = 'Markdown';
$PAGE_TYPE_CHEAT_SHEETS['markdown'] = array('page' => '/Markdown/CheatSheet',
					    'label' => 'Markdown Cheat Sheet');

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
    require_once(APP_DIR_PATH . '/handler/markdown/wikilink.inc');
    require_once(APP_DIR_PATH . '/handler/markdown/image.inc');

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

    /*
     * [[ページ名]]。標準のリンク記法 ([...](...)) より先に見せたいので
     * 優先度を上げて登録する。
     */
    $environment->addInlineParser(new MarkdownWikiLinkParser(), 100);
    $environment->addRenderer('MarkdownWikiLink', new MarkdownWikiLinkRenderer());

    /* ![説明](子ページ) を添付の埋め込みにする。標準の描画より先に見せる */
    $environment->addRenderer(League\CommonMark\Extension\CommonMark\Node\Inline\Image::class,
			      new MarkdownImageRenderer(), 10);

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
/*
 * 変換して HTML の文字列を返す。false は「ライブラリが無い」。
 *
 * $front_matter には先頭のメタ情報が key => value の配列で入る (無ければ空)。
 * ここで受け取れるようにしてあるのは、ライブラリの型を知る場所を増やさないため。
 *
 * [[ページ名]] の相対指定を解決するために、変換中はどのページを描いているかを
 * 大域に置く。変換器はリクエストごとに 1 つを使い回し、1 回のリクエストで
 * 本文とサイドページの両方を描くことがあるので、前の値に必ず戻す。
 */
function markdown_render_html(&$page, $contents, &$front_matter = null) {
    global $MARKDOWN_CURRENT_PAGENAME;

    $front_matter = array();
    $converter = markdown_converter();
    if($converter === false)
	return false;

    /*
     * 末尾に改行が無いとフロントマターとして読まれない。同梱ライブラリの
     * 正規表現 (FrontMatterParser::REGEX_FRONT_MATTER) が、閉じの --- の
     * **後ろの改行まで**を必要とするため。本文がフロントマターだけのページを
     * 書くと、題名も付かず、水平線 + 見出しとして本文に出てしまっていた。
     * 保存する本文には手を入れず、変換に渡すものだけを整える。
     */
    if($contents !== '' && substr($contents, -1) !== "\n")
	$contents .= "\n";

    $previous = isset($MARKDOWN_CURRENT_PAGENAME) ? $MARKDOWN_CURRENT_PAGENAME : '';
    $MARKDOWN_CURRENT_PAGENAME = $page['name'];
    try {
	$rendered = $converter->convert($contents);
    } finally {
	$MARKDOWN_CURRENT_PAGENAME = $previous;
    }

    if($rendered instanceof League\CommonMark\Extension\FrontMatter\FrontMatterProviderInterface) {
	$data = $rendered->getFrontMatter();
	if(is_array($data))
	    $front_matter = $data;
    }
    return (string)$rendered;
}

/*
 * フロントマターの title / tags をページの meta に写す。
 *
 * $is_write が真のとき (= 保存の経路) は、**書いていない項目を消す**。
 * フロントマターは「そのページの題名とタグを本文の中で決める」場所なので、
 * 宣言を消したのに値が残ると、ファイルを見てもページの状態が分からなくなる。
 * 外のエディタと往復させる前提の記法なので、ファイルのほうを正とする。
 * 代償として、メタ情報の画面やタグの画面で付けた値は、次に本文を保存した
 * ときに消える。
 *
 * 表示の経路 (markdown_convert) では消さない。storage には残っているのに
 * 画面にだけ出ない、という食い違いを作らないため。
 *
 * wiki 記法の &title{} は「消しても meta に残る」ままにしてある。
 * あちらは本文の中に埋める記法で、題名を宣言する場所が本文の外に無い。
 */
function markdown_apply_front_matter(&$page, $front_matter, $is_write = false) {
    if(!is_array($front_matter))
	$front_matter = array();
    markdown_set_title($page, $front_matter, $is_write);
    markdown_set_tags($page, $front_matter, $is_write);
}

/*
 * 題名。wiki 記法の &title{} と同じ経路に載せる。$page['title'] に入れておくと
 * page_write() が meta['title'] に保存し (page.inc)、page_handle() が
 * 次からその題名でページを開く。title_dom はいま描いている画面の題名で、
 * 保存前のプレビューでも新しい題名が出るようにするために立てる。
 */
function markdown_set_title(&$page, $front_matter, $is_clearable = false) {
    $title = '';
    if(isset($front_matter['title']) && !is_array($front_matter['title']))
	$title = trim((string)$front_matter['title']);

    if($title === '') {
	if($is_clearable) {
	    /*
	     * 空文字が「消す」の合図 (page_write)。ここで meta を直に
	     * 落としてはいけない。page_write が古い ngram を数える前に
	     * 題名が消え、消した題名の ngram が索引に残る。
	     */
	    $page['title'] = '';
	    unset($page['title_dom']);
	}
	return;
    }

    $page['title'] = $title;
    $page['title_dom'] = dom_create_document();
    sentence_append($page['title_dom'], $title . ' ', $page);
    title_append_actual($page['title_dom'], dom_pagename($page['name']));
}

/*
 * タグ。$page['write_tag'] は page_write() がタグを書き換えるための口で、
 * タグの画面 (tag_set_page_tags) が使っているものと同じ。空文字を入れると
 * meta のタグが消える。
 */
function markdown_set_tags(&$page, $front_matter, $is_clearable = false) {
    if(!isset($front_matter['tags'])) {
	if($is_clearable)
	    $page['write_tag'] = '';
	return;
    }
    $page['write_tag'] = list_to_string(markdown_parse_tags($front_matter['tags']));
}

/*
 * フロントマターのタグを配列にする。次の 4 つを同じに扱う。
 *
 *   tags: [alpha, beta]     tags: alpha, beta     tags: alpha beta
 *   tags:
 *     - alpha
 *     - beta
 *
 * タグの保存形式は半角スペース区切りの 1 行なので、**タグ自体に空白は
 * 入れられない**。「文中 タグ」と書くと 2 個になる。
 */
function markdown_parse_tags($value) {
    if(is_array($value))
	$parts = $value;			/* ブロック形式 */
    else
	$parts = explode(',', trim(trim((string)$value), '[]'));

    $tags = array();
    foreach($parts as $part)
	/* 空白での分割と全角スペースの正規化は string_to_list に任せる */
	foreach(string_to_list(trim((string)$part, " \t\"'")) as $tag)
	    $tags[$tag] = true;
    return array_keys($tags);
}

function markdown_convert(&$page, $dom, $contents = null) {
    if(is_null($contents))
	$contents = page_get_contents($page);

    $front_matter = array();
    $html = markdown_render_html($page, $contents, $front_matter);
    if($html === false) {
	message_error(l('Markdown library is missing. Showing the source instead.'));
	markdown_source_append($page, $dom, $contents);
	return false;
    }
    markdown_apply_front_matter($page, $front_matter);

    $section = dom_append_child($dom, 'section', 'markdown');
    if(!markdown_append_html($section, $html))
	return false;
    markdown_append_sentence($page, $section);
    markdown_set_heading_ids($page, $section);
    return true;
}

/*
 * 変換後の DOM の文字に sentence の後処理を掛ける。
 *
 * wiki 記法は文字を組み立てながら sentence_append() に通すので、検索から
 * ページを開いたときに語が印 (mark) で示される。Markdown はライブラリが
 * 作った HTML を DOM ごと取り込むため、その経路を通らず、印が付かなかった。
 *
 * **ページ名の自動リンク (pagename) は掛けない。** Markdown を書く人は
 * [[...]] で明示的にリンクする習慣を持っており、素の語が勝手にリンクになるのは
 * 驚きになる。種別 text (text_show) と &pre が同じ理由で excludes している。
 *
 * pre / code の中と、リンクの中には掛けない。前者は原文をそのまま見せる場所で、
 * 後者は表示名を組み替えたくないため。
 */
function markdown_append_sentence(&$page, $dom) {
    if(!isset($page['sentence']))
	return;

    $texts = array();
    markdown_collect_texts($dom, $texts);
    foreach($texts as $text_node) {
	$parent = $text_node->parentNode;
	if(!($parent instanceof DOMElement))
	    continue;

	$converted = dom_create_document();
	sentence_append($converted, $text_node->nodeValue, $page,
			array('excludes' => array('pagename')));
	/* 何も当たらなければ文字 1 つが返る。触らずに残す */
	if($converted->childNodes->length == 1 &&
	   $converted->firstChild instanceof DOMText)
	    continue;

	$owner = dom_owner($text_node);
	foreach($converted->childNodes as $child)
	    $parent->insertBefore($owner->importNode($child, true), $text_node);
	$parent->removeChild($text_node);
    }
}

/*
 * 後処理を掛ける文字を集める。あとで木を組み替えるので、先に集めきる。
 */
function markdown_collect_texts($dom, &$texts) {
    static $skips = array('pre' => true, 'code' => true, 'a' => true);
    foreach($dom->childNodes as $child) {
	if($child instanceof DOMText) {
	    if(trim($child->nodeValue) !== '')
		$texts[] = $child;
	    continue;
	}
	if(!($child instanceof DOMElement))
	    continue;
	if(isset($skips[strtolower($child->nodeName)]))
	    continue;
	markdown_collect_texts($child, $texts);
    }
}

function markdown_show(&$page, $dom) {
    markdown_convert($page, $dom);
}

/*
 * 変換後の DOM から見出しを拾う。返すのは [レベル, 文字, 要素] の並び。
 * Markdown の見出しは h1〜h6 で、入れ子ではなく深さが数字で表される。
 */
function markdown_headings($dom, &$headings) {
    foreach($dom->childNodes as $child) {
	if(!($child instanceof DOMElement))
	    continue;
	if(preg_match('/^h([1-6])$/', $child->nodeName, $matches)) {
	    $text = trim($child->textContent);
	    if($text !== '')
		$headings[] = array('level' => (int)$matches[1],
				    'text' => $text,
				    'element' => $child);
	}
	markdown_headings($child, $headings);
    }
}

/*
 * 見出しに id を振る。
 *
 * id の作り方は wiki 記法と同じ page_get_section_id()。同じにしておくと、
 * [[ページ名#見出し]] が種別をまたいで同じアンカーを指す。
 * 同じ文字の見出しが 2 つあるときは先に出たほうだけに振る (wiki と同じ)。
 * サイドページに振らないのも wiki と同じで、本文と id がぶつかるため。
 */
function markdown_set_heading_ids(&$page, $dom) {
    if(empty($page['is_main']))
	return;
    $headings = array();
    markdown_headings($dom, $headings);
    $ids = array();
    foreach($headings as $heading) {
	$id = page_get_section_id($page['name'], $heading['text']);
	if(isset($ids[$id]))
	    continue;
	$ids[$id] = true;
	dom_set_attribute($heading['element'], 'id', $id);
    }
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
    $html = markdown_render_html($page, page_get_contents($page));
    if($html === false)
	return array('body' => explode("\n", page_get_contents($page)));

    $search_dom = dom_create_document();
    markdown_append_html($search_dom, $html);
    $texts_info = array('body' => array());
    dom_texts($search_dom, $texts_info['body']);
    return $texts_info;
}

/*
 * 目次 (?option=summary)。見出しを拾って入れ子の一覧にする。
 *
 * 出す形は wiki 記法の目次と同じ section.summary + ul で、
 * 項目は dom_append_page_link() が作る。見出しの id も同じ規則なので、
 * 目次から飛べるし、他のページから [[ページ名#見出し]] でも同じ場所に届く。
 */
function markdown_summary(&$page, $dom) {
    $html = markdown_render_html($page, page_get_contents($page));
    if($html === false)
	return;

    $converted = dom_create_document();
    if(!markdown_append_html($converted, $html))
	return;

    $headings = array();
    markdown_headings($converted, $headings);

    $summary = dom_append_child($dom, 'section', 'summary',
				array('data-pagename' => $page['name']));
    if(empty($headings)) {
	/* 見出しが 1 つも無いページでは、書き出しを見せる (wiki と同じ考え) */
	$texts = array();
	dom_texts($converted, $texts);
	if(!empty($texts))
	    dom_append_text($summary, $texts[0]);
	return;
    }

    /*
     * 見出しの深さで ul を入れ子にする。
     *
     * Markdown の見出しは深さが数字なので、h1 の次に h3 が来ることもある。
     * 飛んでいても 1 段だけ深くし、浅くなったらその深さまで戻る。
     * 「同じ深さの見出しが入れ子にならない」ことが肝心で、そこを間違えると
     * 目次が本文と違う形に見える。
     */
    $stack = array();	/* 各段の ul と、その段で最後に足した li */
    $root = dom_append_child($summary, 'ul');
    foreach($headings as $heading) {
	$id = page_get_section_id($page['name'], $heading['text']);
	if(isset($ids[$id]))
	    $id = '';		/* 同じ文字の見出しには id が振られていない */
	else
	    $ids[$id] = true;

	while(!empty($stack) && $stack[count($stack) - 1]['level'] > $heading['level'])
	    array_pop($stack);

	if(empty($stack)) {
	    $stack[] = array('level' => $heading['level'], 'ul' => $root, 'last_li' => null);
	} else if($stack[count($stack) - 1]['level'] < $heading['level']) {
	    $parent = &$stack[count($stack) - 1];
	    /* 親の項目がまだ無い深さ (h2 から始まる文書など) では同じ ul に置く */
	    $ul = is_null($parent['last_li']) ? $parent['ul'] : dom_append_child($parent['last_li'], 'ul');
	    unset($parent);
	    $stack[] = array('level' => $heading['level'], 'ul' => $ul, 'last_li' => null);
	}

	$top = count($stack) - 1;
	$li = dom_append_child($stack[$top]['ul'], 'li');
	$stack[$top]['last_li'] = $li;
	dom_append_page_link($li, $heading['text'], $page['name'], $id);
    }
}

/*
 * 保存前の整形。本文には手を入れない。
 *
 * wiki 記法の normalize は表の桁揃えなどを行うが、Markdown の原文は
 * 利用者が書いたとおりに残すべきで、整形すると別のツール (Obsidian など) と
 * 往復したときに差分が出る。この関数が無いと insert / replace / listedit /
 * templateedit の 4 画面が Markdown ページで動かないので、素通しで用意する。
 *
 * 本文は返すだけだが、**フロントマターの title と tags はここで拾う**。
 * wiki 記法の &title{} も wiki_normalize() の中の変換で拾われており、
 * 保存の経路 (write / insert / replace / listedit / templateedit) が
 * どれでも題名が付くのはそのため。同じ場所に置いて経路を揃える。
 * ここは保存の経路なので、書いていない項目は消す (markdown_apply_front_matter)。
 */
function markdown_normalize(&$page, $text) {
    $front_matter = array();
    markdown_render_html($page, $text, $front_matter);
    markdown_apply_front_matter($page, $front_matter, true);
    return $text;
}
?>
