<?php
/*
 * tests/markdown.sh からサイトの中で実行される検証ヘルパ。
 *
 *   php markdown-helper.php <index.php> <管理者ユーザー名> <検査名> [引数..]
 *
 * 検査名:
 *   write <名前> <本文>       種別 markdown として保存する (written=1/0)
 *   write-wiki <名前> <本文>  種別 wiki として保存する (written=1/0)
 *   meta <名前> <キー>        meta の 1 項目 (isset=1/0 value=...)
 *   tags <名前>               ページのタグ (tags=<空白区切り>)
 *   alltag <タグ>             タグ一覧の数え上げ (count=...)
 *   set-tags <名前> <タグ..>  タグ画面と同じ経路でタグを付ける (written=1/0)
 *   set-meta-title <名前> <題名>
 *                             メタ情報画面と同じ経路で題名を付ける (written=1/0)
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

switch($check) {

case 'write':
    printf("written=%d\n", test_write($rest[0], $rest[1]) ? 1 : 0);
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
