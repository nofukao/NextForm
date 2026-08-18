<?php
/*
 * basic の設定項目。
 *
 * 色調・レイアウト・文字の大きさ・ヘッダーの構成は全テーマ共通なので
 * app/theme/common/.setup.php にある。定数名を共通にしておくことで、
 * テーマを切り替えても「外観の設定」の値がそのまま引き継がれる。
 * basic 固有の項目はここに書き足す (現在はなし)。
 */
include(theme_get_common_dir_path() . '/.setup.php');
?>
