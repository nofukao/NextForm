<?php
global $LANGUAGE;
global $IMPORT_OTHERWIKI_ENCODE_OPTIONS;
global $IMPORT_OTHERWIKI_ENCODE;
global $IMPORT_OTHERWIKI_IGNORE_PAGES;
require_once('pukiwikibase.inc');

define('IMPORT_OTHERWIKI_PKWK_SPLITTER', '>>>>>>>>>>');
define('IMPORT_OTHERWIKI_LOCALZONE', date('Z'));

$LANGUAGE['ja']['PukiWiki install directory'] = 'PukiWikiをインストールしたディレクトリ';
$LANGUAGE['ja']['PukiWiki data file not found.'] = 'PukiWikiのデータファイルが見つかりませんでした．';
$LANGUAGE['ja']['PukiWiki encoding'] = 'PukiWikiの文字コード';

$IMPORT_OTHERWIKI_IGNORE_PAGES = array(
    '|^PukiWiki$|',
    '|^PukiWiki/.*|',
    '|^:config.*|',
    '|^:RenameLog$|',
    '|^FormatRule$|',
    '|^FormattingRules$|',
    '|^整形ルール$|',
    '|^Help$|', '|^ヘルプ$|',
    '|^RecentChanges$|', '|^RecentDeleted$|',
    );

$IMPORT_OTHERWIKI_ENCODE = 'EUC-JP';
$IMPORT_OTHERWIKI_ENCODE_OPTIONS = array('EUC-JP' => 'EUC-JP', 'UTF-8' => 'UTF-8');

function import_otherwiki_form($form) {
    global $IMPORT_OTHERWIKI_ENCODE_OPTIONS;
    $dl = dom_append_child($form, 'dl');

    $dt = dom_append_child($dl, 'dt');
    dom_append_text($dt, l('PukiWiki install directory'));
    $dd = dom_append_child($dl, 'dd');
    dom_append_child($dd, 'input', '',
		     array('type' => 'text', 'name' => 'otherwiki_path', 'required' => 'required'));

    $dt = dom_append_child($dl, 'dt');
    dom_append_text($dt, l('PukiWiki encoding'));
    $dd = dom_append_child($dl, 'dd');
    dom_append_selector($dd, 'otherwiki_encode', $IMPORT_OTHERWIKI_ENCODE_OPTIONS);
}

function import_otherwiki_form_check($args) {
    global $IMPORT_OTHERWIKI_ENCODE_OPTIONS;
    if(!isset($args['otherwiki_path']) || $args['otherwiki_path'] === '' ||
       !isset($args['otherwiki_encode']) ||
       !isset($IMPORT_OTHERWIKI_ENCODE_OPTIONS[$args['otherwiki_encode']])) {
	message_error(l('Invalid parameter.'));
	return false;
    }
    if(!is_file($args['otherwiki_path'] . '/pukiwiki.ini.php') ||
       !is_dir($args['otherwiki_path'] . '/wiki') ||
       !is_dir($args['otherwiki_path'] . '/backup')) {
	message_error(l('PukiWiki data file not found.'));
	return false;
    }
    return true;
}

function import_otherwiki_create_queue($args) {
    global $IMPORT_OTHERWIKI_ENCODE;
    $IMPORT_OTHERWIKI_ENCODE = $args['otherwiki_encode'];

    $queue = array();
    $pukiwiki_dir = $args['otherwiki_path'];
    $pukiwiki_backup_dir = $pukiwiki_dir . '/backup';
    $pukiwiki_page_dir = $pukiwiki_dir . '/wiki';
    $pukiwiki_attach_dir = $pukiwiki_dir . '/attach';

    $pukiwiki_backup_filenames = scandir($pukiwiki_backup_dir);
    foreach($pukiwiki_backup_filenames as $pukiwiki_filename) {
	if(preg_match('/^([0-9a-f]+)\.gz$/i', $pukiwiki_filename, $matches)) {
	    $pagename = import_otherwiki_convert_encoding(ascii_decode($matches[1]));
	    $pagename = page_path_to_pagename($pagename);
	    $queue[$pagename]['pagename'] = $pagename;
	    $queue[$pagename]['wiki_backup_file'] = $pukiwiki_filename;
	}
    }

    $pukiwiki_filenames = scandir($pukiwiki_page_dir);
    foreach($pukiwiki_filenames as $pukiwiki_filename) {
	if(preg_match('/^([0-9a-f]+).txt$/i', $pukiwiki_filename, $matches)) {
	    $pagename = import_otherwiki_convert_encoding(ascii_decode($matches[1]));
	    $pagename = page_path_to_pagename($pagename);
	    $queue[$pagename]['pagename'] = $pagename;
	    $queue[$pagename]['wiki_file'] = $pukiwiki_filename;
	}
    }

    $pukiwiki_attach_filenames = scandir($pukiwiki_attach_dir);
    foreach($pukiwiki_attach_filenames as $pukiwiki_filename) {
	if(preg_match('/^([0-9a-f]+)_([0-9a-f]+)(:?\.(\d+))?$/i', $pukiwiki_filename, $matches)) {
	    $pagename = import_otherwiki_convert_encoding(ascii_decode($matches[1]));
	    $filename = import_otherwiki_convert_encoding(ascii_decode($matches[2]));
	    $pagename = $pagename . '/' . $filename;
	    $pagename = page_path_to_pagename($pagename);
	    $queue[$pagename]['pagename'] = $pagename;
	    if(isset($matches[3]))
		$queue[$pagename]['attach_backup_files'][] = $pukiwiki_filename;
	    else
		$queue[$pagename]['attach_file'] = $pukiwiki_filename;
	}
    }
    $queue = array_values($queue);

    return $queue;
}

function import_otherwiki_prepare($args, $dom) {
    return true;
}

function import_otherwiki_initializer($args, $dom, &$func_arg) {
    return true;
}

function import_otherwiki_finalizer($args, $dom, &$func_arg) {
    return true;
}

function import_otherwiki_processor($args, $dom, &$func_arg, &$job) {
    global $IMPORT_OTHERWIKI_ENCODE;
    global $IMPORT_OTHERWIKI_CURRENT_PAGENAME;

    $IMPORT_OTHERWIKI_ENCODE = $args['otherwiki_encode'];
    
    $pagename = $job['pagename'];
    $IMPORT_OTHERWIKI_CURRENT_PAGENAME = $pagename;

    if(import_otherwiki_is_ignore($pagename))
	return;

    $page = page_read($pagename);
    page_truncate($page);
    $page = page_read($pagename);

    $btime = null;
    if(isset($job['wiki_backup_file'])) {
	$btime = pukiwiki_make_backups($page, $args['otherwiki_path'] . '/backup/' . $job['wiki_backup_file']);
    }
    
    if(isset($job['wiki_file'])) {
	$pukiwiki_file = $args['otherwiki_path'] . '/wiki/' . $job['wiki_file'];
	$mtime = filemtime($pukiwiki_file);
	$source = file_get_contents($pukiwiki_file);
	$source = import_otherwiki_convert_encoding($source);
	$page = page_read($pagename);
	pukiwiki_make_backup($page, $args['start_time'], $source);
	$contents = pukiwikibase_convert($page, $source);
	$page['force_mtime'] = $mtime;
	if(!is_null($btime))
	    $page['meta']['btime'] = $btime;
	page_write($page, $contents);
    }

    if(isset($job['attach_backup_files'])) {
	foreach($job['attach_backup_files'] as $attach_backup_file) {
	    $pukiwiki_file = $args['otherwiki_path'] . '/attach/' . $attach_backup_file;
	    $backup_time = filemtime($pukiwiki_file);
	    if(!is_null($btime) || $backup_time < $btime)
		$btime = $backup_time;
	    pukiwiki_make_attach($page, $pukiwiki_file, $backup_time);
	}
    }

    if(isset($job['attach_file'])) {
	$pukiwiki_file = $args['otherwiki_path'] . '/attach/' . $job['attach_file'];
	if(!is_null($btime))
	    $page['meta']['btime'] = $btime;
	pukiwiki_make_attach($page, $pukiwiki_file);
    }
    return true;
}

function import_otherwiki_is_ignore($pagename) {
    global $IMPORT_OTHERWIKI_IGNORE_PAGES;
    foreach($IMPORT_OTHERWIKI_IGNORE_PAGES as $pat) {
	if(preg_match($pat, $pagename))
	    return true;
    }
    return false;
}

function import_otherwiki_convert_encoding($source) {
    global $IMPORT_OTHERWIKI_ENCODE;
    return mb_convert_encoding($source, 'UTF-8', $IMPORT_OTHERWIKI_ENCODE);
}

function pukiwiki_make_backups(&$page, $filepath) {
    $min_backup_time = null;

    $fp = gzopen($filepath, 'rb');
    $buf = '';
    $backup_time = 0;
    while(($line = fgets($fp)) !== false) {
	$line = import_otherwiki_convert_encoding($line);
	if(preg_match('/^'. preg_quote(IMPORT_OTHERWIKI_PKWK_SPLITTER, '/') . '\s*(\d+)/', $line, $matches)) {
	    if(!empty($buf) && !empty($backup_time)) {
		pukiwiki_make_backup($page, $backup_time, $buf);
		$buf = '';
	    }
	    $backup_time = $matches[1];
	    $backup_time += IMPORT_OTHERWIKI_LOCALZONE;
	    if(empty($min_backup_time) || $backup_time < $min_backup_time)
		$min_backup_time = $backup_time;
	} else {
	    $buf .= $line;
	}
    }

    if(!empty($buf) && !empty($backup_time)) {
	pukiwiki_make_backup($page, $backup_time, $buf);
	$buf = '';
    }

    gzclose($fp);
    return $min_backup_time;
}

function pukiwiki_make_backup(&$page, $backup_time, &$contents) {
    return storage_page_write_backup($page, $backup_time, $contents, $error);
}

function pukiwiki_make_attach(&$page, $pukiwiki_file, $backup_time = 0) {
    global $pages;

    $mtime = filemtime($pukiwiki_file);
    $page['force_mtime'] = $mtime;
    $page['meta']['type'] = 'file';
    $page['meta']['original_filename'] = page_get_simplename($page['name']);

    $sizeinfo = @getimagesize($pukiwiki_file);
    if($sizeinfo !== false) {
	$page['meta']['Content-type'] = $sizeinfo['mime'];
    }

    $contents = array('upload_file' => $pukiwiki_file);
    $content_type_handler = &file_get_content_type_handler($page);
    if(!empty($content_type_handler['write']) && function_exists($content_type_handler['write'])) {
	$content_type_handler['write']($page, $contents);
    }
    if($backup_time == 0)
	page_write($page, $contents);
    else
	storage_page_write_backup($page, $backup_time, $contents, $error);
}

function otherwiki_trim_pagename($pagename) {
    return trim($pagename, '[]');
}

/*
  include(APP_DIR_PATH.'/otherwiki/pukiwiki.inc');
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
