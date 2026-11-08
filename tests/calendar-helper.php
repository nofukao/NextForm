<?php
/*
 * tests/calendar.sh からサイトの中で実行される検証ヘルパ。
 *
 *   php calendar-helper.php <index.php> <管理者ユーザー名> <検査名> [引数..]
 *
 * 検査名:
 *   write <名前> <種別> <本文>    normalize を通して保存する (written=1/0)
 *
 * 結果は `key=value` の行で出す。判定は呼び出し側の shell が行う。
 * ページを書くので、必ず複製したサイトに対して実行すること。
 */

$argv = $_SERVER['argv'];
if(count($argv) < 4) {
    fprintf(STDERR, "Usage: php calendar-helper.php <index.php> <admin> <check> [args..]\n");
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

default:
    fprintf(STDERR, "unknown check: %s\n", $check);
    exit(2);
}
?>
