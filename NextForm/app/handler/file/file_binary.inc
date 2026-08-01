<?php
$FILE_CONTENT_TYPE_HANDLER_BINARY = array(
    'embed' => 'file_binary_embed',
    );

$FILE_CONTENT_TYPE_HANDLERS['text/plain'] = &$FILE_CONTENT_TYPE_HANDLER_BINARY;
$FILE_CONTENT_TYPE_HANDLERS['application/pdf'] = &$FILE_CONTENT_TYPE_HANDLER_BINARY;

function file_binary_embed(&$page, $dom, $params = null) {
    $queries = array('action' => 'raw');
    if(!empty($page['backuptime']))
       $queries['backuptime'] = $page['backuptime'];

    if(is_null($params))
	$params = array(l('Download'));

    $name = $page['name'];
    foreach($params as $param) {
	$name = $param;
    }

    $a = dom_append_link($dom, url_make($page['name'], $queries), $name, 'download');
    dom_set_attribute($a, 'data-link-pagename', $page['name']);
    return $a;
}

?>
