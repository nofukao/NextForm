<?php
/*
 * Markdown の ![説明](対象) を、この wiki の添付の仕組みに繋ぐ。
 *
 *   ![図](child.png)          子ページ (種別 file) を埋め込む
 *   ![図](../共通/図.png)      相対指定 (wiki 記法と同じ解決)
 *   ![図](/共通/図.png)        トップ階層から
 *   ![図](https://…/a.png)    外部の画像はそのまま
 *   ![説明](ページ)           ふつうのページは、段落にそれ 1 つだけなら取り込む
 *
 * 埋め込みは wiki 記法の &include と同じ経路 (file ハンドラの embed) を通す。
 * 画像なら img、音声や動画ならそれぞれの再生画面になり、大きさなどの扱いも
 * 1 か所に集まったままになる。
 *
 * 大きさや配置は説明の後ろに | を付けて書く (Obsidian と同じ形)。語は &include と同じ。
 *   ![説明|50% right](図.png "キャプション")
 * タイトルの欄はキャプションにする (&include{...} と同じく figure と figcaption)。
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
	return markdown_image_html((string)$node->getUrl(), $alt, (string)$node->getTitle(),
				   markdown_image_is_alone($node));
    }
}

/* 段落にそれ 1 つだけか。ページを取り込むのはこのときだけ (文の途中で段落を割らない) */
function markdown_image_is_alone($node) {
    return $node->parent() instanceof League\CommonMark\Node\Block\Paragraph &&
	is_null($node->previous()) && is_null($node->next());
}

/*
 * 説明の最後の | より後ろが、すべて &include の指定として読めるときだけ、
 * 指定として切り出す ($alt から外し、語の配列を返す)。読めなければ説明のまま
 * (説明に | を書いた既存のページを壊さない)。max はすぐ後ろの大きさの前置き。
 *   幅x高さ (末尾の ! で比率を無視)、幅x、x高さ、幅 (Obsidian と同じ)、拡大率%、
 *   max 大きさ、!adjust、open、!link、left、right
 */
function markdown_image_split_params(&$alt) {
    $position = strrpos($alt, '|');
    if($position === false)
	return array();
    $tokens = preg_split('/[\s,]+/', substr($alt, $position + 1), -1, PREG_SPLIT_NO_EMPTY);
    if(empty($tokens))
	return array();
    $params = array();
    $max = false;
    foreach($tokens as $token) {
	if($token === 'max' && !$max) {
	    $max = true;
	} else if(preg_match('/^(\d+x\d+!?|\d+x|x\d+|\d+|\d+%)$/', $token)) {
	    $params[] = ($max ? 'max ' : '') . $token;
	    $max = false;
	} else if(!$max && in_array($token, array('!adjust', 'open', '!link', 'left', 'right'), true)) {
	    $params[] = $token;
	} else {
	    return array();
	}
    }
    if($max)
	return array();
    $alt = trim(substr($alt, 0, $position));
    return $params;
}

/*
 * キャプションか left / right があれば、wiki の &include と同じく figure.figure で包む。
 * 包む先を返す (包まないときは $document のまま。今と同じ出力になる)。
 */
function markdown_image_open_figure($document, $params, $caption) {
    $gravity = in_array('left', $params, true) ? 'left' : (in_array('right', $params, true) ? 'right' : '');
    if($caption === '' && $gravity === '')
	return $document;
    $figure = dom_append_child($document, 'figure', 'figure');
    if($gravity !== '')
	dom_append_class($figure, 'gravity_' . $gravity);
    return $figure;
}

function markdown_image_close_figure($container, $caption) {
    if($caption === '' || !($container instanceof DOMElement))
	return;
    $figcaption = dom_append_child($container, 'figcaption');
    dom_append_text($figcaption, $caption);
}

/*
 * 外部の画像の大きさ。数字で書いた幅と高さだけを当てる。拡大率や max は元の大きさが
 * 要るが、それを知るにはサーバーからその URL を取りに行くことになる (wiki 記法の
 * &include は getimagesize() で取りに行く)。ページを書く人の指定でサーバーが外へ
 * 取りに行く口を、Markdown に新しく作らない。片方だけならブラウザが比率を保つ。
 */
function markdown_image_external_size($img, $params) {
    foreach($params as $param) {
	if(preg_match('/^(\d+)x(\d+)!?$/', $param, $matches)) {
	    dom_set_attribute($img, 'width', $matches[1]);
	    dom_set_attribute($img, 'height', $matches[2]);
	} else if(preg_match('/^(\d+)x?$/', $param, $matches)) {
	    dom_set_attribute($img, 'width', $matches[1]);
	} else if(preg_match('/^x(\d+)$/', $param, $matches)) {
	    dom_set_attribute($img, 'height', $matches[1]);
	}
    }
}

/*
 * ふつうのページの取り込みの印。描画の途中で Markdown のページを描くと変換器に
 * 再入する (MarkdownConverter は解析中の状態を持ち回す)。そこで描画では今と同じ
 * リンクを出して印だけ付け、変換が終わってから markdown_expand_embeds() が取り込む。
 * 印は要求ごとの乱数で、ページ名などはここに置く。表に無い印 (生の HTML を許す
 * サイトで手で書いたもの) は取り込まない。
 */
function markdown_embed_register($pagename, $params, $caption) {
    global $MARKDOWN_EMBEDS;
    $token = 'e' . bin2hex(random_bytes(8));
    $MARKDOWN_EMBEDS[$token] = array('pagename' => $pagename, 'params' => $params, 'caption' => $caption);
    return $token;
}

/* スキーム付き (http: など) と // で始まるものは外部とみなす */
function markdown_image_is_external($url) {
    return preg_match('/^[a-zA-Z][a-zA-Z0-9+.-]*:/', $url) === 1 || substr($url, 0, 2) === '//';
}

/*
 * 画像やリンクに書かれた行き先 (URL の符号化を解いたもの) をページ名にする。
 * リンク (link.inc) も同じ規則で解決する。
 *
 * **Web のリンクと同じく、書いた文字だけで決める。** 添付はページの子なので、
 * ページを「フォルダー」と読み替えると次のようになる。
 *
 *   名前 / ./名前   そのページの子 (隣に置いた添付)
 *   ../名前         兄弟
 *   /名前           トップ階層
 *
 * Wiki 記法 ([[名前]]) では名前だけはトップ階層になるが、ここでは揃えない。
 * Markdown を書く人は ![図](image.png) と書いて隣に置いた添付を指すつもりで
 * いるし、よそで書いた Markdown もその前提で書かれている。
 *
 * 0.12.0 までは、名前をまず子として探し、無ければトップ階層として読んでいた。
 * 行き先がページの有無で変わるので、存在しないリンクから作った場所と、
 * 後から作ったページにつながる場所が食い違った。そのときの読み方は
 * app/tool/markdown_link_check が持っている (行き先が変わったリンクを探すため)。
 */
function markdown_resolve_pagename($path) {
    global $MARKDOWN_CURRENT_PAGENAME;

    $base = isset($MARKDOWN_CURRENT_PAGENAME) ? $MARKDOWN_CURRENT_PAGENAME : '';
    if(substr($path, 0, 1) !== '/' && strpos($path, './') !== 0 && strpos($path, '../') !== 0)
	$path = './' . $path;
    return page_path_to_pagename($path, $base);
}

function markdown_image_html($url, $alt, $caption, $is_alone = false) {
    if($url === '')
	return '';
    $params = markdown_image_split_params($alt);
    $document = dom_create_document();

    if(markdown_image_is_external($url)) {
	$container = markdown_image_open_figure($document, $params, $caption);
	$img = dom_append_child($container, 'img', '', array('src' => $url, 'alt' => $alt));
	markdown_image_external_size($img, $params);
	markdown_image_close_figure($container, $caption);
	return dom_save_html($document);
    }

    $pagename = markdown_resolve_pagename(rawurldecode($url));

    if(page_is_exists($pagename) && auth_has_permission($pagename, 'read')) {
	$page = page_read($pagename);
	if(page_get_type($page) !== 'file' && $is_alone) {
	    /* ふつうのページ。今と同じリンクに印を付け、変換のあとで取り込む */
	    $a = dom_append_page_link($document, $alt !== '' ? $alt : $url, $pagename);
	    dom_set_attribute($a, 'data-markdown-embed', markdown_embed_register($pagename, $params, $caption));
	    return dom_save_html($document);
	}
	if(page_get_type($page) === 'file') {
	    $content_type_handler = &file_get_content_type_handler($page);
	    if(!empty($content_type_handler['embed']) &&
	       function_exists($content_type_handler['embed'])) {
		/*
		 * 埋め込みが文字のリンクになる種類 (PDF・md・添付 HTML など) は、
		 * 説明をリンクの表示にする。印 (embed_label) のある種類にだけ
		 * 名前つきで渡す。画像などに並びの引数で渡すと、240x240 や open の
		 * ような大きさ・開き方の指定として読まれてしまう。
		 */
		$embed_params = $params;
		if(!empty($content_type_handler['embed_label']) && $alt !== '')
		    $embed_params['label'] = $alt;
		$container = markdown_image_open_figure($document, $params, $caption);
		$content_type_handler['embed']($page, $container, $embed_params);
		markdown_image_close_figure($container, $caption);
		/*
		 * 画像の説明は代替テキストにする (画像が出ないときや読み上げで使われる)。
		 * 埋め込みの側は &include と共用で、代替テキストにページ名を入れている。
		 */
		if($alt !== '') {
		    foreach($document->getElementsByTagName('img') as $img)
			$img->setAttribute('alt', $alt);
		}
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
