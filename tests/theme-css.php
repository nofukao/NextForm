<?php
/*
 * テーマの静的ファイルを、設定を差し替えながら 1 パターン生成する。
 *
 *   php tests/theme-css.php <サイトのルート> <テーマ> <出力先> [定数名=値 ...]
 *
 * 出力先はサイトのルートからの相対パスで渡すこと (既定の 'theme' と同じ)。
 * 絶対パスを渡すと trw_theme_css_url() が出す画像への相対 URL が変わる。
 *
 * 定数を先に define() しておくと setup_local() が storage の値で上書きしない、
 * という性質を使っている。これで storage を書き換えずに、色調やレイアウトを
 * 変えたときの生成物を取り出せる。
 *
 * 単体で使うことは少ない。tests/theme-lib.sh から呼ばれる。
 */
$argv = $_SERVER['argv'];
if(count($argv) < 4) {
    fprintf(STDERR, "Usage: %s <site_root> <theme> <out_dir> [CONST=value ...]\n", $argv[0]);
    exit(1);
}
$site_root = $argv[1];
$theme     = $argv[2];
$out_dir   = $argv[3];

foreach(array_slice($argv, 4) as $pair) {
    list($name, $value) = array_pad(explode('=', $pair, 2), 2, '');
    define($name, $value);
}

if(!chdir($site_root)) {
    fprintf(STDERR, "chdir failed: %s\n", $site_root);
    exit(1);
}
define('THEME', $theme);
define('THEME_DIR_PATH', $out_dir);
define('THEME_URI', 'theme');
// boot.inc と同じく絶対パス。相対パスにすると生成される相対 URL が変わる。
define('APP_DIR_PATH', realpath($site_root) . '/app');

require_once('./app/main.inc');

if(!theme_convert(THEME)) {
    fprintf(STDERR, "theme_convert failed: theme=%s\n", $theme);
    exit(1);
}
exit(0);
?>
