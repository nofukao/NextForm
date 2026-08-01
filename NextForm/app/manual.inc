<?php
$TOOLS['site']['manual'] = array(
    'order' => 50,
    'show' => 'manual_tool'
    );

function manual_tool(&$page, $dom) {
    $manual_page_name = MANUAL_PAGENAME . '/' . LANG;
    if(page_is_exists($manual_page_name)) {
	tools_add_link(
	    $dom,
	    array('name' => l('Manual'),
		  'target' => '_blank',
		  'seo_follow' => true,
		  'rel' => 'help',
		  'uri' => url_make($manual_page_name)));
    } else {
	tools_add_link(
	    $dom,
	    array('name' => l('Manual') . ' (' . l('official site') .')',
		  'target' => '_blank',
		  'seo_follow' => true,
		  'rel' => 'help',
		  'uri' => OFFICIAL_URL . '?' . url_path_encode($manual_page_name)));
    }
}

function manual_build() {
    auth_check(false, 'admin');

    cache_delete('', 'wiki-helper-info');
    $manual_pages = manual_parse();
    foreach($manual_pages as $pagename => $manual_page) {
	$page = page_read($pagename);
	$page['lock_now'] = true;
	$page['meta']['lock'] = true;
	$page['meta']['lock_user'] = 'admin';
	$page['force_mtime'] = $manual_page['mtime'];
	//debug
	//unset($page['meta']['lock']);
	//unset($page['meta']['lock_user']);
	$body = wiki_normalize($page, $manual_page['body']);
	page_write($page, $body, $page['meta']['ticket']);
    }
}

function manual_parse() {
    $manual_files = array();
    $manual_files[] = APP_DIR_PATH . '/manual/manual-before.txt';
    $manual_files[] = APP_DIR_PATH . '/setup.inc';
    $manual_files = array_merge($manual_files, glob_array(APP_DIR_PATH . '/handler/wiki/function/*.inc'));
    $manual_files = array_merge($manual_files, glob_array(PLUGIN_DIR_PATH . '/*.inc'));
    $manual_files[] = APP_DIR_PATH . '/manual/manual-after.txt';

    //pprint_r($manual_files);

    $manual_pages = array();

    $variables = array(
	'manualpage' => MANUAL_PAGENAME,
	'comment_begin' => '/*',
	'comment_end' => '*/',
	'bq' => '`');
    
    foreach($manual_files as $manual_file) {
	$contents = file_get_contents($manual_file);
	$mtime = filemtime($manual_file);
	//pprint_r($contents);
	if(preg_match_all('|/\*\*(.*?)\*/|s', $contents, $matches) > 0) {
	    foreach($matches[1] as $text) {
		$text = preg_replace_callback('/`(.*)`/', 'manual_replace', $text);
		$text = template_replace_variables($text, $variables);

		$body = '';
		$vars = array();
		$lines = explode("\n", $text);
		foreach($lines as $line) {
		    if(preg_match('|^\s*[@\\\\](\w+)\s+(.*)\s*$|', $line, $matches)) {
			$vars[$matches[1]] = $matches[2];
		    } else {
			$body .= $line . "\n";
		    }
		}
		$body = trim($body) . "\n";
		if(!empty($vars['manual'])) {
		    if(empty($manual_pages[$vars['manual']])) {
			$manual_pages[$vars['manual']] = array(
			    'vars' => $vars,
			    'body' => $body);
		    } else {
			$manual_pages[$vars['manual']]['body'] .= $body;
			$manual_pages[$vars['manual']]['vars'] =
			    array_merge($manual_pages[$vars['manual']]['vars'], $vars);
		    }

		    if(isset($manual_pages[$vars['manual']]['mtime'])) {
			if($mtime > $manual_pages[$vars['manual']]['mtime'])
			    $manual_pages[$vars['manual']]['mtime'] = $mtime;
		    } else {
			$manual_pages[$vars['manual']]['mtime'] = $mtime;
		    }
		}
	    }
	}
    }

    return $manual_pages;
}

function manual_replace($var) {
    global $WIKI_ALIASES;
    return eval('return ' . $var[1] . ';');
}

?>