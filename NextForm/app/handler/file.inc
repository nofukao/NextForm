<?php
$HANDLERS['file'] = array(
    'show' => 'file_show',
    'source' => 'file_show',
    'summary' => 'file_summary',
    'raw'  => 'file_raw',
    'edit' => 'file_edit',
    'write' => 'file_write',
    'view' => 'file_view');

$FILE_CONTENT_TYPE_HANDLERS = array();

require_once(APP_DIR_PATH . '/handler/file/file_binary.inc');
require_once(APP_DIR_PATH . '/handler/file/file_image.inc');
require_once(APP_DIR_PATH . '/handler/file/file_video.inc');
require_once(APP_DIR_PATH . '/handler/file/file_audio.inc');
require_once(APP_DIR_PATH . '/handler/file/file_html.inc');
require_once(APP_DIR_PATH . '/handler/file/file_markdown.inc');

function file_get_content_type(&$page) {
    global $FILE_CONTENT_TYPE_HANDLERS;
    if(!empty($page['meta']['Content-type']) &&
       !empty($FILE_CONTENT_TYPE_HANDLERS[$page['meta']['Content-type']]))
	return $page['meta']['Content-type'];
    return 'application/octet-stream';
}

/*
 * md は種類ではなく拡張子で見分けるので、登録された種類より先に見る
 * (file_markdown.inc)。text/plain で送られてきた .md も md として扱う。
 */
function &file_get_content_type_handler(&$page) {
    global $FILE_CONTENT_TYPE_HANDLERS;
    global $FILE_CONTENT_TYPE_HANDLER_BINARY;
    global $FILE_CONTENT_TYPE_HANDLER_MARKDOWN;
    if(file_is_markdown($page))
	return $FILE_CONTENT_TYPE_HANDLER_MARKDOWN;
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

/*
 * 開いた画面 (action=view)。md は wiki の画面の中に描画する (file_markdown.inc)。
 * 描画する種類でなければ、ファイルのページを出す。
 */
function file_view(&$page, $dom) {
    $content_type_handler = &file_get_content_type_handler($page);
    if(!empty($content_type_handler['view']) && function_exists($content_type_handler['view']))
	$content_type_handler['view']($page, $dom);
    else
	file_show($page, $dom);
}

/*
 * 要約。画像や動画は小さく出す。リンクになる種類 (embed_label の印がある
 * PDF・md など) には大きさを渡さない。並びの引数を表示名として読むので、
 * 「max 100x100」というリンクになっていた。
 */
function file_summary(&$page, $dom) {
    $content_type_handler = &file_get_content_type_handler($page);
    $params = array(WIKI_SUMMARY_IMAGE_RESIZE);
    if(!empty($content_type_handler['embed_label']))
	$params = array();
    $content_type_handler['embed']($page, $dom, $params);
}

/*
 * Content-Disposition。$disposition は attachment (保存させる) か inline (ブラウザで
 * 開く)。名前は RFC 6266 の filename* で UTF-8 のまま伝え、それを読まない相手の
 * ために、英数字以外を _ にした名前も付ける。添付画面のまとめてダウンロードも使う。
 *
 * 上流はメールの書き方 (=?UTF-8?B?...?=) を流用し、MSIE にだけ URL の符号化を
 * 使っていた。HTTP の決まりに無い書き方で、ブラウザ以外の道具は読めない
 * (curl -OJ は Base64 の途中の / から後ろを名前にして保存した)。
 * User-Agent を送らない相手では警告も出ていた。
 */
function file_content_disposition($disposition, $filename) {
    $fallback = preg_replace('/[^A-Za-z0-9._-]/u', '_', $filename);
    return $disposition . '; filename="' . $fallback . '"; filename*=UTF-8\'\'' . rawurlencode($filename);
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

    $filename = page_get_simplename($target_page['name']);

    header('Content-type: '. $content_type);
    //if(isset($target_page['contents_size'])) // don't send content-length because of image resizing
    //header('Content-Length: '. $target_page['contents_size']);
    header('X-Content-Type-Options: nosniff');

    if($content_type == 'application/octet-stream')
	header('Content-Disposition: ' . file_content_disposition('attachment', $filename));
    else
	header('Content-Disposition: ' . file_content_disposition('inline', $filename));

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
