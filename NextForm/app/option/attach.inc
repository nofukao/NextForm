<?php
$TOOLS['page']['attach'] = array(
    'order' => 100,
    'show' => 'attach_tool'
    );

function attach_tool(&$page, $dom) {
    $pagename = empty($page) ? '' : $page['name'];
    if(page_get_type($page) != 'file') {
	tools_add_link(
	    $dom,
	    array('name' => l('Attach'),
		  'disable' => empty($page),
		  'uri' => url_make($pagename, array('option' => 'attach'))));
    }
}

$OPTIONS['attach'] = array(
    'show' => 'attach_show',
    'write' => 'attach_write',
    'list' => 'attach_list',
    'download' => 'attach_download',
    );

function attach_show(&$args, $dom) {
    $page = &main_page();
    title_set_pagename('Attach files in $pagename$', $page['name']);

    ld(message_info(), 'Uploaded files will be subpage of $pagename$.', 
       array('pagename' => dom_pagename($page['name'])));

    $max_size = max_upload_file_size();

    $form = dom_append_page_form($page, $dom, 
				 array('MAX_FILE_SIZE' => $max_size,
				       'option' => 'attach',
				       'action' => 'write'), 'attach');
    dom_set_attribute($form, 'enctype', 'multipart/form-data');
    
    $label = dom_append_child($form, 'label', 'file_upload');
    dom_append_text($label, l('Upload files'));
    
    $file = dom_append_child($label, 'input', '',
			     array('type' => 'file',
				   'name' => 'files[]',
				   'multiple' => '',
				 ));

    $submit = dom_append_child($form, 'input', '', 
			       array('type' => 'submit', 'accesskey' => 's',
				     'value' => l('Upload') . '(S)', 'name' => 'save_upload'));


    dom_append_text($form, '(' . l('File size limit') . ': ' . encode_prefixed_number($max_size) . 'B)');

    $file_subpages = page_find($page['name'], array('type' => 'file', 'is_children_only' => true));
    if(!empty($file_subpages)) {
	$section = dom_append_child($dom, 'section');
	$h = dom_append_child($section, 'h1');
	dom_append_text($h, l('Attachment files'));
	/*
	 * チェックしたものをまとめてダウンロードする (attach_download())。
	 * 「すべて選択」のボタンは JavaScript が無いと働かないので、
	 * nextform.js の attachDownloadSetup() が差し込む。
	 */
	$form = dom_append_page_form($page, $section,
				     array('option' => 'attach', 'action' => 'download'),
				     'attach_download');
	pagelist_append($form, $file_subpages, array('fields' => '[[basename]]'));
	attach_append_download_checkboxes($form, $page);
	$p = dom_append_child($form, 'p');
	dom_append_child($p, 'input', '',
			 array('type' => 'submit', 'name' => 'download_files',
			       'value' => l('Download')));
    }

    return false;
}

/*
 * 一覧の各ファイルの前にチェックボックスを差し込む。一覧そのものは
 * pagelist_append() が作ったまま (読めないファイルはそこで落ちる)。
 * 値はページ名の末尾で、どのページの添付かは送り先のページで決まる。
 * <label> で包むのは、ほかの画面のチェックボックスと見た目を揃えるため
 * (nextform.js の checkboxLabelSetup())。
 */
function attach_append_download_checkboxes($form, &$page) {
    $prefix = $page['name'] . '/';
    foreach(iterator_to_array($form->getElementsByTagName('li')) as $li) {
	$pagename = $li->getAttribute('data-pagename');
	if(strpos($pagename, $prefix) !== 0)
	    continue;
	$label = dom_insert_child($li, 'label');
	dom_append_child($label, 'input', '',
			 array('type' => 'checkbox', 'name' => 'download[]',
			       'value' => substr($pagename, strlen($prefix))));
    }
}

/*
 * 添付の一覧でチェックしたファイルを返す。1 つならそのまま、2 つ以上なら
 * ZIP 1 つにまとめる (zip.inc)。どちらも添付したときのバイト列のままで、
 * 画像の縮小 (file_image_raw()) などはかけない。
 *
 * 選べるのはこのページの添付だけ。名前に / を含むものや、種別 file で
 * ないものは断る。読めないファイルは page_read() の auth_check() で
 * 要求ごと止まる。どれか 1 つでも問題があれば、何も返さずに添付画面に戻す
 * (一部だけ入った ZIP を渡さない)。
 */
function attach_download(&$args, $dom) {
    $page = &target_page($args);

    $basenames = array();
    foreach((array)default_value($args['download'], array()) as $basename) {
	if(is_string($basename))
	    $basenames[] = normalize_string($basename);
    }
    $basenames = array_values(array_unique($basenames));
    if(empty($basenames)) {
	message_error(l('Select the files to download.'));
	return attach_show($args, $dom);
    }

    $file_pages = array();
    foreach($basenames as $basename) {
	if($basename === '' || $basename === '.' || $basename === '..' ||
	   strpos($basename, '/') !== false) {
	    message_error(l('The selected file was not found.'));
	    return attach_show($args, $dom);
	}
	$file_page = page_read($page['name'] . '/' . $basename);
	if(empty($file_page['is_exists']) || page_get_type($file_page) != 'file') {
	    message_error(l('The selected file was not found.'));
	    return attach_show($args, $dom);
	}
	$file_pages[] = $file_page;
    }

    if(count($file_pages) == 1) {
	header('Content-Type: application/octet-stream');
	header('Content-Disposition: ' . attach_download_disposition(page_get_simplename($file_pages[0]['name'])));
	header('X-Content-Type-Options: nosniff');
	page_output_contents($file_pages[0]);
	exit(0);
    }

    $entries = array();
    foreach($file_pages as $file_page) {
	$entries[] = array(
	    'name' => page_get_simplename($file_page['name']),
	    'mtime' => $file_page['mtime'],
	    'open' => function() use ($file_page) { return page_open_contents($file_page); });
    }
    $length = zip_prepare($entries);
    if($length === false) {
	message_error(l('The selected files cannot be put into one ZIP file.'));
	return attach_show($args, $dom);
    }
    header('Content-Type: application/zip');
    header('Content-Disposition: ' . attach_download_disposition(page_get_simplename($page['name']) . '.zip'));
    header('Content-Length: ' . $length);
    header('X-Content-Type-Options: nosniff');
    zip_output($entries);
    exit(0);
}

/*
 * ダウンロードとして渡すときの Content-Disposition。名前は RFC 6266 の
 * filename* で UTF-8 のまま伝え、それを読まない相手のために、英数字以外を
 * _ にした名前も付ける。
 */
function attach_download_disposition($filename) {
    $fallback = preg_replace('/[^A-Za-z0-9._-]/u', '_', $filename);
    return 'attachment; filename="' . $fallback . '"; filename*=UTF-8\'\'' . rawurlencode($filename);
}

function attach_write(&$args, $dom) {
    $page = &target_page($args);
    title_set_pagename('Attach files in $pagename$', $page['name']);

    if(empty($_FILES['files']) || !isset($_FILES['files']['name'][0]) || $_FILES['files']['name'][0] === '') {
	message_error(l('No upload files.'));
	return false;
    }

    $files = &$_FILES['files'];
    $files_num = count($files['name']);

    for($i = 0; $i < $files_num; $i++) {
	if($files['error'][$i] != UPLOAD_ERR_OK ||
	   !is_uploaded_file($files['tmp_name'][$i])) {
	    message_error(l('Upload failed.'));
	    return false;
	}
    }

    for($i = 0; $i < $files_num; $i++) {
	$new_pagename = $page['name'] . '/' . $files['name'][$i];
	$new_page = page_read($new_pagename);
	$new_page['meta']['type'] = 'file';
	$new_page['meta']['Content-type'] = $files['type'][$i];
	$new_page['meta']['original_filename'] = $files['name'][$i];
	$contents = array('upload_file' => $files['tmp_name'][$i]);

	if(!empty($new_page['meta']['ticket']))
	    $ticket = $new_page['meta']['ticket'];
	else
	    $ticket = '';

	$content_type_handler = &file_get_content_type_handler($new_page);
	if(!empty($content_type_handler['write']) && function_exists($content_type_handler['write']))
	    $content_type_handler['write']($new_page, $contents);

	if(page_write($new_page, $contents, $ticket)) {
	    ld(message_notice(), 'Upload $filename$.', array('filename' => dom_pagename($new_pagename)));
	} else {
	    message_error(lf('Upload $filename$ failed.', array('filename' => $files['name'][$i])));
	}
    }
    
    return false;
}

function attach_list(&$args, $dom) {
    $page = &target_page($args);
    $response = array('status' => 'ok', 'pages' => array());
    $attach_pages = page_find($page['name'], array('type' => 'file', 'is_children_only' => true));
    foreach($attach_pages as &$attach_page) {
	$relativename = preg_replace('|^' . preg_quote($page['name'], '|') . '|', '.', $attach_page['name']);
	$response['pages'][] = array(
	    'name' => $attach_page['name'],
	    'relativename' => $relativename
	    );
    }
    $json_text = json_encode($response);
    header("Content-Type: application/json; charset=utf-8");
    echo($json_text);
    exit(0);
}
?>
