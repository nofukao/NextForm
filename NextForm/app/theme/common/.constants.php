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

/*
 * 本文の詰まり具合 (THEME_DENSITY) を 1 つの倍率にする。
 *
 * 余白と行間をこの倍率で伸び縮みさせる。テーマごとの余白の取り方
 * (plain は広く、docs はもっと広く) は保ったまま、全体を寄せたり離したり
 * できるようにするため、絶対値ではなく倍率にしてある。
 * 標準 (normal) は 1.0 なので、出力は倍率を入れる前と 1 ビットも変わらない。
 */
$THEME_DENSITY_SCALES = array('loose' => 1.15, 'normal' => 1.0, 'compact' => 0.82);
define('THEME_DENSITY_SCALE',
       isset($THEME_DENSITY_SCALES[THEME_DENSITY]) ? $THEME_DENSITY_SCALES[THEME_DENSITY] : 1.0);

if(!function_exists('theme_density_px')) {
    /* 余白 (px) を詰まり具合に合わせる。0 は 0 のまま */
    function theme_density_px($px) {
	if($px == 0)
	    return 0;
	return max(1, (int)round($px * THEME_DENSITY_SCALE));
    }

    /* 行間などの単位なしの値を詰まり具合に合わせる */
    function theme_density_num($value) {
	$scaled = round($value * THEME_DENSITY_SCALE, 2);
	return rtrim(rtrim(number_format($scaled, 2, '.', ''), '0'), '.');
    }
}
?>
