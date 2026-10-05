<?php
/*
 * tests/attach-download.sh からサイトの中で実行される検証ヘルパ。
 *
 *   php attach-download-helper.php <index.php> <管理者ユーザー名> <検査名> [引数..]
 *
 * 検査名:
 *   guest-write [読めないページ..]
 *                     ログインしていない利用者に write 権限を与える。
 *                     ページ名を並べると、そのページだけ読めなくする (keepout)
 *   set-encoding <値> サイト設定の POST_ENCODING を書き換える (空なら消して既定に戻す)
 *   zip-length <ページ名> <ファイル名..>
 *                     そのページの添付を ZIP にしたときの大きさ (zip_prepare() の値。length=...)
 *   mtime-text <ページ名>
 *                     そのページの保存日時を、サイトの日時の書式 (TIME_FORMAT) で出す (mtime=...)
 *   cleanup           このヘルパの対象のページを消す
 *
 * 結果は `key=value` の行で出す。判定は呼び出し側の shell が行う。
 *
 * 権限を書き換えるので、必ず複製したサイトに対して実行すること。
 */

$argv = $_SERVER['argv'];
if(count($argv) < 4) {
    fprintf(STDERR, "Usage: php attach-download-helper.php <index.php> <admin> <check> [args..]\n");
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

define('TEST_PAGE_PREFIX', 'AttachDownloadTest');

switch($check) {

case 'guest-write':
    /*
     * HTTP からの検査に資格情報を使わずに済ませる。ページごとの権限は
     * 先に書いたものから当てはめられる (auth_permission_can_action() の fnmatch)
     * ので、読めなくするページを先に並べ、最後に '*' を置く。
     */
    global $AUTH_PERMISSIONS;
    $permission = array();
    foreach($rest as $pagename)
	$permission[$pagename] = 'keepout';
    $permission['*'] = 'write';
    $AUTH_PERMISSIONS[AUTH_GUEST_USERNAME] = empty($rest) ? 'write' : $permission;
    printf("saved=%d\n", auth_save_permissions() ? 1 : 0);
    break;

case 'set-encoding':
    $contents = setup_read('site');
    $values = ($contents === false) ? array() : unserialize($contents);
    if($rest[0] === '')
	unset($values['POST_ENCODING']);
    else
	$values['POST_ENCODING'] = $rest[0];
    $contents = serialize($values);
    printf("saved=%d\n", setup_write('site', $contents) ? 1 : 0);
    break;

case 'zip-length':
    /*
     * Content-Length に出す値。web サーバによっては Content-Length を落として
     * chunked で返す (この開発環境の Apache + PHP-FPM がそう) ので、HTTP の
     * ヘッダだけでは確かめられない。
     */
    $entries = array();
    foreach(array_slice($rest, 1) as $basename) {
	$file_page = page_read($rest[0] . '/' . $basename);
	$entries[] = array(
	    'name' => $basename,
	    'mtime' => $file_page['mtime'],
	    'open' => function() use ($file_page) { return page_open_contents($file_page); });
    }
    $length = zip_prepare($entries);
    printf("length=%s\n", $length === false ? 'false' : $length);
    break;

case 'mtime-text':
    $page = page_read($rest[0]);
    printf("mtime=%s\n", nf_date(TIME_FORMAT, $page['mtime']));
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
