<?php
/*
 * tests/wiki-to-markdown.sh からサイトの中で実行される検証ヘルパ。
 *
 *   php wiki-to-markdown-helper.php <index.php> <管理者ユーザー名> <検査名> [引数..]
 *
 * 検査名:
 *   guest <権限>              ログインしていない利用者の権限を決める (read / write)
 *   write-wiki <名前> <ファイル> 種別 wiki のページを書く (画面から保存したときと同じく normalize を通す)
 *   write-wiki-guest <名前> <ファイル> 同じく、ログインしていない利用者として書く
 *   attach <名前> <ファイル> <Content-type> 種別 file のページ (添付) を書く
 *   set-tags <名前> <タグ..>   タグの画面と同じ経路でタグを付ける
 *   set-meta-title <名前> <題名> メタ情報の画面と同じ経路で題名を付ける
 *   meta <名前> <項目>        meta の値 (meta=...)
 *   tags <名前>               タグ (tags=空白区切り)
 *   body <名前>               本文を base64 で (body=...)
 *   backup-times <名前>       履歴の版の時刻を古い順に (times=a,b,..)
 *   backup-type <名前> <時刻> その版の種別 (type=...)
 *   backup-body <名前> <時刻> その版の本文を base64 で (body=...)
 *   index-ok <名前>           索引のそのページの ngram が本文と一致するか (ok=1/0)
 *   lock <名前> / unlock <名前>
 *   cleanup                   このヘルパが作ったページを消す
 *
 * 結果は `key=value` の行で出す。判定は呼び出し側の shell が行う。
 * 権限を書き換えるので、必ず複製したサイトに対して実行すること。
 */

$argv = $_SERVER['argv'];
if(count($argv) < 4) {
    fprintf(STDERR, "Usage: php wiki-to-markdown-helper.php <index.php> <admin> <check> [args..]\n");
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

define('TEST_PAGE_PREFIX', 'WikiMdTest');

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

case 'write-wiki-guest':
    $GLOBALS['AUTH_GET_USER_CACHE'] = false;
    /* FALLTHROUGH */
case 'write-wiki':
    $page = page_create($rest[0]);
    storage_page_read($page);
    page_setup($page);
    $page['meta']['type'] = 'wiki';
    $contents = file_get_contents($rest[1]);
    $normalize = handler_function($page, 'normalize');
    if($normalize !== false)
	$contents = $normalize($page, $contents);
    $ticket = default_value($page['meta']['ticket'], '');
    printf("written=%d\n", page_write($page, $contents, $ticket) ? 1 : 0);
    break;

case 'attach':
    /* tests/markdown-helper.php の attach と同じ */
    $page = page_create($rest[0]);
    storage_page_read($page);
    page_setup($page);
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

case 'set-tags':
    $page = page_read($rest[0]);
    printf("written=%d\n", tag_set_page_tags($page, array_slice($rest, 1)) ? 1 : 0);
    break;

case 'set-meta-title':
    $page = page_read($rest[0]);
    $page['title'] = $rest[1];
    $page['keep_mtime'] = true;
    $fp = page_open_contents($page);
    printf("written=%d\n", page_write($page, $fp, $page['meta']['ticket']) ? 1 : 0);
    break;

case 'meta':
    $page = page_read($rest[0]);
    printf("meta=%s\n", default_value($page['meta'][$rest[1]], ''));
    break;

case 'tags':
    $page = page_read($rest[0]);
    printf("tags=%s\n", implode(' ', tag_get_page_tags($page)));
    break;

case 'body':
    if(!page_is_exists($rest[0])) {
	printf("body=(none)\n");
	break;
    }
    $page = page_read($rest[0]);
    printf("body=%s\n", base64_encode((string)page_get_contents($page)));
    break;

case 'backup-times':
    $page = page_read($rest[0]);
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

case 'index-ok':
    $page = page_read($rest[0]);
    $expected = search_page_ngram($page);
    $indexed = index_ngrams_of($rest[0]);
    printf("ok=%d\n", (count(array_diff_key($expected, $indexed)) == 0 &&
			count(array_diff_key($indexed, $expected)) == 0) ? 1 : 0);
    break;

case 'lock':
    $page = page_read($rest[0]);
    printf("locked=%d\n", page_lock($page) ? 1 : 0);
    break;

case 'unlock':
    $page = page_read($rest[0]);
    printf("unlocked=%d\n", page_unlock($page) ? 1 : 0);
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
