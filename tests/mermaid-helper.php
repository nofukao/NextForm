<?php
/*
 * tests/mermaid.sh からサイトの中で実行される検証ヘルパ。
 * ページの書き込みと設定の書き換えは file-html-helper.php を使う。
 *
 *   php mermaid-helper.php <index.php> <管理者ユーザー名> <検査名> [引数..]
 *
 * 検査名:
 *   preview <名前> <本文>  保存せずにプレビューしたときの <head> の中身を出す。
 *                          編集画面の「プレビュー」と同じく markdown_write() に
 *                          preview を渡す。HTTP で POST すると認証と CSRF の
 *                          検査に当たるので、サイトの中で直接呼ぶ
 */

$argv = $_SERVER['argv'];
if(count($argv) < 4) {
    fprintf(STDERR, "Usage: php mermaid-helper.php <index.php> <admin> <check> [args..]\n");
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
message_init();

switch($check) {
case 'preview':
    $page = page_read($rest[0]);
    $page['meta']['type'] = 'markdown';
    $page['args'] = array('contents' => $rest[1], 'preview' => '1');
    $dom = dom_create_document();
    ob_start();
    markdown_write($page, $dom);
    ob_end_clean();
    echo head_tags();
    break;
default:
    fprintf(STDERR, "unknown check: %s\n", $check);
    exit(2);
}
?>
