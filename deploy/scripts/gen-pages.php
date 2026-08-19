<?php
/*
 * NextForm のインスタンスにページを生成する CLI ツール。
 *
 *   php gen-pages.php <index.php のパス> --dir <ディレクトリ>
 *       ディレクトリ内の *.wiki を、ファイル名 (拡張子なし) をページ名として投入する。
 *       ゴールデンマスターの入力データ投入に使う。
 *
 *   php gen-pages.php <index.php のパス> --count <件数> [--prefix <接頭辞>]
 *       連番のページを大量生成する。storage_page_find() の性能計測に使う。
 *
 * storage/ に直接書き込まず wiki 本体の API を経由する。検索インデックスと
 * メタ情報の整合が取れるため。
 *
 * index.php の所有者と実行ユーザーが一致している必要がある (app/tool/common の制約)。
 * Apache 配下のインスタンスに対しては sudo -u apache で実行すること。
 */

$argv = $_SERVER['argv'];

function usage() {
    fprintf(STDERR, "Usage: php gen-pages.php <path/to/index.php> --dir <dir>\n");
    fprintf(STDERR, "       php gen-pages.php <path/to/index.php> --count <n> [--prefix <prefix>] [--user <name>]\n");
    exit(1);
}

if(count($argv) < 4)
    usage();

$index_path = $argv[1];
$mode = null;
$dir = null;
$count = 0;
$prefix = 'GenPage';
$user = 'admin';    // page_write() の権限に使う管理者ユーザー名 (--user で指定)

for($i = 2; $i < count($argv); $i++) {
    switch($argv[$i]) {
    case '--dir':    $mode = 'dir';   $dir   = $argv[++$i]; break;
    case '--count':  $mode = 'count'; $count = (int)$argv[++$i]; break;
    case '--prefix': $prefix = $argv[++$i]; break;
    case '--user':   $user   = $argv[++$i]; break;
    default: usage();
    }
}
if($mode === null)
    usage();

// ブートストラップは対象インスタンス側から読む。このスクリプト自体は
// リポジトリのどこに置かれていてもよく、実行ユーザーがリポジトリを
// 読めなくても動く (Apache 配下へ sudo -u apache で実行する場合など)。
require_once(dirname(realpath($index_path)) . '/app/tool/common');
eval_index_php($index_path);   // ここで index.php のあるディレクトリに chdir される
define('APP_DIR_PATH', getcwd() . '/app');
require_once(APP_DIR_PATH . '/main.inc');

/*
 * CLI にはセッションが無く auth_get_user() が false を返すため、page_write() の
 * auth_check(..., 'write') を通らない。認証結果のキャッシュに管理者を差し込む。
 */
$GLOBALS['AUTH_GET_USER_CACHE'] = array('name' => $user, 'method' => 'digest');

/*
 * page_write() は検索インデックス更新のために本文を変換する。&pre(code) などは
 * その過程で head_tags_add_javascript() を呼ぶが、通常のリクエストと違って
 * CLI では $head_tags が初期化されていないため Fatal error になる。
 * ページ本体は書けたのにインデックス更新だけ落ちる、という半端な状態になる。
 */
head_tags_init();

function gen_find_wiki_files($dir) {
    $files = array();
    foreach(scandir($dir) as $entry) {
        if($entry === '.' || $entry === '..')
            continue;
        $path = $dir . '/' . $entry;
        if(is_dir($path))
            $files = array_merge($files, gen_find_wiki_files($path));
        else if(substr($entry, -5) === '.wiki' || substr($entry, -3) === '.md')
            $files[] = $path;
    }
    return $files;
}

/*
 * 拡張子でページ種別を決める。.wiki は wiki、.md は markdown。
 * page_setup() は空のときだけ既定を入れるので、その前に指定しておけばよい。
 */
function gen_page_type($path) {
    return substr($path, -3) === '.md' ? 'markdown' : 'wiki';
}

function gen_write_page($pagename, $contents, $type = 'wiki') {
    $page = page_create($pagename);
    storage_page_read($page);
    if(empty($page['meta']['type']))
	$page['meta']['type'] = $type;
    page_setup($page);
    $ticket = isset($page['meta']['ticket']) ? $page['meta']['ticket'] : '';
    $error = PAGE_WRITE_ERROR_NONE;
    if(!page_write($page, $contents, $ticket, $error)) {
        fprintf(STDERR, "  failed: %s (error=%s)\n", $pagename, $error);
        return false;
    }
    return true;
}

$written = 0;

if($mode === 'dir') {
    $dir = rtrim($dir, '/');
    // ディレクトリの階層をそのままページ名の階層にする。
    // input/GoldenMaster/Top.wiki -> ページ名 'GoldenMaster/Top'
    $files = gen_find_wiki_files($dir);
    if(count($files) === 0) {
        fprintf(STDERR, "No *.wiki or *.md files under %s\n", $dir);
        exit(1);
    }
    sort($files);
    foreach($files as $file) {
        $type = gen_page_type($file);
        $extension = $type === 'markdown' ? '.md' : '.wiki';
        $pagename = substr($file, strlen($dir) + 1, -strlen($extension));
        $contents = file_get_contents($file);
        printf("write %s [%s] (%d bytes) .. ", $pagename, $type, strlen($contents));
        if(gen_write_page($pagename, $contents, $type)) { printf("ok\n"); $written++; }
    }
} else {
    for($i = 1; $i <= $count; $i++) {
        $pagename = sprintf('%s/%04d', $prefix, $i);
        $contents = sprintf("*%s\nGenerated page %d of %d.\n\n" .
                            "検索用のダミー本文です。keyword%03d を含みます。\n",
                            $pagename, $i, $count, $i % 100);
        if(gen_write_page($pagename, $contents)) $written++;
        if($i % 100 === 0) printf("  %d / %d\n", $i, $count);
    }
}

printf("wrote %d page(s)\n", $written);
