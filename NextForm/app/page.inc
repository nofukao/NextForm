<?php
$PAGE_TYPES_TEXTBASE = array();
define('PAGE_WRITE_ERROR_NONE',        0);
define('PAGE_WRITE_ERROR_FAIL',        1);
define('PAGE_WRITE_ERROR_BACKUP',      2);
define('PAGE_WRITE_ERROR_IO',          3);
define('PAGE_WRITE_ERROR_CONFLICT',    4);

define('PAGE_SHORTID_KEY_BYTES', 6);
define('PAGE_SHORTID_KEY_REGEX', '[-_0-9a-zA-Z]{8}');

function page_get_pageid($pagename) {
    return ascii_encode($pagename);
}

function page_get_shortid($pagename) {
    $key = substr(md5($pagename, true), 0, PAGE_SHORTID_KEY_BYTES);
    return strtr(base64_encode($key), '/+', '-_');
}

function page_get_pagename($pageid) {
    return ascii_decode($pageid);
}

function page_get_simplename($pagename) {
    $names = explode('/', $pagename);
    return end($names);
}

function page_is_exists($pagename) {
    if(page_is_manual($pagename))
	return true;
    return storage_page_is_exists($pagename);
}

function page_is_deleted($pagename) {
    return storage_page_is_deleted($pagename);
}

function page_is_system_page($pagename) {
    return $pagename === MANUAL_PAGENAME ||
	strpos($pagename, MANUAL_PAGENAME . '/') === 0;
}

/*
 * 組み込みマニュアルのページか。
 *
 * マニュアルは storage に実体を持たない読み取り専用の仮想ページで、
 * 本文は app/ のソースから組み立てられる (manual.inc を参照)。
 *
 * 先に page_is_system_page() の前方一致で門番をする。ふつうのページは
 * この比較 1 回で抜けるので、マニュアルの一覧を引く費用はかからない。
 * page_is_exists() はリンク 1 本ごとに呼ばれるので、ここは効いてくる。
 *
 * 名前空間の中でも、マニュアルとして実在する名前だけが真になる。
 * 利用者が MANUAL_PAGENAME の下に作った普通のページは今までどおり扱う。
 */
function page_is_manual($pagename) {
    return page_is_system_page($pagename) && manual_page_is_exists($pagename);
}

/*
 * マニュアルを書き換えようとしたときの断り。
 * 書き込み・削除・改名・施錠のすべてがここを通る。
 */
function page_reject_manual($pagename) {
    if(!page_is_manual($pagename))
	return false;
    message_error(l('This page is a built-in manual and cannot be changed.'));
    return true;
}

function page_create($pagename) {
    $page = array();
    $page['name'] = $pagename;
    $page['id'] = page_get_pageid($pagename);
    $page['meta'] = array();
    $page['is_exists'] = false;
    return $page;
}

function page_setup(&$page) {
    if(empty($page['meta']['type']))
	$page['meta']['type'] = DEFAULT_PAGE_TYPE;
}

function &page_read($pagename) {
    auth_check($pagename, 'read');
    $page = page_create($pagename);
    if(page_is_manual($pagename))
	manual_page_read($page);
    else
	storage_page_read($page);
    page_setup($page);
    return $page;
}

function page_read_backup($pagename, $backuptime) {
    auth_check($pagename, 'read');
    $page = page_create($pagename);
    storage_page_read_backup($page, $backuptime);
    page_setup($page);
    $page['backuptime'] = $backuptime;
    return $page;
}

function page_open_contents(&$page) {
    if(page_is_manual($page['name']))
	return manual_page_open_contents($page);
    return storage_page_open_contents($page);
}

function page_get_contents(&$page) {
    $fp = page_open_contents($page);
    if($fp === false)
	return false;
    $contents = stream_get_contents($fp);
    fclose($fp);
    return $contents;
}

function page_output_contents(&$page) {
    $fp = page_open_contents($page);
    if($fp === false)
	return false;
    while(!feof($fp))
	echo(fread($fp, CONTENTS_READ_BUFFER_SIZE));
    fclose($fp);
}


function page_write(&$page, &$contents, &$ticket = '', &$error = PAGE_WRITE_ERROR_NONE) {
    if(page_reject_manual($page['name'])) {
	$error = PAGE_WRITE_ERROR_FAIL;
	return false;
    }
    if(!lock_has_permission($page) && !empty($page['meta']['lock']) && empty($page['lock_now'])) {
	message_error(l('This page was locked.'));
	return false;
    }

    if(!empty($page['insert']))
	auth_check($page['name'], 'insert');
    else
	auth_check($page['name'], 'write');
    $error = PAGE_WRITE_ERROR_NONE;

    if(!empty($page['is_exists']) && !empty($page['meta']['ticket'])) {
	if(empty($ticket) || $page['meta']['ticket'] != $ticket) {
	    message_error(l('Confliction detected.'));
	    $error = PAGE_WRITE_ERROR_CONFLICT;
	    return false;
	}
    }

    cache_delete($page['name']);
    cache_delete($page['name'], 'page_find_', true);

    $page['last_modify_user'] = default_value($page['meta']['last_modify_user'], '');
    if(($user = auth_get_user()) !== false) {
	$page['meta']['last_modify_user'] = $user['name'];
	if(empty($page['is_exists'])) {
	    $page['meta']['create_user'] = $user['name'];
	}
    } else {
	unset($page['meta']['last_modify_user']);
    }

    $page['meta']['ticket'] = md5($page['name'] . time());

    if(!isset($page['meta']['btime'])) {
	$page['meta']['btime'] = time();
    }

    if(SEARCH_ENGINE === 'builtin') {
	$old_page = $page; // avoid title overwrite
	$old_ngrams = search_page_ngram($old_page);
    }
    if(isset($page['write_tag'])) {
	if($page['write_tag'] == '')
	    unset($page['meta']['tag']);
	else
	    $page['meta']['tag'] = $page['write_tag'];
    }
    if(isset($page['title']))
	$page['meta']['title'] = $page['title'];
    if(isset($page['description']))
	$page['meta']['description'] = $page['description'];
    $result = storage_page_write($page, $contents, $error);
    if($result) {
	user_history_add_page_view($page['name']);
	$npage = page_read($page['name']);
	if(SEARCH_ENGINE === 'builtin') {
	    $new_ngrams = search_page_ngram($npage);
	    search_index_update($page['name'], $old_ngrams, $new_ngrams);
	} else {
	    search_engine_index_update($npage);
	}
    }
    return $result;
}

function page_delete(&$page) {
    if(page_reject_manual($page['name']))
	return false;
    if(!lock_has_permission($page) && !empty($page['meta']['lock'])) {
	message_error(l('This page was locked.'));
	return false;
    }
    auth_check($page['name'], 'write');
    cache_delete($page['name']);
    cache_delete($page['name'], 'page_find_', true);
    if(page_is_exists($page['name'])) {
	if(SEARCH_ENGINE === 'builtin')
	    $old_ngrams = search_page_ngram($page);
	$result = storage_page_delete($page);
	if($result) {
	    if(SEARCH_ENGINE === 'builtin')
		search_index_remove($page['name'], $old_ngrams);
	    else
		search_engine_index_delete($page);
	}
	return $result;
    }
    return false;
}

function page_truncate(&$page) {
    if(page_reject_manual($page['name']))
	return false;
    auth_check($page['name'], 'admin');
    page_delete($page);
    return storage_page_truncate($page);
}

function page_lock(&$page, $username = '') {
    if(page_reject_manual($page['name']))
	return false;
    auth_check($page['name'], 'lock');

    if(isset($page['meta']['lock_user']) && $page['meta']['lock_user'] != $username) {
	message_error(l('This page was locked.'));
	return false;
    }

    $page['meta']['lock'] = true;
    $page['lock_now'] = true;
    $page['keep_mtime'] = true;

    if($username !== '')
	$page['meta']['lock_user'] = $username;
    $fp = page_open_contents($page);
    return page_write($page, $fp, $page['meta']['ticket']);
}

function page_unlock(&$page, $username = false) {
    /*
     * マニュアルは施錠されているものとして見せているので、管理者には
     * 「解錠」が出る。押されたときに page_write() まで行かせず、
     * ここで理由の分かる断りを出す。
     */
    if(page_reject_manual($page['name']))
	return false;
    if(empty($page['meta']['lock']))
	return true;
    auth_check($page['name'], 'lock');

    if(isset($page['meta']['lock_user']) &&
       (!auth_has_permission($page['name'], 'admin') && $page['meta']['lock_user'] != $username)) {
	message_error(lf('Only $user$ can unlock this page.', array('user' => $page['meta']['lock_user'])));
	return false;
    }
    unset($page['meta']['lock']);
    unset($page['meta']['lock_user']);
    $fp = page_open_contents($page);
    $page['keep_mtime'] = true;
    
    return page_write($page, $fp, $page['meta']['ticket']);
}

function page_rename($oldpagename, $newpagename, $is_rename_subpage = true) {
    if(page_reject_manual($oldpagename))
	return false;
    auth_check($oldpagename, 'write');
    auth_check($newpagename, 'write');
    return storage_page_rename($oldpagename, page_path_to_pagename($newpagename), $is_rename_subpage);
}

function page_get_type(&$page) {
    if(isset($page['meta']['type']))
	return $page['meta']['type'];
    else
	return '';
}

function page_get_mtime($pagename) {
    if(page_is_manual($pagename))
	return manual_page_get_mtime($pagename);
    return storage_page_get_mtime($pagename);
}

function page_type_is_text_base($page_type) {
    global $PAGE_TYPES_TEXTBASE;
    return in_array($page_type, $PAGE_TYPES_TEXTBASE);
}

function page_find($base_pagename = '', $condition = array()) {
    return storage_page_find($base_pagename, $condition);
}

function page_find_deleted($base_pagename = '', $condition = array()) {
    return storage_page_find_deleted($base_pagename, $condition);
}

function page_get_backup_times(&$page) {
    /* マニュアルは実体を持たないので版も持たない */
    if(page_is_manual($page['name']))
	return array();
    return storage_page_get_backup_times($page);
}

function page_get_backup_pages(&$page) {
    $backup_times = page_get_backup_times($page);
    $backup_pages = array();
    foreach($backup_times as $backup_time) {
	$backup_pages[$backup_time] = page_read_backup($page['name'], $backup_time);
    }
    return $backup_pages;
}

function page($pagename, &$page = null, &$args = null, $is_main = false) {
    global $page_stop_processing;
    if($page_stop_processing)
	return '';
    if(empty($page))
	$page = page_read($pagename);
    $page['args'] = &$args;
    $page['is_main'] = $is_main;

    $dom = dom_create_document();
    page_handle($page, $dom);
    return dom_save_html($dom);
}

function page_args(&$page) {
    if(isset($page['is_main']) && $page['is_main'] === false) { // side page
	return args_get();
    } else {
	if(isset($page['args']))
	    return $page['args'];
	else
	    return array();
    }
}

function page_stop_processing() {
    global $page_stop_processing;
    $page_stop_processing = true;
}

function page_handle(&$page, $dom, $action = '') {
    if(isset($page['meta']['title'])) {
	$page['title_dom'] = dom_create_document();
	sentence_append($page['title_dom'], $page['meta']['title'] . ' ', $page);
	title_append_actual($page['title_dom'], dom_pagename($page['name']));
    }
    $page_dom = page_append_page_dom($page, $dom);
    handle($page, $page_dom, $action);
    if(!empty($page['meta']['ticket']))
	dom_set_attribute($page_dom, 'data-ticket', $page['meta']['ticket']);
    return dom_remove_empty($page_dom);
}

function page_path_to_pagename($path, $basename = '', $enable_default_page = true) {
    $path = preg_replace('/[\x09\x0a]/', '', $path);
    $path = trim($path);
    $path = trim($path, '/');
    if(substr($path, 0, 2) == './' || substr($path, 0, 3) === '../' ||
       $path == '.'  || $path == '..')
	$path = $basename . '/' . $path;
    $names = explode('/', $path);
    $result_names = array();
    foreach($names as $name) {
	if($name == '.' || $name == '') {
	} else if($name == '..') {
	    if(!empty($result_names))
		array_pop($result_names);
	} else {
	    $result_names[] = $name;
	}
    }
    if(empty($result_names) && $enable_default_page)
	return DEFAULT_PAGENAME;
    
    return implode('/', $result_names);
}

function page_relative_path($from, $to) {
    $from = trim($from, '/');
    $to = trim($to, '/');
    $froms = explode('/', $from);
    $tos = explode('/', $to);
    $fn = count($froms);
    $tn = count($tos);
    $path = array();
    
    for($i = 0; $i < $fn && $i < $tn && $froms[$i] === $tos[$i]; $i++);
    $same_index = $i;
    for($i = 0; $i < $fn - $same_index; $i++)
	$path[] = '..';
    if($same_index < $tn && empty($path))
	$path[] = '.';
    for($i = $same_index; $i < $tn; $i++)
	$path[] = $tos[$i];

    if(count($froms) <= 1 && count($path) >= 1 && $path[0] == '..') {
	array_shift($path);
    }

    if(empty($path))
	$path = '.';
    else
	$path = implode('/', $path);
    return $path;
}

function page_get_relations($from_pagename, $to_pagename) {
    $relations = array();
    if($from_pagename == $to_pagename)
	$relations[] = 'self';
    else if(strpos($to_pagename, $from_pagename . '/') === 0) {
	if(dirname($to_pagename) == $from_pagename)
	    $relations[] = 'child';
	$relations[] = 'offspring';
    } else if(strpos($from_pagename, $to_pagename . '/') === 0) {
	if(dirname($from_pagename) == $to_pagename)
	    $relations[] = 'parent';
	$relations[] = 'ancestor';
    }
    return $relations;
}

function page_get_section_id($pagename, $sectionname) {
    $id = substr(md5($pagename . '#' . $sectionname), 0, 8);
    $id = strtr($id[0], '0123456789', 'ghijklmnop') . substr($id, 1);
    return $id;
}

function page_append_page_dom(&$page, $dom) {
    $page_element = dom_append_child($dom, 'section', 'page section',
				     array('data-pagename' => $page['name']));
    return $page_element;
}

class PageContentStream {
    var $pagename;
    var $fp;
    /*
     * PHP がストリームを開くときにこのオブジェクトへ代入する。
     * 宣言しておかないと PHP 8.2 以降で「動的プロパティ」の非推奨が出る。
     * 画像を縮小して返すたびにログが 1 行増えていた。
     */
    var $context;

    function stream_open($path, $mode, $options, &$opened_path) {
        $url = parse_url($path);
        $this->pagename = urldecode($url["host"]);
	$page = page_read($this->pagename);
        $this->fp = page_open_contents($page);
        return $this->fp !== false;
    }

    function stream_read($count) {
	return fread($this->fp, $count);
    }

    function stream_stat() {
        return fstat($this->fp);
    }

    function stream_tell() {
        return ftell($this->fp);
    }

    function stream_eof() {
        return feof($this->fp);
    }
}
stream_wrapper_register('twpage', 'PageContentStream');


?>
