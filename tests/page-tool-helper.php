<?php
/*
 * tests/page-tool.sh からサイトの中で実行される検証ヘルパ。
 *
 *   php page-tool-helper.php <index.php> <管理者ユーザー名> <検査名> [引数..]
 *
 * 検査名:
 *   guest <権限>               ログインしていない利用者の権限を決める (read / write)
 *   set-user <名前> <権限> [<パスワード>]
 *                              利用者を足す。パスワードを渡せばダイジェストも保存する。
 *                              権限は read などの名前か、ページの型ごとの JSON
 *                              ({"Secret":"keepout","*":"write"})
 *   digest-leaked <ファイル>   どれかの利用者のダイジェストがファイルに出ているか (leaked=1/0)
 *   meta <名前>                meta を JSON で 1 行に。保存のたびに変わるもの
 *                              (ticket・btime・更新者・作成者) は除く
 *   body-sha <名前>            本文の sha256 (sha=...)。無ければ空
 *   exists <名前>              ページがあるか (exists=1/0)
 *   backups <名前>             履歴の版の数 (backups=...)
 *   lock <名前>                管理者として施錠する (locked=1/0)
 *   chunked <名前>             ページのディレクトリが区切られているか (chunked=1/0)
 *
 * 結果は `key=value` の行で出す。判定は呼び出し側の shell が行う。
 *
 * 権限を書き換えるので、必ず複製したサイトに対して実行すること。
 */

$argv = $_SERVER['argv'];
if(count($argv) < 4) {
    fprintf(STDERR, "Usage: php page-tool-helper.php <index.php> <admin> <check> [args..]\n");
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

/* CLI にはセッションが無いので、page_lock() の auth_check() 用に管理者を差し込む */
$GLOBALS['AUTH_GET_USER_CACHE'] = array('name' => $admin, 'method' => 'digest');
head_tags_init();
message_init();

function test_page($name) {
    $page = page_create($name);
    storage_page_read($page);
    page_setup($page);
    return $page;
}

switch($check) {

case 'guest':
    global $AUTH_PERMISSIONS;
    $AUTH_PERMISSIONS[AUTH_GUEST_USERNAME] = $rest[0];
    printf("saved=%d\n", auth_save_permissions() ? 1 : 0);
    break;

case 'set-user':
    /* パスワードはテストが毎回作る使い捨てで、リポジトリには置かない */
    global $AUTH_PERMISSIONS;
    $AUTH_PERMISSIONS[$rest[0]] = ($rest[1][0] === '{') ? json_decode($rest[1], true) : $rest[1];
    $saved = auth_save_permissions();
    if(isset($rest[2]))
	$saved = $saved && setup_write('auth_digest_' . $rest[0],
				       md5($rest[0] . ':' . auth_digest_realm() . ':' . $rest[2]));
    printf("saved=%d\n", $saved ? 1 : 0);
    break;

case 'digest-leaked':
    global $AUTH_PERMISSIONS;
    $text = file_get_contents($rest[0]);
    $leaked = 0;
    foreach(array_keys($AUTH_PERMISSIONS) as $username) {
	$digest = auth_digest_get_digest($username);
	if($digest !== false && $digest !== '' && strpos($text, $digest) !== false)
	    $leaked = 1;
    }
    printf("leaked=%d\n", $leaked);
    break;

case 'meta':
    $page = test_page($rest[0]);
    $meta = $page['meta'];
    foreach(array('ticket', 'btime', 'last_modify_user', 'create_user') as $name)
	unset($meta[$name]);
    ksort($meta);
    printf("meta=%s\n", json_encode($meta, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES));
    break;

case 'body-sha':
    $page = test_page($rest[0]);
    $contents = empty($page['is_exists']) ? false : page_get_contents($page);
    printf("sha=%s\n", $contents === false ? '' : hash('sha256', $contents));
    break;

case 'exists':
    printf("exists=%d\n", storage_page_is_exists($rest[0]) ? 1 : 0);
    break;

case 'backups':
    $page = test_page($rest[0]);
    printf("backups=%d\n", count(page_get_backup_times($page)));
    break;

case 'lock':
    $page = test_page($rest[0]);
    printf("locked=%d\n", page_lock($page, $admin) ? 1 : 0);
    break;

case 'chunked':
    printf("chunked=%d\n",
	   strpos(storage_page_get_dirpath($rest[0]), '_/') !== false ? 1 : 0);
    break;

default:
    fprintf(STDERR, "unknown check: %s\n", $check);
    exit(2);
}
?>
