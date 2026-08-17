<?php
/*
 * NextForm 自身の版数。
 *
 * アップグレードツール (app/tool/upgrade) が対象サイトの現在の版数を
 * 判定するのに使う。この定数が無いサイトは「まだ NextForm 化されていない
 * ToraToraWiki」とみなされる。書式は メジャー.マイナー.パッチ。
 */
define('NEXTFORM_VERSION', '0.4.1');

/*
 * fork 元の ToraToraWiki の版数。
 *
 * NextForm がどの上流版を基にしているかという情報自体に意味があるので残す。
 * app/option/admin_info.inc が参照している。
 */
define('TORATORAWIKI_VERSION', '1.3.10');
define('TORATORAWIKI_REVISION', '1285');
?>
