<?php
/*
 * tests/short-title.sh からサイトの中で実行される検証ヘルパ。
 *
 *   php short-title-helper.php <index.php> <管理者ユーザー名> <検査名> [引数..]
 *
 * 検査名:
 *   write <名前> <種別> <本文>    normalize を通して保存する (written=1/0)
 *   write-many <接頭辞> <数>      <接頭辞>/001 .. を種別 markdown で書く (written=数)
 *   meta <名前> <キー>            meta の 1 項目 (isset=1/0 value=...)
 *   meta-screen <名前> <短いタイトル> [タイトル]
 *                                 メタ情報の画面の保存 (meta_write) をそのまま呼ぶ。
 *                                 うまくいくと meta_write が exit するので何も出ない。
 *                                 失敗したときだけ failed=1
 *   backup-now <名前>             今の版を履歴に残す (backuptime=その時刻)
 *   restore <名前> <時刻>         履歴の「この版に戻す」(written=1/0)
 *   views <名前>                  種別 wiki のページを描き、そのあいだに「読んだ」と
 *                                 記録されたページ (views=<空白区切り>)
 *   to-markdown <名前>            Wiki 記法から Markdown への変換の結果
 *                                 (markdown=<改行を \n に直したもの>)
 *   cleanup                       このヘルパが作ったページを消す (removed=...)
 *
 * 結果は `key=value` の行で出す。判定は呼び出し側の shell が行う。
 */

$argv = $_SERVER['argv'];
if(count($argv) < 4) {
    fprintf(STDERR, "Usage: php short-title-helper.php <index.php> <admin> <check> [args..]\n");
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

define('TEST_PAGE_PREFIX', 'ShortTitleTest');

/* 編集画面の保存と同じ順 (種別を決めてから normalize → page_write) */
function test_write($pagename, $type, $contents) {
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
    printf("written=%d\n", test_write($rest[0], $rest[1], $rest[2]) ? 1 : 0);
    break;

case 'write-many':
    $written = 0;
    for($i = 1; $i <= (int)$rest[1]; $i++) {
	$name = sprintf('%s/%03d', $rest[0], $i);
	if(test_write($name, 'markdown', sprintf("## 見出し %03d\n\n本文\n", $i)))
	    $written++;
    }
    printf("written=%d\n", $written);
    break;

case 'meta':
    $page = page_read($rest[0]);
    $key = $rest[1];
    printf("isset=%d\nvalue=%s\n",
	   isset($page['meta'][$key]) ? 1 : 0,
	   isset($page['meta'][$key]) ? $page['meta'][$key] : '');
    break;

case 'meta-screen':
    /* 画面が送るのと同じ項目。説明は今の値をそのまま送る */
    $page = page_read($rest[0]);
    $args = array('pagename' => $rest[0],
		  'short_title' => $rest[1],
		  'title' => isset($rest[2]) ? $rest[2] : default_value($page['meta']['title'], ''),
		  'description' => default_value($page['meta']['description'], ''));
    meta_write($args, dom_create_document());
    printf("failed=1\n");
    break;

case 'backup-now':
    $page = page_read($rest[0]);
    storage_page_backup($page, 0);
    $times = page_get_backup_times($page);
    printf("backuptime=%s\n", empty($times) ? '' : max($times));
    break;

case 'restore':
    $page = page_read($rest[0]);
    $ticket = $page['meta']['ticket'];
    printf("written=%d\n", page_restore($page, $rest[1], $ticket) ? 1 : 0);
    break;

case 'views':
    global $USER_HISTORY_PAGE_VIEW;
    $USER_HISTORY_PAGE_VIEW = array('pages' => array());
    $page = page_read($rest[0]);
    $page['is_main'] = true;
    wiki_convert($page, dom_create_document());
    printf("views=%s\n", implode(' ', array_keys($USER_HISTORY_PAGE_VIEW['pages'])));
    break;

case 'to-markdown':
    /* Wiki 記法から Markdown への変換の結果 (改行は \n に直す) */
    $page = page_read($rest[0]);
    $result = wiki_markdown_convert($page, page_get_contents($page));
    printf("markdown=%s\n", str_replace("\n", '\n', $result['markdown']));
    break;

case 'cleanup':
    $removed = 0;
    foreach(page_find(TEST_PAGE_PREFIX, array('is_pagename_only' => true)) as $found) {
	$page = page_read($found['name']);
	if(!empty($page['is_exists']) && page_delete($page))
	    $removed++;
    }
    printf("removed=%d\n", $removed);
    break;

default:
    fprintf(STDERR, "unknown check: %s\n", $check);
    exit(2);
}
?>
