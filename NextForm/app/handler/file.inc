<?php
$HANDLERS['file'] = array(
    'show' => 'file_show',
    'source' => 'file_show',
    'summary' => 'file_summary',
    'raw'  => 'file_raw',
    'edit' => 'file_edit',
    'write' => 'file_write');

$FILE_CONTENT_TYPE_HANDLERS = array();

require_once(APP_DIR_PATH . '/handler/file/file_binary.inc');
require_once(APP_DIR_PATH . '/handler/file/file_image.inc');
require_once(APP_DIR_PATH . '/handler/file/file_video.inc');
require_once(APP_DIR_PATH . '/handler/file/file_audio.inc');

function file_get_content_type(&$page) {
    global $FILE_CONTENT_TYPE_HANDLERS;
    if(!empty($page['meta']['Content-type']) &&
       !empty($FILE_CONTENT_TYPE_HANDLERS[$page['meta']['Content-type']]))
	return $page['meta']['Content-type'];
    return 'application/octet-stream';
}

function &file_get_content_type_handler(&$page) {
    global $FILE_CONTENT_TYPE_HANDLERS;
    global $FILE_CONTENT_TYPE_HANDLER_BINARY;
    if(!empty($page['meta']['Content-type']) &&
       !empty($FILE_CONTENT_TYPE_HANDLERS[$page['meta']['Content-type']]))
	return $FILE_CONTENT_TYPE_HANDLERS[$page['meta']['Content-type']];
    return $FILE_CONTENT_TYPE_HANDLER_BINARY;
}

function file_append_meta_info($dom, $name, &$value, $unit = '', $f = '') {
    if(isset($value)) {
	$dt = dom_append_child($dom, 'dt');
	dom_append_text($dt, $name);
	$dd = dom_append_child($dom, 'dd');
	if(!empty($unit))
	    $u = ' ' . $unit;
	else
	    $u = '';
	if(function_exists($f))
	    dom_append_text($dd, $f($value) . $u);
	else
	    dom_append_text($dd, $value . $u);
	return $dd;
    }
}

function file_show(&$page, $dom) {
    $content_type = file_get_content_type($page);
    $content_type_handler = &file_get_content_type_handler($page);

    $content_type_handler['embed']($page, $dom);

    $dl = dom_append_child($dom, 'dl');

    file_append_meta_info($dl, l('Original file name'), $page['meta']['original_filename']);
    file_append_meta_info($dl, l('File type'), $page['meta']['Content-type']);
    file_append_meta_info($dl, l('File size'), $page['contents_size'], l('byte(s)'), 'number_format');
    file_append_meta_info($dl, l('Description'), $page['meta']['description']);

    if(!empty($content_type_handler['show']) && function_exists($content_type_handler['show']))
	$content_type_handler['show']($page, $dl);
}

function file_summary(&$page, $dom) {
    $content_type_handler = &file_get_content_type_handler($page);
    $content_type_handler['embed']($page, $dom, array(WIKI_SUMMARY_IMAGE_RESIZE));
}

function file_raw(&$page, $dom) {
    $args =& $page['args'];
    if(!empty($args['backuptime']))
	$target_page = page_read_backup($page['name'], $args['backuptime']);
    else
	$target_page = &$page;
    if(empty($target_page['is_exists'])) {
	message_error(l('Backup not found.'));
	return;
    }

    $target_page['args'] =& $page['args'];

    $content_type = file_get_content_type($target_page);

    $user_agent = $_SERVER['HTTP_USER_AGENT'];
    $filename = page_get_simplename($target_page['name']);
    if(strpos($user_agent, 'MSIE') !== false)
	$filename = urlencode($filename);
    else
	$filename = '=?UTF-8?B?' . base64_encode($filename) . '?=';

    header('Content-type: '. $content_type);
    //if(isset($target_page['contents_size'])) // don't send content-length because of image resizing
    //header('Content-Length: '. $target_page['contents_size']);
    header('X-Content-Type-Options: nosniff');

    if($content_type == 'application/octet-stream')
	header('Content-Disposition: attachment; filename=' . $filename);
    else
	header('Content-Disposition: inline; filename='. $filename);

    $content_type_handler = &file_get_content_type_handler($target_page);
    if(!empty($content_type_handler['raw']) && function_exists($content_type_handler['raw']))
	$content_type_handler['raw']($target_page);
    else
	page_output_contents($target_page);
    exit(0);
}

function file_edit(&$page, $dom) {
    global $FILE_CONTENT_TYPE_HANDLERS;

    $has_permission = auth_warn($page['name'], 'write') && lock_warn($page);
    title_set_pagename('Editing $pagename$', $page['name']);

    $max_size = max_upload_file_size();

    $hiddens = array(
	'MAX_FILE_SIZE' => $max_size,
	'action' => 'write');
    if(!$page['is_exists'])
	$hiddens['newpage_type'] = 'file';

    $form = dom_append_page_form($page, $dom, $hiddens, 'file_edit');
    dom_set_attribute($form, 'enctype', 'multipart/form-data');

    $ul = dom_append_child($form, 'ul', 'form');
    $li = dom_append_child($ul, 'li');
    
    $label = dom_append_child($li, 'label', 'file_upload');
    if($page['is_exists'])
	dom_append_text($label, l('Overwrite file'));
    else
	dom_append_text($label, l('Upload file'));

    $file = dom_append_child($label, 'input', '', array('type' => 'file', 'name' => 'file'));

    $submit = dom_append_child($li, 'input', '', 
			       array('type' => 'submit', 'accesskey' => 's',
				     'value' => l('Upload') . '(S)', 'name' => 'save_upload'));
    if(!$has_permission)
	dom_set_attribute($submit, 'disabled', 'disabled');

    dom_append_text($li, '(' . l('File size limit') . ': ' . encode_prefixed_number($max_size) . 'B)');
    
    if($page['is_exists']) {

	$li = dom_append_child($ul, 'li');
	$label = dom_append_child($li, 'label', 'content_type');
	dom_append_text($label, l('Change Content-type'));
	dom_append_child($label, 'input', '',
			 array('type' => 'text',
			       'name' => 'content_type',
			       'autocomplete' => 'on',
			       'list' => 'content_type_list',
			       'value' => default_value($page['meta']['Content-type'])));
	$datalist = dom_append_child($label, 'datalist', '', array('id' => 'content_type_list'));
	$content_types = array_keys($FILE_CONTENT_TYPE_HANDLERS);
	foreach($content_types as $content_type) {
	    $option = dom_append_child($datalist, 'option', '', array('value' => $content_type));
	    dom_append_text($option, '');
	}

	$submit = dom_append_child($li, 'input', '', 
				   array('type' => 'submit', 'value' => l('Save'), 'name' => 'save_content_type'));
	if(!$has_permission)
	    dom_set_attribute($submit, 'disabled', 'disabled');

	$li = dom_append_child($ul, 'li');
	$label = dom_append_child($li, 'label');
	dom_append_child($label, 'input', '', array('type' => 'checkbox', 'name' => 'keep_mtime', 'value' => 'on'));
	dom_append_text($label, l('Keep modify time'));
    }
}

function file_write(&$page, $dom) {
    $page['meta']['type'] = 'file';
    if(!empty($page['args']['keep_mtime']))
	$page['keep_mtime'] = true;

    if(!empty($page['args']['save_content_type'])) {
	if(!empty($page['args']['content_type']))
	    $page['meta']['Content-type'] = $page['args']['content_type'];
	$contents = page_open_contents($page);
	$is_stream = true;
    } else {
	if(empty($_FILES['file']) || !isset($_FILES['file']['name']) || $_FILES['file']['name'] === '') {
	    message_error(l('No upload files.'));
	    return;
	}

	$file = &$_FILES['file'];

	if($file['error'] != UPLOAD_ERR_OK ||
	   !is_uploaded_file($file['tmp_name'])) {
	    message_error(l('Upload failed.'));
	    return;
	}

	$page['meta']['Content-type'] = $file['type'];
	$page['meta']['original_filename'] = $file['name'];
	$contents = array('upload_file' => $file['tmp_name']);
	$is_stream = false;
    }

    $content_type_handler = &file_get_content_type_handler($page);
    if(!empty($content_type_handler['write']) && function_exists($content_type_handler['write']))
	$content_type_handler['write']($page, $contents);

    if($is_stream) {
	$contents = page_open_contents($page);
    }

    if(page_write($page, $contents, $page['args']['ticket'])) {
	header('Location: ' . url_make($page['name']));
	exit(0);
    } else {
	message_error(l('Page write failed.'));
    }
}
?>
