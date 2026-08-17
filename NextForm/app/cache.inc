<?php
function cache_create($pagename, $cachekey, &$contents) {
    return storage_cache_create($pagename, $cachekey, $contents);
}

function cache_serialized_create($pagename, $cachekey, &$contents) {
    $data = serialize($contents);
    return storage_cache_create($pagename, $cachekey, $data);
}

function cache_create_open($pagename, $cachekey) {
    return storage_cache_create_open($pagename, $cachekey);
}

function cache_create_close($cfp, $pagename, $cachekey) {
    return storage_cache_create_close($cfp, $pagename, $cachekey);
}

function cache_open($pagename, $cachekey, $limit_time = 0) {
    return storage_cache_open($pagename, $cachekey, $limit_time);
}

function cache_get($pagename, $cachekey, $limit_time = 0) {
    $fp = cache_open($pagename, $cachekey, $limit_time);
    if(!$fp) {
	return false;
    }
    $contents = stream_get_contents($fp);
    fclose($fp);
    return $contents;
}

function cache_serialized_get($pagename, $cachekey, $limit_time = 0) {
    $data = cache_get($pagename, $cachekey, $limit_time);
    if($data === false)
	return false;
    return unserialize($data);
}

function cache_rename($pagename, $from_cachekey, $to_cachekey) {
    return storage_cache_rename($pagename, $from_cachekey, $to_cachekey);
}

function cache_delete($pagename, $cachekey = '', $is_parent = false) {
    return storage_cache_delete($pagename, $cachekey, $is_parent);
}

function cache_get_keys_prefix($pagename, $cachekeyprefix) {
    return storage_cache_get_keys_prefix($pagename, $cachekeyprefix);
}

function cache_get_as_filepath($pagename, $cachekey = '') {
    return storage_cache_get_as_filepath($pagename, $cachekey);
}

class CacheContentStream {
    var $pagename;
    var $key;
    var $fp;

    function stream_open($path, $mode, $options, &$opened_path) {
	if(substr($path, 0, 11) === 'twcache:///') {
	    $url = parse_url($path);
	    $this->pagename = '';
	    $this->key = urldecode(substr($path, 11));
	} else {
	    $url = parse_url($path);
	    $this->pagename = urldecode($url["host"]);
	    $this->key = urldecode(ltrim($url["path"], '/'));
	}
        $this->fp = cache_open($this->pagename, $this->key);
	var_dump($this->fp);
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

    function stream_seek($offset, $whence) {
	return fseek($this->fp, $offset, $whence);
    }
}
stream_wrapper_register('twcache', 'CacheContentStream');

?>
