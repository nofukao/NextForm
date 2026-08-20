<?php
define('START_MICROTIME', microtime(true));

function execution_time() {
    return microtime(true) - START_MICROTIME;
}

function args_get() {
    $args = $_GET + $_POST;
    foreach($args as $key => &$val) {
	if(is_string($val))
	    $val = normalize_string($val);
    }
    return $args;
}

function normalize_string($binary) {
    $binary = preg_replace('/[\x00-\x08\x0b-\x1f\x7f]/', '', $binary);
    if(!preg_match('//u', $binary))
	return '';
    return $binary;
}

function hash_to_pairs($hash) {
    $pairs = array();
    foreach($hash as $k => $v) {
	$pairs[] = array($k, $v);
    }
    return $pairs;
}

function glob_array($pattern) {
    $result = array();
    $files = glob($pattern);
    if($files !== false) {
	foreach($files as $file) {
	    if(is_file($file)) {
		$result[] = $file;
	    }
	}
    }
    return $result;
}

function nextstop(&$arr) {
    $val = next($arr);
    if($val === false)
	end($arr);
    return $val;
}

function scalar_walk(&$v, $func, &$opt = null) {
    if(is_array($v)) {
	foreach($v as &$e)
	    scalar_walk($e, $func, $opt);
    } else {
	if(!is_null($opt))
	    $v = $func($v, $opt);
	else
	    $v = $func($v);
    }
}

function order_compare($a, $b) {
    if($a['order'] < $b['order'])
	return -1;
    else if($a['order'] > $b['order'])
	return 1;
    return 0;
}

function ascii_encode($data) {
    $ar = unpack('H*', $data);
    return reset($ar);
    //return array_shift(unpack('H*', $data));
}

function ascii_decode($data) {
    return pack('H*', $data);
}

function decode_prefixed_number($val) {
    $val = trim($val, ' i');
    $l = strlen($val);
    if($l < 1)
	return $val;
    $last = strtolower($val[strlen($val)-1]);
    if($last === 't' || $last === 'g' || $last === 'm' || $last === 'k')
	$val = (int)substr($val, 0, -1);

    switch($last) {
    case 't':
	$val *= 1024;
    case 'g':
	$val *= 1024;
    case 'm':
	$val *= 1024;
    case 'k':
	$val *= 1024;
    }
    return $val;
}

function fraction_to_um($frac) {
    $parts = explode('/', $frac);
    if(count($parts) <= 0)
	return 0;
    if(count($parts) == 1)
	return $parts[0];
    return floatval($parts[0]) / floatval($parts[1]);
}

function encode_prefixed_number($val) {
    $prefix = array('', 'Ki', 'Mi', 'Gi', 'Ti');
    $p = 0;
    while($val >= 1024 && isset($prefix[$p])) {
	$val = floor($val/1024);
	$p++;
    }
    return $val . $prefix[$p];
}

function max_upload_file_size() {
    $max_upload = decode_prefixed_number(ini_get('upload_max_filesize'));
    $max_post = decode_prefixed_number(ini_get('post_max_size'));
    $memory_limit = decode_prefixed_number(ini_get('memory_limit'));
    return min($max_upload, $max_post, $memory_limit);
}

function default_value(&$var, $default = '') {
    if(isset($var))
	return $var;
    else
	return $default;
}

function pprint($msg) {
    print('<pre>' . htmlspecialchars($msg) . '</pre>');
}
function pprint_r(&$var) {
    pprint(print_r($var, true));
}

function email_is_valid($email) {
    return preg_match('/^[-+.\\w]+@[-a-z0-9]+(\\.[-a-z0-9]+)*\\.[a-z]{2,6}$/i', $email);
}

function color_is_valid($color) {
    $color = color_keyword_to_rgb($color);
    return preg_match('/^(#[0-9a-f]{3}|#[0-9a-f]{6})$/', $color);
}

function color_keyword_to_rgb($keyword) {
    global $COLOR_KEYWORDS;
    if(!isset($COLOR_KEYWORDS)) {
	require_once(APP_DIR_PATH . '/color_keywords.inc');
    }
    $keyword = strtolower($keyword);
    if(isset($COLOR_KEYWORDS[$keyword]))
	return $COLOR_KEYWORDS[$keyword];
    else
	return $keyword;
}

function color_html_to_rgb($color) {
    $color = color_keyword_to_rgb($color);
    if(strlen($color) == 4)
	$color = '#' . $color[1] . $color[1] . $color[2] . $color[2] . $color[3] . $color[3];
    if(preg_match('/^#([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})$/', $color, $maches)) {
	return array(
	    'r' => hexdec($maches[1]),
	    'g' => hexdec($maches[2]),
	    'b' => hexdec($maches[3]));
    } else {
	return false;
    }
}

function color_html_to_hsv($color) {
    $rgb = color_html_to_rgb($color);
    if($rgb === false)
	return false;
    return color_rgb_to_hsv($rgb);
}

function color_rgb_to_html($rgb) {
    return sprintf('#%02x%02x%02x', $rgb['r'], $rgb['g'], $rgb['b']);
}

function color_rgb_to_hsv($rgb) {
    $r = $rgb['r'] / 255;
    $g = $rgb['g'] / 255;
    $b = $rgb['b'] / 255;
    $max = max($r, $g, $b);
    $min = min($r, $g, $b);
    $v = $max;

    $diff = $max - $min;
    if($diff == 0){
	$h = 0;
    } else if($r === $max){
	$h = 60 * ($g - $b) / $diff + 0;
    } else if($g === $max){
	$h = 60 * ($b - $r) / $diff + 120;
    } else {
	$h = 60 * ($r - $g) / $diff + 240;
    }
    if($h < 0)
	$h = $h + 360;
 
    $s = ($v != 0) ? $diff / $max : 0;
   
    return array("h" => $h, "s" => $s, "v" => $v);
}

function color_hsv_to_rgb($hsv){
    $h = $hsv['h'];
    $s = $hsv['s'];
    $v = $hsv['v'];
    if($s == 0) {
	$r = $v * 255;
	$g = $v * 255;
	$b = $v * 255;
    } else {
	$h = ($h >= 0) ? $h % 360 / 360 : $h % 360 / 360 + 1;
	$var_h = $h * 6;
	$i = (int)$var_h;
	$f = $var_h - $i;
     
	$p = $v * ( 1 - $s );
	$q = $v * ( 1 - $s * $f );
	$t = $v * ( 1 - $s * ( 1 - $f ) );

	$a_r = array($v,$q,$p,$p,$t,$v);
	$a_g = array($t,$v,$v,$q,$p,$p);
	$a_b = array($p,$p,$t,$v,$v,$q);
	$r = round($a_r[$i] * 255);
	$g = round($a_g[$i] * 255);
	$b = round($a_b[$i] * 255);
    }
    return array('r' => $r, 'g' => $g, 'b' => $b);
}

function preg_match_table(&$patterns, $pattern_prefix, $pattern_postfix, $subject, &$maches = null) {
    foreach($patterns as $pattern => &$data) {
	if(preg_match($pattern_prefix . $pattern . $pattern_postfix, $subject, $maches)) {
	    return $data;
	}
    }
    return false;
}

function url_path_encode($pagename) {
    $names = explode('/', $pagename);
    $names = array_map('urlencode', $names);
    return implode('/', $names);
}

function url_make_query($h) {
    $records = array();
    foreach($h as $name => $value) {
	// default_value() は参照渡しなので、未設定のキーを引くと $args に null を
	// 作ってしまう。それがそのままクエリに流れてくる。
	// urlencode(null) と urlencode('') の出力は同じなので変換して受ける。
	$records[] = urlencode($name) . '='. urlencode($value === null ? '' : $value);
    }
    return implode('&', $records);
}

function url_parse_query($querystring) {
    $args = array();
    if($querystring == '')
	return $args;
    $fields = explode('&', $querystring);
    foreach($fields as $field) {
	$pair = array_map('urldecode', explode('=', $field));
	if(isset($pair[0])) {
	    $args[$pair[0]] = default_value($pair[1], '');
	}
    }
    return $args;
}

function url_make($p = '', $h = array()) {
    $url = BASE_URI . '?';
    if($p !== '')
	$url .= url_path_encode($p);
    if(!empty($h))
	$url .= ($p !== '' ? '&' : '') . url_make_query($h);
    return $url;
}

function url_get_site_url() {
    if(BASE_URI !== '')
	return BASE_URI;
    $scheme = isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] === 'on' ? 'https' : 'http';
    $port = $_SERVER['SERVER_PORT'];
    if(($port == 80  && $scheme === 'http') ||
       ($port == 443  && $scheme === 'https'))
	$port = '';
    $path = $_SERVER['REQUEST_URI'];
    $path = preg_replace('/\?.*$/', '', $path);
    return $scheme . '://' . $_SERVER['HTTP_HOST'] . $port . $path;
}

function relative_path($from, $to) {
    $from = str_replace('\\', '/', $from);
    $to = str_replace('\\', '/', $to);
    $froms = explode('/', $from);
    $tos = explode('/', $to);
    $fn = count($froms);
    $tn = count($tos);
    $path = array();
    
    for($i = 0; $i < $fn && $i < $tn - 1 && $froms[$i] === $tos[$i]; $i++);
    $same_index = $i;
    for($i = 0; $i < $fn - $same_index - 1; $i++)
	$path[] = '..';
    for($i = $same_index; $i < $tn; $i++)
	$path[] = $tos[$i];

    if(empty($path))
	$path = './';
    else {
	$last = array_pop($path);
	if(preg_match('_^index\.html?$_', $last))
	    $last = '';
	if($last === '' && empty($path))
	    $path = './';
	else {
	    array_push($path, $last);
	    $path = array_map('urlencode', $path);
	    $path = implode('/', $path);
	}
    }
    return $path;
}

function ancestor_paths($path) {
    $path = str_replace('\\', '/', $path);
    $path = str_replace('//', '/', $path);
    $names = explode('/', $path);
    $paths = array();
    $stack = array();
    foreach($names as $name) {
	$stack[] = $name;
	$paths[] = implode('/', $stack);
    }
    return $paths;
}

function ouths($str) {
    echo(htmlspecialchars($str));
}

function outraw($str) {
    echo($str);
}

function string_to_list($string, $separator = ' ') {
    $list = explode($separator, language_sanitize($string));
    $list = array_map('trim', $list);
    return array_filter($list, 'strlen');
}

function list_to_string($list, $separator = ' ') {
    return implode($separator, $list);
}

function logical_conditions_eval($conditions, $values, $wildcard = false) {
    $is_match = true;
    foreach($conditions as $or_conditions) {
	$is_or_match = empty($or_conditions);
	foreach($or_conditions as $cond) {
	    $reverse = false;
	    if($cond[0] === '!') {
		$reverse = true;
		$cond = substr($cond, 1);
	    }
	    if(!$wildcard) {
		$result = in_array($cond, $values);
	    } else {
		$result = false;
		foreach($values as $value) {
		    if(fnmatch($cond, $value)) {
			$result = true;
			break;
		    }
		}
	    }
	    if($reverse)
		$result = !$result;
	    if($result) {
		$is_or_match = true;
		break;
	    }
	}
	if(!$is_or_match)
	    return false;
    }
    return true;
}

/*
 * strftime() の互換ラッパー。
 *
 * strftime() は PHP 8.1 で非推奨、PHP 9 で削除される。TIME_FORMAT / DATE_FORMAT は
 * 管理画面から変更できる strftime 形式の文字列なので、書式そのものは維持したまま
 * date() で実装し直す。日付処理はこの 1 箇所に集約する。
 *
 * date() は strftime の素直な置き換えにならない。実測で確認した差異:
 *   %j  strftime は 1 始まりの 3 桁。date('z') は 0 始まり (2015-05-20: 140 対 139)
 *   %W  strftime は月曜始まりの週番号。date('W') は ISO-8601 で定義が違う (20 対 21)
 * この 2 つは個別に計算する。
 *
 * 桁詰め抑制の %-X (glibc) と %#X (MSVC) にも対応する。app/language.inc の
 * 日本語定義が '%Y年%-m月' / '%Y年%#m月' を OS で切り替えて使っている。
 * なお glibc の strftime は %# を解さず '%#m' を 05 と出していたが、ここでは
 * MSVC の意図どおり 5 にする。OS の strftime を呼ばなくなったので
 * language.inc の OS 分岐はどちらの形でも同じ結果になる。
 *
 * 未知の指定子はそのまま出力する。setlocale() はこのアプリでは呼ばれないため、
 * ロケール依存の %a %A %b %B %x %X は C ロケール相当 (英語) になる。これは
 * strftime() 時代の挙動と同じ。
 */
function nf_date($format, $timestamp = null) {
    if($timestamp === null)
	$timestamp = time();
    $output = '';
    $length = strlen($format);
    for($i = 0; $i < $length; $i++) {
	if($format[$i] !== '%') {
	    $output .= $format[$i];
	    continue;
	}
	$i++;
	if($i >= $length) {
	    $output .= '%';
	    break;
	}
	$nopad = false;
	if($format[$i] === '-' || $format[$i] === '#') {
	    $nopad = true;
	    $i++;
	    if($i >= $length) {
		$output .= '%';
		break;
	    }
	}
	$output .= nf_date_specifier($format[$i], $timestamp, $nopad);
    }
    return $output;
}

function nf_date_specifier($specifier, $t, $nopad) {
    switch($specifier) {
    case 'a': return date('D', $t);
    case 'A': return date('l', $t);
    case 'b': case 'h': return date('M', $t);
    case 'B': return date('F', $t);
    case 'C': return nf_date_pad((int)((int)date('Y', $t) / 100), 2, $nopad);
    case 'd': return date($nopad ? 'j' : 'd', $t);
    case 'D': return date('m/d/y', $t);
    case 'e': return $nopad ? date('j', $t) : sprintf('%2d', (int)date('j', $t));
    case 'F': return date('Y-m-d', $t);
    case 'g': return substr(date('o', $t), -2);
    case 'G': return date('o', $t);
    case 'H': return date($nopad ? 'G' : 'H', $t);
    case 'I': return date($nopad ? 'g' : 'h', $t);
    case 'j': return nf_date_pad((int)date('z', $t) + 1, 3, $nopad);
    case 'k': return date('G', $t);
    case 'l': return date('g', $t);
    case 'm': return date($nopad ? 'n' : 'm', $t);
    case 'M': return $nopad ? (string)(int)date('i', $t) : date('i', $t);
    case 'n': return "\n";
    case 'p': return date('A', $t);
    case 'P': return date('a', $t);
    case 'r': return date('h:i:s A', $t);
    case 'R': return date('H:i', $t);
    case 's': return date('U', $t);
    case 'S': return $nopad ? (string)(int)date('s', $t) : date('s', $t);
    case 't': return "\t";
    case 'T': return date('H:i:s', $t);
    case 'u': return date('N', $t);
    case 'U': return nf_date_week_number($t, 0, $nopad);
    case 'V': return date('W', $t);
    case 'w': return date('w', $t);
    case 'W': return nf_date_week_number($t, 1, $nopad);
    case 'x': return date('Y-m-d', $t);
    case 'X': return date('H:i:s', $t);
    case 'y': return $nopad ? (string)(int)date('y', $t) : date('y', $t);
    case 'Y': return date('Y', $t);
    case 'z': return date('O', $t);
    case 'Z': return date('T', $t);
    case '%': return '%';
    }
    return '%' . $specifier;
}

function nf_date_pad($value, $width, $nopad) {
    return $nopad ? (string)$value : sprintf('%0' . $width . 'd', $value);
}

/*
 * %U (日曜始まり) と %W (月曜始まり) の週番号。
 * date('W') は ISO-8601 週番号で定義が違うので使えない。
 */
function nf_date_week_number($t, $first_weekday, $nopad) {
    $yday = (int)date('z', $t);
    $wday = (int)date('w', $t);
    if($first_weekday === 1)
	$wday = ($wday + 6) % 7;
    return nf_date_pad((int)(($yday + 7 - $wday) / 7), 2, $nopad);
}

function time_format($t, $type = 'time') {
    if($t == 'now')
	return l('Now');
    if($type == 'date')
	return nf_date(DATE_FORMAT, $t);
    return nf_date(TIME_FORMAT, $t);
}

function time_xmlformat($time) {
    return date('Y-m-d', $time) . 'T' . date('H:i:sP', $time);
}

function time_get_timezones() {
    $timezones = timezone_abbreviations_list();
    $result = array();
    foreach($timezones as &$timezonegroup) {
	foreach($timezonegroup as &$timezone) {
	    if(!empty($timezone['timezone_id']))
		$result[$timezone['timezone_id']] = $timezone['timezone_id'];
	}
    }
    return $result;
}

define('DATEPAGE_REGEX', '\d{4}\-\d{2}\-\d{2}');

function datepage_format($t) {
    return nf_date('%Y-%m-%d', $t);
}

function datepage_is_date($string) {
    return preg_match('|^' . DATEPAGE_REGEX . '$|', $string);
}

function datepage_is_datepage($pagename, &$date = null) {
    if(preg_match('|/(' . DATEPAGE_REGEX . ')$|', $pagename, $maches)) {
	$date = $maches[1];
	return true;
    }
    return false;
}

function options_parse($text) {
    $fields = explode('/', $text);
    $options = array();
    foreach($fields as $field) {
	$kv = explode(':', $field);
	if(count($kv) == 1)
	    $options[$kv[0]] = $kv[0];
	else if(count($kv) > 1) {
	    $options[$kv[1]] = $kv[0];
	}
    }
    return $options;
}

function image_parse_size($arg, &$width, &$height) {
    $is_max = false;

    if(preg_match('/^max *(.*)$/', $arg, $maches)) {
	$is_max = true;
	$arg = $maches[1];
    }

    if(preg_match('/^(\d+)%$/', $arg, $matches)) {
	$width = round($matches[1] * $width / 100.0);
	$height = round($matches[1] * $height / 100.0);
    } else if(preg_match('/^(\d+)x(\d+)!$/', $arg, $matches)) {
	$width = $matches[1];
	$height = $matches[2];
    } else if(preg_match('/^(\d+)x(\d+)$/', $arg, $matches)) {
	if($height * $width > 0) {
	    if($is_max && $width <= $matches[1] && $height <= $matches[2])
		return;
	    $ratio = $height / $width;
	    $width = $matches[1];
	    $height = round($width * $ratio);
	    if($height > $matches[2]) {
		$height = $matches[2];
		$width = round($height / $ratio);
	    }
	} else {
	    $width = $matches[1];
	    $height = $matches[2];
	}
    } else if(preg_match('/^(\d+)x?$/', $arg, $matches)) {
	if($height * $width > 0) {
	    if($is_max && $width <= $matches[1])
		return;
	    $ratio = $height / $width;
	    $width = $matches[1];
	    $height = round($width * $ratio);
	}	    
    } else if(preg_match('/^x(\d+)$/', $arg, $matches)) {
	if($height * $width > 0) {
	    if($is_max && $height <= $matches[1])
		return;
	    $ratio = $height / $width;
	    $height = $matches[1];
	    $width = round($height / $ratio);
	}	    
    }
}

$ob_buffer_writer_handler = null;
function ob_buffer_writer($buffer) {
    global $ob_buffer_writer_handler;
    fwrite($ob_buffer_writer_handler, $buffer);
    return '';
}

function ob_error_handler($str) {
    $error = error_get_last();
    // && は || より強く結合するため、括弧が無いと null チェックが 2 つ目の条件に
    // 掛からず、error_get_last() が null のときに $error["type"] を読んでしまう
    if ($error && ($error["type"] == E_USER_ERROR || $error["type"] == E_ERROR)) {
	return "$error[message] in $error[file] on line $error[line]\n";
    }
    return $str;
}

function string_insert(&$src, $pos, $text) {
    return substr($src, 0, $pos) . $text . substr($src, $pos);
}

function alpha_to_int($alpha) {
    $alpha = strtoupper($alpha);
    $x = 0;
    $len = strlen($alpha);
    for($i = 0; $i < $len; $i++) {
	$x += (ord($alpha[$i]) - ord('A') + 1) * pow(26, $len - $i - 1);
    }
    $x--;
    return (int)$x;
}

function int_to_alpha($num) {
    $i = 0;
    $alpha = '';
    $fix = 0;
    $num++;
    do {
	$num--;
	$x = ($num % 26);
	$alpha = chr($x + ord('A')) . $alpha;
	$num = floor($num / 26);
	$fix = 1;
    } while($num > 0);
    return $alpha;
}

function common_strwidth($text) {
    if(function_exists('mb_strwidth'))
	return mb_strwidth($text);
    else
	return strlen($text);
}

function run_queue($args, $dom, $show_options, &$queue,
		   $processor, $initializer = null, $finalizer = null, &$func_arg = null) {
    $time_limit = 10;
    $max_execution_time = ini_get('max_execution_time');
    if($max_execution_time * 0.8 < $time_limit)
	$time_limit = $max_execution_time * 0.8;

    $queue_num = count($queue);

    if(empty($args['start_time']))
	$args['start_time'] = time();

    $offset = 0;
    if(!isset($args['offset'])) {
	if(!empty($initializer)) {
	    if($initializer($args, $dom, $func_arg) === false)
		return;
	}
    } else {
	$offset = max(0, (int)$args['offset']);
	while($offset < $queue_num) {
	    if($processor($args, $dom, $func_arg, $queue[$offset]) === false)
		return;
	    $execution_time = execution_time();
	    $offset++;
	    if($time_limit < $execution_time) {
		break;
	    } 
	}
    }

    $p = dom_append_child($dom, 'p');
    $text = lf($show_options['progress'], 
	       array('num' => $offset,
		     'totalnum' => $queue_num,
		     'percent' => $queue_num == 0 ? 100 : round($offset * 100 / $queue_num, 1)));
    dom_append_text($p, $text);
    $progress = dom_append_child($dom, 'progress', '', array('max' => $queue_num, 'value' => $offset));
    dom_append_text($progress, l($show_options['no_progress']));

    $elapsed = time() - $args['start_time'];
    dom_append_text(
	$p, ' ' . lf('Elapsed time $time$',
		     array('time' => nf_date('%H:%M:%S', $elapsed - date('Z')))));

    if(!empty($args['done'])) {
	if(!empty($finalizer))
	    $finalizer($args, $dom, $func_arg);
	else {
	    $p = dom_append_child($dom, 'p');
	    dom_append_text($p, l('Done.'));
	}
    } else {
	$new_args = $args;
	$new_args['offset'] = $offset;
	if($offset >= $queue_num)
	    $new_args['done'] = 'true';

	if(DEBUG_MODE === 'true' && false) {
	    $p = dom_append_child($dom, 'p');
	    dom_append_link($p, url_make('', $new_args), l('Next'));
	} else {
	    /*
	     * 続きは POST で送る。
	     *
	     * 上流はここで <meta http-equiv="refresh"> を出していたが、
	     * それは GET なので 0.4.0 の CSRF 対策に弾かれる (app/csrf.inc)。
	     * 1 回目のリクエストは準備しかしないため、実際には 1 ページも
	     * 処理されずに 403 で終わっていた (検索インデックスの再構築と
	     * 他 wiki からのインポート)。
	     *
	     * フォームを出して JavaScript で送る。JavaScript が無いときは
	     * 「続ける」を押してもらう (押さなければ途中で止まるだけで、
	     * 作りかけのものが使われることはない)。
	     */
	    $form = dom_append_form($dom, $new_args, 'run_queue');
	    $noscript = dom_append_child($form, 'noscript');
	    dom_append_child($noscript, 'input', '',
			     array('type' => 'submit', 'value' => l('Continue')));
	    head_tags_add_javascript('resource/script/run_queue.js');
	}
    }
}

function print_stacktrace($message = '') {
    $frames = debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS);
    if(!empty($frames)) {
	print("<pre>\n");
	print($message . "\n");
	foreach($frames as $frame) {
	    printf("%20s:%d %s()\n", basename(@$frame['file']), @$frame['line'], $frame['function']);
	}
	print("</pre>\n");
    }
}

if(DEBUG_MODE === 'true') {
    function stacktrace_error_handler($errno,$message,$file,$line,$context) {
	print_stacktrace($message);
	return false;
    }
    set_error_handler("stacktrace_error_handler");

    function debug_log($message) {
	$frames = debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS);
	$frame = reset($frames);

	$output = nf_date(TIME_FORMAT) . ' ';
	$output .= basename($frame['file']) .':'. $frame['line'] . ' ';
	if(is_scalar($message)) {
	    if(strpos($message, "\n") === false) {
		$output .= $message;
	    } else {
		$output .= "\n" . $message;
	    }
	} else {
	    $output .= "\n" . print_r($message, true);
	}
	$output .= "\n";
	error_log($output, 3, 'debug.log');
    }

    function debug_log_backtrace($message) {
	$frames = debug_backtrace();
	//array_shift($frames);
	$output = $message . "\n";
	if(!empty($frames)) {
	    foreach($frames as $frame) {
		$output .= sprintf("%20s:%d %s()\n", basename(@$frame['file']), @$frame['line'], $frame['function']);
	    }
	}
	debug_log($output);
    }

    $debug_laps = array();
    function debug_watch($name, $facility = 'default') {
	global $debug_laps;
	$debug_laps[$facility][] = array(
	    'name' => $name,
	    'time' => microtime(true),
	    'memory' => memory_get_usage(),
	    );
    }

    function debug_watch_show($facility = 'default') {
	global $debug_laps;
	$output = '<table style="clear: both;">';
	$prelap = array_shift($debug_laps[$facility]);
	foreach($debug_laps[$facility] as $lap) {
	    $output .= sprintf("<tr style=\"text-align:right\">" .
			       "<td>%s</td><td>%.5f</td><td>%s bytes</td><td>%s bytes</td></tr>\n",
			       $lap['name'], 
			       $lap['time'] - $prelap['time'],
			       number_format($lap['memory']),
			       number_format($lap['memory'] - $prelap['memory'])
		);
	    $prelap = $lap;
	}
	$output .= '</table>';
	echo($output);
    }

} else {
    function debug_log($message) {}
    function debug_log_backtrace($frames) {}
    function debug_watch($name, $facility = 'default') {}
    function debug_watch_show($facility = 'default') {}
}
?>
