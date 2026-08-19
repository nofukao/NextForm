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

function tone_get_dir_paths() {
    return array(TONE_DIR_PATH);
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
function tone_read($id) {
    static $cache = array();
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
