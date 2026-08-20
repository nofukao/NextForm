<?php
/*
 * tests/search-cache.sh からサイトの中で実行される検証ヘルパ。
 *
 *   php search-cache-helper.php <index.php> <管理者ユーザー名> <検査名> [引数..]
 *
 * 検査名:
 *   write-page <名前> <本文>   ページを 1 件書く
 *   delete-page <名前>         ページを 1 件消す
 *   result <問い合わせ>        検索結果 (count= と digest=)。digest は順序込みの要約
 *   time <問い合わせ>          検索にかかった時間 (ms=)
 *   cache-count                テキストキャッシュのファイル数
 *   cache-of <名前>            そのページのキャッシュがあるか (exists=1/0)
 *   corrupt-cache <名前>       そのページのキャッシュを壊す
 *   clear-cache                テキストキャッシュを全部消す
 *   index-add <名前>           索引を作り直す処理を 1 ページ分だけ走らせる
 *   cleanup                    このヘルパが作ったページを消す
 *
 * 結果は `key=value` の行で出す。判定は呼び出し側の shell が行う。
 *
 * キャッシュを壊す検査を含むので、必ず複製したサイトに対して実行すること。
 */

$argv = $_SERVER['argv'];
if(count($argv) < 4) {
    fprintf(STDERR, "Usage: php search-cache-helper.php <index.php> <admin> <check> [args..]\n");
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

define('TEST_PAGE_PREFIX', 'SearchCacheTest');

function test_write($pagename, $contents) {
    $page = page_create($pagename);
    storage_page_read($page);
    page_setup($page);
    $ticket = default_value($page['meta']['ticket'], '');
    return page_write($page, $contents, $ticket);
}

/* テキストキャッシュのファイルを数える。ページごとに 1 つ作られる */
function texts_cache_paths() {
    $found = array();
    foreach(page_find('', array('is_pagename_only' => true)) as $p) {
        $pagename = is_array($p) ? $p['name'] : $p;
        $path = cache_get_as_filepath($pagename, SEARCH_TEXTS_CACHE_KEY);
        if($path !== false)
            $found[$pagename] = $path;
    }
    return $found;
}

switch($check) {

case 'write-page':
    printf("written=%d\n", test_write($rest[0], $rest[1]) ? 1 : 0);
    break;

case 'delete-page':
    $page = page_read($rest[0]);
    printf("deleted=%d\n", page_delete($page) ? 1 : 0);
    break;

case 'result':
    /*
     * 件数だけでなく順序も見る。キャッシュを挟んだせいでスコアが変わると
     * 並び順が動く。件数が同じでも順序が違えば別の結果なので、
     * ページ名とスコアを繋いだものを要約にする。
     */
    $query = search_parse_query($rest[0]);
    $results = search($query);
    $parts = array();
    foreach($results as $pagename => $result)
        $parts[] = $pagename . ':' . $result['score'] . ':' . count($result['snippets']);
    printf("count=%d\n", count($results));
    printf("digest=%s\n", md5(implode('|', $parts)));
    printf("first=%s\n", count($parts) ? $parts[0] : '');
    break;

case 'time':
    $query = search_parse_query($rest[0]);
    $t = hrtime(true);
    $results = search($query);
    printf("ms=%d\n", (int)round((hrtime(true) - $t) / 1000000));
    printf("count=%d\n", count($results));
    break;

case 'cache-count':
    printf("count=%d\n", count(texts_cache_paths()));
    break;

case 'cache-of':
    printf("exists=%d\n",
           cache_get_as_filepath($rest[0], SEARCH_TEXTS_CACHE_KEY) === false ? 0 : 1);
    break;

case 'corrupt-cache':
    $path = cache_get_as_filepath($rest[0], SEARCH_TEXTS_CACHE_KEY);
    if($path === false) {
        printf("corrupted=0\n");
        break;
    }
    /* unserialize できない中身にする。時刻はキャッシュが新しいままにしておく */
    file_put_contents($path, 'this is not serialized data');
    printf("corrupted=1\n");
    break;

case 'clear-cache':
    $removed = 0;
    foreach(texts_cache_paths() as $pagename => $path) {
        cache_delete($pagename, SEARCH_TEXTS_CACHE_KEY);
        $removed++;
    }
    printf("removed=%d\n", $removed);
    break;

case 'index-add':
    /*
     * 索引を作る経路がテキストキャッシュを作らないことを見る。
     * page_write() は cache_delete() のあとに旧本文の ngram を計算するので、
     * ここでキャッシュを書くと古い内容が残ってしまう。
     */
    $page = page_read($rest[0]);
    $before = count(texts_cache_paths());
    search_page_index_add($page);
    $after = count(texts_cache_paths());
    printf("before=%d\nafter=%d\n", $before, $after);
    break;

case 'rebuild':
    /*
     * 複製元に残っているずれを持ち込まないよう、まっさらな索引から始める。
     * search-index.sh と同じ理由。
     */
    $rebuild_args = array();
    $rebuild_dom = dom_create_document();
    $alltags = array();
    search_index_initializer($rebuild_args, $rebuild_dom, $alltags);
    $indexed = 0;
    /* 本物の再構築 (option/search_index.inc) と同じ一覧を見る */
    foreach(search_index_target_pages() as $found) {
        search_index_processor($rebuild_args, $rebuild_dom, $alltags, $found);
        $indexed++;
    }
    search_index_finalizer($rebuild_args, $rebuild_dom, $alltags);
    printf("indexed=%d\n", $indexed);
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

default:
    fprintf(STDERR, "unknown check: %s\n", $check);
    exit(2);
}
