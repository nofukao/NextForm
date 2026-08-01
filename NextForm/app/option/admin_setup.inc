<?php
$OPTIONS['admin_setup_site'] = array('show' => 'admin_setup_site');
$OPTIONS['admin_setup_theme'] = array('show' => 'admin_setup_theme');

function admin_setup_site(&$args, $dom) {
    auth_check(false, 'admin');
    title_set_l('Site preferences');

    $is_first = false;
    if(isset($args['first']) && $args['first'] === 'true') {
	theme_convert(THEME);
	page_stop_processing();
	message_info(l('You can reconfigure these items on Admin tools > Site preferences.'));
	$is_first = true;
    }

    $target_categories = array('site' => true);
    if(DEBUG_MODE === 'true')
	$target_categories['debug'] = true;
    if(admin_setup($args, $dom, $target_categories)) {
	if($is_first) {
	    header('Location: ' . url_make('', array('option' => 'admin_setup_theme', 'first' => 'true')));
	    exit(0);
	} else {
	    cache_delete('', 'calendar-');
	    header('Location: ' . url_make('', array('option' => 'admin_setup_site')));
	    exit(0);
	}
    }

    return false;
}

function admin_setup_theme(&$args, $dom) {
    auth_check(false, 'admin');
    title_set_l('Appearance theme preferences');

    $theme_setup_script_path = APP_DIR_PATH . '/theme/' . THEME . '/script/setup.js';
    if(file_exists($theme_setup_script_path)) {
	if(theme_debug_is_enable())
	    $theme_setup_script_url = url_make('', array('option' => 'theme', 'path' => 'script/setup.js'));
	else
	    $theme_setup_script_url = THEME_URI . '/' . THEME . '/script/setup.js';
	head_tags_add_javascript($theme_setup_script_url);
    }

    $is_first = isset($args['first']) && $args['first'] === 'true';

    if(isset($args['apply']) && $args['apply'] === 'theme') {
	if(!theme_convert(THEME)) {
	    message_error(l('Could not apply theme preferences.'));
	} else {
	    $new_args = array();
	    if(!$is_first)
		$new_args['option'] = 'admin_setup_theme';
	    header('Location: ' . url_make('', $new_args));
	    exit(0);
	}
    }

    if($is_first) {
	theme_convert(THEME);
	page_stop_processing();
	message_info(l('You can reconfigure these items on Admin tools > Appearance theme preferences.'));
	$is_first = true;
    }

    $target_categories = array('theme' => true);
    if(admin_setup($args, $dom, $target_categories, l('Need to refresh browser applying changes'))) {
	cache_delete('', 'wiki-helper-info');
	header('Location: ' . 
	       url_make('',
			array('option' => 'admin_setup_theme',
			      'apply' => 'theme',
			      'first' => default_value($args['first'], ''))));
    }

    return false;
}

function admin_setup(&$args, $dom, $target_categories, $note = '') {
    global $SETUP_CONSTANTS;
    auth_check(false, 'admin');

    head_tags_add_javascript('resource/script/setup.js');

    $values = array();

    $contents = setup_read('site');
    if($contents !== false) {
	$values = unserialize($contents);
    }

    if(isset($args['apply']) && $args['apply'] === 'true') {
	$is_success = true;
	foreach($SETUP_CONSTANTS as $key => &$setup_constant) {
	    $category = default_value($setup_constant['category'], 'site');
	    if(empty($target_categories[$category]))
		continue;
	    $name = 'const_' . $key;
	    $type = default_value($setup_constant['type'], 'string');

	    if($type === 'theme_file_image') {
		if((!isset($_FILES[$name]['name']) || $_FILES[$name]['name'] === '') &&
		   isset($args[$name . '_delete']) && $args[$name . '_delete'] === 'on') {
		    $values[$key] = '';
		    $delete_path = THEME_DIR_PATH . '/' . $setup_constant['path'];
		    if(isset($setup_constant['extensions'])) {
			foreach($setup_constant['extensions'] as $extension) {
			    if(is_file($delete_path . $extension))
				unlink($delete_path . $extension);
			}
		    } else if(is_file($delete_path)) {
			unlink($delete_path);
		    }
		    continue;
		}
		
		if(empty($_FILES[$name]) || !isset($_FILES[$name]['name']) || $_FILES[$name]['name'] === '')
		    continue;

		$path_info = pathinfo($_FILES[$name]['name']);
		$extension = strtolower($path_info['extension']);
		if(isset($setup_constant['extensions']) &&
		   !in_array($extension, $setup_constant['extensions'])) {
		    message_error(lf('You must enter a $term$', array('term' => l($setup_constant['description']))));
		    $is_success = false;
		    $setup_constant['error'] = true;
		    continue;
		}
		    
		$path = THEME_DIR_PATH . '/' . $setup_constant['path'] . $extension;
		if(!move_uploaded_file($_FILES[$name]['tmp_name'], $path)) {
		    message_error(l('Upload failed.'));
		    $is_success = false;
		    $setup_constant['error'] = true;
		    continue;
		}
		$values[$key] = THEME_URI . '/' . $setup_constant['path'] . $extension;
		if(isset($setup_constant['extensions'])) {
		    foreach($setup_constant['extensions'] as $extension) {
			$delete_path = THEME_DIR_PATH . '/' . $setup_constant['path'] . $extension;
			if($delete_path !== $path && is_file($delete_path)) {
			    unlink($delete_path);
			}
		    }
		}
		continue;
	    }

	    if(isset($args[$name])) {
		if($type == 'pagename')
		    $values[$key] = page_path_to_pagename($args[$name], '', false);
		//else if($type == 'color')
		//    $values[$key] = color_keyword_to_rgb($args[$name]);
		else
		    $values[$key] = $args[$name];
	    }

	    if(empty($setup_constant['allowempty']) && (!isset($values[$key]) || $values[$key] === '')) {
		message_error(lf('You must enter a $term$', array('term' => l($setup_constant['description']))));
		$is_success = false;
		$setup_constant['error'] = true;
		continue;
	    }

	    if(isset($setup_constant['emptyvalue']) && (!isset($values[$key]) || $values[$key] === '')) {
		$values[$key] = $setup_constant['emptyvalue'];
	    }

	    switch($type) {
	    case 'select':
		if(!isset($setup_constant['options'][$args[$name]]))
		    $setup_constant['error'] = true;
		break;
	    case 'timezone':
		$options = time_get_timezones();
		if(!isset($options[$args[$name]]))
		    $setup_constant['error'] = true;
		break;
	    case 'theme':
		$options = theme_get_themes();
		if(!isset($options[$args[$name]]))
		    $setup_constant['error'] = true;
		break;
	    case 'integer':
		if(!preg_match('/^[0-9]*$/', $args[$name]))
		    $setup_constant['error'] = true;
		$values[$key] = (int)$values[$key];
		if(isset($setup_constant['round'])) {
		    $values[$key] = max($setup_constant['round'][0], min($setup_constant['round'][1], $values[$key]));
		}
		break;
	    case 'color':
		if($args[$name] !== '' && !color_is_valid($args[$name]))
		    $setup_constant['error'] = true;
		break;
	    }
	    if(!empty($setup_constant['error'])) {
		message_error(lf('Invalid $term$.', array('term' => l($setup_constant['description']))));
		$is_success = false;
	    }
	}

	if($is_success) {
	    $contents = serialize($values);
	    if(!setup_write('site', $contents)) {
		message_error(l('Could not save preferences.'));
		return false;
	    } else {
		return true;
	    }
	}
    }

    $form = dom_append_form($dom,array(
				'option' => $args['option'],
				'apply' => 'true',
				'first' => default_value($args['first'], '')), 'setup', 'POST');
    dom_set_attribute($form, 'enctype', 'multipart/form-data');

    $dl = dom_append_child($form, 'dl');

    $groups = array();
    $parents = array();

    foreach($SETUP_CONSTANTS as $key => &$setup_constant) {
	$category = default_value($setup_constant['category'], 'site');
	if(empty($target_categories[$category]))
	    continue;

	if(isset($setup_constant['childof']) && isset($parents[$setup_constant['childof']])) {
	    $parent = &$parents[$setup_constant['childof']];
	    $item_dt = $dt = $parent['dt'];
	    $item_dd = $dd = $parent['dd'];
	} else if(isset($setup_constant['group'])) {
	    if(!isset($groups[$setup_constant['group']])) {
		$dt = dom_append_child($dl, 'dt');
		dom_append_text($dt, l($setup_constant['group']));
		$dd = dom_append_child($dl, 'dd');
		$group_dl = dom_append_child($dd, 'dl', 'group');
		$groups[$setup_constant['group']] = $group_dl;
		$item_dt = $dt;
		$item_dd = $dd;
	    } else {
		$group_dl = $groups[$setup_constant['group']];
	    }
	    $dt = dom_append_child($group_dl, 'dt');
	    dom_append_text($dt, l($setup_constant['description']));
	    $dd = dom_append_child($group_dl, 'dd');
	} else {
	    $dt = dom_append_child($dl, 'dt');
	    dom_append_text($dt, l($setup_constant['description']));
	    $dd = dom_append_child($dl, 'dd');
	    $item_dt = $dt;
	    $item_dd = $dd;
	}
	$parents[$key] = array('dt' => $dt, 'dd' => $dd);

	if(!empty($setup_constant['error']))
	    dom_append_class($dd, 'error');

	if(isset($setup_constant['need_name']) && isset($setup_constant['need_value'])) {
	    foreach(array($item_dt, $item_dd) as $e) {
		dom_set_attribute($e, 'data-setup-need-name', $setup_constant['need_name']);
		dom_set_attribute($e, 'data-setup-need-value', $setup_constant['need_value']);
		if(isset($setup_constant['need_option']))
		    dom_set_attribute($e, 'data-setup-need-option', $setup_constant['need_option']);
	    }
	}
	
	$type = default_value($setup_constant['type'], 'string');
	if(isset($values[$key]))
	    $value = $values[$key];
	else
	    $value = $setup_constant['default'];
	$name = 'const_' . $key;
	switch($type) {
	case 'select':
	    $options = array_map('l', $setup_constant['options']);
	    $append_element = dom_append_selector($dd, $name, $options, $value);
	    break;
	case 'timezone':
	    $options = time_get_timezones();
	    ksort($options);
	    $append_element = dom_append_selector($dd, $name, $options, $value);
	    break;
	case 'theme':
	    $options = theme_get_themes();
	    ksort($options);
	    $append_element = dom_append_selector($dd, $name, $options, $value);
	    break;
	case 'color':
	    $attributes = array('name' => $name,
				'type' => 'text',
				'value' => $value);
	    if(empty($setup_constant['allowempty']))
		$attributes['required'] = '';
	    
	    $append_element = dom_append_child($dd, 'input', 'color {hash:true,caps:false,adjust:false}', $attributes);
	    head_tags_add_javascript('resource/script/jscolor/jscolor.js');
	    break;
	case 'theme_file_image':
	    $label = dom_append_child($dd, 'label', 'file_upload');
	    $file = dom_append_child($label, 'input', '',
				     array('type' => 'file', 'name' => $name,
					   'accept' => 'image/*'));
	    if(!empty($setup_constant['allowempty']) && $value !== '') {
		$label = dom_append_child($dd, 'label', 'file_upload_delete');
		$check = dom_append_child($label, 'input', '',
					  array(
					      'type' => 'checkbox',
					      'name' => $name . '_delete',
					      'value' => 'on',
					      ));
		dom_append_text($label, l('Delete'));
	    }

	    if($value !== '') {
		$span = dom_append_child($dd, 'span', 'current_image');
		dom_append_text($span, l('Current image') . ' ');
		dom_append_child($span, 'img', 'current_image', array('src' => $value, 'alt' => l('Current image')));
	    }
	    $append_element = $label;
	    break;
	case 'integer':
	    $attributes = array('name' => $name,
				'type' => 'text',
				'value' => $value);
	    if(empty($setup_constant['allowempty']))
		$attributes['required'] = '';
	    $append_element = dom_append_child($dd, 'input', 'integer', $attributes);
	    break;
	case 'multilinestring':
	    $attributes = array('name' => $name);
	    if(empty($setup_constant['allowempty']))
		$attributes['required'] = '';
	    $append_element = dom_append_child($dd, 'textarea', '', $attributes);
	    dom_append_text($append_element, $value);
	    break;
	case 'string':
	case 'pagename':
	default:
	    $attributes = array('name' => $name,
				'type' => 'text',
				'value' => $value);
	    if(empty($setup_constant['allowempty']))
		$attributes['required'] = '';
	    $append_element = dom_append_child($dd, 'input', '', $attributes);
	    break;
	}
	if(isset($setup_constant['childof']))
	    dom_append_class($append_element, 'child');

	if(isset($setup_constant['note'])) {
	    $small = dom_append_child($dd, 'small');
	    dom_append_text($small, l($setup_constant['note']));
	}
    }

    $p = dom_append_child($form, 'p', 'apply');
    $submit = dom_append_child($p, 'input', '', array('type' => 'submit', 'value' => l('Apply')));

    if($note !== '') {
	$small = dom_append_child($p, 'small');
	dom_append_text($small, '(' . $note . ')');
    }

    return false;
}
?>
