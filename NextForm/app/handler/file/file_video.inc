<?php
$FILE_CONTENT_TYPE_HANDLER_VIDEO = array(
    'embed' => 'file_video_embed',
    'show' => 'file_video_show',
    'write' => 'file_video_write',
    );

$FILE_VIDEO_CONTENT_TYPES = array(
    'video/3gpp', 'video/3gpp2',
    'video/m4v', 'video/x-m4v',
    'video/mp4', 'video/mpeg', 'video/x-mpeg',
    'video/ogg',
    'video/webm',
    );
foreach($FILE_VIDEO_CONTENT_TYPES as $mime) {
    $FILE_CONTENT_TYPE_HANDLERS[$mime] = &$FILE_CONTENT_TYPE_HANDLER_VIDEO;
}


function file_video_embed(&$page, $dom, $params = array(PAGE_FILE_IMAGE_RESIZE)) {
    $queries = array('action' => 'raw');
    if(!empty($page['backuptime'])) {
       $queries['option'] = 'history';
       $queries['backuptime'] = $page['backuptime'];
    }

    $width = $height = 0;
    if(!empty($page['meta']['video_width']))
	$width = $page['meta']['video_width'];
    if(!empty($page['meta']['video_height']))
	$height = $page['meta']['video_height'];
    foreach($params as $param) {
	image_parse_size($param, $width, $height);
    }

    $video_attributes = array();
    $video_attributes['controls'] = '';
    if(!empty($width))
	$video_attributes['width'] = $width;
    if(!empty($height))
	$video_attributes['height'] = $height;

    $video = dom_append_child($dom, 'video', '', $video_attributes);
    $content_type = file_get_content_type($page);
    $source = dom_append_child($video, 'source', '',
			       array('src' => url_make($page['name'], $queries),
				     'type' => $content_type
				   ));
    $a = dom_append_link($video, url_make($page['name'], $queries), $page['name'], 'download');
    dom_set_attribute($a, 'data-link-pagename', $page['name']);
    return $video;
}

function file_video_show(&$page, $dom) {
    file_append_meta_info($dom, l('Video duration'), $page['meta']['video_duration']);
    file_append_meta_info($dom, l('Video width'), $page['meta']['video_width']);
    file_append_meta_info($dom, l('Video height'), $page['meta']['video_height']);
}

function file_video_write(&$page, &$contents) {
    if(is_array($contents) && !empty($contents['upload_file'])) {
	$fp = fopen($contents['upload_file'], 'rb');
    } else {
	$fp = $contents;
    }
    if($fp === false)
	return;

    $descriptorspec = array(
	0 => array("pipe", "r"),
	1 => array("pipe", "w"),
	2 => array("pipe", "w"),
	);
    $process = proc_open(FFMPEG_PATH . ' -i -', $descriptorspec, $pipes);

    if(!is_resource($process))
	return;

    stream_copy_to_stream($fp, $pipes[0]);
    fclose($fp);
    fclose($pipes[0]);

    $ffmepg_output = stream_get_contents($pipes[2]);
    fclose($pipes[2]);
    fclose($pipes[1]);
    proc_close($process);

    $lines = explode("\n", $ffmepg_output);
    foreach($lines as &$line) {
	if(preg_match('/Duration:?\s*([\d:\.]{2,})/', $line, $matches))
	    $page['meta']['video_duration'] = $matches[1];
	if(preg_match('/Stream.*Video:.*\s+(\d+)x(\d+)/', $line, $matches)) {
	    $page['meta']['video_width'] = $matches[1];
	    $page['meta']['video_height'] = $matches[2];
	}
    }
}

?>
