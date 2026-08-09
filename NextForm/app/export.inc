<?php
$EXPORT_TEMPLATE_CACHES = array();

$EXPORT_SANITIZE_ATTRIBUTES = array(
    'data-pagename',
    'data-ref',
    'data-ref-page',
    'data-ref-pagename',
    'data-ref-ticket',
    'data-save',
    'data-ticket',
    'data-twl',
    'data-twll',
    'data-twlp',
    'data-twp',
    'data-value',
    'data-link-pagename',
    'data-link-section',
    'data-link-args',
    'data-options',
    'data-option-value',
    );

function export_pages($config_pagename, $export_directory, $is_force) {
    global $export_configs;
    global $export_data;
    
    $export_configs = export_read_export_configs($config_pagename);
    $export_configs['export_directory'] = $export_directory;
    $export_configs['is_force'] = $is_force;

    $export_data = array();
    foreach($export_configs['page'] as $pagename => &$export_config) {
	export_set_data($pagename, '');
    }

    $is_exported_all = false;
    while(!$is_exported_all) {
	$is_exported_all = true;
	foreach($export_data as $key => $export_datum) {
	    if(empty($export_datum['exported'])) {
		$pagename = $export_datum['pagename'];
		$argsstring = $export_datum['argsstring'];
		$html = export_convert($pagename, $argsstring);
		$is_exported_all = false;
	    }
	}
    }

    if(defined('EXPORT_POST_COMMAND') && EXPORT_POST_COMMAND != ''){
	$command_output = array();
	$status = 0;
	if(EXPORT_DIR_PATH != '')
	    $export_directory = EXPORT_DIR_PATH . '/' . $export_directory;
	$command = EXPORT_POST_COMMAND . ' ' .  escapeshellarg($export_directory);

	$descriptorspec = array(
	    0 => array("pipe", "r"),
	    1 => array("pipe", "w"),
	    2 => array("pipe", "w"),
	    );
	$process = proc_open($command, $descriptorspec, $pipes);
	if(is_resource($process)) {
	    foreach($export_data as $key => $export_datum) {
		foreach($export_datum['outputs'] as $output) {
		    if(isset($output['status']) && $output['status'] == 'success')
			fwrite($pipes[0], $output['filepath'] . "\0");
		}
	    }
	    fclose($pipes[0]);
	    fclose($pipes[1]);
	    fclose($pipes[2]);
	    $status = proc_close($process);
	    if($status != 0) {
		message_error(lf('command $command$ failed. status $status$',
				 array('command' => $command,
				       'status' => $status)));
	    }
	} else {
	    message_error(lf('Cannot execute command: $command$',
			     array('command' => $command)));
	}
    }

    return $export_data;
}

function export_read_export_configs($pagename) {
    $export_configs_page = array();
    $export_configs_pattern = array();
    $page = page_read($pagename);
    if(!empty($page['is_exists'])) {
	$dom = dom_create_document();
	page_handle($page, $dom);
	$xpath = new DOMXPath(dom_owner($dom));
	$trs = $xpath->query('//table/tbody/tr');
	if(is_null($trs) || $trs->length <= 0) {
	    ld(message_error(), 'Export configuration table not found in $pagename$.',
	       array('pagename' => dom_pagename($pagename)));
	}

	foreach($trs as $tr) {
	    $tds = $xpath->query('.//td', $tr);

	    $export_pagename = '';
	    $template = '';
	    $filepath = '';
	    $is_page = false;
	    if($td = $tds->item(0)) {
		$as = $xpath->query("./a[@data-link-pagename]", $td);
		if($a = $as->item(0)) {
		    $export_pagename = $a->getAttribute('data-link-pagename');
		    $is_page = true;
		} else {
		    $export_pagename = trim($td->textContent);
		}
	    }
	    if($export_pagename == '')
		continue;

	    if($td = $tds->item(1)) {
		$filepath = trim($td->textContent);
		if(strpos($filepath, '..') !== false || strpos($filepath, '\\') !== false) {
		    message_error(lf('Invalid file name $filename$.', array('filename' => $filepath)));
		    continue;
		}
	    }

	    if($td = $tds->item(2)) {
		$links = $xpath->query(".//a[@data-link-pagename]", $td);
		if($link = $links->item(0)) {
		    $template = $link->getAttribute('data-link-pagename');
		}
	    }
	    
	    $output = array();
	    if($template !== '')
		$output['template'] = $template;
	    if($filepath !== '')
		$output['filepath'] = $filepath;

	    if($is_page)
		$export_configs_page[$export_pagename]['outputs'][] = $output;
	    else
		$export_configs_pattern[$export_pagename]['outputs'][] = $output;
	}
    }
    return array(
	'page' => $export_configs_page,
	'pattern' =>  $export_configs_pattern
	);
}

function export_get_key($pagename, $argsstring = '') {
    $key = page_get_pageid($pagename);
    if($argsstring != '')
	$key .= '-' . $argsstring;
    return $key;
}

function export_set_data($pagename, $argsstring) {
    global $export_configs;
    global $export_data;

    $key = export_get_key($pagename, $argsstring);

    if(!empty($export_data[$key]))
	return;

    $page = page_read($pagename);
    $type = page_get_type($page);
    
    $export_data[$key]['pagename'] = $pagename;
    $export_data[$key]['type'] = $type;
    $export_data[$key]['argsstring'] = $argsstring;

    $outputs = array();

    if(isset($export_configs['page'][$pagename])) {
	$outputs = array_merge($outputs, $export_configs['page'][$pagename]['outputs']);
    } else {
	foreach($export_configs['pattern'] as $pattern => &$export_config) {
	    if(preg_match('/^type\s*=\s*(.*)\s*$/', $pattern, $matches)) {
		if($type == trim($matches[1])) {
		    $outputs = array_merge($outputs, $export_config['outputs']);
		    break;
		}
	    } else if(fnmatch($pattern, $pagename)) {
		$outputs = array_merge($outputs, $export_config['outputs']);
		break;
	    }
	}
    }

    if(empty($outputs))
	$outputs[] = array();
    $export_data[$key]['outputs'] = $outputs;
    $export_data[$key]['exported'] = false;

    foreach($export_data[$key]['outputs'] as &$output) {
	if(isset($output['filepath'])) {
	    $filepath = $output['filepath'];
	} else {
	    if($type == 'wiki')
		$filepath = '$shortid$.html';
	    else if($type == 'file')
		$filepath = '$shortid$$extension$';
	    else
		$filepath = '$shortid$';
	}

	$filepath = export_replace_variables($page, $argsstring, $filepath, true);

	if(strpos($filepath, '..') !== false) {
	    message_error(lf('Invalid file name $filename$.', array('filename' => $filepath)));
	    unset($output['filepath']);
	} else {
	    $output['filepath'] = $filepath;
	}

	if(!isset($export_data[$key]['path'])) {
	    $export_data[$key]['path'] = $filepath;
	}
    }
}

function export_convert($pagename, $argsstring) {
    global $export_configs;
    global $export_data;
    global $EXPORT_TEMPLATE_CACHES;
    $time_start = microtime(true);
    $key = export_get_key($pagename, $argsstring);

    $export_data[$key]['exported'] = true;
    if(!page_is_exists($pagename)) {
	$export_data[$key]['status'] = 'error';
	ld(message_error(), '$pagename$ is not exist.',
	   array('pagename' => dom_pagename($pagename)));
	return;
    }
    $type = $export_data[$key]['type'];

    $page = page_read($pagename);
    $page_mtime = $page['mtime'];

    foreach($export_data[$key]['outputs'] as &$output) {
	if(!isset($output['filepath']))
	    continue;

	$cache_key = 'export_' . (isset($output['template']) ? $output['template'] : '');

	$output_filepath = EXPORT_DIR_PATH . '/' . $export_configs['export_directory'] . '/' . $output['filepath'];

	if(!$export_configs['is_force'] && is_file($output_filepath)) {
	    $export_cache = cache_serialized_get($pagename, $cache_key);
	    if($export_cache !== false) {
		if(empty($export_cache['depend_pages']))
		    $export_cache['depend_pages'] = array();

		$output_mtime = filemtime($output_filepath);
		if($output_mtime > $page_mtime) {
		    $can_skip = true;
		    foreach($export_cache['depend_pages'] as $depend_pagename => $dummy) {
			$depend_page_mtime = page_get_mtime($depend_pagename);
			if($output_mtime <= $depend_page_mtime) {
			    $can_skip = false;
			    break;
			}
		    }
		    if($can_skip) {
			if(!empty($export_cache['link_pages'])) {
			    foreach($export_cache['link_pages'] as $link_page) {
				export_set_data($link_page['pagename'], $link_page['argsstring']);
			    }
			}
			$output['status'] = 'skip';
			continue;
		    }
		}
	    }
	}

	$template_pagename = '';
	$template_contents = false;
	
	if(isset($output['template'])) {
	    $template_pagename = $output['template'];
	    if(isset($EXPORT_TEMPLATE_CACHES[$template_pagename])) {
		$template_contents = $EXPORT_TEMPLATE_CACHES[$template_pagename];
	    } else {
		$template_page = page_read($output['template']);
		$template_type = page_get_type($template_page);

		if(!empty($template_page['contents_size']) &&
		   $template_page['contents_size'] > CONTENTS_READ_MAX_SIZE) {
		    ld(message_error(), 'Template $template_pagename$ too large for $pagename$.',
		       array('pagename' => dom_pagename($pagename),
			     'template_pagename' => dom_pagename($output['template'])));
		    continue;
		} else if($template_type == 'wiki') {
		    ld(message_error(), 'Invalid template page $template_pagename$ for $pagename$.',
		       array('pagename' => dom_pagename($pagename),
			     'template_pagename' => dom_pagename($output['template'])));
		    continue;
		} else {
		    $template_contents = page_get_contents($template_page);
		    $EXPORT_TEMPLATE_CACHES[$output['template']] = $template_contents;
		}
	    }
	} else {
	    if($type == 'wiki')
		$template_contents = EXPORT_DEFAULT_TEMPLATE_WIKI;
	}
	
	$tmp_filepath = $output_filepath . '.' . uniqid();
	$dirname = dirname($output_filepath);
	if(!file_exists($dirname)) {
	    if(is_link($dirname) || !mkdir($dirname, 0777, true)) {
		message_error(lf('mkdir failed. Check $dir$ permission.',
				 array('dir' => $dirname)));
		continue;
	    }
	}
	
	$is_success = false;

	$wfp = fopen($tmp_filepath, 'wb');
	if($wfp === false) {
	    message_error(lf('Cannot write file $filename$.',
			     array('filename' => $output_filepath)));
	    continue;
	}

	if($template_contents === false) {
	    if(!empty($page['is_exists'])) {
		$rfp = page_open_contents($page);
		while(!feof($rfp)) {
		    $buf = fread($rfp, CONTENTS_READ_BUFFER_SIZE);
		    fwrite($wfp, $buf);
		}
		fclose($rfp);
		$is_success = true;
	    }
	} else {
	    dom_set_self_link_pagename($page['name']);
	    $export_contents = export_convert_get_contents(
		$output, $page, $argsstring, $export_data[$key]['path']);

	    $template_contents = export_replace_template(
		$output, $page,
		$template_pagename, $template_contents,
		$export_data[$key]['path'], array());

	    $template_contents =
		template_replace_variables(
		    $template_contents,
		    array(
			'contents' => default_value($export_contents['contents'], ''),
			'title' => htmlspecialchars(
			    default_value($export_contents['title'], page_get_simplename($pagename))),
			));
	    $template_contents = export_replace_variables($page, $argsstring, $template_contents);

	    fwrite($wfp, $template_contents);
	    $is_success = true;
	}
	fclose($wfp);
	    
	if($is_success) {
	    rename($tmp_filepath, $output_filepath);
	    $output['status'] = 'success';

	    if(!isset($page['depend_pages_unknown'])) {
		if(!isset($output['depend_pages']))
		    $output['depend_pages'] = null;
		if(!isset($output['link_pages']))
		    $output['link_pages'] = null;
		$export_page_cache = array(
		    'depend_pages' => &$output['depend_pages'],
		    'link_pages' => &$output['link_pages']
		    );
		cache_serialized_create($page['name'], $cache_key, $export_page_cache);
	    }
	}
    }
    $time_end = microtime(true);
    $export_data[$key]['convert_time'] = $time_end - $time_start;
}

function export_replace_template(&$output, &$page, $template_pagename, $template, $path, $parent_pages) {
    global $export_configs;
    global $export_data;

    if($template_pagename != '')
	$output['depend_pages'][$template_pagename] = true;

    if(in_array($template_pagename, $parent_pages)) {
	$parent_pages[] = $template_pagename;
	ld(message_error(), '$pagename$ is circular reference. ($pages$)',
	   array('pagename' => dom_pagename($template_pagename),
		 'pages' => implode(' => ', $parent_pages)));
	return '';
    }
    $parent_pages[] = $template_pagename;

    $replace_data = array();
    $include_contents_data = array();
    if(preg_match_all('/\$\[\[(.+)\]\]\$/', $template, $matches, PREG_SET_ORDER) > 0) {
	foreach($matches as $match) {
	    $include_pagename = page_path_to_pagename($match[1], $template_pagename, false);
	    $output['depend_pages'][$include_pagename] = true;
	    if(!isset($include_contents_data[$include_pagename])) {
		$include_page = page_read($include_pagename);
		$include_contents = export_convert_get_contents(
		    $output, $include_page, '', $path);
		$include_contents = export_replace_template(
		    $output, $page,
		    $include_pagename, $include_contents['contents'],
		    $path, $parent_pages);
		$include_contents_data[$include_pagename] = $include_contents;
	    }
	    $replace_data[$match[0]] = &$include_contents_data[$include_pagename];
	}

    }

    if(preg_match_all('/\$(\w+)(\?([^\$]+))?\$/', $template, $matches, PREG_SET_ORDER) > 0) {
	foreach($matches as $match) {
	    $name = $match[1];
	    $opts = array();
	    if(isset($match[3])) {
		$fields = explode('&', $match[3]);
		foreach($fields as $field) {
		    $eqs = explode('=', $field);
		    $opts[$eqs[0]] = default_value($eqs[1], '');
		}
	    }
	    switch($name) {
	    case 'tags':
		$tags = tag_get_page_tags($page);
		$list_pagename = '';
		$sep = default_value($opts['sep'], ' ');
		$prefix = '';
		$suffix = '';
		if(!empty($tags)) {
		    $prefix = default_value($opts['prefix'], '');
		    $suffix = default_value($opts['suffix'], '');
		}
		if(isset($opts['list']) && preg_match('/^\[\[(.*)\]\]$/', $opts['list'], $pagematch)) {
		    $dom = dom_create_document();
		    $list_pagename = page_path_to_pagename($pagematch[1], $template_pagename, false);
		    $is_first = true;
		    foreach($tags as $tag) {
			$list_argsstring = 'tag=' . urlencode($tag);

			$relative_path = export_link_set_data(
			    $output, $list_pagename, $list_argsstring, $path);
			if($is_first)
			    $is_first = false;
			else
			    dom_append_text($dom, $sep);
			dom_append_link($dom, $relative_path, $tag);
		    }
		    $replace_data[$match[0]] = $prefix . dom_save_html($dom) . $suffix;
		} else {
		    $tags = array_map('htmlspecialchars', $tags);
		    $replace_data[$match[0]] = $prefix . implode($sep, $tags) . $suffix;
		}
		break;
	    case 'mtime':
	    case 'btime':
	    case 'exporttime':
		$time = null;
		$format = default_value($opts['format'], TIME_FORMAT);
		$prefix = default_value($opts['prefix'], '');
		$suffix = default_value($opts['suffix'], '');
		if($name == 'mtime') {
		    $time = $page['mtime'];
		} else if($name == 'exporttime') {
		    $time = time();
		} else if(!empty($page['meta'][$name])) {
		    $time = $page['meta'][$name];
		}
		if($time != null)
		    $replace_data[$match[0]] = $prefix . nf_date($format, $time) . $suffix;
		else
		    $replace_data[$match[0]] = '';
		break;
	    }
	}
    }

    foreach($replace_data as $pattern => &$contents) {
	$template = str_replace($pattern, $contents, $template);
    }
    return $template;
}

function export_convert_get_contents(&$output, &$page, $argsstring, $path) {
    global $export_configs;
    global $export_data;

    if(empty($page['is_exists'])) {
	ld(message_error(), '$pagename$ is not exist.',
	   array('pagename' => dom_pagename($page['name'])));
	return false;
    } else if(!empty($page['contents_size']) &&
	      $page['contents_size'] > CONTENTS_READ_MAX_SIZE) {
	ld(message_error(), '$pagename$ too large.',
	   array('pagename' => dom_pagename($page['name'])));
	return false;
    } else {
	$result = array();
	switch(page_get_type($page)) {
	case 'wiki':
	    $result = export_convert_get_contents_wiki($output, $page, $argsstring, $path);
	    break;
	default:
	    $contents = page_get_contents($page);
	    $result = array('title' => page_get_simplename($page['name']), 'contents' => $contents);
	    break;
	}
	return $result;
    }
}

function export_convert_get_contents_wiki(&$output, &$page, $argsstring, $path) {
    global $export_configs;
    global $export_data;

    $dom = dom_create_document();
    $page['is_main'] = true;
    $page['is_export'] = true;
    $page['args'] = url_parse_query($argsstring);
    $title = '';
    handle($page, $dom, 'show');
    if(isset($page['depend_pages'])) {
	if(isset($output['depend_pages']))
	    $output['depend_pages'] += $page['depend_pages'];
	else
	    $output['depend_pages'] = $page['depend_pages'];
    }
    if(isset($page['title_dom'])) {
	$export_title_dom = $page['title_dom'];
	title_remove_actual($export_title_dom);
	$title = $export_title_dom->textContent;
    }
    if($title == '')
	$title = page_get_simplename($page['name']);

    $xpath = new DOMXPath(dom_owner($dom));
    $links = $xpath->query('//*[@data-link-pagename]');
    if(!is_null($links)) {
	foreach($links as $link) {
	    $link_pagename = $link->getAttribute('data-link-pagename');
	    $link_args = $link->getAttribute('data-link-args');

	    $relative_path = export_link_set_data(
		$output, $link_pagename, $link_args, $path);

	    $link_name = $link->textContent;
	    if(strpos($link_name, './') === 0 || strpos($link_name, '../') === 0) {
		$link->removeChild($link->firstChild);
		dom_append_text($link, page_get_simplename($link_pagename));
	    }
		
	    if($relative_path != '') {
		if(($section_id = $link->getAttribute('data-link-section')) != '') {
		    $relative_path .= '#' . $section_id;
		}
		if($link->hasAttribute('href'))
		    $link->setAttribute('href', $relative_path);
		if($link->hasAttribute('src'))
		    $link->setAttribute('src', $relative_path);
	    }
	}
    }
    export_sanitize_dom($dom);
    $contents = dom_save_html($dom);
    return array(
	'title' => $title,
	'contents' => $contents
	);
}

function export_link_set_data(&$output, $link_pagename, $link_args, $path) {
    global $export_configs;
    global $export_data;

    $link_key = export_get_key($link_pagename, $link_args);

    $link_pages_found = false;
    if(isset($output['link_pages'])) {
	foreach($output['link_pages'] as $link_page) {
	    if($link_page['pagename'] == $link_pagename && $link_page['argsstring'] == $link_args)
		$link_pages_found = true;
	}
    }
    if($link_pages_found == false)
	$output['link_pages'][] = array('pagename' => $link_pagename, 'argsstring' => $link_args);

	    
    if($link_pagename != '') {
	if(empty($export_data[$link_key])) {
	    export_set_data($link_pagename, $link_args);
	}
    }

    if(!empty($export_data[$link_key]['path'])) {
	$link_path = $export_data[$link_key]['path'];
	$relative_path = relative_path($path, $link_path);
	return $relative_path;
    } else 
	return '';
}

function export_sanitize_path($path) {
    return str_replace(array('/', '%', '=', '&'), array('_', '', '', '-'), $path);
}

function export_sanitize_dom($dom) {
    global $EXPORT_SANITIZE_ATTRIBUTES;

    foreach($EXPORT_SANITIZE_ATTRIBUTES as $name) {
	$dom->removeAttribute($name);
    }
    
    foreach($dom->childNodes as $child) {
	if($child instanceof DOMElement) {
	    if($child->tagName == 'form') {
		$dom->removeChild($child);
		dom_remove_empty($dom);
		return;
	    }
	    export_sanitize_dom($child);
	}
    }
}

function export_replace_variables(&$page, $argsstring, $src, $is_filepath = false) {
    $pathinfo['basename'] = page_get_simplename($page['name']);
    if(preg_match('|(.*)\.([^\.]+)|', $pathinfo['basename'], $matches)) {
	$pathinfo['filename'] = $matches[1];
	$pathinfo['extension'] = $matches[2];
    } else {
	$pathinfo['filename'] = $pathinfo['basename'];
    }

    $id = md5($page['name']);
    $shortid = page_get_shortid($page['name']);
    $sanitized_argsstring = '';
    if($argsstring != '') {
	$sanitized_argsstring = '-' . export_sanitize_path($argsstring);
	$id .= $sanitized_argsstring;
    }
    
    $variables = array(
	'id' => $id,
	'shortid' => $shortid,
	'basename' => default_value($pathinfo['basename'], ''),
	'filename' => default_value($pathinfo['filename'], ''),
	'args' => $sanitized_argsstring,
	);
    if(isset($pathinfo['extension']))
	$variables['extension'] = '.' . $pathinfo['extension'];
    else
	$variables['extension'] = '';

    if($is_filepath)
	return template_replace_variables($src, $variables);

    $variables['editor'] = default_value($page['meta']['last_modify_user'], '');
    $variables['creator'] = default_value($page['meta']['create_user'], '');
    $variables['pagename'] = default_value($page['name'], '');

    $variables = array_map('htmlspecialchars', $variables);

    return template_replace_variables($src, $variables);
}
?>
