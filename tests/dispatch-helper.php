<?php
/*
 * tests/dispatch.sh からサイトの中で実行される検証ヘルパ。
 *
 *   php dispatch-helper.php <index.php> <管理者ユーザー名> <検査名> [引数..]
 *
 * 検査名:
 *   make-page <名前> <本文>  ページを 1 件作る
 *   page-body <名前>        ページの本文 (body=...)
 *   normalize-lookup <名前>  handler_function() が normalize を引けるか (func=名前|false)
 *   normalize-apply <名前>   引いた関数で実際に整形できるか
 *                           (changed=1/0, aligned=1/0, warnings=件数)
 *   texts-lookup <名前>      handler_function() が texts を引けるか (func=名前|false)
 *   cleanup                 このヘルパが作ったページを消す
 *
 * 結果は `key=value` の行で出す。判定は呼び出し側の shell が行う。
 *
 * normalize と texts はハンドラに登録されているが URL から呼ぶものではない。
 * ディスパッチから外す変更で、本来の呼び出し経路まで壊していないかを見る。
 */

$argv = $_SERVER['argv'];
if(count($argv) < 4) {
    fprintf(STDERR, "Usage: php dispatch-helper.php <index.php> <admin> <check> [args..]\n");
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

define('TEST_PAGE_PREFIX', 'DispatchTest');

function test_write($pagename, $contents) {
    $page = page_create($pagename);
    storage_page_read($page);
    page_setup($page);
    $ticket = default_value($page['meta']['ticket'], '');
    return page_write($page, $contents, $ticket);
}

switch($check) {

case 'make-page':
    printf("written=%d\n", test_write($rest[0], $rest[1]) ? 1 : 0);
    break;

case 'page-body':
    $page = page_read($rest[0]);
    printf("body=%s\n", str_replace("\n", '/', (string)page_get_contents($page)));
    break;

case 'normalize-lookup':
    $page = page_read($rest[0]);
    $func = handler_function($page, 'normalize');
    printf("func=%s\n", $func === false ? 'false' : $func);
    break;

case 'texts-lookup':
    $page = page_read($rest[0]);
    $func = handler_function($page, 'texts');
    printf("func=%s\n", $func === false ? 'false' : $func);
    break;

case 'normalize-apply':
    /*
     * option/insert.inc option/replace.inc option/listedit.inc
     * option/templateedit.inc が使っているのと同じ呼び方をする。
     * 4 箇所とも同じ書き方なので、ここが通れば 4 つとも通る。
     */
    $page = page_read($rest[0]);
    $contents = page_get_contents($page);
    $func = handler_function($page, 'normalize');
    if($func === false) {
        printf("changed=0\naligned=0\nwarnings=0\n");
        break;
    }
    /*
     * 整形の途中で PHP の警告が出ないことも見る。&include で取り込んだ
     * ページの表の位置を自分の本文に当てて substr() が範囲外になる不具合が
     * あった (handler/wiki.inc の wiki_normalize)。
     */
    $normalize_warnings = 0;
    set_error_handler(function($no, $str, $file, $line) use (&$normalize_warnings) {
        $normalize_warnings++;
        return true;
    });
    $normalized = $func($page, $contents);
    restore_error_handler();
    printf("changed=%d\n", $normalized !== $contents ? 1 : 0);
    /* 表の桁が揃うと、どの行も同じ長さになる */
    $widths = array();
    foreach(explode("\n", $normalized) as $line)
        if(isset($line[0]) && $line[0] === '|')
            $widths[strlen($line)] = true;
    printf("aligned=%d\n", count($widths) === 1 ? 1 : 0);
    printf("result=%s\n", str_replace("\n", '/', $normalized));
    printf("warnings=%d\n", $normalize_warnings);
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
