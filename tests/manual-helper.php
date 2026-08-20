<?php
/*
 * tests/manual.sh からサイトの中で実行される検証ヘルパ。
 *
 *   php manual-helper.php <index.php> <管理者ユーザー名> <検査名> [引数..]
 *
 * 検査名:
 *   make-page <名前> <本文>   ページを 1 件作る
 *   page-exists <名前>        page_is_exists() の答え (exists=1/0)
 *   page-head <名前>          本文の先頭 200 バイト (head=...)
 *   page-mtime <名前>         page_get_mtime() の答え (mtime=...)
 *   manual-count              マニュアルのページ数 (count=...)
 *   find-count                page_find() が返すマニュアルの件数 (count=...)
 *   write-try <名前> <本文>   書き込みを試す (written=1/0)
 *   delete-try <名前>         削除を試す (deleted=1/0)
 *   lock-try <名前>           施錠を試す (locked=1/0)
 *   rename-try <旧> <新>      改名を試す (renamed=1/0)
 *   backup-count <名前>       版の数 (count=...)
 *   search <語>               検索の結果件数とマニュアルの件数
 *                             (total=... manual=...)
 *   index-has <名前>          索引にそのページが載っているか (indexed=1/0)
 *   index-ghosts              索引にあって実在しないページの数 (ghosts=...)
 *   cleanup                   このヘルパが作ったページを消す
 *
 * 結果は `key=value` の行で出す。判定は呼び出し側の shell が行う。
 */

$argv = $_SERVER['argv'];
if(count($argv) < 4) {
    fprintf(STDERR, "Usage: php manual-helper.php <index.php> <admin> <check> [args..]\n");
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

define('TEST_PAGE_PREFIX', 'ManualTest');

function test_write($pagename, $contents) {
    $page = page_create($pagename);
    storage_page_read($page);
    page_setup($page);
    $ticket = default_value($page['meta']['ticket'], '');
    return page_write($page, $contents, $ticket);
}

/* マニュアルの名前空間に入るページ名か */
function test_is_manual($pagename) {
    return $pagename === MANUAL_PAGENAME ||
	strpos($pagename, MANUAL_PAGENAME . '/') === 0;
}

switch($check) {

case 'make-page':
    printf("written=%d\n", test_write($rest[0], $rest[1]) ? 1 : 0);
    break;

case 'page-exists':
    printf("exists=%d\n", page_is_exists($rest[0]) ? 1 : 0);
    break;

case 'page-head':
    $page = page_read($rest[0]);
    $body = (string)page_get_contents($page);
    printf("head=%s\n", str_replace("\n", '/', substr($body, 0, 200)));
    break;

case 'page-mtime':
    printf("mtime=%s\n", (string)page_get_mtime($rest[0]));
    break;

case 'manual-count':
    printf("count=%d\n", count(manual_get_pages()));
    break;

case 'find-count':
    /* page_find() はマニュアルを返さない。返すようになったら気づけるようにする */
    $count = 0;
    foreach(page_find('', array('is_pagename_only' => true)) as $p) {
	$pagename = is_array($p) ? $p['name'] : $p;
	if(test_is_manual($pagename))
	    $count++;
    }
    printf("count=%d\n", $count);
    break;

case 'write-try':
    printf("written=%d\n", test_write($rest[0], $rest[1]) ? 1 : 0);
    break;

case 'delete-try':
    $page = page_read($rest[0]);
    printf("deleted=%d\n", page_delete($page) ? 1 : 0);
    break;

case 'lock-try':
    $page = page_read($rest[0]);
    printf("locked=%d\n", page_lock($page, $admin) ? 1 : 0);
    break;

case 'rename-try':
    printf("renamed=%d\n", page_rename($rest[0], $rest[1]) ? 1 : 0);
    break;

case 'backup-count':
    $page = page_read($rest[0]);
    printf("count=%d\n", count(page_get_backup_times($page)));
    break;

case 'search':
    $query = array($rest[0]);
    $results = search($query, array());
    $manual = 0;
    foreach($results as $pagename => $dummy)
	if(test_is_manual($pagename))
	    $manual++;
    printf("total=%d\nmanual=%d\n", count($results), $manual);
    break;

case 'index-has':
    $collected = search_index_collect();
    printf("indexed=%d\n", isset($collected['indexed_pages'][$rest[0]]) ? 1 : 0);
    break;

case 'index-ghosts':
    /*
     * 索引にあって実在しないページ。マニュアルを索引に載せる以上、
     * 整合検査がマニュアルを「幽霊」と呼ばないことまで含めて確かめる。
     */
    $check_result = search_index_check(false);
    printf("ghosts=%d\n", count($check_result['ghosts']));
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
