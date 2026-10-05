<?php
/*
 * tests/clone.sh からサイトの中で実行される検証ヘルパ。
 * クローン元とクローン先の両方のサイトで使う。
 *
 *   php clone-helper.php <index.php> <管理者ユーザー名> <検査名> [引数..]
 *
 * 検査名:
 *   guest <権限> [読めないページ..]
 *                     ログインしていない利用者の権限を決める (read / write)。
 *                     ページ名を並べると、そのページだけ読めなくする (keepout)
 *   set-user <名前> <パスワード> <権限> [読めないページ..]
 *                     ダイジェスト認証でログインできる利用者を作る。パスワードは
 *                     テストが毎回作る使い捨て (リポジトリには置かない)
 *   set-allowed-hosts <ホスト>
 *                     サイト設定の CLONE_ALLOWED_HOSTS を書き換える (空なら消して既定に戻す)
 *   make-text <名前> <種別> <ファイル>
 *                     本文のページを書く (markdown / wiki / text)。画面から保存したときと
 *                     同じく、種別の normalize (題名やタグを本文から取る) を通す
 *   make-file <名前> <ファイル> <Content-type>
 *                     種別 file のページを書く (添付と同じ)
 *   exists <名前>     ページがあるか (exists=1/0)
 *   body <名前>       本文を base64 で 1 行に (body=...。無ければ body=(none))
 *   meta <名前> <項目> meta の値 (meta=...)
 *   children <名前>   子孫のページ名を並べる (child=... の行)
 *   cleanup <接頭辞>  その接頭辞のページを消す
 *
 * 結果は `key=value` の行で出す。判定は呼び出し側の shell が行う。
 *
 * 権限と設定を書き換えるので、必ず複製したサイトに対して実行すること。
 */

$argv = $_SERVER['argv'];
if(count($argv) < 4) {
    fprintf(STDERR, "Usage: php clone-helper.php <index.php> <admin> <check> [args..]\n");
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

function test_new_page($name) {
    $page = page_create($name);
    storage_page_read($page);
    page_setup($page);
    return $page;
}

switch($check) {

case 'guest':
    /*
     * ページごとの権限は先に書いたものから当てはめられる
     * (auth_permission_can_action() の fnmatch) ので、読めなくするページを
     * 先に並べ、最後に '*' を置く。
     */
    global $AUTH_PERMISSIONS;
    $permission = array();
    foreach(array_slice($rest, 1) as $pagename)
	$permission[$pagename] = 'keepout';
    $permission['*'] = $rest[0];
    $AUTH_PERMISSIONS[AUTH_GUEST_USERNAME] = count($rest) == 1 ? $rest[0] : $permission;
    printf("saved=%d\n", auth_save_permissions() ? 1 : 0);
    break;

case 'set-user':
    /* 保存するのは上流の password 画面と同じ HA1 (md5(利用者:realm:パスワード)) */
    global $AUTH_PERMISSIONS;
    $username = $rest[0];
    $permission = array();
    foreach(array_slice($rest, 3) as $pagename)
	$permission[$pagename] = 'keepout';
    $permission['*'] = $rest[2];
    $AUTH_PERMISSIONS[$username] = count($rest) == 3 ? $rest[2] : $permission;
    $digest = md5($username . ':' . auth_digest_realm() . ':' . $rest[1]);
    printf("saved=%d\n",
	   setup_write('auth_digest_' . $username, $digest) && auth_save_permissions() ? 1 : 0);
    break;

case 'set-allowed-hosts':
    $contents = setup_read('site');
    $values = ($contents === false) ? array() : unserialize($contents);
    if($rest[0] === '')
	unset($values['CLONE_ALLOWED_HOSTS']);
    else
	$values['CLONE_ALLOWED_HOSTS'] = $rest[0];
    $contents = serialize($values);
    printf("saved=%d\n", setup_write('site', $contents) ? 1 : 0);
    break;

case 'make-text':
    $page = test_new_page($rest[0]);
    $page['meta']['type'] = $rest[1];
    $contents = file_get_contents($rest[2]);
    $normalize = handler_function($page, 'normalize');
    if($normalize !== false)
	$contents = $normalize($page, $contents);
    $ticket = default_value($page['meta']['ticket'], '');
    printf("written=%d\n", page_write($page, $contents, $ticket) ? 1 : 0);
    break;

case 'make-file':
    $page = test_new_page($rest[0]);
    $page['meta']['type'] = 'file';
    $page['meta']['Content-type'] = $rest[2];
    $page['meta']['original_filename'] = page_get_simplename($rest[0]);
    $contents = array('upload_file' => $rest[1]);
    $content_type_handler = &file_get_content_type_handler($page);
    if(!empty($content_type_handler['write']) && function_exists($content_type_handler['write']))
	$content_type_handler['write']($page, $contents);
    $ticket = default_value($page['meta']['ticket'], '');
    printf("written=%d\n", page_write($page, $contents, $ticket) ? 1 : 0);
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

case 'meta':
    $page = page_read($rest[0]);
    printf("meta=%s\n", default_value($page['meta'][$rest[1]], ''));
    break;

case 'children':
    $names = array();
    foreach(page_find($rest[0], array('is_pagename_only' => true)) as $p) {
	if(strpos($p['name'], $rest[0] . '/') === 0)
	    $names[] = $p['name'];
    }
    sort($names);
    foreach($names as $name)
	printf("child=%s\n", $name);
    break;

case 'cleanup':
    $removed = 0;
    foreach(page_find($rest[0], array('is_pagename_only' => true)) as $p) {
	$page = page_read($p['name']);
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
