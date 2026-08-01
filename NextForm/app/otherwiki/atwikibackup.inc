<?php
global $LANGUAGE;
global $IMPORT_OTHERWIKI_ENCODE_OPTIONS;
global $IMPORT_OTHERWIKI_ENCODE;
global $IMPORT_OTHERWIKI_MAX_PAGE_NUM;
require_once('pukiwikibase.inc');

$LANGUAGE['ja']['@wiki backup zip file'] = '@wikiデータバックアップのzipファイル';
$LANGUAGE['ja']['@wiki backup zip file encoding'] = 'バックアップ時の文字コード';
$LANGUAGE['ja']['Zip extension for PHP is not enabled.'] =
    'PHP Zip 拡張が有効でないためこの機能を使用できません．';
$LANGUAGE['ja']['Page not found in backup file.'] =
    'バックアップファイルの中にページ情報が見つかりませんでした．';
$LANGUAGE['ja']['You cannot import more than $num$ pages.'] =
    '$num$ページより多いファイルはインポートできません．';

$IMPORT_OTHERWIKI_MAX_PAGE_NUM = 1000;
$IMPORT_OTHERWIKI_ENCODE = 'Shift_JIS';
$IMPORT_OTHERWIKI_ENCODE_OPTIONS = array('Shift_JIS' => 'Shift_JIS', 'UTF-8' => 'UTF-8', 'EUC-JP' => 'EUC-JP');

function import_otherwiki_form($form) {
    global $IMPORT_OTHERWIKI_ENCODE_OPTIONS;

    $a = dom_create('a');
    dom_set_attribute($a, 'target', '_blank');
    dom_append_text($a, l('このページ'));
    dom_set_attribute($a, 'href', 'http://toratora.wiki/?%40wiki%E3%81%8B%E3%82%89%E3%81%AE%E7%A7%BB%E8%A1%8C%E6%96%B9%E6%B3%95');
    ld(message_error(), 
       '@wikiデータバックアップからのインポートはまだ十分に動作が検証されていません．'.
       '$cautionpage$を確認の上で使用して下さい．',
       array('cautionpage' => $a));
    ld(message_error(), 'この機能は@wikiの安全性を保証しているわけではありませんので自己責任で利用して下さい．');

    $max_size = max_upload_file_size();
    dom_append_input_hidden(
	$form, array('MAX_FILE_SIZE' => $max_size));
    dom_set_attribute($form, 'method', 'POST');
    dom_set_attribute($form, 'enctype', 'multipart/form-data');
    
    $dl = dom_append_child($form, 'dl');

    $dt = dom_append_child($dl, 'dt');
    dom_append_text($dt, l('@wiki backup zip file'));
    $dd = dom_append_child($dl, 'dd');
    $label = dom_append_child($dd, 'label', 'file_upload');
    dom_append_child($label, 'input', '',
		     array('type' => 'file',
			   'name' => 'otherwiki_file',
			   'accept' => 'application/zip',
			   'required' => 'required'));

    $dt = dom_append_child($dl, 'dt');
    dom_append_text($dt, l('@wiki backup zip file encoding'));
    $dd = dom_append_child($dl, 'dd');
    dom_append_selector($dd, 'otherwiki_encode', $IMPORT_OTHERWIKI_ENCODE_OPTIONS);
}

function import_otherwiki_form_check(&$args) {
    global $IMPORT_OTHERWIKI_ENCODE_OPTIONS;
    if(!isset($IMPORT_OTHERWIKI_ENCODE_OPTIONS[$args['otherwiki_encode']])) {
	message_error(l('Invalid parameter.'));
	return false;
    }
    unset($args['MAX_FILE_SIZE']);
    return true;
}

function import_otherwiki_create_queue($args) {
    global $IMPORT_OTHERWIKI_ENCODE;
    global $IMPORT_OTHERWIKI_MAX_PAGE_NUM;
    $IMPORT_OTHERWIKI_ENCODE = $args['otherwiki_encode'];

    $queue = array();

    if(($zip = import_otherwiki_open_zipfile()) === false)
	return false;
    for($i = 0; $i < $zip->numFiles; $i++) {
	$filename = $zip->getNameIndex($i);
	if(preg_match('|/[0-9]+.txt$|', $filename)) {
	    $queue[] = $i;
	}
    }
    if(empty($queue)) {
	message_error(l('Page not found in backup file.'));
	return false;
    }

    if(count($queue) > $IMPORT_OTHERWIKI_MAX_PAGE_NUM) {
	message_error(lf('You cannot import more than $num$ pages.',
			 array('num' => $IMPORT_OTHERWIKI_MAX_PAGE_NUM)));
	return false;
    }

    return $queue;
}

function import_otherwiki_prepare($args, $dom) {
    if(empty($_FILES['otherwiki_file']) ||
       !isset($_FILES['otherwiki_file']['name']) ||
       $_FILES['otherwiki_file']['name'] === '') {
	message_error(l('No upload files.'));
	return false;
    }

    if(!is_uploaded_file($_FILES['otherwiki_file']['tmp_name']) ||
       !($fp = fopen($_FILES['otherwiki_file']['tmp_name'], 'rb')) ||
       !(cache_create('', 'otherwiki_file', $fp))) {
	message_error(l('Upload failed.'));
	return false;
    }
    return true;
}

function import_otherwiki_initializer($args, $dom, &$func_arg) {
    return true;
}

function import_otherwiki_finalizer($args, $dom, &$func_arg) {
    cache_delete('', 'otherwiki_file');
    return true;
}

function import_otherwiki_processor($args, $dom, &$zip, &$job) {
    global $IMPORT_OTHERWIKI_ENCODE;
    $IMPORT_OTHERWIKI_ENCODE = $args['otherwiki_encode'];

    if($zip === false) {
	if(($zip = import_otherwiki_open_zipfile()) === false) {
	    message_error(l('zip open failed.'));
	    return false;
	}
    }

    $index = $job;
    $stat = $zip->statIndex($index);
    if(!$stat || !isset($stat['size']) || !isset($stat['mtime'])) {
	message_error(l('zip entry stat failed.'));
	return false;
    }
    if($stat['size'] > CONTENTS_READ_MAX_SIZE) {
	message_error(l('zip entry too large.'));
	return true;
    }
    $contents = $zip->getFromIndex($index);
    if($contents === false) {
	message_error(l('zip entry read failed.'));
	return true;
    }
    $contents = import_otherwiki_convert_encoding($contents);

    $lines = explode("\n", $contents);
    $meta = array();
    $source = '';
    $lines_num = count($lines);
    for($i = 0; $i < $lines_num; $i++) {
	$line = trim($lines[$i]);
	if(preg_match('/^(.*):(.*)$/', $line, $matches)) {
	    $meta[$matches[1]] = $matches[2];
	} else if(preg_match('/^-{5,}$/', $line)) {
	    break;
	}
    }
    $source = implode("\n", array_slice($lines, $i + 1));
    $source = str_replace("\r", '', $source);

    if(!isset($meta['ページ番号']) ||
       !isset($meta['ページ名']) ||
       !isset($meta['編集モード番号'])) {
	return true;
    }

    $pagename = $meta['ページ名'];
    //$page = page_read($pagename);
    //page_truncate($page);
    $page = page_read($pagename);
    $page['force_mtime'] = $stat['mtime'];
    $page['meta']['btime'] = $stat['mtime'];
    if(!isset($page['meta']['ticket']))
	$page['meta']['ticket'] = '';
    $page['keep_mtime'] = true;

    //storage_page_write_backup($page, $stat['mtime'], $source, $error);

    if($meta['編集モード番号'] === '0') {
	$contents = pukiwikibase_convert($page, $source);
	if(page_write($page, $contents, $page['meta']['ticket']) === false) {
	    return false;
	}
    }

    if(isset($meta['タグ'])) {
	$tags = preg_split('/[ ,]/', $meta['タグ']);
	$tags = array_map('trim', $tags);
	$tags = array_filter($tags, 'strlen');
	if(!empty($tags)) {
	    $page = page_read($pagename);
	    $page['keep_mtime'] = true;
	    tag_set_page_tags($page, $tags);
	}
    }

    return true;
}

function import_otherwiki_open_zipfile() {
    if(!class_exists('ZipArchive')) {
	message_error(l('Zip extension for PHP is not enabled.'));
	return false;
    }
    $cach_filepath = cache_get_as_filepath('',  'otherwiki_file');
    if($cach_filepath === false) {
	message_error(l('Upload failed.'));
	return false;
    }
    $zip = new ZipArchive();
    if(($zip->open($cach_filepath)) === false) {
	message_error(l('Upload failed.'));
	return false;
    }
    return $zip;
}

function import_otherwiki_convert_encoding($source) {
    global $IMPORT_OTHERWIKI_ENCODE;
    return mb_convert_encoding($source, 'UTF-8', $IMPORT_OTHERWIKI_ENCODE);
}

function otherwiki_arg_search(&$args, $needle) {
    foreach($args as $arg) {
	if(trim($arg) == $needle)
	    return true;
    }
    return false;
}

function otherwiki_trim_pagename($pagename) {
    return trim($pagename, '[]');
}

/*
  import_otherwiki_test();
  exit(1);
*/
function import_otherwiki_test() {
    header('Content-type: text/plain');
    $source = file_get_contents('test.txt');
    $page = page_read('Top');
    $output = pukiwikibase_convert($page, $source);
    
    print($output);
    print("-----------------------------------------\n");
    print($source);
}
?>
