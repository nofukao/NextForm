<?php
/*
 * tests/markdown.sh からサイトの中で実行される検証ヘルパ。
 *
 *   php markdown-helper.php <index.php> <管理者ユーザー名> <検査名> [引数..]
 *
 * 検査名:
 *   write <名前> <本文>       種別 markdown として保存する (written=1/0)
 *   write-file <名前> <パス>  本文をファイルから読んで保存する (written=1/0)
 *   write-wiki <名前> <本文>  種別 wiki として保存する (written=1/0)
 *   meta <名前> <キー>        meta の 1 項目 (isset=1/0 value=...)
 *   tags <名前>               ページのタグ (tags=<空白区切り>)
 *   alltag <タグ>             タグ一覧の数え上げ (count=...)
 *   index-stale <名前>        索引に残った余分な ngram の数 (stale=...)
 *   texts <名前>              索引に入る文字 (texts=<空白区切り>)
 *   set-tags <名前> <タグ..>  タグ画面と同じ経路でタグを付ける (written=1/0)
 *   set-meta-title <名前> <題名>
 *                             メタ情報画面と同じ経路で題名を付ける (written=1/0)
 *   positions <名前>          変換後の DOM に付いた部分編集の範囲。
 *                             1 行 1 要素で pos<TAB>タグ<TAB>位置<TAB>長さ<TAB>原文の切り出し
 *   contents <名前>           保存されている本文 (contents=<改行を \n に直したもの>)
 *   replace <名前> <位置> <長さ> <値>
 *                             ?option=replace と同じ経路で範囲を差し替える
 *                             (written=1/0 contents=...)
 *   cleanup                   このヘルパが作ったページを消す (removed=...)
 *
 * 結果は `key=value` の行で出す。判定は呼び出し側の shell が行う。
 *
 * **保存は HTTP を通さず、ハンドラを直に呼ぶ。** markdown_write() のうち
 * 画面と リダイレクトを除いた部分 (normalize → page_write) をそのままなぞる。
 * 画面の側は markdown.sh が curl で別に見る。
 */

$argv = $_SERVER['argv'];
if(count($argv) < 4) {
    fprintf(STDERR, "Usage: php markdown-helper.php <index.php> <admin> <check> [args..]\n");
    exit(2);
}
$index_path = $argv[1];
$admin      = $argv[2];
$check      = $argv[3];
$rest       = array_slice($argv, 4);

require_once(dirname(realpath($index_path)) . '/app/tool/common');
eval_index_php($index_path);
define('APP_DIR_PATH', getcwd() . '/app');
require_once(APP_DIR_PATH . '/main.inc');

$GLOBALS['AUTH_GET_USER_CACHE'] = array('name' => $admin, 'method' => 'digest');
head_tags_init();

define('TEST_PAGE_PREFIX', 'MarkdownTest');

/*
 * markdown_write() の保存側と同じ順で通す。
 * 種別を先に決めてから normalize を引くのは、handler_function() が
 * meta['type'] を見てハンドラを選ぶため。
 */
function test_write($pagename, $contents, $type = 'markdown') {
    $page = page_create($pagename);
    storage_page_read($page);
    $page['meta']['type'] = $type;
    page_setup($page);

    $normalize = handler_function($page, 'normalize');
    if($normalize !== false)
	$contents = $normalize($page, $contents);

    $page['meta']['type'] = $type;
    $ticket = default_value($page['meta']['ticket'], '');
    return page_write($page, $contents, $ticket);
}

/*
 * 改行とタブを見える形にする。結果は 1 行 1 件で読むので、
 * 中身に改行が入ったままだと行がずれる。
 */
function test_escape($text) {
    return str_replace(array("\t", "\r", "\n"), array('\t', '\r', '\n'), $text);
}

/*
 * data-twp / data-twl を持つ要素を、文書に出てくる順で集める。
 */
function test_collect_positions($dom, &$found = array()) {
    foreach($dom->childNodes as $child) {
	if(!($child instanceof DOMElement))
	    continue;
	if($child->hasAttribute('data-twp') && $child->hasAttribute('data-twl'))
	    $found[] = array($child,
			     (int)$child->getAttribute('data-twp'),
			     (int)$child->getAttribute('data-twl'));
	test_collect_positions($child, $found);
    }
    return $found;
}

switch($check) {

case 'write':
    printf("written=%d\n", test_write($rest[0], $rest[1]) ? 1 : 0);
    break;

case 'write-file':
    /* 本文をファイルから読む。CRLF や末尾の改行をそのまま渡すため */
    printf("written=%d\n", test_write($rest[0], file_get_contents($rest[1])) ? 1 : 0);
    break;

case 'write-wiki':
    printf("written=%d\n", test_write($rest[0], $rest[1], 'wiki') ? 1 : 0);
    break;

case 'meta':
    $page = page_read($rest[0]);
    $key = $rest[1];
    printf("isset=%d\nvalue=%s\n",
	   isset($page['meta'][$key]) ? 1 : 0,
	   isset($page['meta'][$key]) ? $page['meta'][$key] : '');
    break;

case 'tags':
    $page = page_read($rest[0]);
    printf("tags=%s\n", list_to_string(tag_get_page_tags($page)));
    break;

case 'alltag':
    $alltags = tag_alltags_get();
    printf("count=%d\n", isset($alltags[$rest[0]]) ? $alltags[$rest[0]] : 0);
    break;

case 'set-tags':
    /* タグ画面 (option/tag.inc) と同じ経路 */
    $page = page_read($rest[0]);
    printf("written=%d\n", tag_set_page_tags($page, array_slice($rest, 1)) ? 1 : 0);
    break;

case 'set-meta-title':
    /* メタ情報画面 (option/meta.inc) と同じ経路 */
    $page = page_read($rest[0]);
    $page['title'] = $rest[1];
    $page['keep_mtime'] = true;
    $fp = page_open_contents($page);
    printf("written=%d\n", page_write($page, $fp, $page['meta']['ticket']) ? 1 : 0);
    break;

case 'index-stale':
    /* 索引に残っているのに本文には無い ngram の数 (search_index_check の stale) */
    $collected = search_index_collect(true);
    $page = page_read($rest[0]);
    $should = search_page_ngram($page);
    $actual = isset($collected['page_ngrams'][$rest[0]]) ?
	$collected['page_ngrams'][$rest[0]] : array();
    printf("stale=%d\n", count(array_diff_key($actual, $should)));
    break;

case 'texts':
    /* 索引に載る文字。折りたたんだ中身が漏れていないかを見る */
    $page = page_read($rest[0]);
    $texts = handler_function($page, 'texts');
    $lines = array();
    if($texts !== false) {
	foreach($texts($page) as $group)
	    foreach($group as $line)
		$lines[] = $line;
    }
    printf("texts=%s\n", str_replace("\n", ' ', implode(' ', $lines)));
    break;

case 'positions':
    /*
     * 部分編集の範囲。**原文の切り出しをそのまま出す**ので、
     * 呼び出し側は「その要素の原文はこれ」と書くだけで検査できる。
     * 範囲が 1 バイトでもずれれば、切り出しが期待と違う形になる。
     */
    $page = page_read($rest[0]);
    $page['is_main'] = true;
    $contents = page_get_contents($page);
    $dom = dom_create_document();
    markdown_convert($page, $dom);
    foreach(test_collect_positions($dom) as $found) {
	list($element, $position, $length) = $found;
	printf("pos\t%s\t%d\t%d\t%s\n",
	       $element->nodeName, $position, $length,
	       test_escape(substr($contents, $position, $length)));
    }
    break;

case 'contents':
    $page = page_read($rest[0]);
    printf("contents=%s\n", test_escape(page_get_contents($page)));
    break;

case 'replace':
    /* ?option=replace と同じ経路。JavaScript が送るものと同じ引数を作る */
    require_once(APP_DIR_PATH . '/option/replace.inc');
    $page = page_read($rest[0]);
    $args = array('pagename' => $rest[0],
		  'position' => (int)$rest[1],
		  'length'   => (int)$rest[2],
		  'value'    => $rest[3],
		  'ticket'   => default_value($page['meta']['ticket'], ''));
    $dom = dom_create_document();
    printf("written=%d\n", replace_write($args, $dom) ? 1 : 0);
    $page = page_read($rest[0]);
    printf("contents=%s\n", test_escape(page_get_contents($page)));
    break;

case 'cleanup':
    $removed = 0;
    foreach(page_find(TEST_PAGE_PREFIX, array('is_pagename_only' => true)) as $p) {
	$pagename = is_array($p) ? $p['name'] : $p;
	$page = page_read($pagename);
	if(page_delete($page))
	    $removed++;
    }
    printf("removed=%d\n", $removed);
    break;

default:
    fprintf(STDERR, "unknown check: %s\n", $check);
    exit(2);
}
?>
