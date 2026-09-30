<?php
/*
 * tests/export.sh からサイトの中で実行される検証ヘルパ。
 *
 *   php export-helper.php <index.php> <管理者ユーザー名> <検査名> [引数..]
 *
 * 検査名:
 *   export <設定表のページ> <書き出し先>
 *                   export_pages() を強制 (force) で走らせる。書き出し先は
 *                   EXPORT_DIR_PATH から見たディレクトリ。1 行 1 出力で
 *                   output<TAB>ページ名<TAB>ファイル名<TAB>結果 を出し、
 *                   画面に出るはずだったエラーを error<TAB>文 で出す
 *
 * **ボタン (?option=export) を通さず、書き出しの本体を直に呼ぶ。**
 * ボタンの経路は管理者の資格情報が要るうえ、書き出しの中身は
 * export_pages() がすべて決める。ボタンの側は利用者が画面で確かめる。
 *
 * ページを書き出すので、必ず複製したサイトに対して実行すること。
 */

$argv = $_SERVER['argv'];
if(count($argv) < 4) {
    fprintf(STDERR, "Usage: php export-helper.php <index.php> <admin> <check> [args..]\n");
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
/* main() の中でしか呼ばれない。無いと書き出しのエラーの報告で落ちる */
message_init();
head_tags_init();

switch($check) {

case 'export':
    $export_data = export_pages($rest[0], $rest[1], true);
    foreach($export_data as $export_datum) {
	foreach($export_datum['outputs'] as $output) {
	    printf("output\t%s\t%s\t%s\n", $export_datum['pagename'],
		   default_value($output['filepath'], ''),
		   default_value($output['status'], default_value($export_datum['status'], '')));
	}
    }
    /* 画面に出るはずだったエラー (message_error()) を拾う */
    global $messages_dom;
    if($messages_dom !== null) {
	foreach($messages_dom->childNodes as $li) {
	    if($li instanceof DOMElement && $li->getAttribute('class') === 'error')
		printf("error\t%s\n", str_replace("\n", ' ', $li->textContent));
	}
    }
    break;

default:
    fprintf(STDERR, "unknown check: %s\n", $check);
    exit(2);
}
?>
