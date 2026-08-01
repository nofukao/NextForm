<?php
$FILE_CONTENT_TYPE_HANDLER_AUDIO = array(
    'embed' => 'file_audio_embed',
    'show' => 'file_audio_show',
    'write' => 'file_audio_write',
    );

$FILE_AUDIO_CONTENT_TYPES = array(
    'audio/mp3', 'audio/x-mp3', 'audio/mpeg3', 'audio/x-mp3', 'audio/x-mpeg3',
    'audio/mpeg', 'audio/x-mpeg',
    'audio/3gpp', 'audio/3gpp2', 
    'audio/aac', 'audio/x-aac', 'audio/aacp',
    'audio/amr',
    'audio/aiff', 'audio/x-aiff',
    'audio/mid', 'audio/midi', 'audio/x-midi', 
    'audio/mp4', 'audio/m4a', 'audio/x-m4a', 'audio/x-m4p', 'audio/x-m4b', 'audio/mp4a-latm',
    'audio/mpga', 'audio/mpeg4-generic',
    'audio/wave', 'audio/wav', 'audio/x-wav',
    'audio/ogg',
    'audio/webm',
    );
foreach($FILE_AUDIO_CONTENT_TYPES as $mime) {
    $FILE_CONTENT_TYPE_HANDLERS[$mime] = &$FILE_CONTENT_TYPE_HANDLER_AUDIO;
}

function file_audio_embed(&$page, $dom, $params = array()) {
    $queries = array('action' => 'raw');
    if(!empty($page['backuptime'])) {
       $queries['option'] = 'history';
       $queries['backuptime'] = $page['backuptime'];
    }
    $audio_attributes = array();
    $audio_attributes['controls'] = '';
    $audio_attributes['preload'] = 'none';
    $audio = dom_append_child($dom, 'audio', '', $audio_attributes);
    $content_type = file_get_content_type($page);
    $source = dom_append_child($audio, 'source', '', 
			       array('src' => url_make($page['name'], $queries),
				     'type' => $content_type
				   ));
    $a = dom_append_link($audio, url_make($page['name'], $queries), $page['name'], 'download');
    dom_set_attribute($a, 'data-link-pagename', $page['name']);
    return $audio;
}

function file_audio_show(&$page, $dom) {
    file_append_meta_info($dom, l('Audio duration'), $page['meta']['audio_duration']);
}

function file_audio_write(&$page, &$contents) {
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
	    $page['meta']['audio_duration'] = $matches[1];
    }
}

?>
