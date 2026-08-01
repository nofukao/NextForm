<?php
$FILE_CONTENT_TYPE_HANDLER_IMAGE = array(
    'embed' => 'file_image_embed',
    'show' => 'file_image_show',
    'raw' => 'file_image_raw',
    'write' => 'file_image_write',
    );

$FILE_CONTENT_TYPE_HANDLERS['image/jpeg'] = &$FILE_CONTENT_TYPE_HANDLER_IMAGE;
$FILE_CONTENT_TYPE_HANDLERS['image/jpg'] = &$FILE_CONTENT_TYPE_HANDLER_IMAGE;
$FILE_CONTENT_TYPE_HANDLERS['image/pjpeg'] = &$FILE_CONTENT_TYPE_HANDLER_IMAGE;
$FILE_CONTENT_TYPE_HANDLERS['image/png'] = &$FILE_CONTENT_TYPE_HANDLER_IMAGE;
$FILE_CONTENT_TYPE_HANDLERS['image/x-png'] = &$FILE_CONTENT_TYPE_HANDLER_IMAGE;
$FILE_CONTENT_TYPE_HANDLERS['image/gif'] = &$FILE_CONTENT_TYPE_HANDLER_IMAGE;

$FILE_IMAGE_EXIF_TAGS = array(
    'Model', 'DateTime', 'DateTimeOriginal', 'DateTimeDigitized', 'Software',
    'ISOSpeedRatings', 'ExposureTime', 'FNumber', 'ExifVersion', 'ShutterSpeedValue');

function file_image_embed(&$page, $dom, $params = array(PAGE_FILE_IMAGE_RESIZE)) {
    $queries = array('action' => 'raw');
    if(!empty($page['backuptime'])) {
       $queries['option'] = 'history';
       $queries['backuptime'] = $page['backuptime'];
    }

    if(in_array('!link', $params) || dom_in_tag_name($dom, 'a')) {
	$e = $dom;
    } else {
	$e = dom_append_link($dom, url_make($page['name'], $queries));
	if(in_array('open', $params))
	    dom_set_attribute($e, 'target', '_blank');
	dom_set_attribute($e, 'data-link-pagename', $page['name']);
    }
    
    $width = $height = 0;
    if(!empty($page['meta']['image_width']))
	$width = $page['meta']['image_width'];
    if(!empty($page['meta']['image_height']))
	$height = $page['meta']['image_height'];

    foreach($params as $param) {
	image_parse_size($param, $width, $height);
    }

    if(!empty($width) && !empty($height) && !in_array('!adjust', $params)) {
	$queries['width'] = $width;
	$queries['height'] = $height;
    }
    $img = dom_append_child($e, 'img', '',
			    array(
				'src' => url_make($page['name'], $queries),
				'alt' => $page['name'],
				'data-link-pagename' => $page['name'],
				));
    if(!empty($width))
	dom_set_attribute($img, 'width', $width);
    if(!empty($height))
	dom_set_attribute($img, 'height', $height);
    return $e;
}

function file_image_show(&$page, $dom) {
    global $FILE_IMAGE_EXIF_TAGS;

    file_append_meta_info($dom, l('Image width'), $page['meta']['image_width']);
    file_append_meta_info($dom, l('Image height'), $page['meta']['image_height']);

    if(function_exists('exif_imagetype')) {
	$contetns = page_get_contents($page);
	$image_type = exif_imagetype("data://image/jpeg;base64," . base64_encode(substr($contetns, 0, 1024)));
	if($image_type == IMAGETYPE_JPEG) {
	    $exif = exif_read_data("data://image/jpeg;base64," . base64_encode(substr($contetns, 0, 20000)));
	    foreach($FILE_IMAGE_EXIF_TAGS as $name) {
		if(!empty($exif[$name])) {
		    file_append_meta_info($dom, 'Exif ' . $name, $exif[$name]);
		}
	    }
	    if(PAGE_FILE_SHOW_GPS == 'true' && !empty($exif['GPSLatitude']) && !empty($exif['GPSLongitude'])) {
		$latlng = array();
		foreach(array('GPSLatitude', 'GPSLongitude') as $k) {
		    $v = $exif[$k];
		    if(count($v) == 3) {
			$v = array_map('fraction_to_um', $v);
			$val = $v[0] + $v[1] / 60 + ($v[2]/100)/3600;
			if(!empty($exif[$k.'Ref']) && !($exif[$k.'Ref'] == 'N' || $exif[$k.'Ref'] == 'E'))
			    $val = -$val;
			$latlng[] = $val;
		    }
		}
		if(count($latlng) == 2) {
		    $latlng = implode(',', $latlng);
		    $uri = 'http://maps.google.com/maps?q=' . $latlng;
		    $dd = file_append_meta_info($dom, 'Exif GPS', $latlng);
		    dom_append_text($dd, ' ');
		    dom_append_link($dd, $uri, 'Google map');
		}
	    }
	}
    }
}

function file_image_raw(&$page) {
    $args =& $page['args'];
    $content_type = file_get_content_type($page);
    if($page['contents_size'] < PAGE_FILE_IMAGE_RESIZE_MAX_SIZE &&
	!empty($args['width']) && !empty($args['height']) &&
       ($args['width'] != default_value($page['meta']['image_width']) ||
	$args['height'] != default_value($page['meta']['image_height']))) {
	$cachekey = 'resize' . $args['width'] . 'x' . $args['height'] . '_' . default_value($page['backuptime']);
	
	if(($cfp = cache_open($page['name'], $cachekey)) === false) {
	    $cfp = @cache_create_open($page['name'], $cachekey);
	    if(!$cfp) {
		page_output_contents($page);
		return;
	    }
	    $result = file_image_resize($page['name'], $cfp, $content_type, $args['width'], $args['height']);
	    cache_create_close($cfp, $page['name'], $cachekey);
	    if($result)
		$cfp = cache_open($page['name'], $cachekey);
	    else {
		$cfp = false;
		cache_delete($page['name'], $cachekey);
	    }
	}
	if($cfp !== false) {
	    while(!feof($cfp))
		echo(fread($cfp, CONTENTS_READ_BUFFER_SIZE));
	    return;
	}
    }

    page_output_contents($page);
}

function file_image_write(&$page, &$contents) {
    if(is_array($contents) && !empty($contents['upload_file'])) {
	if(function_exists('getimagesize')) {
	    $size = getimagesize($contents['upload_file']);
	    $page['meta']['image_width'] = default_value($size[0], '');
	    $page['meta']['image_height'] = default_value($size[1], '');
	    if(!empty($size['mime']))
		$page['meta']['Content-type'] = $size['mime'];
	}
    } else {
	if(function_exists('imagecreatefromstring')) {
	    $data = stream_get_contents($contents);
	    $src = imagecreatefromstring($data);
	    $page['meta']['image_width'] = imagesx($src);
	    $page['meta']['image_height'] = imagesy($src);
	    fclose($contents);
	}
    }
}

function file_image_resize($pagename, $wfp, $content_type, $width, $height) {
    global $ob_buffer_writer_handler;
    if(!function_exists('imagecreatefromstring'))
	return false;
    $source_filename = 'twpage://' . urlencode($pagename);

    $function_imagecreate = false;
    $function_image = false;
    $transparent = '';
    
    if(preg_match('|jpe?g$|', $content_type)) {
	$function_imagecreate = 'imagecreatefromjpeg';
	$function_image = 'imagejpeg';
    } else if(preg_match('|png$|', $content_type)) {
	$function_imagecreate = 'imagecreatefrompng';
	$function_image = 'imagepng';
	$transparent = 'alpha';
    } else if(preg_match('|gif$|', $content_type)) {
	$function_imagecreate = 'imagecreatefromgif';
	$function_image = 'imagegif';
	$transparent = 'fill';
    } else {
	return false;
    }
    $src = $function_imagecreate($source_filename);
    if($src === false)
	return false;
    $dst = imagecreatetruecolor($width, $height);
    if($dst === false)
	return false;

    if($transparent === 'alpha') {
	imagealphablending($dst, false);
	imagesavealpha($dst, true);
    } else if($transparent == 'fill') {
	$alpha = imagecolortransparent($src);
	imagefill($dst, 0, 0, $alpha);
	imagecolortransparent($dst, $alpha);
    }

    imagecopyresampled($dst, $src, 0, 0, 0, 0,
		       $width, $height, imagesx($src), imagesy($src));

    $ob_buffer_writer_handler = $wfp;
    ob_start('ob_buffer_writer', CONTENTS_READ_BUFFER_SIZE);
    $function_image($dst);
    ob_end_clean();

    imagedestroy($src);
    imagedestroy($dst);
    
    return true;
}

?>
