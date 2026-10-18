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

/*
 * 見出しの共通定義。
 *
 * 同じ「見出し」でも、出来上がる DOM は種別で違う。
 *
 *   wiki      深さを section.section の入れ子で表す。要素はどの深さでも h1
 *   markdown  h1〜h6 のフラット。入れ子にはならない
 *
 * Markdown の # (h1) はふつう題名になって本文から消える (handler/markdown.inc の
 * markdown_title_heading())。本文の見出しは ## から始まるので、**wiki の * (深さ 1) に
 * 当たるのは ## (h2)**。Wiki 記法からの変換も * を ## に写す。本文に残った #
 * (title: があるとき・# が 2 つ以上あるとき・本文が # で始まらないとき) は、
 * * と同じ飾りで、一段大きく出す (利用者の判断 2026-10-18。v0.14.0 までは # が * に当たっていた)。
 *
 * セレクタは別々に書くしかないが、**どこに何を当てるかは 1 箇所で決める**。
 * 分けて書くと、片方だけ直したときに必ずずれる。v0.6 で markdown を足したとき、
 * 実際に「wiki は同じ大きさでインデント、markdown は大きさの段差」とずれた。
 *
 * 使い分けは 2 つ。
 *
 *   theme_headings_selector()        深さによらない「見出しという要素」全部。
 *                                    上流が bare な h1 に当てていたものの置き換え
 *   theme_heading_selectors($level)  深さ別。文字の大きさや余白の段差に使う
 */
define('THEME_HEADING_LEVEL_MAX', 4);

/*
 * 種別 wiki の節は * をいくつでも重ねられるが、セレクタは深さを数えて書くしか
 * ない。一番深い段 (THEME_HEADING_LEVEL_MAX) は、ここまでの深さをまとめて
 * 受け持つ。これより深い節は素の見出しとして出る。
 *
 * 「それ以降ぜんぶ」を子結合子なしの 1 本で書くこともできるが、そうすると
 * 浅い見出しにも当たってしまい、浅い側が上書きしていない性質 (letter-spacing
 * など) が漏れる。深さを明示すれば特定度が深さの順に上がり、漏れない。
 */
define('THEME_HEADING_DEPTH_MAX', 8);

if(!function_exists('theme_headings_selector')) {
    /*
     * 深さによらない見出しの見た目 (罫線・行間・太さ) に使うセレクタ。
     *
     * h1 だけでは種別 markdown の h2〜h6 に当たらない。上流は見出しが常に h1
     * だったので bare な h1 で足りていた。
     */
    function theme_headings_selector() {
	$selectors = array('h1');
	for($i = 2; $i <= 6; $i++)
	    $selectors[] = 'section.markdown h' . $i;
	return implode(",\n", $selectors);
    }

    /*
     * その深さに当たる Markdown の見出しの段 (h の数字)。wiki の深さ N は h(N+1)。
     * 一番深い段は h6 までを受け持つ。
     */
    function theme_heading_markdown_levels($level) {
	$first = $level + 1;
	return range($first, $level >= THEME_HEADING_LEVEL_MAX ? 6 : $first);
    }

    /*
     * その深さの見出しに当たるセレクタ (wiki と markdown の両方)。
     *
     * 深さ THEME_HEADING_LEVEL_MAX の規則は「それより深い見出しぜんぶ」を
     * 受け持つ。wiki の * には数の制限が無く、Markdown にも h5 h6 があるため。
     * そのため wiki 側は子結合子を使わず、深さを数えない書き方にしてある。
     *
     * 深さ 1 には、本文に残った Markdown の # (h1) も入れる。* と同じ飾り (罫線・余白) を
     * 当てるため。大きさだけは .heading.css があとで一段上げる。
     *
     * article.main に限るのは、サイド (article.side) を巻き込まないため。
     * サイドの見出しはページの中身ではなく道具の見出しで、テーマが別に決めている。
     */
    function theme_heading_selectors($level) {
	$selectors = array();
	$is_last = ($level >= THEME_HEADING_LEVEL_MAX);

	/* wiki: section.section を深さの数だけたどる */
	$depth_last = $is_last ? THEME_HEADING_DEPTH_MAX : $level;
	for($depth = $level; $depth <= $depth_last; $depth++) {
	    $wiki = 'article.main section.page';
	    for($i = 0; $i < $depth; $i++)
		$wiki .= ' > section.section';
	    $selectors[] = $wiki . ' > h1';
	}

	/* markdown: 1 段深い h (# は題名に使うので、* は ##) */
	foreach(theme_heading_markdown_levels($level) as $i)
	    $selectors[] = 'article.main section.markdown h' . $i;
	if($level == 1)
	    $selectors[] = 'article.main section.markdown h1';

	return implode(",\n", $selectors);
    }

    /*
     * 見出しを書いた折りたたみ (:::details ## 見出し) の札のセレクタ。$h は # の数。
     * 書かなかった札は data-heading-level が空なので当たらない (本文と同じ大きさのまま)。
     * 当てるのは大きさだけ。罫線や色帯は開閉の印とぶつかるので付けない。
     */
    function theme_heading_summary_selectors($h) {
	return 'article.main section.markdown details > summary[data-heading-level="' . $h . '"]';
    }

    /* 深さ別の規則を書く順。深さの順に特定度が上がるので、浅いほうから書く */
    function theme_heading_levels() {
	$levels = array();
	for($i = 1; $i <= THEME_HEADING_LEVEL_MAX; $i++)
	    $levels[] = $i;
	return $levels;
    }

    /* 深さによらず、本文の見出しすべて (余白をまとめて指定したいとき) */
    function theme_heading_selectors_all() {
	$selectors = array();
	foreach(theme_heading_levels() as $level)
	    $selectors[] = theme_heading_selectors($level);
	return implode(",\n", $selectors);
    }

    /*
     * その深さの見出しの文字の大きさ (px)。
     * 値は docs テーマが先に採っていたものを正とする。
     */
    function theme_heading_font_size($level) {
	switch($level) {
	case 1:  return THEME_FONT_SIZE_HUGE - 2;
	case 2:  return THEME_FONT_SIZE_LARGE + 2;
	case 3:  return THEME_FONT_SIZE_LARGE;
	default: return THEME_FONT_SIZE_NORMAL;
	}
    }

    /* 本文に残った Markdown の # の大きさ。深さ 1 より一段 (2px) 上で、ページ見出しと同じ */
    function theme_heading_top_font_size() {
	return THEME_FONT_SIZE_HUGE;
    }
}
?>
