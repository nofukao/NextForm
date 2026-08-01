<?php
$OPTIONS['theme'] = array(
    'show' => 'theme_show',
    );

function theme_show(&$args, $dom) {
    if(!theme_debug_is_enable())
	exit(0);
    $path = default_value($args['path'], '');
    if(strpos($path, '..') !== false) {
	exit(0);
    }

    $path_info = pathinfo($path);
    $extension = default_value($path_info['extension'], '');
    $content_types = array(
	'html' => 'text/html',
	'htm' => 'text/html',
	'css' => 'text/css',
	'js' => 'text/javascript',
	'jpg' => 'image/jpeg',
	'jpeg' => 'image/jpeg',
	'png' => 'image/png',
	'gif' => 'image/gif',
	);
    if(isset($content_types[$extension])) {
	$content_type = $content_types[$extension];
    } else {
	$content_type = 'application/octet-stream';
    }
    header('Content-type: ' . $content_type);
    theme_convert_file(THEME, theme_get_source_dir_path(THEME) . '/' . $path, 'php://output');
    exit(0);
}
?>
