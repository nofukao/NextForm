<?php
/*
 * tests/csrf.sh からサイトの中で実行される検証ヘルパ。
 *
 *   php csrf-helper.php <index.php> <管理者ユーザー名> <検査名> [引数..]
 *
 * 検査名:
 *   guest-write            ログインしていない利用者に write 権限を与える
 *   make-page <名前> <本文>  ページを 1 件作る
 *   page-exists <名前>      ページがあるか (exists=1/0)
 *   page-body <名前>        ページの本文 (body=...)
 *   is-locked <名前>        ページがロックされているか (locked=1/0)
 *   cleanup                 このヘルパが作ったページを消す
 *
 *   realm                   いま使われている realm (realm=...)
 *   realm-init              サイト固有の realm を決める (インストーラと同じ動き)
 *   nonce-issue             nonce を 1 つ発行する (nonce=...)
 *   nonce-check <nonce> <nc>  nonce の検証結果 (result=ok|stale|invalid)
 *   digest-auth <nonce> <nc>  組み立てた Authorization で認証できるか (user=名前|false)
 *
 * 結果は `key=value` の行で出す。判定は呼び出し側の shell が行う。
 *
 * 権限を書き換えるので、必ず複製したサイトに対して実行すること。
 */

$argv = $_SERVER['argv'];
if(count($argv) < 4) {
    fprintf(STDERR, "Usage: php csrf-helper.php <index.php> <admin> <check> [args..]\n");
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

define('TEST_PAGE_PREFIX', 'CsrfTest');

/* このヘルパ用の利用者。パスワードは実行のたびに作る (リポジトリには入らない) */
define('TEST_DIGEST_USER', 'csrftestuser');

function test_write($pagename, $contents) {
    $page = page_create($pagename);
    storage_page_read($page);
    page_setup($page);
    $ticket = default_value($page['meta']['ticket'], '');
    return page_write($page, $contents, $ticket);
}

switch($check) {

case 'guest-write':
    /*
     * HTTP からの検査に資格情報を使わずに済ませる。
     * CSRF の検査は認証の有無と無関係な経路に入れるので、
     * ログインしていない利用者で試しても同じ経路を通る。
     */
    global $AUTH_PERMISSIONS;
    $AUTH_PERMISSIONS[AUTH_GUEST_USERNAME] = 'write';
    printf("saved=%d\n", auth_save_permissions() ? 1 : 0);
    break;

case 'make-page':
    printf("written=%d\n", test_write($rest[0], $rest[1]) ? 1 : 0);
    break;

case 'page-exists':
    printf("exists=%d\n", page_is_exists($rest[0]) ? 1 : 0);
    break;

case 'page-body':
    if(!page_is_exists($rest[0])) {
        printf("body=\n");
        break;
    }
    $page = page_read($rest[0]);
    printf("body=%s\n", str_replace("\n", ' ', (string)page_get_contents($page)));
    break;

case 'is-locked':
    $page = page_read($rest[0]);
    printf("locked=%d\n", empty($page['meta']['lock']) ? 0 : 1);
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

case 'realm':
    printf("realm=%s\n", auth_digest_realm());
    break;

case 'realm-init':
    /* インストーラが管理者のパスワードを作る直前に行うのと同じこと */
    auth_realm_init();
    printf("realm=%s\n", auth_digest_realm());
    break;

case 'nonce-issue':
    printf("nonce=%s\n", auth_nonce_create());
    break;

case 'nonce-check':
    printf("result=%s\n", auth_nonce_check($rest[0], $rest[1]));
    break;

case 'digest-auth':
    /*
     * ブラウザが作るのと同じ Authorization ヘッダを組み立てて、
     * auth_digest_get_user() が通すかどうかを見る。
     * パスワードはここで作って捨てる。
     */
    $password = bin2hex(random_bytes(8));
    setup_write('auth_digest_' . TEST_DIGEST_USER,
                md5(TEST_DIGEST_USER . ':' . auth_digest_realm() . ':' . $password));

    $nonce  = $rest[0];
    $nc     = $rest[1];
    $cnonce = 'abcdef0123456789';
    $qop    = 'auth';
    $uri    = '/';

    $a1 = md5(TEST_DIGEST_USER . ':' . auth_digest_realm() . ':' . $password);
    $a2 = md5('GET:' . $uri);
    $response = md5($a1 . ':' . $nonce . ':' . $nc . ':' . $cnonce . ':' . $qop . ':' . $a2);

    $_SERVER['REQUEST_METHOD'] = 'GET';
    $_SERVER['PHP_AUTH_DIGEST'] = sprintf(
        'username="%s", realm="%s", nonce="%s", uri="%s", qop=%s, nc=%s, cnonce="%s", response="%s"',
        TEST_DIGEST_USER, auth_digest_realm(), $nonce, $uri, $qop, $nc, $cnonce, $response);

    $GLOBALS['AUTH_GET_USER_CACHE'] = null;
    $user = auth_digest_get_user();
    printf("user=%s\n", $user === false ? 'false' : $user['name']);
    break;

default:
    fprintf(STDERR, "unknown check: %s\n", $check);
    exit(2);
}
