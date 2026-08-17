<?php
define('STORAGE_FILE_DIR_PAGE_NAME', 'page');
define('STORAGE_FILE_DIR_CACHE_NAME', 'cache');
define('STORAGE_FILE_DIR_SETUP_NAME', 'setup');
define('STORAGE_FILE_PAGE_HEAD_NAME', 'head');
define('STORAGE_FILE_WRITE_STOP_FILE_PATH', STORAGE_FILE_DIR_PATH . '/stop');
define('STORAGE_FILE_NAME_CHUNK_SIZE', 254); // <filesytem max file name length> - 1
define('STORAGE_FILE_PATH_LENGTH_MAX', 3841); // <os max filepath length> - STORAGE_FILE_NAME_CHUNK_SIZE (4096 - 255)

define('STORAGE_FILE_DIR_PAGE_PATH', STORAGE_FILE_DIR_PATH . '/' . STORAGE_FILE_DIR_PAGE_NAME);
define('STORAGE_FILE_DIR_CACHE_PATH', STORAGE_FILE_DIR_PATH . '/' . STORAGE_FILE_DIR_CACHE_NAME);
define('STORAGE_FILE_DIR_SETUP_PATH', STORAGE_FILE_DIR_PATH . '/' . STORAGE_FILE_DIR_SETUP_NAME);

function storage_install() {
    $directories = array(
	STORAGE_FILE_DIR_PAGE_PATH,
	STORAGE_FILE_DIR_CACHE_PATH,
	STORAGE_FILE_DIR_SETUP_PATH);
    $result = true;
    foreach($directories as $directory) {
	if(!is_dir($directory) && !@mkdir($directory, 0777, true)) {
	    message_error(lf('mkdir failed. Check $dir$ permission.',
			     array('dir' => $directory)));
	    $result = false;
	}
    }
    
    if(!$result) {
	message_notice(lf('Check $dir$ directory permission.', array('dir' => STORAGE_FILE_DIR_PATH)) ."\n");
	return false;
    }

    return $result;
}

function storage_page_get_dirpath_by_pageid($pageid) {
    return STORAGE_FILE_DIR_PAGE_PATH . '/' .
	rtrim(chunk_split($pageid, STORAGE_FILE_NAME_CHUNK_SIZE, '_/'), '_/');
}

function storage_page_get_pageid_by_dirpath($dirpath) {
    $dirpath = substr($dirpath, strlen(STORAGE_FILE_DIR_PAGE_PATH . '/'));
    return str_replace(array('/', '_'), '', $dirpath);
}

function storage_page_get_dirpath($pagename) {
    return storage_page_get_dirpath_by_pageid(page_get_pageid($pagename));
}

function storage_page_get_filepath($pagename, $backuptime = 0) {
    $basedir = storage_page_get_dirpath($pagename) . '/';
    if(empty($backuptime) || $backuptime == 'now')
	return $basedir . STORAGE_FILE_PAGE_HEAD_NAME;
    else
	return $basedir . storage_page_encode_time($backuptime);
}

function storage_page_encode_time($time) {
    return sprintf('%011d', $time);
}

function storage_page_decode_time($str) {
    sscanf($str, '%011d', $time);
    return $time;
}

function storage_page_encode_meta($metadata) {
    $output = '';
    foreach($metadata as $name => $value) {
	$output .= ascii_encode($name) . '=' . ascii_encode($value) . "\n";
    }
    return $output;
}

function storage_page_decode_meta_line(&$metadata, $line) {
    if($line !== '') {
	$fields = explode('=', $line);
	if(isset($fields[0]) && isset($fields[1]))
	    $metadata[ascii_decode($fields[0])] = ascii_decode($fields[1]);
    }
    return $metadata;
}

function storage_page_is_exists($pagename) {
    return @file_exists(storage_page_get_filepath($pagename));
}

function storage_page_is_deleted($pagename) {
    return !storage_page_is_exists($pagename) && 
	@file_exists(storage_page_get_dirpath($pagename));
}

function storage_page_read(&$page) {
    $page['storage_filepath'] = storage_page_get_filepath($page['name']);
    return storage_page_read_meta($page);
}

function storage_page_read_backup(&$page, $backuptime) {
    $page['storage_filepath'] = storage_page_get_filepath($page['name'], $backuptime);
    return storage_page_read_meta($page);
}

function storage_page_read_meta(&$page) {
    if(!@is_file($page['storage_filepath']))
	return false;
    $fp = fopen($page['storage_filepath'], 'rb');
    if(!$fp)
	return false;

    $page['meta_size'] = 0;
    $metatext = '';
    while(($line = fgets($fp)) !== false) {
	$page['meta_size'] += strlen($line);
	$metatext .= $line;
	if($line === '' || $line == "\n")
	    break;
	storage_page_decode_meta_line($page['meta'], rtrim($line));
    }
    fclose($fp);
    $page['contents_size'] = filesize($page['storage_filepath']) - $page['meta_size'];
    $page['mtime'] = filemtime($page['storage_filepath']);
    $page['is_exists'] = true;
    return true;
}

function storage_page_open_contents(&$page) {
    if(!$page['is_exists'])
	return false;
    $fp = fopen($page['storage_filepath'], 'rb');
    if(!$fp)
	return false;
    fseek($fp, $page['meta_size'], SEEK_SET);
    return $fp;
}

function storage_page_file_write(&$page, $filepath, &$contents, &$error) {
    $tmp_filepath = storage_file_get_temporary_filepath($filepath);
    $wfp = fopen($tmp_filepath, 'wb');
    if(!$wfp) {
	message_error(l('File open error'));
	$error = PAGE_WRITE_ERROR_IO;
	return false;
    }
    if(fwrite($wfp, storage_page_encode_meta($page['meta']) . "\n") === false) {
	message_error(l('File write error'));
	$error = PAGE_WRITE_ERROR_IO;
	return false;
    }

    $rfp = null;
    if(is_array($contents) && isset($contents['upload_file']) && $contents['upload_file'] !== '') {
	$rfp = fopen($contents['upload_file'], 'rb');
    } else if(is_resource($contents)) {
	$rfp = $contents;
    } else {
	if(fwrite($wfp, $contents) === false) {
	    message_error(l('File write error'));
	    $error = PAGE_WRITE_ERROR_IO;
	    return false;
	}
    }
    if(!is_null($rfp)) {
	stream_copy_to_stream($rfp, $wfp);
	fclose($rfp);
    }
    fclose($wfp);

    if(!rename($tmp_filepath, $filepath)) {
	message_error(l('File write error'));
	$error = PAGE_WRITE_ERROR_IO;
	return false;
    }
    return true;
}

function storage_page_write(&$page, &$contents, &$error) {
    if(@is_file(STORAGE_FILE_WRITE_STOP_FILE_PATH)) {
	message_error(l(file_get_contents(STORAGE_FILE_WRITE_STOP_FILE_PATH)));
	$error = PAGE_WRITE_ERROR_IO;
	return false;
    }

    $dir = storage_page_get_dirpath($page['name']);
    if(strlen($dir) > STORAGE_FILE_PATH_LENGTH_MAX) {
	message_error(l('Page name is too long.'));
	$error = PAGE_WRITE_ERROR_IO;
	return false;
    }
    if(!is_dir($dir) && !mkdir($dir, 0777, true)) {
	message_error(lf('mkdir failed. Check $dir$ permission.',
			 array('dir' => STORAGE_FILE_DIR_PATH)));
	$error = PAGE_WRITE_ERROR_IO;
	return false;
    }
    
    $page['storage_filepath'] = storage_page_get_filepath($page['name']);
    if(!storage_page_backup($page, BACKUP_SKIP_TIME_SECONDS)) {
	message_error(l('Backup failed.'));
	$error = PAGE_WRITE_ERROR_BACKUP;
	return false;
    }

    $mtime = null;
    if(!empty($page['keep_mtime']) && file_exists($page['storage_filepath']))
	$mtime = filemtime($page['storage_filepath']);
    if(!empty($page['force_mtime']))
	$mtime = $page['force_mtime'];

    if(!storage_page_file_write($page, $page['storage_filepath'], $contents, $error))
	return false;

    if(!empty($mtime)) {
	touch($page['storage_filepath'], $mtime);
    }

    return true;
}

function storage_page_write_backup(&$page, $backup_time, $contents, &$error) {
    $dir = storage_page_get_dirpath($page['name']);
    $backuppath = storage_page_get_filepath($page['name'], $backup_time);
    if(!is_dir($dir) && !mkdir($dir, 0777, true)) {
	message_error(lf('mkdir failed. Check $dir$ permission.',
			 array('dir' => $dir)));
	$error = PAGE_WRITE_ERROR_IO;
	return false;
    }

    if(!storage_page_file_write($page, $backuppath, $contents, $error))
	return false;
    touch($backuppath, $backup_time);

    return true;
}

function storage_page_backup(&$page, $skip_time = 0) {
    if(!file_exists($page['storage_filepath']))
	return true;
    $now = time();

    if(isset($page['meta']['last_modify_user']))
	$modify_user = $page['meta']['last_modify_user'];
    else
	$modify_user = '';
    if(!empty($skip_time) && $modify_user === $page['last_modify_user']) {
	$backuptimes = storage_page_get_backup_times($page);
	if(!empty($backuptimes)) {
	    $lasttime = $backuptimes[count($backuptimes) - 1];
	    if($now - $lasttime < $skip_time) {
		return true;
	    }
	}
    }

    $backuppath = storage_page_get_filepath($page['name'], $now);
    if(!copy($page['storage_filepath'], $backuppath)) {
	message_error(l('File copy error'));
	return false;	
    } 
    touch($backuppath, $page['mtime']);
    return true;
}

function storage_page_get_backup_times(&$page) {
    $backuppath = storage_page_get_dirpath($page['name']);
    $backup_times = array();
    if(!is_dir($backuppath))
	return $backup_times;

    $backupfilenames = scandir($backuppath);
    foreach($backupfilenames as $name) {
	if($name[0] != '.' && $name != STORAGE_FILE_PAGE_HEAD_NAME) {
	    $backup_times[] = storage_page_decode_time($name);
	}
    }
    return $backup_times;
}

function storage_page_delete(&$page) {
    $page['storage_filepath'] = storage_page_get_filepath($page['name']);
    if(!storage_page_backup($page, 0)) {
	message_error(l('Backup failed.'));
	return false;
    }
    return unlink($page['storage_filepath']);
}

function storage_page_truncate(&$page) {
    $dir = storage_page_get_dirpath($page['name']);
    if(!is_dir($dir))
	return true;

    $backupfilenames = scandir($dir);
    foreach($backupfilenames as $backupfilename) {
	if($backupfilename[0] != '.') {
	    if(!unlink($dir . '/' . $backupfilename))
		return false;
	}
    }
    return storage_file_clean_directory(STORAGE_FILE_DIR_PAGE_PATH, $dir);
}

function storage_page_rename($oldpagename, $newpagename, $is_rename_subpage) {
    if($is_rename_subpage)
	$pages = storage_page_find($oldpagename, array('is_pagename_only' => true));
    else
	$pages = array();

    array_unshift($pages, array('name' => $oldpagename));
    $renameset = array();
    $is_error = false;
    foreach($pages as &$page) {
	$oldname = $page['name'];
	$newname = preg_replace('/^' . preg_quote($oldpagename, '/') . '/', $newpagename, $oldname);
	if(page_is_exists($newname)) {
	    ld(message_error(), '$pagename$ already exists.', array('pagename' => dom_pagename($newname)));
	    $is_error = true;
	} else if(!page_is_exists($oldname)) {
	    ld(message_error(), '$pagename$ is not exist.', array('pagename' => dom_pagename($oldname)));
	    $is_error = true;
	} else {
	    auth_check($oldname, 'write');
	    auth_check($newname, 'write');
	    $renameset[] = array($oldname, $newname);
	}
    }
    if($is_error) {
	message_error(l('Rename aborted.'));
	return false;
    }

    foreach($renameset as $rename) {
	list($oldname, $newname) = $rename;
	$is_error = true;
	$renamepage = page_read($oldname);
	$fp = page_open_contents($renamepage);
	$renamepage['name'] = $newname;
	$renamepage['is_exists'] = false;
	$renamepage['force_mtime'] = $renamepage['mtime'];
	if(page_write($renamepage, $fp)) {
	    $renamepage['is_exists'] = true;
	    $renamepage['name'] = $oldname;
	    if(page_delete($renamepage)) {
		$is_error = false;
		ld(message_notice(), 'Rname $pagename1$ to $pagename2$.',
		   array('pagename1' => dom_pagename($oldname),
			 'pagename2' => dom_pagename($newname)));
	    }
	}
	if($is_error) {
	    ld(message_error(), 'Rname $pagename1$ to $pagename2$ failed.',
	       array('pagename1' => dom_pagename($oldname),
		     'pagename2' => dom_pagename($newname)));
	}
    }
    return true;
}

function storage_page_get_mtime($pagename) {
    return filemtime(storage_page_get_filepath($pagename));
}

function storage_page_find($base_pagename, $condition) {
    global $HANDLERS;

    $cache_limit = 0;
    if(isset($condition['analyze']) || 
       (isset($condition['sort']) && $condition['sort'] == 'pageview')) {
	$cache_limit = time() - 60 * 5;
    }
    //if(DEBUG_MODE === 'true')
    //$cache_limit = time();

    foreach($condition as $key => &$cond)
	if(isset($condition[$key]) && $condition[$key] === '')
	    unset($condition[$key]);
    $condition['user_permission'] = auth_get_permission();
    ksort($condition);
    $cachekey = 'page_find_' . md5(serialize($condition), true);
    
    $cache = cache_serialized_get($base_pagename, $cachekey, $cache_limit);
    if($cache !== false) {
	//debug_log('cache hit [['.$base_pagename . ']]');
	//debug_log(print_r($condition, true));
	return $cache;
    }
    //debug_log('cache miss [['.$base_pagename.']]');
    //debug_log(print_r($condition, true));

    $time_start = microtime(true);

    if($base_pagename !== '') {
	$find_pagename_prefix = $base_pagename . '/';
	$find_path = storage_page_get_dirpath($find_pagename_prefix);
	$find_pagename_prefix_length = strlen($find_pagename_prefix);
    } else {
	$find_path = STORAGE_FILE_DIR_PAGE_PATH . '/';
	$find_pagename_prefix_length = 0;
    }

    $candidate_filepaths = storage_find_filepaths_by_prefix($find_path);

    $filepaths = array();
    if(empty($condition['is_deleted_only'])) {
	foreach($candidate_filepaths as $candidate_filepath) {
	    if(file_exists($candidate_filepath . '/' . STORAGE_FILE_PAGE_HEAD_NAME))
		$filepaths[] = $candidate_filepath;
	}
    } else {
	$condition['is_pagename_only'] = true;
	foreach($candidate_filepaths as $candidate_filepath) {
	    if(!file_exists($candidate_filepath . '/' . STORAGE_FILE_PAGE_HEAD_NAME))
		$filepaths[] = $candidate_filepath;
	}
    }

    // compatibility
    if(isset($condition['type']) && !isset($condition['types'])) {
	$condition['types'] = array(array($condition['type']));
    }

    if(!empty($condition['types'])) {
	$match_types = array();
	foreach($HANDLERS as $type => $dummy) {
	    if(logical_conditions_eval($condition['types'], array($type), true))
		$match_types[$type] = true;
	}
    } else {
	$match_types = $HANDLERS;
    }

    $is_pagename_only = !empty($condition['is_pagename_only']) &&
	!isset($condition['types']) && !isset($condition['tags']) &&
	(!isset($condition['sort']) || $condition['sort'] == 'mtime');

    $pages = array();
    if(!empty($filepaths)) {
	foreach($filepaths as &$filepath) {
	    if(preg_match('/^[0-9a-f]+$/', basename($filepath))) {
		$pagename = page_get_pagename(storage_page_get_pageid_by_dirpath($filepath));

		if(!empty($condition['is_children_only']) &&
		   strpos(substr($pagename, $find_pagename_prefix_length), '/') !== false)
		    continue;

		if(!empty($condition['is_ignore_system_page']) && page_is_system_page($pagename))
		    continue;
		
		if(isset($condition['name_regex']) &&
		   !preg_match($condition['name_regex'], $pagename))
		    continue;

		if(isset($condition['names']) && !logical_conditions_eval($condition['names'], array($pagename), true))
		    continue;

		if($is_pagename_only) {
		    $page = array('name' => $pagename);
		    if(empty($condition['is_deleted_only']))
			$page['mtime'] = filemtime($filepath . '/' . STORAGE_FILE_PAGE_HEAD_NAME);
		    else {
			$page['mtime'] = filemtime($filepath);
			$page['deleted'] = true;
		    }
		    $pages[] = $page;
		} else {
		    $is_match = false;
			
		    if(auth_has_permission($pagename, 'read')) {
			$page = page_read($pagename);
			$type = $page['meta']['type'];
			if(isset($match_types[$type]))
			    $is_match = true;

			if(isset($condition['tags']) && $is_match) {
			    $page_tags = tag_get_page_tags($page);
			    $is_match = logical_conditions_eval($condition['tags'], $page_tags, true);
			}
		    }

		    if($is_match)
			$pages[] = $page;
		}
	    }
	}
    }

    storage_page_find_sort($pages, $condition);

    $time_end = microtime(true);
    $time = $time_end - $time_start;

    if($time > PAGE_FIND_CREATE_CACHE_TIME) {
	cache_serialized_create($base_pagename, $cachekey, $pages);
    }

    return $pages;
}

function storage_page_find_deleted($base_pagename, $condition) {
    $condition['is_deleted_only'] = true;
    return storage_page_find($base_pagename, $condition);
}

function storage_page_find_sort(&$pages, $condition) {
    if(default_value($condition['sort'], '') == 'mtime') {
	usort($pages, 'storage_page_find_compare_mtime');
    } else if(default_value($condition['sort'], '') == 'btime') {
	usort($pages, 'storage_page_find_compare_btime');
    } else if(default_value($condition['sort'], '') == 'size') {
	usort($pages, 'storage_page_find_compare_size');
    } else if(default_value($condition['sort'], '') == 'pageview') {
	if(isset($condition['analyze']))
	    $analyze_condition = $condition['analyze'];
	else
	    $analyze_condition = array('start' => ANALYZE_PAGEVIEW_DATE_TOTAL, 'end' => ANALYZE_PAGEVIEW_DATE_TOTAL);
	
	foreach($pages as &$page) {
	    $page['pageview'] = analyze_pageview_get(
		$page['name'], $analyze_condition['start'], $analyze_condition['end']);
	}
	usort($pages, 'storage_page_find_compare_pageview');
    }
    if(!empty($condition['reverse'])) {
	$pages = array_reverse($pages);
    }
    if(!empty($condition['limit'])) {
	$pages = array_slice($pages, 0, $condition['limit']);
    }
}

function storage_page_find_compare_mtime($a, $b) {
    return ($b['mtime'] - $a['mtime']);
}
function storage_page_find_compare_btime($a, $b) {
    return default_value($b['meta']['btime'], 0) - default_value($a['meta']['btime'], 0);
}
function storage_page_find_compare_size($a, $b) {
    return ($b['contents_size'] - $a['contents_size']);
}
function storage_page_find_compare_pageview($a, $b) {
    return ($b['pageview'] - $a['pageview']);
}

function storage_cache_get_cachepath($pagename, $cachekey = '') {
    return STORAGE_FILE_DIR_CACHE_PATH . '/' .
	rtrim(chunk_split(page_get_pageid($pagename) . '-' . ascii_encode($cachekey),
			  STORAGE_FILE_NAME_CHUNK_SIZE, '_/'), '_/');
}

function storage_cache_get_key_by_cachepath($cachepath) {
    $cachepath = substr($cachepath, strlen(STORAGE_FILE_DIR_CACHE_PATH . '/'));
    $cachepath = str_replace(array('/', '_'), '', $cachepath);
    list($pageid, $cachekey) = explode('-', $cachepath);
    return array('pagename' => page_get_pagename($pageid), 'cachekey' => ascii_decode($cachekey));
}

function storage_cache_create($pagename, $cachekey, &$contents) {
    $wfp  = storage_cache_create_open($pagename, $cachekey);
    if(!$wfp)
	return false;
    if(is_resource($contents)) {
	$rfp = $contents;
	stream_copy_to_stream($rfp, $wfp);
	fclose($rfp);
    } else {
	if(fwrite($wfp, $contents) === false) {
	    fclose($wfp);
	    unlink($tmp_cachepath);
	    return false;
	}
    }
    storage_cache_create_close($wfp, $pagename, $cachekey);
    return true;
}

function storage_cache_create_open($pagename, $cachekey) {
    $cachepath = storage_cache_get_cachepath($pagename, $cachekey);
    $dirpath = dirname($cachepath);
    if(!is_dir($dirpath) && !mkdir($dirpath, 0777, true))
	return false;
    $tmp_cachepath = storage_file_get_temporary_filepath($cachepath);
    return fopen($tmp_cachepath, 'wb');
}

function storage_cache_create_close($cfp, $pagename, $cachekey) {
    $cachepath = storage_cache_get_cachepath($pagename, $cachekey);
    $meta = stream_get_meta_data($cfp);
    $tmp_cachepath = $meta['uri'];
    fclose($cfp);
    rename($tmp_cachepath, $cachepath);
}

/*
 * キャッシュを別のキーに付け替える。中身は読まないので、大きなファイルでも一瞬。
 * 付け替え先が既にあれば置き換わる (rename(2) の挙動)。
 */
function storage_cache_rename($pagename, $from_cachekey, $to_cachekey) {
    $from_cachepath = storage_cache_get_cachepath($pagename, $from_cachekey);
    if(!@is_file($from_cachepath))
	return false;
    $to_cachepath = storage_cache_get_cachepath($pagename, $to_cachekey);
    $dirpath = dirname($to_cachepath);
    if(!is_dir($dirpath) && !mkdir($dirpath, 0777, true))
	return false;
    if(!rename($from_cachepath, $to_cachepath))
	return false;
    storage_file_clean_directory(STORAGE_FILE_DIR_CACHE_PATH, dirname($from_cachepath));
    return true;
}

function storage_cache_open($pagename, $cachekey, $limit_time = 0) {
    $cachepath = storage_cache_get_cachepath($pagename, $cachekey);
    if(@is_file($cachepath)) {
	if(!empty($limit_time) && filemtime($cachepath) < $limit_time) {
	    return false;
	}
	return fopen($cachepath, 'rb');
    }
    return false;
}

function storage_cache_delete($pagename, $cachekey = '', $is_parent = false) {
    $names = explode('/', $pagename);
    while(true) {
	$pagename = implode('/', $names);
	$cachepath = storage_cache_get_cachepath($pagename, $cachekey);
	$cachepaths = storage_find_filepaths_by_prefix($cachepath);
	foreach($cachepaths as $cachepath) {
	    unlink($cachepath);
	    storage_file_clean_directory(STORAGE_FILE_DIR_CACHE_PATH, dirname($cachepath));
	}
	if(!$is_parent || $pagename === '')
	    break;
	array_pop($names);
    }
}

function storage_cache_get_keys_prefix($pagename, $cachekeyprefix) {
    $cachepath = storage_cache_get_cachepath($pagename, $cachekeyprefix);
    $cachepaths = storage_find_filepaths_by_prefix($cachepath);
    $cachekeys = array();
    foreach($cachepaths as $cachepath) {
	$names = storage_cache_get_key_by_cachepath($cachepath);
	$cachekeys[] = $names['cachekey'];
    }
    return $cachekeys;
}

function storage_cache_get_as_filepath($pagename, $cachekey = '') {
    $cachepath = storage_cache_get_cachepath($pagename, $cachekey);
    if(@is_file($cachepath))
	return $cachepath;
    else
	return false;
}

function storage_setup_get_filepath($name) {
    return STORAGE_FILE_DIR_SETUP_PATH . '/' . ascii_encode($name);
}

function storage_setup_write($name, &$contents) {
    $setuppath = storage_setup_get_filepath($name);
    $tmp_setuppath = storage_file_get_temporary_filepath($setuppath);
    if(file_put_contents($tmp_setuppath, $contents) === false)
	return false;
    return rename($tmp_setuppath, $setuppath);
}

function storage_setup_delete($name) {
    $setuppath = storage_setup_get_filepath($name);
    if(!@is_file($setuppath))
	return false;
    return unlink($setuppath);
}

function storage_setup_read($name) {
    $setuppath = storage_setup_get_filepath($name);
    if(!@is_file($setuppath))
	return false;
    return file_get_contents($setuppath);
}

function storage_find_filepaths_by_prefix($basepath) {
    $globpaths = @glob($basepath . '*');
    if(empty($globpaths))
	return array();
    $paths = array();
    foreach($globpaths as $globpath) {
	storage_find_filepaths_by_prefix_recursive($globpath, $paths);
    }
    return $paths;
}
function storage_find_filepaths_by_prefix_recursive($globpath, &$paths) {
    if(substr($globpath, -1) === '_') {
	$entries = scandir($globpath);
	foreach($entries as $entry) {
	    if($entry[0] == '.') continue;
	    $path = $globpath . '/' . $entry;
	    storage_find_filepaths_by_prefix_recursive($path, $paths);
	}
    } else {
	$paths[] = $globpath;
    }
}

function storage_file_clean_directory($parent_dir, $dirpath) {
    $remove_dirs = array_reverse(ancestor_paths($dirpath));
    foreach($remove_dirs as $remove_dir) {
	if($remove_dir === $parent_dir)
	    return true;
	$scandir_result = scandir($remove_dir);
	if($scandir_result === false || count($scandir_result) != 2)
	    return true;
	if(!rmdir($remove_dir))
	    return false;
    }
    return true;
}

function storage_file_get_temporary_filepath($filepath) {
    $unique = uniqid('', true);
    $last_num = -(STORAGE_FILE_NAME_CHUNK_SIZE - strlen($unique) - 1);
    if($last_num < 0)
	$name = substr(basename($filepath), $last_num);
    else
	$name = '';
    $unique_file_name =	$name . '.' . $unique;
    return dirname($filepath) . '/' . $unique_file_name;
}

?>
