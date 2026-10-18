<?php
/*
 * tests/history-restore.sh からサイトの中で実行される検証ヘルパ。
 *
 *   php history-restore-helper.php <index.php> <管理者ユーザー名> <検査名> [引数..]
 *
 * 検査名:
 *   guest <権限>             ログインしていない利用者の権限を決める (read / write)
 *   make-text <名前> <種別> <ファイル>
 *                            本文のページを書く。画面から保存したときと同じく
 *                            種別の normalize (題名やタグを本文から取る) を通す
 *   make-file <名前> <ファイル> <Content-type>
 *                            種別 file のページを書く (添付と同じ)
 *   retype-as-guest <名前> <種別> <ファイル>
 *                            ログインしていない利用者として、種別を変えて書く
 *                            (版の種別が混ざった履歴を作るための準備。索引は後で作り直す)
 *   backup-now <名前>        今の版を履歴に残す (backuptime=その時刻)
 *   backup-times <名前>      履歴の版の時刻を古い順に (times=a,b,..)
 *   backup-type <名前> <時刻> その版の種別 (type=...)
 *   backup-body <名前> <時刻> その版の本文を base64 で (body=...)
 *   rebuild                  検索の索引を作り直す
 *   index-ok <名前>          索引に載っているそのページの ngram が本文と一致するか
 *                            (ok=1/0, missing=件数, extra=件数)
 *   lock <名前> / unlock <名前>
 *   delete <名前>            ページを消す
 *   exists <名前>            ページがあるか (exists=1/0)
 *   body <名前>              本文を base64 で 1 行に (body=...)
 *   meta <名前> <項目>       meta の値 (meta=...)
 *   cleanup                  このヘルパが作ったページを消す
 *
 * 結果は `key=value` の行で出す。判定は呼び出し側の shell が行う。
 *
 * 権限を書き換えるので、必ず複製したサイトに対して実行すること。
 */

$argv = $_SERVER['argv'];
if(count($argv) < 4) {
    fprintf(STDERR, "Usage: php history-restore-helper.php <index.php> <admin> <check> [args..]\n");
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

define('TEST_PAGE_PREFIX', 'RestoreTest');

function test_page($name) {
    $page = page_create($name);
    storage_page_read($page);
    page_setup($page);
    return $page;
}

function test_write_text($name, $type, $file) {
    $page = test_page($name);
    $page['meta']['type'] = $type;
    $contents = file_get_contents($file);
    $normalize = handler_function($page, 'normalize');
    if($normalize !== false)
	$contents = $normalize($page, $contents);
    $ticket = default_value($page['meta']['ticket'], '');
    return page_write($page, $contents, $ticket);
}

/* 索引を全部なめて、あるページ名が載っている ngram を集める (search-index-helper.php と同じ) */
function index_ngrams_of($pagename) {
    $found = array();
    foreach(cache_get_keys_prefix('', SEARCH_INDEX_KEY_PREFIX) as $cachekey) {
	$index = @unserialize((string)cache_get('', $cachekey));
	if($index === false)
	    continue;
	foreach($index as $ngram => $pagenames)
	    if(isset($pagenames[$pagename]))
		$found[$ngram] = true;
    }
    return $found;
}

switch($check) {

case 'guest':
    global $AUTH_PERMISSIONS;
    $AUTH_PERMISSIONS[AUTH_GUEST_USERNAME] = $rest[0];
    printf("saved=%d\n", auth_save_permissions() ? 1 : 0);
    break;

case 'make-text':
    printf("written=%d\n", test_write_text($rest[0], $rest[1], $rest[2]) ? 1 : 0);
    break;

case 'make-file':
    $page = test_page($rest[0]);
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

case 'retype-as-guest':
    $GLOBALS['AUTH_GET_USER_CACHE'] = false;
    printf("written=%d\n", test_write_text($rest[0], $rest[1], $rest[2]) ? 1 : 0);
    break;

case 'backup-now':
    $page = test_page($rest[0]);
    storage_page_backup($page, 0);
    $times = page_get_backup_times($page);
    printf("backuptime=%s\n", empty($times) ? '' : max($times));
    break;

case 'backup-times':
    $page = test_page($rest[0]);
    $times = page_get_backup_times($page);
    sort($times);
    printf("times=%s\n", implode(',', $times));
    break;

case 'backup-type':
    $page = page_read_backup($rest[0], $rest[1]);
    printf("type=%s\n", default_value($page['meta']['type'], ''));
    break;

case 'backup-body':
    $page = page_read_backup($rest[0], $rest[1]);
    printf("body=%s\n", base64_encode((string)page_get_contents($page)));
    break;

case 'rebuild':
    $rebuild_args = array();
    $rebuild_dom = dom_create_document();
    $alltags = array();
    search_index_initializer($rebuild_args, $rebuild_dom, $alltags);
    foreach(search_index_target_pages() as $found)
	search_index_processor($rebuild_args, $rebuild_dom, $alltags, $found);
    search_index_finalizer($rebuild_args, $rebuild_dom, $alltags);
    printf("rebuilt=1\n");
    break;

case 'index-ok':
    $page = page_read($rest[0]);
    $expected = search_page_ngram($page);
    $indexed = index_ngrams_of($rest[0]);
    $missing = count(array_diff_key($expected, $indexed));
    $extra = count(array_diff_key($indexed, $expected));
    printf("ok=%d\nmissing=%d\nextra=%d\n", ($missing == 0 && $extra == 0) ? 1 : 0, $missing, $extra);
    break;

case 'lock':
    $page = page_read($rest[0]);
    printf("locked=%d\n", page_lock($page) ? 1 : 0);
    break;

case 'unlock':
    $page = page_read($rest[0]);
    printf("unlocked=%d\n", page_unlock($page) ? 1 : 0);
    break;

case 'delete':
    $page = page_read($rest[0]);
    printf("deleted=%d\n", page_delete($page) ? 1 : 0);
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

case 'cleanup':
    $removed = 0;
    foreach(page_find(TEST_PAGE_PREFIX, array('is_pagename_only' => true)) as $p) {
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
