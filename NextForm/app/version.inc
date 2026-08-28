<?php
/*
 * NextForm 自身の版数。
 *
 * アップグレードツール (app/tool/upgrade) が対象サイトの現在の版数を
 * 判定するのに使う。この定数が無いサイトは「まだ NextForm 化されていない
 * ToraToraWiki」とみなされる。書式は メジャー.マイナー.パッチ。
 */
define('NEXTFORM_VERSION', '0.8.0');

/*
 * この版の NextForm が動くのに必要な PHP の最低版数。
 *
 * composer.json の require.php と揃える。値を 2 か所に散らさないため、
 * コードから見るときは必ずこの定数を読む。
 *
 * 読む人が 2 人いる:
 *
 *   - app/tool/upgrade    … 書き換えを始める前に、走っている PHP を検査する
 *   - app/php_version.inc … web から開かれたときに、同じ検査をする
 *
 * ツールが見られるのは CLI の PHP だけで、web サーバ (PHP-FPM / mod_php) は
 * 別の版ということがある。だから両方で検査する。
 *
 * 2026-08-28、PHP 5.4 のサーバに 0.8.0 を当てて全ページが 500 になった。
 * 当時ツールは PHP の版数を表示するだけで、検査していなかった。
 */
define('NEXTFORM_PHP_MIN', '8.3');

/*
 * fork 元の ToraToraWiki の版数。
 *
 * NextForm がどの上流版を基にしているかという情報自体に意味があるので残す。
 * app/option/admin_info.inc が参照している。
 */
define('TORATORAWIKI_VERSION', '1.3.10');
define('TORATORAWIKI_REVISION', '1285');
?>
