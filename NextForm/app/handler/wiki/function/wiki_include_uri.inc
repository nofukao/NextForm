<?php
$WIKI_OPERATOR_INCLUDE_URI_HANDLERS['image'] = array(
    'regex' => '/\.(jpe?g|png|gif)$/i',
    'function' => 'wiki_operator_include_uri_image');
function wiki_operator_include_uri_image($dom, $uri, $params) {
    $width = $height = 0;

    if(!empty($params)) {
	if(function_exists('getimagesize')) {
	    $cachekey = 'imagedata-' . $uri;
	    $data = cache_serialized_get('', $cachekey, time() - 60 * 60 * 3);
	    if($data !== false) {
		$width = $data['width'];
		$height = $data['height'];
	    } else {
		$size = @getimagesize($uri);
		if(isset($size[0]) && isset($size[1])) {
		    $width = $size[0];
		    $height = $size[1];
		}
		$data = array('width' => $width, 'height' => $height);
		cache_serialized_create('', $cachekey, $data);
	    }
	}
	
	foreach($params as $param) {
	    image_parse_size($param, $width, $height);
	}
    }

    if(in_array('!link', $params) || dom_in_tag_name($dom, 'a')) {
	$e = $dom;
    } else {
	$e = dom_append_link($dom, $uri);
	if(in_array('open', $params))
	    dom_set_attribute($e, 'target', '_blank');
    }
    $img = dom_append_child($e, 'img', '',
			    array(
				'src' => $uri,
				'alt' => $uri,
				));
    if(!empty($width))
	dom_set_attribute($img, 'width', $width);
    if(!empty($height))
	dom_set_attribute($img, 'height', $height);

    return $e;
}

$WIKI_OPERATOR_INCLUDE_URI_HANDLERS['youtube'] = array(
    'regex' => '|^https?://(www.)?' . preg_quote('youtube.com/watch?', '|') . '|',
    'function' => 'wiki_operator_include_uri_youtube');
function wiki_operator_include_uri_youtube($dom, $uri, $params) {
    $parsed_url = parse_url($uri);
    if(empty($parsed_url['query']))
	return dom_append_link($dom, $uri, $uri);

    parse_str($parsed_url['query'], $query);
    if(empty($query['v']))
	return dom_append_link($dom, $uri, $uri);

    $video_id = $query['v'];

    $oembed_url = 'http://www.youtube.com/oembed?url=' . urlencode($uri) . '&format=json';
    if(($json_text = @file_get_contents($oembed_url)) !== false) {
	$json = json_decode($json_text, true);
    }
    $width = default_value($json['width'], WIKI_INCLUDE_URI_DEFAULT_VIDEO_WIDTH);
    $height = default_value($json['height'], WIKI_INCLUDE_URI_DEFAULT_VIDEO_HEIGHT);
    foreach($params as $param) {
	image_parse_size($param, $width, $height);
    }
    $embed_url = '//www.youtube.com/embed/' . urlencode($video_id);
    $iframe = dom_append_child($dom, 'iframe', 'youtube',
			       array('width' => $width, 'height' => $height,
				     'src' => $embed_url,
				     //'frameborder' => '0',
				     //'allowfullscreen' => ''
				   ));
    dom_append_text($iframe, '');
    return $iframe;
}
$WIKI_OPERATOR_INCLUDE_URI_HANDLERS['nico'] = array(
    'regex' => '|^' . preg_quote('http://www.nicovideo.jp/watch/', '|') . '|',
    'function' => 'wiki_operator_include_uri_nico');
function wiki_operator_include_uri_nico($dom, $uri, $params) {
    if(!preg_match('|^' . preg_quote('http://www.nicovideo.jp/watch/', '|') . '(\w+)|', $uri, $matches)) {
	return dom_append_link($dom, $uri, $uri);
    }
    $video_id = $matches[1];
    $width = false;
    $height = false;
    /*
    $info_url = 'http://ext.nicovideo.jp/api/getthumbinfo/' . urlencode($video_id);
    if(($info_text = @file_get_contents($info_url)) !== false) {
	$info_xml = new DOMDocument('1.0','UTF-8');
	if(($info_xml->loadXML($info_text)) !== false) {
	    $xpath = new DOMXPath($info_xml);
	}
    }
    */
    if(!empty($params)) {
	$width = WIKI_INCLUDE_URI_DEFAULT_VIDEO_WIDTH;
	$height = WIKI_INCLUDE_URI_DEFAULT_VIDEO_HEIGHT;
	foreach($params as $param) {
	    image_parse_size($param, $width, $height);
	}
    }

    if(!empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] == 'on' /*&& stripos($_SERVER['HTTP_USER_AGENT'], 'Chrome') !== false*/) {
	$video_url = 'http://ext.nicovideo.jp/thumb/' . urlencode($video_id);
	$iframe = dom_append_child($dom, 'iframe');
	dom_set_attribute($iframe, 'width', '312');
	dom_set_attribute($iframe, 'height', '176');
	dom_set_attribute($iframe, 'src', $video_url);
	dom_set_attribute($iframe, 'scrolling', 'no');
	dom_set_attribute($iframe, 'style', 'border:solid 1px #CCC;');
	dom_set_attribute($iframe, 'frameborder', '0');
	dom_append_text($iframe, '');
	return $iframe;
    }

    if($width !== false && $height !== false)
	$video_url = 'http://ext.nicovideo.jp/thumb_watch/' . urlencode($video_id) . '?w=' . $width . '&h=' . $height;
    else
	$video_url = 'http://ext.nicovideo.jp/thumb_watch/' . urlencode($video_id);
    $script = dom_append_child($dom, 'script', 'nico', 
			       array('type' => 'text/javascript',
				     'src' => $video_url));
    dom_append_text($script, '');
    $noscript = dom_append_child($dom, 'noscript', 'nico');
    dom_append_link($noscript, $uri, $uri);
    return $script;
}
?>
