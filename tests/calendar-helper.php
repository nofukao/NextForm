<?php
/*
 * tests/calendar.sh からサイトの中で実行される検証ヘルパ。
 *
 *   php calendar-helper.php <index.php> <管理者ユーザー名> <検査名> [引数..]
 *
 * 検査名:
 *   write <名前> <種別> <本文>    normalize を通して保存する (written=1/0)
 *   delete <名前>                 ページを消す (deleted=1/0)
 *   render <名前> [calendar の値] 管理者 (書ける人) としてページの本文を描いた HTML
 *   views <名前>                  種別 wiki のページを描き、そのあいだに「読んだ」と
 *                                 記録されたページ (views=<空白区切り>)
 *   guest <型>=<権限>..           ログインしていない利用者の権限を、ページ名の型 (fnmatch) ごとに
 *                                 決める。先に書いたものが勝つ。権限は read・write・keepout などの
 *                                 名前か、writeonly (読めないが書ける)。saved=1/0
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

case 'delete':
    $page = page_read($rest[0]);
    printf("deleted=%d\n", page_delete($page) ? 1 : 0);
    break;

case 'render':
    $page = page_read($rest[0]);
    $page['is_main'] = true;
    $page['args'] = array();
    if(isset($rest[1]))
	$page['args']['calendar'] = $rest[1];
    $dom = dom_create_document();
    wiki_convert($page, $dom);
    echo(dom_save_html($dom));
    echo("\n");
    break;

case 'views':
    global $USER_HISTORY_PAGE_VIEW;
    $USER_HISTORY_PAGE_VIEW = array('pages' => array());
    $page = page_read($rest[0]);
    $page['is_main'] = true;
    $page['args'] = array('calendar' => '2026-08');
    wiki_convert($page, dom_create_document());
    printf("views=%s\n", implode(' ', array_keys($USER_HISTORY_PAGE_VIEW['pages'])));
    break;

case 'guest':
    /* ページごとの権限は先に書いたものから当てはめられるので、'*' は最後に書く */
    global $AUTH_PERMISSIONS;
    $permission = array();
    foreach($rest as $rule) {
	list($pattern, $preset) = explode('=', $rule, 2);
	if($preset === 'writeonly')
	    $preset = array('read' => false, 'insert' => true, 'write' => true,
			    'lock' => false, 'export' => false, 'admin' => false);
	$permission[$pattern] = $preset;
    }
    $AUTH_PERMISSIONS[AUTH_GUEST_USERNAME] = $permission;
    printf("saved=%d\n", auth_save_permissions() ? 1 : 0);
    break;

default:
    fprintf(STDERR, "unknown check: %s\n", $check);
    exit(2);
}
?>
