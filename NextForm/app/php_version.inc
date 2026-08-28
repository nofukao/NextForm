<?php
/*
 * 走っている PHP が NextForm の必要とする版数に届いているかを見る。
 *
 * ここは **web から開かれた経路の最後の砦**。app/tool/upgrade も同じ検査を
 * するが、あちらが見られるのは CLI の PHP だけで、web サーバ
 * (PHP-FPM / mod_php) が別の版ということがある。
 *
 * ★このファイルは、どんなに古い PHP でも構文解析が通る書き方で書くこと。★
 *
 * 構文エラーはファイルを読み込んだ時点で起きるので、新しい書き方が 1 つでも
 * 混じると、この検査そのものが動かず、利用者は理由の書かれていない 500 を
 * 見ることになる。実際 2026-08-28 に PHP 5.4 のサーバで起きたのがそれで、
 * 原因は app/handler/markdown.inc の `Foo::class` (PHP 5.5 以降) だった。
 *
 * つまり:
 *   - 型宣言・戻り値型・null 合体演算子・アロー関数・match・::class を使わない
 *   - 短い配列構文 [] も使わない (array() と書く)
 *   - 定数は define() で読む
 *
 * 読み込む側:
 *
 *   app/boot.inc   … 通常のページ。main.inc より **先に** 読む。
 *                    boot.inc 自身も同じ理由で 5 行のままにしてある
 *   app/install.inc … インストーラ。index.php が boot.inc ではなく
 *                    install.inc を読む状態のサイト用。ただし install.inc は
 *                    大きいので、install.inc 自体の構文解析に失敗すると
 *                    ここまで来ない。そこは割り切る (上げる前に一度も
 *                    インストールしていないサイトは、まず無い)
 */

require_once(APP_DIR_PATH . '/version.inc');

if(version_compare(PHP_VERSION, NEXTFORM_PHP_MIN, '<')) {
    $php_version_message =
	'NextForm には PHP ' . NEXTFORM_PHP_MIN . ' 以上が必要です' .
	' (このサーバの PHP は ' . PHP_VERSION . " です)。\n" .
	'NextForm requires PHP ' . NEXTFORM_PHP_MIN . ' or later,' .
	' but this server is running PHP ' . PHP_VERSION . ".\n";

    /*
     * display_errors が Off のサイトでも読めるように、warning ではなく
     * 本文として出す。同時にサーバのログにも残す (運用者が最初に見る場所)。
     */
    error_log($php_version_message);

    if(PHP_SAPI !== 'cli') {
	header('HTTP/1.1 500 Internal Server Error');
	header('Content-Type: text/plain; charset=UTF-8');
    }
    echo($php_version_message);
    echo("\n");
    echo("PHP を上げてから開き直してください。手順は同梱の UPGRADE.md にあります。\n");
    echo("Upgrade PHP first. See UPGRADE.md in the distribution.\n");
    exit(1);
}
?>
