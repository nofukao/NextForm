<?php
$TOOLS['page']['attach'] = array(
    'order' => 100,
    'show' => 'attach_tool'
    );

function attach_tool(&$page, $dom) {
    if(page_get_type($page) != 'file') {
	tools_add_link(
	    $dom,
	    array('name' => l('Attach'),
		  'disable' => empty($page),
		  'uri' => url_make($page['name'], array('option' => 'attach'))));
    }
}

$OPTIONS['attach'] = array(
    'show' => 'attach_show',
    'write' => 'attach_write',
    'list' => 'attach_list',
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
	pagelist_append($section, $file_subpages, array('fields' => '[[basename]]'));
    }

    return false;
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
