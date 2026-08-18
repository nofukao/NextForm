<?php
/*
 * テーマ共通の寸法定数。
 *
 * 各テーマの style/main.css が、CSS を出力する前に最初に読み込む。
 * common/style/*.css はここと .colors.php で定義した定数にだけ依存する。
 * テーマ固有の寸法 (本文幅・ヘッダーの最小高さなど) は各テーマ側で定義する。
 */
define('THEME_IS_DEVICE_SMALL', '(max-width: 767px)');
define('THEME_IS_NOT_DEVICE_SMALL', '(min-width: 768px)');
define('THEME_FONT_SIZE_NORMAL', floor(round(THEME_FONT_SIZE) / 2 * 2));
define('THEME_FONT_SIZE_LARGE', floor(round(THEME_FONT_SIZE * (16 / 14) / 2) * 2));
define('THEME_FONT_SIZE_HUGE', floor(round((THEME_FONT_SIZE + 6 + floor(THEME_FONT_SIZE / 13) * 2) / 2) * 2));
?>
