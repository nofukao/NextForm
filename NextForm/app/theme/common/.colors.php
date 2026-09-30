<?php
/*
 * 色調 (THEME_TONE) を THEME_COLOR_* 定数に展開する。
 *
 * 全テーマがこのファイルを通して同じ定数名を使う。管理画面の「外観の設定」で
 * 選んだ色調とカスタム色は storage/setup/site に定数名で保存されるので、
 * ここを共有している限り、テーマを切り替えても色の設定はそのまま引き継がれる。
 * テーマ側は THEME_COLOR_* をどの部品に割り当てるかだけを決める。
 */
if(THEME_TONE !== TONE_CUSTOM_ID) {
    /* 色調そのものは app/tone.inc が持つ。テーマの下には置かない */
    foreach(tone_get_colors(THEME_TONE) as $name => $value) {
	define($name, $value);
    }
    if(THEME_COLOR_MAIN_CUSTOM !== '')
	define('THEME_COLOR_MAIN', THEME_COLOR_MAIN_CUSTOM);
    else
	define('THEME_COLOR_MAIN', THEME_COLOR_MAIN_DEFAULT);
} else {
    global $THEME_CUSTOM_COLORS;
    foreach($THEME_CUSTOM_COLORS as $custom_color_name => $custom_color_value) {
	/* 空は「別の色と同じ」。下で埋める */
	if(constant($custom_color_name) === '')
	    continue;
	define(str_replace('THEME_CUSTOM_COLOR_', 'THEME_COLOR_', $custom_color_name), constant($custom_color_name));
    }
    define('THEME_COLOR_MAIN', THEME_COLOR_MAIN_DEFAULT);
}

/*
 * 色調・個別設定が持っていない色を埋める ($THEME_COLOR_FALLBACKS、.setup.php)。
 * Markdown のページへのリンクの色は後から足したので、既存の色調のファイルにも
 * 個別設定のサイトにも無い。無ければリンクの色と同じにする。
 */
global $THEME_COLOR_FALLBACKS;
foreach($THEME_COLOR_FALLBACKS as $color_name => $fallback) {
    if(!defined($color_name))
	define($color_name, constant($fallback['color']));
}

$THEME_IMAGE_BACKGROUND_HEADER_WIDTH = false;
$THEME_IMAGE_BACKGROUND_HEADER_HEIGHT = false;
if(THEME_IMAGE_BACKGROUND_HEADER !== '' && function_exists('getimagesize')) {
    $size = getimagesize(THEME_IMAGE_BACKGROUND_HEADER);
    if(isset($size[0]) && isset($size[1])) {
	$THEME_IMAGE_BACKGROUND_HEADER_WIDTH = $size[0];
	$THEME_IMAGE_BACKGROUND_HEADER_HEIGHT = $size[1];
    }
}
?>
