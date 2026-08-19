<?php
/*
 * 色調 (トーン)。THEME_COLOR_* 25 色の組を JSON ファイル 1 つで表す。
 *
 *   app/tone/<識別子>.json       配布物。組み込みの色調。書き換えない
 *   storage/tone/<識別子>.json   そのサイトが保存した色調
 *
 * 同じ識別子なら storage が app を隠す。storage 側のファイルを消せば
 * 組み込みの色調に戻る。形式は両者で同じ:
 *
 *   {
 *     "order": 10,                              一覧の並び順 (小さいほど先)
 *     "names": {"en": "beige/green", "ja": "ベージュ/グリーン"},
 *     "colors": {"THEME_COLOR_BACKGROUND": "#fbf6ea", ...}
 *   }
 *
 * 色調はテーマと直交する。どのテーマも同じ THEME_COLOR_* を使うので、
 * テーマを切り替えても色調はそのまま効く (project-overview.md §7)。
 */
define('TONE_DIR_PATH', APP_DIR_PATH . '/tone');
define('TONE_FILE_EXTENSION', '.json');
define('TONE_DEFAULT_ID', 'beige-green');
define('TONE_CUSTOM_ID', 'custom');
define('TONE_ORDER_DEFAULT', 100);

/*
 * 上流は 'beige/green' のようにスラッシュ入りの名前を設定に保存していた。
 * ファイル名にできないので改名したが、既存サイトの storage/setup/site には
 * 古い名前が入っている。読むときにここで読み替える。
 */
global $TONE_ALIASES;
$TONE_ALIASES = array(
    'beige/green' => 'beige-green',
    'yellow/pink' => 'yellow-pink',
    'white/blue'  => 'white-blue',
    'black/blue'  => 'black-blue',
    'navy/yellow' => 'navy-yellow',
    'gray/orange' => 'gray-orange',
    );

function tone_normalize_id($id) {
    global $TONE_ALIASES;
    if(isset($TONE_ALIASES[$id]))
	return $TONE_ALIASES[$id];
    return $id;
}

/*
 * 識別子はそのままファイル名になる。管理者が中身を読んだり自分で置いたり
 * できるように ASCII に限る (日本語の名前は JSON の "names" に持たせる)。
 */
function tone_is_valid_id($id) {
    return preg_match('/^[A-Za-z0-9][A-Za-z0-9._-]{0,31}$/', $id) === 1 &&
	strpos($id, '..') === false;
}

/*
 * 色調を探すディレクトリ。後ろが前を隠す。
 * storage を後ろに置くことで、組み込みと同じ識別子で保存すると上書きになり、
 * storage 側のファイルを消せば組み込みに戻る。
 */
function tone_get_dir_paths() {
    return array(TONE_DIR_PATH, STORAGE_FILE_DIR_TONE_PATH);
}

function tone_get_file_path($id) {
    return STORAGE_FILE_DIR_TONE_PATH . '/' . $id . TONE_FILE_EXTENSION;
}

function tone_is_saved($id) {
    return tone_is_valid_id($id) && is_file(tone_get_file_path($id));
}

function tone_is_builtin($id) {
    return tone_is_valid_id($id) &&
	is_file(TONE_DIR_PATH . '/' . $id . TONE_FILE_EXTENSION);
}

/*
 * 色調を保存する。既にあれば上書きする。
 * $names は言語 => 表示名。$colors は THEME_COLOR_* => #rrggbb。
 */
function tone_write($id, $names, $colors, $order = TONE_ORDER_DEFAULT) {
    if(!tone_is_valid_id($id))
	return false;

    $tone = array(
	'order' => $order,
	'names' => $names,
	'colors' => $colors);
    if(tone_parse(json_encode($tone)) === false)
	return false;

    if(!is_dir(STORAGE_FILE_DIR_TONE_PATH) &&
       !@mkdir(STORAGE_FILE_DIR_TONE_PATH, 0777, true)) {
	message_error(lf('mkdir failed. Check $dir$ permission.',
			 array('dir' => STORAGE_FILE_DIR_TONE_PATH)));
	return false;
    }

    /*
     * 人が読み書きする前提のファイルなので整形して書く。
     * 管理者が別のサイトへ持って行ったり、手で作ったりできる。
     */
    $contents = json_encode($tone, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE |
			    JSON_UNESCAPED_SLASHES) . "\n";
    if(@file_put_contents(tone_get_file_path($id), $contents) === false)
	return false;

    tone_cache_clear();
    return true;
}

/*
 * 保存した色調を消す。組み込みと同じ識別子なら、組み込みが再び見えるようになる。
 */
function tone_delete($id) {
    if(!tone_is_saved($id))
	return false;
    if(!@unlink(tone_get_file_path($id)))
	return false;
    tone_cache_clear();
    return true;
}

function tone_cache_clear() {
    tone_read('', true);
}

function tone_parse($contents) {
    $tone = json_decode($contents, true);
    if(!is_array($tone) || empty($tone['colors']) || !is_array($tone['colors']))
	return false;
    foreach($tone['colors'] as $name => $value) {
	if(substr($name, 0, 12) !== 'THEME_COLOR_' || !color_is_valid($value))
	    return false;
    }
    if(empty($tone['names']) || !is_array($tone['names']))
	$tone['names'] = array();
    if(!isset($tone['order']))
	$tone['order'] = TONE_ORDER_DEFAULT;
    return $tone;
}

/*
 * 色調を 1 つ読む。無ければ false。
 * ページの表示では呼ばれない (テーマを生成するときと設定画面だけ)。
 */
function tone_read($id, $clear_cache = false) {
    static $cache = array();
    if($clear_cache) {
	$cache = array();
	return false;
    }
    $id = tone_normalize_id($id);
    if(isset($cache[$id]))
	return $cache[$id];
    if(!tone_is_valid_id($id))
	return false;

    $tone = false;
    foreach(tone_get_dir_paths() as $dir_path) {
	$file_path = $dir_path . '/' . $id . TONE_FILE_EXTENSION;
	if(!is_file($file_path))
	    continue;
	$contents = @file_get_contents($file_path);
	if($contents === false)
	    continue;
	$parsed = tone_parse($contents);
	if($parsed !== false) {
	    $parsed['id'] = $id;
	    $tone = $parsed;	/* 後ろのディレクトリが前を隠す */
	}
    }
    $cache[$id] = $tone;
    return $tone;
}

/* 使える色調を全部返す (識別子 => 色調)。並び順は order、同じなら識別子順 */
function tone_get_all() {
    $ids = array();
    foreach(tone_get_dir_paths() as $dir_path) {
	foreach(glob_array($dir_path . '/*' . TONE_FILE_EXTENSION) as $file_path) {
	    $ids[basename($file_path, TONE_FILE_EXTENSION)] = true;
	}
    }

    $tones = array();
    foreach(array_keys($ids) as $id) {
	$tone = tone_read($id);
	if($tone !== false)
	    $tones[$id] = $tone;
    }
    uasort($tones, 'tone_compare');
    return $tones;
}

function tone_compare($a, $b) {
    if($a['order'] !== $b['order'])
	return $a['order'] < $b['order'] ? -1 : 1;
    return strcmp($a['id'], $b['id']);
}

/* 画面に出す名前。その言語の名前が無ければ英語、それも無ければ識別子 */
function tone_get_name($tone) {
    if(!empty($tone['names'][LANG]))
	return $tone['names'][LANG];
    if(!empty($tone['names']['en']))
	return $tone['names']['en'];
    return $tone['id'];
}

/*
 * 色調の 25 色。読めなければ組み込みの既定に落とす。
 * 色調のファイルが消えても、色が 1 つも決まらない状態にはしない。
 */
function tone_get_colors($id) {
    $tone = tone_read($id);
    if($tone === false)
	$tone = tone_read(TONE_DEFAULT_ID);
    if($tone === false)
	return array();
    return $tone['colors'];
}

/*
 * 「個別に色を設定」の入力欄に出す色。
 *
 * 色調がプリセットのとき、保存されている個別色はどこにも使われない置き土産で、
 * 画面のいまの見た目とは無関係な色になっている。選んでいる色調の色を出せば、
 * 「個別に色を設定」に切り替えて適用しても見た目が変わらないところから
 * 調整を始められる。
 */
function tone_custom_color_value($name, $value) {
    if(THEME_TONE === TONE_CUSTOM_ID)
	return $value;
    $colors = tone_get_colors(THEME_TONE);
    $color_name = str_replace('THEME_CUSTOM_COLOR_', 'THEME_COLOR_', $name);
    return default_value($colors[$color_name], $value);
}

/* 「色調」の選択肢 (識別子 => 表示名) */
function tone_get_options() {
    $options = array();
    foreach(tone_get_all() as $id => $tone) {
	$options[$id] = tone_get_name($tone);
    }
    $options[TONE_CUSTOM_ID] = l('custom color');
    return $options;
}
?>
