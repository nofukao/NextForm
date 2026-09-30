<?php
/*
 * tests/file-html.sh からサイトの中で実行される検証ヘルパ。
 *
 *   php file-html-helper.php <index.php> <管理者ユーザー名> <検査名> [引数..]
 *
 * 検査名:
 *   write-file <名前> <パス> <Content-type>
 *                           種別 file のページとして保存する (written=1/0)。
 *                           添付の画面 (file_write) と同じく、Content-type を
 *                           入れてから種別ごとの write を通す
 *   write <名前> <本文>      種別 markdown として保存する (written=1/0)
 *   set-setting <定数名> <値>
 *                           サイトの設定 (storage/setup/site) に書く (saved=1/0)。
 *                           管理画面の「サイトの設定」と同じ保存先
 *   unset-setting <定数名>  サイトの設定から項目を取り除く (saved=1/0)。
 *                           既定値 (setup.inc) が効く状態に戻す
 *
 * 結果は `key=value` の行で出す。判定は呼び出し側の shell が行う。
 *
 * 設定を書き換えるので、必ず複製したサイトに対して実行すること。
 */

$argv = $_SERVER['argv'];
if(count($argv) < 4) {
    fprintf(STDERR, "Usage: php file-html-helper.php <index.php> <admin> <check> [args..]\n");
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

function test_write($pagename, $contents, $type, $content_type = '') {
    $page = page_create($pagename);
    storage_page_read($page);
    $page['meta']['type'] = $type;
    page_setup($page);
    if($type === 'file') {
	$page['meta']['Content-type'] = $content_type;
	$page['meta']['original_filename'] = basename($pagename);
	$contents = array('upload_file' => $contents);
	$content_type_handler = &file_get_content_type_handler($page);
	if(!empty($content_type_handler['write']) && function_exists($content_type_handler['write']))
	    $content_type_handler['write']($page, $contents);
    }
    $ticket = default_value($page['meta']['ticket'], '');
    return page_write($page, $contents, $ticket);
}

switch($check) {

case 'write-file':
    printf("written=%d\n", test_write($rest[0], $rest[1], 'file', $rest[2]) ? 1 : 0);
    break;

case 'write':
    printf("written=%d\n", test_write($rest[0], $rest[1], 'markdown') ? 1 : 0);
    break;

case 'set-setting':
    $contents = setup_read('site');
    $values = ($contents === false) ? array() : unserialize($contents);
    $values[$rest[0]] = $rest[1];
    $contents = serialize($values);
    printf("saved=%d\n", setup_write('site', $contents) ? 1 : 0);
    break;

case 'unset-setting':
    $contents = setup_read('site');
    $values = ($contents === false) ? array() : unserialize($contents);
    unset($values[$rest[0]]);
    $contents = serialize($values);
    printf("saved=%d\n", setup_write('site', $contents) ? 1 : 0);
    break;

default:
    fprintf(STDERR, "unknown check: %s\n", $check);
    exit(2);
}
?>
