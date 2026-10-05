<?php
/*
 * 他の NextForm のページのクローン (?新しいページ名&option=clone)。
 *
 * 新規ページの種別で「他のNextFormページのクローン」を選ぶとここに来る
 * (option/newpage.inc、handle.inc)。クローン元のページの URL を入れると、
 * このサーバーが取りに行き、本文と添付を同じ種別で写す。
 *
 * 取り方は、どの版の NextForm (上流の ToraToraWiki 1.3.10 を含む) にもある
 * 画面だけを使う。
 *   本文   ?ページ&action=edit の入力欄。読む権限だけで出る。フォームの
 *          class の type_<種別> で種別が、file_edit で種別 file と分かる
 *   添付   ?ページ&option=attach&action=list (JSON)。この版から各ファイルの
 *          大きさ (size) も返すので、超えるものがあれば転送の前に断る。
 *          古い版には無いので、転送しながら数え、上限で打ち切る
 *   中身   ?ページ/ファイル&action=raw。大きさを指定しなければ元のバイト列
 *
 * 写すのは、本文 (題名とタグは本文にある) と、直下の添付 (中身、名前、
 * Content-type) だけ。履歴・ロック・権限・メタ情報の説明、添付以外の子ページ、
 * ほかのページの添付 (../画像.png など) は写さない。本文は書き換えないので、
 * 後者へのリンクはクローン先に同じ名前のページが無ければリンク切れになる。
 *
 * まず全部を取ってきて確かめ、問題が無いときだけ書く。取ってきたものは
 * 一時ファイルに置く。書く途中で失敗したら、作った分を消す (論理削除なので
 * 「削除されたページ」から戻せる)。
 *
 * 取りに行く先の制限は app/remote.inc。プライベートなネットワークのホストは、
 * サイト設定の CLONE_ALLOWED_HOSTS に書いたときだけ許す。
 */

$OPTIONS['clone'] = array(
    'show' => 'clone_show',
    'write' => 'clone_write',
    );

/* 一度に写せる添付の数 */
define('CLONE_MAX_ATTACHMENTS', 100);
/* 編集画面の HTML の上限 (本文が入力欄と hidden の 2 回入る) */
define('CLONE_MAX_PAGE_BYTES', 16 * 1024 * 1024);
/* 添付の一覧 (JSON) の上限 */
define('CLONE_MAX_LIST_BYTES', 1024 * 1024);

function clone_show(&$args, $dom) {
    $page = &main_page();
    clone_append_form($dom, $page, '', true);
    return false;
}

function clone_write(&$args, $dom) {
    $page = &main_page();
    auth_check($page['name'], 'write');

    $source_url = default_value($args['source_url'], '');
    $source_url = is_string($source_url) ? trim($source_url) : '';

    $errors = array();
    if(clone_page($page['name'], $source_url, $errors)) {
	header('Location: ' . url_make($page['name']));
	exit(0);
    }
    foreach($errors as $error)
	message_error($error);
    clone_append_form($dom, $page, $source_url, false);
    return false;
}

/*
 * クローンの画面。同じ名前のページがあるときは作れないので、フォームを出さない
 * ($notice_exists は、書けなかった理由として既に出しているときに false)。
 */
function clone_append_form($dom, &$page, $source_url, $notice_exists) {
    title_set_pagename('Create $pagename$ by cloning another NextForm page', $page['name']);
    if(!empty($page['is_exists'])) {
	if($notice_exists)
	    message_error(l('Page already exists.'));
	return;
    }
    $has_permission = auth_warn($page['name'], 'write');

    $p = dom_append_child($dom, 'p');
    dom_append_text($p, l('Enter the URL of the NextForm page to clone. Its text and attached files are copied with the same type.'));

    $form = dom_append_page_form($page, $dom, array('option' => 'clone', 'action' => 'write'), 'clone');
    $label = dom_append_child($form, 'label');
    dom_append_text($label, l('URL of the page to clone'));
    dom_append_child($label, 'input', '',
		     array('type' => 'url', 'name' => 'source_url', 'value' => $source_url,
			   'required' => 'required', 'size' => '60'));
    $submit = dom_append_child($form, 'input', '',
			       array('type' => 'submit', 'value' => l('Clone')));
    if(!$has_permission)
	dom_set_attribute($submit, 'disabled', 'disabled');

    $ul = dom_append_child($dom, 'ul');
    foreach(array(
		'Only the attached files directly under the page are copied. Attached files of other pages (such as ../image.png) are not, so links to them may be broken.',
		'History, lock, permissions and the descriptions of the page and its files are not copied.',
		'Pages that require login cannot be cloned.',
		'Each attached file must be within the size limit of this site ($limit$B).')
	    as $note) {
	$li = dom_append_child($ul, 'li');
	dom_append_text($li, lf($note, array('limit' => encode_prefixed_number(max_upload_file_size()))));
    }
}

/*
 * クローンする。書けたら true。書けなかったら理由を $errors に入れて false
 * (何も書かない)。
 */
function clone_page($pagename, $source_url, &$errors) {
    if(page_is_exists($pagename)) {
	$errors[] = l('Page already exists.');
	return false;
    }
    $source = clone_parse_source_url($source_url);
    if($source === false) {
	$errors[] = l('Enter the URL of a NextForm page.');
	return false;
    }

    $temp_files = array();
    try {
	$remote = clone_fetch_source($source, $errors);
	if($remote === false)
	    return false;

	$attachments = array();
	$main_file = null;
	if($remote['type'] === 'file') {
	    $main_file = clone_fetch_file($source, $source['pagename'], null, $temp_files, $errors);
	    if($main_file === false)
		return false;
	} else {
	    $attachments = clone_fetch_attachments($source, $pagename, $temp_files, $errors);
	    if($attachments === false)
		return false;
	}

	return clone_write_pages($pagename, $remote, $main_file, $attachments, $errors);
    } finally {
	foreach($temp_files as $temp_file)
	    @unlink($temp_file);
    }
}

/*
 * 入れられた URL を、サイトの入口とページ名に分ける。
 *   https://example.com/wiki/?Foo/Bar
 *   https://example.com/wiki/index.php?Foo%2FBar&action=edit
 *   https://example.com/wiki/?page=Foo/Bar
 * NextForm のページの URL に見えなければ false。
 */
function clone_parse_source_url($url) {
    $parts = @parse_url($url);
    if($parts === false || empty($parts['host']) || isset($parts['user']) || isset($parts['pass']))
	return false;
    $scheme = strtolower(default_value($parts['scheme'], ''));
    if($scheme !== 'http' && $scheme !== 'https')
	return false;

    $query = default_value($parts['query'], '');
    $fields = explode('&', $query);
    $pagename = '';
    if($fields[0] !== '' && strpos($fields[0], '=') === false) {
	$pagename = urldecode($fields[0]);
    } else {
	parse_str($query, $queries);
	if(isset($queries['page']) && is_string($queries['page']))
	    $pagename = $queries['page'];
    }
    $pagename = normalize_string($pagename);
    if($pagename === '')
	return false;

    $base = $scheme . '://' . $parts['host'] . (isset($parts['port']) ? ':' . $parts['port'] : '') .
	default_value($parts['path'], '/');
    return array('base' => $base, 'pagename' => $pagename);
}

/* クローン元のサイトの URL。ページ名は / ごとに符号化する */
function clone_remote_url(&$source, $pagename, $queries) {
    $path = implode('/', array_map('rawurlencode', explode('/', $pagename)));
    return $source['base'] . '?' . $path . '&' . http_build_query($queries);
}

/* サイト設定の「クローン元として許すホスト」(1 行に 1 つ) */
function clone_allowed_hosts() {
    $hosts = array();
    foreach(preg_split('/[\s,]+/', strtolower(CLONE_ALLOWED_HOSTS)) as $host) {
	if($host !== '')
	    $hosts[] = $host;
    }
    return $hosts;
}

function clone_fetch(&$source, $pagename, $queries, $max_bytes, $to_file = null) {
    return remote_fetch(clone_remote_url($source, $pagename, $queries), $max_bytes, $to_file,
			clone_allowed_hosts());
}

/*
 * 取れなかった理由を $errors に入れる。取れて 200 なら true。
 * $too_large_message は大きすぎたときに出す言葉 (取るものによって違う)。
 */
function clone_check_response(&$response, $too_large_message, &$errors) {
    if(isset($response['error'])) {
	switch($response['error']) {
	case 'disabled':
	    $errors[] = l('Fetching from other servers is turned off on this server (allow_url_fopen).');
	    break;
	case 'url':
	    $errors[] = l('Enter the URL of a NextForm page.');
	    break;
	case 'private':
	    $errors[] = lf('$host$ is a host on a private network. To clone from it, write it in "Hosts allowed as clone sources" in the site settings.',
			   array('host' => $response['host']));
	    break;
	case 'too_large':
	    $errors[] = $too_large_message;
	    break;
	default:
	    $errors[] = lf('Could not connect to $host$.', array('host' => $response['host']));
	    break;
	}
	return false;
    }
    if($response['status'] == 401 || $response['status'] == 403) {
	$errors[] = l('The page to clone requires login. Pages that require login cannot be cloned.');
	return false;
    }
    if($response['status'] != 200) {
	$errors[] = l('The URL is not a page of NextForm.');
	return false;
    }
    return true;
}

/* 本文と種別を取る。array('type' => 種別, 'contents' => 本文) か false */
function clone_fetch_source(&$source, &$errors) {
    global $HANDLERS, $PAGE_TYPES_TEXTBASE;

    $response = clone_fetch($source, $source['pagename'], array('action' => 'edit'), CLONE_MAX_PAGE_BYTES);
    if(!clone_check_response($response, l('The page to clone is too large.'), $errors))
	return false;

    $remote = clone_parse_edit_page($response['body']);
    if(isset($remote['error'])) {
	if($remote['error'] === 'not_found')
	    $errors[] = l('The page to clone was not found.');
	else
	    $errors[] = l('The URL is not a page of NextForm.');
	return false;
    }
    if($remote['type'] !== 'file' &&
       (empty($HANDLERS[$remote['type']]) || !in_array($remote['type'], $PAGE_TYPES_TEXTBASE, true))) {
	$errors[] = lf('Pages of type $type$ cannot be cloned.', array('type' => $remote['type']));
	return false;
    }
    if($remote['type'] !== 'file') {
	$remote['contents'] = normalize_string($remote['contents']);
	if($remote['contents'] === '') {
	    $errors[] = l('The page to clone was not found.');
	    return false;
	}
    }
    return $remote;
}

/*
 * 編集画面の HTML から種別と本文を取り出す。
 * text_edit は入力欄の本文の前に改行を 1 つ足して出す (ブラウザは読み飛ばすが、
 * DOMDocument は残す) ので、それを落とす。まだ無いページの編集画面には
 * newpage_type の hidden があるので、それでページが無いことが分かる。
 */
function clone_parse_edit_page($html) {
    $doc = new DOMDocument();
    if(!@$doc->loadHTML('<?xml encoding="UTF-8">' . $html, LIBXML_NONET))
	return array('error' => 'not_nextform');
    $xpath = new DOMXPath($doc);
    foreach($xpath->query('//form') as $form) {
	$classes = preg_split('/\s+/', $form->getAttribute('class'), -1, PREG_SPLIT_NO_EMPTY);
	$is_text = in_array('text_edit', $classes, true);
	if(!$is_text && !in_array('file_edit', $classes, true))
	    continue;
	if($xpath->query('.//input[@name="newpage_type"]', $form)->length > 0)
	    return array('error' => 'not_found');
	if(!$is_text)
	    return array('type' => 'file');

	$type = '';
	foreach($classes as $class) {
	    if(strpos($class, 'type_') === 0)
		$type = substr($class, strlen('type_'));
	}
	$textareas = $xpath->query('.//textarea[@name="contents"]', $form);
	if($type === '' || $textareas->length != 1)
	    return array('error' => 'not_nextform');
	$contents = $textareas->item(0)->textContent;
	if(substr($contents, 0, 1) === "\n")
	    $contents = substr($contents, 1);
	return array('type' => $type, 'contents' => $contents);
    }
    return array('error' => 'not_nextform');
}

/*
 * 直下の添付を取ってくる。array(ファイル名 => array('path', 'content_type')) か false。
 * 一覧に大きさがあれば、上限を超えるものを転送の前に全部挙げて断る。
 */
function clone_fetch_attachments(&$source, $pagename, &$temp_files, &$errors) {
    $response = clone_fetch($source, $source['pagename'], array('option' => 'attach', 'action' => 'list'),
			    CLONE_MAX_LIST_BYTES);
    if(!clone_check_response($response, l('Could not get the list of attached files.'), $errors))
	return false;
    $list = json_decode($response['body'], true);
    if(!is_array($list) || default_value($list['status'], '') !== 'ok' || !isset($list['pages']) ||
       !is_array($list['pages'])) {
	$errors[] = l('Could not get the list of attached files.');
	return false;
    }

    $prefix = $source['pagename'] . '/';
    $entries = array();
    foreach($list['pages'] as $entry) {
	$name = is_array($entry) ? default_value($entry['name'], '') : '';
	if(!is_string($name) || strpos($name, $prefix) !== 0)
	    continue;
	$basename = normalize_string(substr($name, strlen($prefix)));
	if($basename === '' || $basename === '.' || $basename === '..' || strpos($basename, '/') !== false)
	    continue;
	$entries[$basename] = (isset($entry['size']) && is_int($entry['size'])) ? $entry['size'] : null;
    }

    if(count($entries) > CLONE_MAX_ATTACHMENTS) {
	$errors[] = lf('There are too many attached files ($num$). Up to $max$ files can be cloned.',
		       array('num' => count($entries), 'max' => CLONE_MAX_ATTACHMENTS));
	return false;
    }

    $limit = max_upload_file_size();
    foreach($entries as $basename => $size) {
	$new_pagename = $pagename . '/' . $basename;
	if(page_is_exists($new_pagename))
	    $errors[] = lf('$pagename$ already exists.', array('pagename' => $new_pagename));
	else if(!auth_has_permission($new_pagename, 'write'))
	    $errors[] = lf('You have no permission to write $pagename$.', array('pagename' => $new_pagename));
	else if(!is_null($size) && $size > $limit)
	    $errors[] = clone_too_large_message($basename, $size, $limit);
    }
    if(!empty($errors))
	return false;

    $attachments = array();
    foreach($entries as $basename => $size) {
	$file = clone_fetch_file($source, $source['pagename'] . '/' . $basename, $basename, $temp_files, $errors);
	if($file !== false)
	    $attachments[$basename] = $file;
    }
    if(!empty($errors))
	return false;
    return $attachments;
}

/*
 * ファイルの中身を一時ファイルに取る。array('path', 'content_type') か false。
 * $basename は添付のときのファイル名 (理由に出す)。ページそのものが種別 file のときは null。
 */
function clone_fetch_file(&$source, $remote_pagename, $basename, &$temp_files, &$errors) {
    $limit = max_upload_file_size();
    $label = is_null($basename) ? page_get_simplename($remote_pagename) : $basename;

    /*
     * 一時ファイルは storage/cache/ に置く。共用サーバーでは open_basedir で
     * システムの一時ディレクトリに書けないことがある。storage/ は .htaccess で
     * 外から読めない。
     */
    $path = @tempnam(STORAGE_FILE_DIR_PATH . '/' . STORAGE_FILE_DIR_CACHE_NAME, 'clone-');
    if($path === false) {
	$errors[] = l('Could not make a temporary file.');
	return false;
    }
    $temp_files[] = $path;

    $response = clone_fetch($source, $remote_pagename, array('action' => 'raw'), $limit, $path);
    if(isset($response['error']) && $response['error'] === 'too_large') {
	$errors[] = clone_too_large_message($label, null, $limit);
	return false;
    }
    if(!isset($response['error']) && $response['status'] != 200 &&
       $response['status'] != 401 && $response['status'] != 403) {
	$errors[] = lf('Could not get the attached file $filename$.', array('filename' => $label));
	return false;
    }
    if(!clone_check_response($response, '', $errors))
	return false;

    $content_type = strtolower(trim(explode(';', default_value($response['headers']['content-type'], ''))[0]));
    if($content_type === '')
	$content_type = 'application/octet-stream';
    return array('path' => $path, 'content_type' => $content_type);
}

function clone_too_large_message($filename, $size, $limit) {
    if(is_null($size))
	return lf('The attached file $filename$ exceeds the size limit of this site ($limit$B).',
		  array('filename' => $filename, 'limit' => encode_prefixed_number($limit)));
    return lf('The attached file $filename$ ($size$B) exceeds the size limit of this site ($limit$B).',
	      array('filename' => $filename, 'size' => encode_prefixed_number($size),
		    'limit' => encode_prefixed_number($limit)));
}

/*
 * 書く。本文 (または種別 file の中身) のページ、添付の順。途中で失敗したら
 * 作った分を消して false。
 */
function clone_write_pages($pagename, &$remote, $main_file, &$attachments, &$errors) {
    $created = array();
    if($remote['type'] === 'file')
	$ok = clone_write_file($pagename, $main_file);
    else
	$ok = clone_write_text($pagename, $remote['type'], $remote['contents']);
    if($ok)
	$created[] = $pagename;

    foreach($attachments as $basename => $file) {
	if(!$ok)
	    break;
	$ok = clone_write_file($pagename . '/' . $basename, $file);
	if($ok)
	    $created[] = $pagename . '/' . $basename;
    }
    if($ok)
	return true;

    foreach(array_reverse($created) as $created_pagename) {
	$page = page_read($created_pagename);
	page_delete($page);
    }
    $errors[] = l('Saving failed. The pages made so far were deleted.');
    return false;
}

/* 画面から保存したときと同じく、種別の normalize (題名やタグを本文から取る) を通す */
function clone_write_text($pagename, $type, $contents) {
    $page = page_read($pagename);
    $page['meta']['type'] = $type;
    $normalize = handler_function($page, 'normalize');
    if($normalize !== false)
	$contents = $normalize($page, $contents);
    $ticket = '';
    return page_write($page, $contents, $ticket);
}

/* 添付画面から添付したときと同じ (option/attach.inc の attach_write()) */
function clone_write_file($pagename, $file) {
    $page = page_read($pagename);
    $page['meta']['type'] = 'file';
    $page['meta']['Content-type'] = $file['content_type'];
    $page['meta']['original_filename'] = page_get_simplename($pagename);
    $contents = array('upload_file' => $file['path']);

    $content_type_handler = &file_get_content_type_handler($page);
    if(!empty($content_type_handler['write']) && function_exists($content_type_handler['write']))
	$content_type_handler['write']($page, $contents);

    $ticket = '';
    return page_write($page, $contents, $ticket);
}
?>
