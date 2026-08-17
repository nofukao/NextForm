<?php
/*
 * tests/search-index.sh からサイトの中で実行される検証ヘルパ。
 *
 *   php search-index-helper.php <index.php> <管理者ユーザー名> <検査名>
 *
 * 検査名:
 *   rebuild [件数]    索引を作り直す。件数を渡すとそこで中断する (finalizer を呼ばない)
 *   count-rebuild-files  作りかけの索引ファイルの数
 *   edit-consistency  編集を繰り返しても索引が本文と一致し続けるか
 *   delete-residue    削除したページが索引から完全に消えるか
 *   corrupt-bucket    索引ファイルを 1 つ壊す (破損時の挙動を見るための準備)
 *   save-one-page     ページを 1 件保存する
 *   count-bucket      壊した索引ファイルに何ページ分入っているか数える
 *   cleanup           このヘルパが作ったページを消す
 *
 * 結果は `key=value` の行で出す。判定は呼び出し側の shell が行う。
 *
 * 単体では使わない。検証用に複製したサイトに対してのみ実行すること
 * (corrupt-bucket は索引を壊す)。
 */

$argv = $_SERVER['argv'];
if(count($argv) < 4) {
    fprintf(STDERR, "Usage: php search-index-helper.php <index.php> <admin> <check>\n");
    exit(2);
}
$index_path = $argv[1];
$admin      = $argv[2];
$check      = $argv[3];

require_once(dirname(realpath($index_path)) . '/app/tool/common');
eval_index_php($index_path);
define('APP_DIR_PATH', getcwd() . '/app');
require_once(APP_DIR_PATH . '/main.inc');

/*
 * CLI にはセッションが無く auth_get_user() が false を返すので page_write() の
 * auth_check() を通らない。deploy/scripts/gen-pages.php と同じ方法で管理者を差し込む。
 */
$GLOBALS['AUTH_GET_USER_CACHE'] = array('name' => $admin, 'method' => 'digest');

/*
 * page_write() は索引更新のために本文を変換する。&pre(code) などはその過程で
 * head_tags_add_javascript() を呼ぶが、CLI では $head_tags が未初期化のため
 * Fatal error になる。ページ本体は書けたのに索引更新だけ落ちる状態を避ける。
 */
head_tags_init();

define('TEST_PAGE_PREFIX', 'SearchIndexTest');

/* 索引を全部なめて、あるページ名が載っている ngram を集める */
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

function test_write($pagename, $contents, $extra = array()) {
    $page = page_create($pagename);
    storage_page_read($page);
    page_setup($page);
    foreach($extra as $key => $value)
	$page[$key] = $value;
    $ticket = isset($page['meta']['ticket']) ? $page['meta']['ticket'] : '';
    $error = PAGE_WRITE_ERROR_NONE;
    return page_write($page, $contents, $ticket, $error);
}

/* 一番大きい索引ファイルを選ぶ。壊したときの影響が見えやすい */
function biggest_bucket_key() {
    $biggest = false;
    $biggest_size = -1;
    foreach(cache_get_keys_prefix('', SEARCH_INDEX_KEY_PREFIX) as $cachekey) {
	$filepath = cache_get_as_filepath('', $cachekey);
	if($filepath === false)
	    continue;
	$size = filesize($filepath);
	if($size > $biggest_size) {
	    $biggest_size = $size;
	    $biggest = $cachekey;
	}
    }
    return $biggest;
}

function bucket_pagenames($cachekey) {
    $index = @unserialize((string)cache_get('', $cachekey));
    if($index === false)
	return false;
    $pagenames = array();
    foreach($index as $ngram => $pages)
	foreach($pages as $pagename => $dummy)
	    $pagenames[(string)$pagename] = true;
    return $pagenames;
}

switch($check) {

/*
 * 管理画面の再構築を、run_queue を挟まずにそのまま実行する。
 * 第 4 引数に件数を渡すと、そこまで処理して finalizer を呼ばずに終える。
 * ブラウザが途中で離脱して再構築が中断された状態を作るのに使う。
 */
case 'rebuild':
    $rebuild_args = array();
    $rebuild_dom = dom_create_document();
    $alltags = array();
    $limit = isset($argv[4]) ? (int)$argv[4] : -1;
    search_index_initializer($rebuild_args, $rebuild_dom, $alltags);
    $indexed = 0;
    foreach(page_find('', array('is_pagename_only' => true)) as $found) {
	if($limit >= 0 && $indexed >= $limit)
	    break;
	search_index_processor($rebuild_args, $rebuild_dom, $alltags, $found);
	$indexed++;
    }
    if($limit < 0)
	search_index_finalizer($rebuild_args, $rebuild_dom, $alltags);
    printf("indexed=%d\n", $indexed);
    printf("finalized=%s\n", $limit < 0 ? 'yes' : 'no');
    break;

/*
 * ページを 1 件書いてから検索し、ヒット件数を出す。
 *   search-count <ページ名> <本文> <問い合わせ>
 * 索引ではなく検索の振る舞いを確かめるのに使う。
 */
case 'search-count':
    if(!isset($argv[6])) {
	fprintf(STDERR, "search-count needs a pagename, a body and a query\n");
	exit(2);
    }
    if(!test_write($argv[4], $argv[5])) {
	printf("hits=-1\n");
	exit(1);
    }
    $query = search_parse_query($argv[6]);
    printf("words=%d\n", count($query));
    printf("hits=%d\n", count(search($query)));
    break;

/* 作りかけの索引ファイルが何個残っているか */
case 'count-rebuild-files':
    printf("rebuild_files=%d\n",
	   count(cache_get_keys_prefix('', SEARCH_INDEX_REBUILD_KEY_PREFIX)));
    printf("index_files=%d\n",
	   count(cache_get_keys_prefix('', SEARCH_INDEX_KEY_PREFIX)));
    break;

/*
 * 索引は「旧本文と新本文の差分」で更新される。差分の計算が正しくても、
 * 索引の中身を確認せずに当てているので、ずれれば戻らない。まずは
 * 正常系で本当に一致し続けることを固定しておく (ここが崩れる変更は入れない)。
 */
case 'edit-consistency':
    $pagename = TEST_PAGE_PREFIX . '/Edit';
    $cases = array(
	'新規作成'        => array("* 最初\n本文に alphaword を書く。\n", array()),
	'語を差し替え'    => array("* 最初\n本文に betaword を書く。\n", array()),
	'語を追加'        => array("* 最初\n本文に betaword と gammaword を書く。\n", array()),
	'タイトルを付ける'=> array("* 最初\n本文に betaword と gammaword を書く。\n",
				   array('title' => 'デルタ表題')),
	'タグを付ける'    => array("* 最初\n本文に betaword と gammaword を書く。\n",
				   array('write_tag' => 'epsilontag')),
	'&title を書く'   => array("&title{ゼータ表題}\n* 最初\n本文に betaword。\n", array()),
	'同じ内容で再保存'=> array("&title{ゼータ表題}\n* 最初\n本文に betaword。\n", array()),
	'&title を消す'   => array("* 最初\n本文に betaword。\n", array()),
	'表題とタグを空に'=> array("* 最初\n本文に betaword。\n",
				   array('title' => '', 'write_tag' => '')),
	);
    $missing_total = 0;
    $extra_total = 0;
    foreach($cases as $label => $case) {
	if(!test_write($pagename, $case[0], $case[1])) {
	    printf("write_failed=%s\n", $label);
	    exit(1);
	}
	$page = page_read($pagename);
	$should = search_page_ngram($page);      /* 現在の本文から本来あるべき ngram */
	$actual = index_ngrams_of($pagename);    /* 索引に実際に入っている ngram */
	$missing_total += count(array_diff_key($should, $actual));
	$extra_total   += count(array_diff_key($actual, $should));
    }
    printf("cases=%d\n", count($cases));
    printf("missing=%d\n", $missing_total);
    printf("extra=%d\n", $extra_total);
    break;

/* 削除したページの ngram が索引に残らないこと */
case 'delete-residue':
    $residue_total = 0;
    $bodies = array(
	'Plain' => "* 平文\nゴースト検査用の語 zebracat を含む段落です。\n",
	'Title' => "&title{幽霊検査タイトル}\n* 見出し\n本文に qwertymark を置く。\n",
	'Pre'   => "* コード\n&pre(code){\n\$x = 'plumfox';\n}\n段落に kiwibadger。\n",
	'Table' => "|項目|値|h\n|apricotmole|1|\n\n段落に lemonstoat。\n",
	);
    foreach($bodies as $suffix => $contents) {
	$pagename = TEST_PAGE_PREFIX . '/Delete' . $suffix;
	test_write($pagename, $contents);
	$page = page_read($pagename);
	page_delete($page);
	$residue_total += count(index_ngrams_of($pagename));
    }
    printf("cases=%d\n", count($bodies));
    printf("residue=%d\n", $residue_total);
    break;

/*
 * 索引ファイルを 1 つ、途中で切れた状態にする。
 * ディスクフルや書き込み中断で実際に起こりうる状態を作っている。
 */
case 'corrupt-bucket':
    $cachekey = biggest_bucket_key();
    if($cachekey === false) {
	printf("bucket=none\n");
	exit(1);
    }
    $pagenames = bucket_pagenames($cachekey);
    $filepath = cache_get_as_filepath('', $cachekey);
    $data = file_get_contents($filepath);
    file_put_contents($filepath, substr($data, 0, 500));
    printf("bucket=%s\n", bin2hex($cachekey));
    printf("pages_before=%d\n", count($pagenames));
    break;

case 'save-one-page':
    $ok = test_write(TEST_PAGE_PREFIX . '/AfterCorrupt',
		     "* 保存\n設定と確認とサーバの語を含む段落です。\n");
    printf("saved=%s\n", $ok ? 'yes' : 'no');
    break;

case 'count-bucket':
    if(!isset($argv[4])) {
	fprintf(STDERR, "count-bucket needs the bucket key\n");
	exit(2);
    }
    $cachekey = hex2bin($argv[4]);
    $pagenames = bucket_pagenames($cachekey);
    /* 読めないままなら -1。上書きされていないことの確認に使う */
    printf("pages_after=%d\n", $pagenames === false ? -1 : count($pagenames));
    $filepath = cache_get_as_filepath('', $cachekey);
    printf("bytes=%d\n", $filepath === false ? -1 : filesize($filepath));
    break;

/*
 * 索引ファイル 1 つから、あるページの登録だけを消す。
 * 「ファイルとしては正しいが 1 ページ分の ngram が欠けている」状態を
 * 意図的に作る。集合を比べるだけでは見つからず、--deep でしか分からない。
 */
case 'drop-page-from-bucket':
    if(!isset($argv[4]) || !isset($argv[5])) {
	fprintf(STDERR, "drop-page-from-bucket needs the bucket key and a pagename\n");
	exit(2);
    }
    $cachekey = hex2bin($argv[4]);
    $pagename = $argv[5];
    $index = @unserialize((string)cache_get('', $cachekey));
    if($index === false) {
	printf("dropped=-1\n");
	exit(1);
    }
    $dropped = 0;
    foreach($index as $ngram => $pagenames)
	if(isset($pagenames[$pagename])) {
	    unset($index[$ngram][$pagename]);
	    $dropped++;
	}
    cache_serialized_create('', $cachekey, $index);
    printf("dropped=%d\n", $dropped);
    break;

/* 索引ファイルの中で一番多くの ngram を持っているページを選ぶ */
case 'busiest-page-in-bucket':
    if(!isset($argv[4])) {
	fprintf(STDERR, "busiest-page-in-bucket needs the bucket key\n");
	exit(2);
    }
    $index = @unserialize((string)cache_get('', hex2bin($argv[4])));
    if($index === false) {
	printf("pagename=\n");
	exit(1);
    }
    $counts = array();
    foreach($index as $ngram => $pagenames)
	foreach($pagenames as $pagename => $dummy)
	    $counts[(string)$pagename] = (isset($counts[(string)$pagename]) ? $counts[(string)$pagename] : 0) + 1;
    arsort($counts);
    printf("pagename=%s\n", key($counts));
    printf("ngrams=%d\n", current($counts));
    break;

/*
 * 管理画面 (?option=search_index) の描画を CLI で実行して中身を出す。
 * HTTP 経由だと digest 認証が要るので、ここでは option の関数を直接呼ぶ。
 *   引数: show | check | repair <ページ名>
 */
case 'render-screen':
    if(!isset($argv[4])) {
	fprintf(STDERR, "render-screen needs an action\n");
	exit(2);
    }
    $action = $argv[4];
    /* message_add() の書き込み先は message_init() でしか作られない */
    message_init();
    $args = array('option' => 'search_index');
    if(isset($argv[5]))
	$args['pagename'] = $argv[5];
    $screen = dom_create_document();
    switch($action) {
    case 'show':   search_index_show($args, $screen); break;
    case 'check':  search_index_check_show($args, $screen); break;
    case 'repair': search_index_repair_write($args, $screen); break;
    default:
	fprintf(STDERR, "unknown action: %s\n", $action);
	exit(2);
    }
    /* メッセージは別の DOM に溜まるので合わせて出す */
    print(dom_save_html($screen));
    print(message_html());
    print("\n");
    break;

case 'cleanup':
    $deleted = 0;
    foreach(page_find(TEST_PAGE_PREFIX, array('is_pagename_only' => true)) as $found) {
	$page = page_read($found['name']);
	if(page_delete($page))
	    $deleted++;
    }
    printf("deleted=%d\n", $deleted);
    break;

default:
    fprintf(STDERR, "unknown check: %s\n", $check);
    exit(2);
}
exit(0);
?>
