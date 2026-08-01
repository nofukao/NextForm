<?php
define('THEME_HTML_FILE_NAME', 'html.php');
define('THEME_SETUP_FILE_NAME', 'setup.php');

function theme_debug_is_enable() {
    return DEBUG_MODE === 'true' && THEME_DEBUG_MODE === 'true';
}

function theme_get_themes() {
    $themes = array();
    $files = glob_array(APP_DIR_PATH . '/theme/*/' . THEME_HTML_FILE_NAME);
    foreach($files as $file) {
	$theme_name = basename(dirname($file));
	$themes[$theme_name] = $theme_name;
    }
    return $themes;
}

function theme_get_source_dir_path($theme = '') {
    return APP_DIR_PATH . '/theme' . ($theme === '' ? '' : '/' . $theme);
}

function theme_setup() {
    global $SETUP_CONSTANTS;
    global $LANGUAGE;

    $theme_setup_file = theme_get_source_dir_path(THEME) . '/' . THEME_SETUP_FILE_NAME;
    if(is_file($theme_setup_file)) {
	$result = @include($theme_setup_file);
	if($result === false) {
	    message_error(lf('Invalid theme file $file$.', array('file' => l($theme_setup_file))));
	}
    }

    setup_local();
}

function theme_convert($theme) {
    return theme_convert_dir($theme, $theme, $theme);
}

function theme_convert_dir($theme, $src, $dst) {
    $src_path = theme_get_source_dir_path() . '/' . $src;
    $files = glob($src_path . '/*');
    if($files === false)
	return false;
    foreach($files as $file) {
	$filename = basename($file);
	$dst_path = THEME_DIR_PATH . '/' . $dst;
	if(is_file($file)) {
	    if(!is_dir($dst_path)) {
		if(!mkdir($dst_path, 0777, true)) {
		    message_error(lf('mkdir failed. Check $dir$ permission.', array('dir' => $dst_path)));
		    return false;
		}
	    }
	    if(!theme_convert_file($theme, $src_path . '/' . $filename, $dst_path . '/' . $filename))
		return false;
	} else {
	    if(!theme_convert_dir($theme, $src . '/' . $filename, $dst . '/' . $filename))
		return false;
	}
    }
    return true;
}

function theme_convert_file($theme, $src, $dst) {
    global $ob_buffer_writer_handler;
    if(!is_file($src))
	return false;
    $path_info = pathinfo($src);
    $extension = default_value($path_info['extension'], '');

    if($extension === 'php')
	return true;

    $wfp = fopen($dst, 'wb');
    if(!$wfp) {
	return false;
    }
    $convert_extension = array('css' => true, 'js' => true, 'html' => true);
    if(isset($convert_extension[$extension])) {
	ob_start('ob_error_handler');
	$result = @include($src);
	$output = ob_get_clean();
	if($result === false) {
	    fclose($wfp);
	    return false;
	}
	fwrite($wfp, $output);
    } else {
	$rfp = fopen($src, 'rb');
	if(!$rfp) {
	    fclose($wfp);
	    return false;
	}
	stream_copy_to_stream($rfp, $wfp);
	fclose($rfp);
    }
    fclose($wfp);
    return true;
}

function theme_simple_screen_init() {
    global $TOOLS;
    unset($TOOLS['site']);
    unset($TOOLS['user']);
    unset($TOOLS['page']);
    unset($TOOLS['search']);
    message_init();
    head_tags_init();
    title_init();
}

function theme_simple_screen() {
    global $main_page;
    $main_page = null;
    page_stop_processing();
    require_once(APP_DIR_PATH . '/theme/' . THEME . '/' . THEME_HTML_FILE_NAME);
}

function theme_avoid_cache($enable) {
    global $THEME_AVOID_CACHE_ENABLE;
    $THEME_AVOID_CACHE_ENABLE = $enable;
}

function theme_get_call_file($depth) {
    $frames = debug_backtrace();
    debug_log($frames);
    while(!is_null($frame = array_shift($frames)) && --$depth > 0);
    return $frame['file'];
}

function trw_echo($string) {
    echo(htmlspecialchars($string));
}

function trw_lang() {
    trw_echo(LANG);
}

function trw_site_url() {
    $filename = basename($_SERVER['SCRIPT_NAME']);
    if($filename === 'index.php' || $filename === '')
	trw_echo('.');
    else
	trw_echo($filename);
}

function trw_theme_url($file_path, $escape = true) {
    global $THEME_AVOID_CACHE_ENABLE;
    if(theme_debug_is_enable())
	$theme_path = url_make('', array('option' => 'theme', 'path' => $file_path));
    else {
	$theme_path = THEME_URI . '/' . THEME . '/' . $file_path;
	if(isset($THEME_AVOID_CACHE_ENABLE) && $THEME_AVOID_CACHE_ENABLE)
	    $theme_path .= '?' . md5(rand() . uniqid());
    }
    if($escape)
	echo(htmlspecialchars($theme_path));
    else
	echo $theme_path;
}

function trw_theme_css_url($file_path) {
    if(theme_debug_is_enable())
	$theme_path = url_make('', array('option' => 'theme', 'path' => $file_path));
    else {
	$call_filepath = theme_get_call_file(3);
	$theme_path = relative_path($call_filepath, APP_DIR_PATH . '/theme/' . THEME . '/' . $file_path);
    }
    echo($theme_path);
}

function trw_theme_css_upload_url($file_path) {
    $theme_path = $file_path;
    if(!theme_debug_is_enable()) {
	$call_filepath = theme_get_call_file(3);
	$theme_path = relative_path($call_filepath, APP_DIR_PATH . '/' . $file_path);
    }
    echo($theme_path);
}

function trw_head_tags() {
    echo(head_tags());
}

function trw_title_head($page_template = '$pagename$ - $sitename$', $site_template = '$sitename$') {
    echo(title_head($page_template, $site_template));
}

function trw_title_html() {
    echo(title_html());    
}

function trw_site_name() {
    echo(htmlspecialchars(SITE_NAME));
}

function trw_page_menu() {
    global $main_page;
    echo(tools('page', $main_page));
}

function trw_tools($category, &$page = null) {
    echo(tools($category, $page));
}

function trw_message() {
    echo(message_html());
}

function trw_main_contents() {
    global $main_page_output;
    echo($main_page_output);
}

function trw_page_tags() {
    global $main_page;
    echo(tool_page_tags($main_page));
}

function trw_page_info() {
    global $main_page;
    echo(tool_page_info($main_page));
}

function trw_page($pagename) {
    if(auth_has_permission($pagename, 'read')) {
	echo(page($pagename));
    }
}

function trw_official_url() {
    echo(htmlspecialchars(OFFICIAL_URL));
}

function trw_color($color) {
    echo($color);
}

function trw_color_invert_rgb($color) {
    $rgb = color_html_to_rgb($color);
    echo(color_rgb_to_html(array('r' => 255 - $rgb['r'], 'g' => 255 - $rgb['g'], 'b' => 255 - $rgb['b'])));
}

function trw_color_comp($color) {
    $rgb = color_html_to_rgb($color);
    $t = max($rgb) + min($rgb);
    echo(color_rgb_to_html(array('r' => $t - $rgb['r'], 'g' => $t - $rgb['g'], 'b' => $t - $rgb['b'])));
}

function trw_color_hsv($color, $fh = 0, $fs = 1, $fv = 1) {
    $hsv = color_html_to_hsv($color);
    if($hsv !== false) {
	if($fs < 0) $hsv['s'] = 1 - $hsv['s'];
	if($fv < 0) $hsv['v'] = 1 - $hsv['v'];
	$hsv['h'] += $fh;
	$hsv['s'] *= abs($fs);
	$hsv['v'] *= abs($fv);
	$hsv['s'] = max(0, min($hsv['s'], 1));
	$hsv['v'] = max(0, min($hsv['v'], 1));
	$rgb = color_hsv_to_rgb($hsv);
	echo(color_rgb_to_html($rgb));
    } else
	echo($color);
}

function trw_color_h($color, $fh = 0) {
    trw_color_hsv($color, $fh,  1, 1);
}

function trw_color_s($color, $fs = 0) {
    trw_color_hsv($color, 0, $fs, 1);
}

function trw_color_v($color, $fv = 0) {
    trw_color_hsv($color, 0, 1, $fv);
}

function trw_color_blend($color1, $color2) {
    $hsv1 = color_html_to_hsv($color1);
    $hsv2 = color_html_to_hsv($color2);
    if($hsv1 !== false && $hsv2 !== false) {
	$hsv1['h'] = ($hsv1['h'] + $hsv2['h']) / 2;
	$hsv1['s'] = ($hsv1['s'] + $hsv2['s']) / 2;
	$hsv1['v'] = ($hsv1['v'] + $hsv2['v']) / 2;
	$rgb = color_hsv_to_rgb($hsv1);
	echo(color_rgb_to_html($rgb));
    } else
	echo($color1);
}

?>