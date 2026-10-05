<?php
/*
 * tests/post-encoding.sh からサイトの中で実行される検証ヘルパ。
 *
 *   php post-encoding-helper.php <index.php> <管理者ユーザー名> <検査名> [引数..]
 *
 * 検査名:
 *   guest-write       ログインしていない利用者に write 権限を与える
 *   exists <名前>     ページがあるか (exists=1/0)
 *   body <名前>       ページの本文を base64 で 1 行に (body=...。無ければ body=(none))
 *   set-encoding <値> サイト設定の POST_ENCODING を書き換える (空なら消して既定に戻す)
 *   cleanup           このヘルパの対象のページを消す
 *
 * 結果は `key=value` の行で出す。判定は呼び出し側の shell が行う。
 * 本文は改行や制御文字が落ちたかまで比べたいので、手を加えずに base64 で出す。
 *
 * 権限を書き換えるので、必ず複製したサイトに対して実行すること。
 */

$argv = $_SERVER['argv'];
if(count($argv) < 4) {
    fprintf(STDERR, "Usage: php post-encoding-helper.php <index.php> <admin> <check> [args..]\n");
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

/* CLI にはセッションが無いので、page_delete() の auth_check() 用に管理者を差し込む */
$GLOBALS['AUTH_GET_USER_CACHE'] = array('name' => $admin, 'method' => 'digest');
head_tags_init();

define('TEST_PAGE_PREFIX', 'PostEncodingTest');

switch($check) {

case 'guest-write':
    /*
     * HTTP からの検査に資格情報を使わずに済ませる。値を戻すのは
     * 認証より前 (args_get()) なので、誰として通ったかは経路に影響しない。
     */
    global $AUTH_PERMISSIONS;
    $AUTH_PERMISSIONS[AUTH_GUEST_USERNAME] = 'write';
    printf("saved=%d\n", auth_save_permissions() ? 1 : 0);
    break;

case 'exists':
    printf("exists=%d\n", page_is_exists($rest[0]) ? 1 : 0);
    break;

case 'body':
    if(!page_is_exists($rest[0])) {
	printf("body=(none)\n");
	break;
    }
    $page = page_read($rest[0]);
    printf("body=%s\n", base64_encode((string)page_get_contents($page)));
    break;

case 'set-encoding':
    /*
     * サイト設定の画面と同じ保存先 (storage の setup/site) を直接書き換える。
     * 設定は要求のたびにここから読まれるので、次の HTTP から効く。
     */
    $contents = setup_read('site');
    $values = ($contents === false) ? array() : unserialize($contents);
    if($rest[0] === '')
	unset($values['POST_ENCODING']);
    else
	$values['POST_ENCODING'] = $rest[0];
    $contents = serialize($values);
    printf("saved=%d\n", setup_write('site', $contents) ? 1 : 0);
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
