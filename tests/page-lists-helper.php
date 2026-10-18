<?php
/*
 * tests/page-lists.sh からサイトの中で実行される検証ヘルパ。
 *
 *   php page-lists-helper.php <index.php> <管理者ユーザー名> <検査名> [引数..]
 *
 * 検査名:
 *   make <名前> [タグ..]     種別 wiki のページを書く (タグはタグの画面と同じ write_tag で付ける)
 *   delete <名前>            ページを消す
 *   rename <元の名前> <新しい名前>
 *                            ページの名前を変える (子ページは含めない)
 *   alltag <タグ>            タグ一覧の件数の控え (count=...。無ければ 0)
 *   alltags                  控えの全部を `タグ=件数` の行で (タグ名の順)
 *   rebuild                  検索の索引とタグ一覧の件数を作り直す
 *   guest <権限>             ログインしていない利用者の権限を決める (read / admin など)
 *   guest-keepout <型..>     ログインしていない利用者は、型に当たるページを読めず、
 *                            ほかは読める (ページごとの権限)
 *   cleanup                  このヘルパが作ったページを履歴ごと消す
 *
 * 結果は `key=value` の行で出す。判定は呼び出し側の shell が行う。
 *
 * 権限を書き換えるので、必ず複製したサイトに対して実行すること。
 */
$argv = $_SERVER['argv'];
if(count($argv) < 4) {
    fprintf(STDERR, "Usage: php page-lists-helper.php <index.php> <admin> <check> [args..]\n");
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

/* CLI にはセッションが無いので、page_write() の auth_check() 用に管理者を差し込む */
$GLOBALS['AUTH_GET_USER_CACHE'] = array('name' => $admin, 'method' => 'digest');
head_tags_init();

define('TEST_PAGE_PREFIX', 'ListTest');

function test_page($name) {
    $page = page_create($name);
    storage_page_read($page);
    page_setup($page);
    return $page;
}

switch($check) {

case 'make':
    $page = test_page($rest[0]);
    $page['meta']['type'] = 'wiki';
    $page['write_tag'] = implode(' ', array_slice($rest, 1));
    $ticket = default_value($page['meta']['ticket'], '');
    $contents = "本文 " . $rest[0] . "\n";
    printf("written=%d\n", page_write($page, $contents, $ticket) ? 1 : 0);
    break;

case 'delete':
    $page = page_read($rest[0]);
    printf("deleted=%d\n", page_delete($page) ? 1 : 0);
    break;

case 'rename':
    printf("renamed=%d\n", page_rename($rest[0], $rest[1], false) ? 1 : 0);
    break;

case 'alltag':
    $alltags = tag_alltags_get();
    printf("count=%d\n", isset($alltags[$rest[0]]) ? $alltags[$rest[0]] : 0);
    break;

case 'alltags':
    $alltags = tag_alltags_get();
    ksort($alltags, SORT_STRING);
    foreach($alltags as $tag => $num)
	printf("%s=%d\n", $tag, $num);
    break;

case 'rebuild':
    $rebuild_args = array();
    $rebuild_dom = dom_create_document();
    $alltags = array();
    search_index_initializer($rebuild_args, $rebuild_dom, $alltags);
    foreach(search_index_target_pages() as $found)
	search_index_processor($rebuild_args, $rebuild_dom, $alltags, $found);
    search_index_finalizer($rebuild_args, $rebuild_dom, $alltags);
    /* 画面の再構築 (search_index_write) と同じく、数え直した件数を最後に書き戻す */
    tag_alltags_set($alltags);
    printf("rebuilt=1\n");
    break;

case 'guest':
    global $AUTH_PERMISSIONS;
    $AUTH_PERMISSIONS[AUTH_GUEST_USERNAME] = $rest[0];
    printf("saved=%d\n", auth_save_permissions() ? 1 : 0);
    break;

case 'guest-keepout':
    /* ページごとの権限は先に書いたものから当てはめられるので、'*' を最後に置く */
    global $AUTH_PERMISSIONS;
    $permission = array();
    foreach($rest as $pattern)
	$permission[$pattern] = 'keepout';
    $permission['*'] = 'read';
    $AUTH_PERMISSIONS[AUTH_GUEST_USERNAME] = $permission;
    printf("saved=%d\n", auth_save_permissions() ? 1 : 0);
    break;

case 'cleanup':
    $removed = 0;
    $names = array();
    foreach(page_find(TEST_PAGE_PREFIX, array('is_pagename_only' => true)) as $p)
	$names[] = $p['name'];
    foreach(page_find_deleted(TEST_PAGE_PREFIX) as $p)
	$names[] = $p['name'];
    foreach(array_unique($names) as $name) {
	$page = test_page($name);
	if(page_truncate($page))
	    $removed++;
    }
    printf("removed=%d\n", $removed);
    break;

default:
    fprintf(STDERR, "unknown check: %s\n", $check);
    exit(2);
}
?>
