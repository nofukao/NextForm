<?php
/*
 * tests/search-query.sh からサイトの中で実行される検証ヘルパ。
 *
 *   php search-query-helper.php <index.php> <管理者ユーザー名> <検査名> [引数..]
 *
 * 検査名:
 *   write <名前> <種別> <本文>     ページを 1 件書く (種別は wiki / markdown)
 *   set-mtime <名前> <YYYY-MM-DD>  ページの更新日時をその日の正午にする
 *   rebuild                        検索の索引をまっさらから作り直す
 *   guest <権限の JSON>            ログインしていない利用者の権限を差し替える
 *   set-password <利用者> <パスワード>
 *                                  HTTP Digest でログインできる利用者を作る (権限は admin)
 *   manual-title <ページ名>        組み込みマニュアルのページの meta の題名 (title=...)
 *   manual-stale-cache <ページ名>  そのマニュアルのページの検索用キャッシュを、題名の無い
 *                                  古い中身で作り直す (時刻は今。アップグレードの前に作られた
 *                                  キャッシュのまね)
 *   manual-cache <ページ名>        そのマニュアルのページの検索用キャッシュの題名 (title=...、無ければ (none))
 *   manual-rebuild                 マニュアルのキャッシュを作り直させる (ソースが新しくなったまね)
 *   index-has <語> <ページ名>      索引で、そのページにその語の 2-gram が全部あるか (has=1/0)。
 *                                  マニュアルの題名が索引に入ったかを見る
 *   index-forget <ページ名>        そのページを索引から外し、マニュアルの索引の控えも消す
 *                                  (次にマニュアルを作り直すと、全部を入れ直す)
 *   highlights-keep <ページ名>     検索用キャッシュを消してから、開いたページの印を作る
 *                                  (search_page_highlights)。表示中のページの題名や &calc の
 *                                  値を書き換えないか (kept=1/0)
 *
 * 結果は `key=value` の行で出す。判定は呼び出し側の shell が行う。
 *
 * 権限を書き換えるので、必ず複製したサイトに対して実行すること。
 */
$argv = $_SERVER['argv'];
if(count($argv) < 4) {
    fprintf(STDERR, "Usage: php search-query-helper.php <index.php> <admin> <check> [args..]\n");
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

/* manual-rebuild は CLI で未ログインのまま動かす (アップグレードと同じ) */
if($check !== 'manual-rebuild')
    $GLOBALS['AUTH_GET_USER_CACHE'] = array('name' => $admin, 'method' => 'digest');
head_tags_init();

function test_write($pagename, $contents, $type) {
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
    printf("written=%d\n", test_write($rest[0], $rest[2], $rest[1]) ? 1 : 0);
    break;

case 'set-mtime':
    $page = page_read($rest[0]);
    $time = strtotime($rest[1] . ' 12:00:00');
    printf("touched=%d\n", touch($page['storage_filepath'], $time) ? 1 : 0);
    /* 検索用キャッシュは mtime で判定するので、古い時刻にしたら作り直させる */
    cache_delete($rest[0], SEARCH_TEXTS_CACHE_KEY);
    break;

case 'rebuild':
    $rebuild_args = array();
    $rebuild_dom = dom_create_document();
    $alltags = array();
    search_index_initializer($rebuild_args, $rebuild_dom, $alltags);
    $indexed = 0;
    foreach(search_index_target_pages() as $found) {
	search_index_processor($rebuild_args, $rebuild_dom, $alltags, $found);
	$indexed++;
    }
    search_index_finalizer($rebuild_args, $rebuild_dom, $alltags);
    printf("indexed=%d\n", $indexed);
    break;

case 'guest':
    global $AUTH_PERMISSIONS;
    $permission = json_decode($rest[0], true);
    $AUTH_PERMISSIONS[AUTH_GUEST_USERNAME] = $permission === null ? $rest[0] : $permission;
    printf("saved=%d\n", auth_save_permissions() ? 1 : 0);
    break;

case 'set-password':
    global $AUTH_PERMISSIONS;
    $digest = md5($rest[0] . ':' . auth_digest_realm() . ':' . $rest[1]);
    $AUTH_PERMISSIONS[$rest[0]] = 'admin';
    printf("saved=%d\n",
	   setup_write('auth_digest_' . $rest[0], $digest) && auth_save_permissions() ? 1 : 0);
    break;

case 'manual-title':
    $page = page_read($rest[0]);
    printf("title=%s\n", isset($page['meta']['title']) ? $page['meta']['title'] : '(none)');
    break;

case 'manual-stale-cache':
    $page = page_read($rest[0]);
    $texts = search_page_texts($page);
    $texts['title'] = $page['name'];
    cache_serialized_create($page['name'], SEARCH_TEXTS_CACHE_KEY, $texts);
    printf("written=1\n");
    break;

case 'manual-cache':
    $texts = cache_serialized_get($rest[0], SEARCH_TEXTS_CACHE_KEY);
    printf("title=%s\n", is_array($texts) && isset($texts['title']) ? $texts['title'] : '(none)');
    break;

case 'manual-rebuild':
    /*
     * マニュアルのキャッシュの印 (版とソースの時刻) を古くして、次に読んだときに
     * 作り直させる。アップグレードで app/ が入れ替わったときと同じ経路を通す。
     */
    $entry = cache_serialized_get('', MANUAL_CACHE_KEY);
    if(is_array($entry)) {
	$entry['source_mtime'] = 0;
	cache_serialized_create('', MANUAL_CACHE_KEY, $entry);
    }
    $GLOBALS['MANUAL_PAGES_CACHE'] = null;
    printf("pages=%d\n", count(manual_get_pages()));
    break;

case 'index-has':
    $word = strtolower($rest[0]);
    $ngrams = array();
    search_word_to_ngrams($word, $ngrams);
    $has = 1;
    foreach($ngrams as $ngram => $dummy) {
	$index = cache_serialized_get('', SEARCH_INDEX_KEY_PREFIX . search_ngram_key($ngram));
	if(!is_array($index) || empty($index[$ngram][$rest[1]]))
	    $has = 0;
    }
    printf("has=%d\n", $has);
    break;

case 'index-forget':
    $all = array();
    foreach(cache_get_keys_prefix('', SEARCH_INDEX_KEY_PREFIX) as $cachekey) {
	$index = cache_serialized_get('', $cachekey);
	if(!is_array($index))
	    continue;
	foreach($index as $ngram => $pagenames)
	    if(isset($pagenames[$rest[0]]))
		$all[$ngram] = true;
    }
    search_index_remove($rest[0], $all);
    cache_delete('', MANUAL_INDEX_CACHE_KEY);
    printf("removed=%d\n", count($all));
    break;

case 'highlights-keep':
    cache_delete($rest[0], SEARCH_TEXTS_CACHE_KEY);
    $page = page_read($rest[0]);
    $page['title'] = 'kept-title';
    $page['calc'] = array('vars' => array('kept' => 1));
    search_page_highlights($page, search_query_parse('re:sq'));
    printf("kept=%d\n", isset($page['title']) && $page['title'] === 'kept-title' &&
	   isset($page['calc']['vars']) && $page['calc']['vars'] === array('kept' => 1) ? 1 : 0);
    break;

default:
    fprintf(STDERR, "unknown check: %s\n", $check);
    exit(2);
}
