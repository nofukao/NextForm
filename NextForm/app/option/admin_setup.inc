<?php
/* 画像の設定でできること (アップロード以外) */
define('ADMIN_SETUP_IMAGE_DEFAULT', 'default');
define('ADMIN_SETUP_IMAGE_NONE', 'none');

$OPTIONS['admin_setup_site'] = array('show' => 'admin_setup_site');
$OPTIONS['admin_setup_theme'] = array('show' => 'admin_setup_theme');
$OPTIONS['admin_setup_tone'] = array('show' => 'admin_setup_tone');

function admin_setup_site(&$args, $dom) {
    auth_check(false, 'admin');
    title_set_l('Site preferences');

    $is_first = false;
    if(isset($args['first']) && $args['first'] === 'true') {
	theme_convert(THEME);
	page_stop_processing();
	message_info(l('You can reconfigure these items on Admin tools > Site preferences.'));
	$is_first = true;
    } else if(isset($args['apply']) && $args['apply'] === 'theme') {
	/*
	 * テーマを変えた直後のリダイレクト先。
	 *
	 * 定数 THEME は起動時に define() 済みで、保存したリクエストの中では
	 * まだ古いテーマを指している。新しいテーマの生成はここで行う。
	 */
	if(!theme_convert(THEME)) {
	    message_error(l('Could not apply theme preferences.'));
	} else {
	    header('Location: ' . url_make('', array('option' => 'admin_setup_site')));
	    exit(0);
	}
    } else {
	admin_setup_theme_repair();
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
	    $new_args = array('option' => 'admin_setup_site');
	    /*
	     * テーマを変えたなら、そのテーマの静的ファイルをまだ誰も作っていない。
	     * ブラウザが読むのは生成物 (theme/<名前>/) なので、作らずに戻ると
	     * CSS が 404 になり、画面が素の HTML になる。
	     * このリクエストでは THEME を切り替えられないので、生成は上の
	     * apply=theme に任せる。
	     */
	    if(admin_setup_saved_theme() !== THEME)
		$new_args['apply'] = 'theme';
	    header('Location: ' . url_make('', $new_args));
	    exit(0);
	}
    }

    return false;
}

function admin_setup_theme(&$args, $dom) {
    auth_check(false, 'admin');
    title_set_l('Appearance theme preferences');

    $is_first = isset($args['first']) && $args['first'] === 'true';

    if(!$is_first)
	admin_setup_theme_repair();

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

/*
 * 色調の設定。
 *
 * 画面で扱うのは 25 色そのもので、「色調を 1 つ選ぶ」段は無い。組み込みや
 * 保存した色調は「読み込む」で入力欄に流し込んでから調整する。
 */
function admin_setup_tone(&$args, $dom) {
    auth_check(false, 'admin');
    title_set_l('Tone preferences');

    if(isset($args['apply']) && $args['apply'] === 'theme') {
	if(!theme_convert(THEME)) {
	    message_error(l('Could not apply theme preferences.'));
	} else {
	    header('Location: ' . url_make('', array('option' => 'admin_setup_tone')));
	    exit(0);
	}
    } else {
	admin_setup_theme_repair();
	/*
	 * 選んでいる色調のファイルが無い (消した / 別のサイトから持ってくるのを
	 * 忘れた)。生成済みの CSS は使われ続けるので画面は壊れないが、作り直した
	 * 時点で既定の色調に変わる。黙って変わると分からないので知らせる。
	 */
	if(THEME_TONE !== TONE_CUSTOM_ID && tone_read(THEME_TONE) === false)
	    message_error(lf('The tone \'$tone$\' is gone. $default$ is used instead.',
			     array('tone' => THEME_TONE, 'default' => TONE_DEFAULT_ID)));
    }

    /*
     * 読み込み・保存・削除。設定と同じフォームから送られてくる。
     * 「読み込む」と「削除」は適用しない (編集途中の色をうっかり焼き付けない)。
     */
    if(!empty($args['tone_load'])) {
	admin_setup_tone_load($args);
	unset($args['apply']);
    } else if(!empty($args['tone_delete'])) {
	admin_setup_tone_delete($args);
	unset($args['apply']);
    } else if(!empty($args['tone_save'])) {
	if(!admin_setup_tone_save($args))
	    unset($args['apply']);
    }

    /* この画面で決めた 25 色をそのまま使う */
    $args['const_THEME_TONE'] = TONE_CUSTOM_ID;

    if(admin_setup($args, $dom, array('tone' => true), l('Need to refresh browser applying changes'),
		   'admin_setup_tone_form_head', 'admin_setup_tone_form_foot')) {
	cache_delete('', 'wiki-helper-info');
	header('Location: ' . url_make('', array('option' => 'admin_setup_tone', 'apply' => 'theme')));
	exit(0);
    }

    return false;
}

/* 選んだ色調の色を入力欄に出す。保存はしない (適用は利用者が押す) */
function admin_setup_tone_load(&$args) {
    global $TONE_LOADED_COLORS;

    $id = default_value($args['tone_load_id'], '');
    $tone = tone_read($id);
    if($tone === false) {
	message_error(l('Could not load the tone.'));
	return false;
    }
    $TONE_LOADED_COLORS = $tone['colors'];
    message_info(lf('Loaded the tone \'$tone$\'. Press apply to use it.',
		    array('tone' => tone_get_name($tone))));
    return true;
}

/* いま入力欄にある 25 色 */
function admin_setup_tone_colors(&$args) {
    global $THEME_CUSTOM_COLORS;
    global $TONE_LOADED_COLORS;

    $colors = array();
    foreach($THEME_CUSTOM_COLORS as $custom_name => $default_color) {
	$name = str_replace('THEME_CUSTOM_COLOR_', 'THEME_COLOR_', $custom_name);
	if(isset($TONE_LOADED_COLORS[$name])) {
	    $colors[$name] = $TONE_LOADED_COLORS[$name];
	    continue;
	}
	$value = default_value($args['const_' . $custom_name], $default_color);
	if(!color_is_valid($value))
	    $value = $default_color;
	$colors[$name] = $value;
    }
    return $colors;
}

function admin_setup_tone_save(&$args) {
    $id = trim(default_value($args['tone_id'], ''));
    if(!tone_is_valid_id($id)) {
	message_error(l('Use half-width alphanumerics, dot, underscore and hyphen for the identifier.'));
	return false;
    }

    $name = trim(default_value($args['tone_name'], ''));
    if($name === '')
	$name = $id;

    if(!tone_write($id, array(LANG => $name), admin_setup_tone_colors($args))) {
	message_error(l('Could not save the tone.'));
	return false;
    }
    return true;
}

function admin_setup_tone_delete(&$args) {
    $id = default_value($args['tone_delete_id'], '');
    if(!tone_delete($id)) {
	message_error(l('Could not delete the tone.'));
	return false;
    }
    message_info(l('Deleted the tone.'));
    return true;
}

/*
 * 「外観の設定」のフォームの末尾に付ける色調の保存欄。
 */
/*
 * 入力欄の前: 適用と、色調の読み込み。
 * 25 色は縦に長いので、上の方を直したときに一番下まで送らずに済むよう
 * 適用を上にも置く (下の適用と同じもの)。
 */
function admin_setup_tone_form_head($form) {
    $p = dom_append_child($form, 'p', 'apply');
    dom_append_child($p, 'input', '', array('type' => 'submit', 'value' => l('Apply')));
    $small = dom_append_child($p, 'small');
    dom_append_text($small, '(' . l('Need to refresh browser applying changes') . ')');

    $dl = dom_append_child($form, 'dl', 'tone');

    $dt = dom_append_child($dl, 'dt');
    dom_append_text($dt, l('Load a tone'));
    $dd = dom_append_child($dl, 'dd');

    /* 先頭は空。押し間違いで色を流し込んでしまわないように */
    $options = array('' => '');
    foreach(tone_get_all() as $id => $tone) {
	$options[$id] = tone_get_name($tone) . ' (' . $id . ')';
    }
    dom_append_selector($dd, 'tone_load_id', $options);
    dom_append_text($dd, ' ');
    dom_append_child($dd, 'input', '',
		     array('type' => 'submit', 'name' => 'tone_load', 'value' => l('Load')));
}

/* 入力欄の後ろ: 保存と削除 */
function admin_setup_tone_form_foot($form) {
    $dl = dom_append_child($form, 'dl', 'tone');

    $dt = dom_append_child($dl, 'dt');
    dom_append_text($dt, l('Save this tone'));
    $dd = dom_append_child($dl, 'dd');
    dom_append_text($dd, l('Identifier') . ' ');
    dom_append_child($dd, 'input', 'tone_id',
		     array('type' => 'text', 'name' => 'tone_id', 'value' => ''));
    dom_append_text($dd, ' ' . l('Display name') . ' ');
    dom_append_child($dd, 'input', 'tone_name',
		     array('type' => 'text', 'name' => 'tone_name', 'value' => ''));
    dom_append_text($dd, ' ');
    dom_append_child($dd, 'input', '',
		     array('type' => 'submit', 'name' => 'tone_save', 'value' => l('Save')));
    $small = dom_append_child($dd, 'small');
    dom_append_text($small, l('the identifier becomes the file name'));

    $saved = array();
    foreach(tone_get_all() as $id => $tone) {
	if(tone_is_saved($id))
	    $saved[$id] = tone_get_name($tone) . ' (' . $id . ')';
    }
    if(count($saved) <= 0)
	return;

    $dt = dom_append_child($dl, 'dt');
    dom_append_text($dt, l('Delete a saved tone'));
    $dd = dom_append_child($dl, 'dd');
    dom_append_selector($dd, 'tone_delete_id', $saved);
    dom_append_text($dd, ' ');
    dom_append_child($dd, 'input', '',
		     array('type' => 'submit', 'name' => 'tone_delete', 'value' => l('Delete')));
    $small = dom_append_child($dd, 'small');
    dom_append_text($small, l('deleting a tone that came with NextForm brings the original back'));
}

/*
 * アップロードした画像を消す。拡張子は上げたときのものが残るので全部見る。
 */
function admin_setup_image_delete_files($setup_constant) {
    $delete_path = THEME_DIR_PATH . '/' . $setup_constant['path'];
    if(isset($setup_constant['extensions'])) {
	foreach($setup_constant['extensions'] as $extension) {
	    if(is_file($delete_path . $extension))
		unlink($delete_path . $extension);
	}
    } else if(is_file($delete_path)) {
	unlink($delete_path);
    }
}

/*
 * 保存されているテーマ名を返す。定数 THEME は起動時の値なので、
 * 設定を保存した直後のリクエストでは両者が食い違う。
 */
function admin_setup_saved_theme() {
    $contents = setup_read('site');
    if($contents === false)
	return THEME;
    $values = unserialize($contents);
    if($values === false)
	return THEME;
    return default_value($values['THEME'], THEME);
}

/*
 * 静的ファイルを失ったテーマを作り直す。
 *
 * 生成物 (theme/<名前>/) はデータではなくキャッシュで、追跡もされない。
 * 消えても設定は正しいまま画面だけが崩れるので、気付いた人が設定画面を
 * 開けば戻るようにしておく。既にあるときは何もしない。
 */
function admin_setup_theme_repair() {
    if(is_dir(THEME_DIR_PATH . '/' . THEME))
	return;
    if(!theme_convert(THEME))
	message_error(l('Could not apply theme preferences.'));
}

/*
 * $form_head / $form_foot には関数名を渡す。設定の入力欄の前と後ろ (適用ボタンの
 * 手前) に要素を足せる。色調の設定の「読み込み」「保存」「削除」がこれを使う。
 * 設定の入力欄と同じフォームに入れることで、まだ適用していない色をそのまま
 * 保存できる。
 */
function admin_setup(&$args, $dom, $target_categories, $note = '',
		     $form_head = '', $form_foot = '') {
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
		$is_uploaded = isset($_FILES[$name]['name']) && $_FILES[$name]['name'] !== '';
		$action = default_value($args[$name . '_action'], '');
		if($is_uploaded && $action !== '') {
		    /*
		     * どちらが後の操作なのかはサーバには分からない。片方を黙って
		     * 捨てると、選んだはずの指定が効かなかったように見える。
		     */
		    message_error(lf('$term$: cannot choose an image and an action at the same time.',
				     array('term' => l($setup_constant['description']))));
		    $is_success = false;
		    $setup_constant['error'] = true;
		    continue;
		}
		if(!$is_uploaded &&
		   ($action === ADMIN_SETUP_IMAGE_DEFAULT || $action === ADMIN_SETUP_IMAGE_NONE)) {
		    /*
		     * 標準に戻す = 設定値そのものを消す。setup_local() が
		     * $setup_constant['default'] を使うので、テーマ同梱の画像に戻り、
		     * テーマを切り替えればそのテーマの画像に追随する。
		     * 空文字を保存すると「設定された値」になってしまい、戻れない。
		     *
		     * 表示しない = 空文字を保存する。html.php は空なら出さない。
		     */
		    if($action === ADMIN_SETUP_IMAGE_DEFAULT)
			unset($values[$key]);
		    else
			$values[$key] = '';
		    admin_setup_image_delete_files($setup_constant);
		    continue;
		}

		if(!$is_uploaded)
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

	    /*
	     * 画面に出していない項目 (hidden) はフォームから送られてこない。
	     * 送られてこなかった項目は保存されている値のままにする。
	     */
	    if(!isset($args[$name]))
		continue;

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
	    case 'tone':
		$options = tone_get_options();
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

    if($form_head !== '')
	call_user_func($form_head, $form);

    $dl = dom_append_child($form, 'dl');

    $groups = array();
    $parents = array();

    foreach($SETUP_CONSTANTS as $key => &$setup_constant) {
	$category = default_value($setup_constant['category'], 'site');
	if(empty($target_categories[$category]))
	    continue;
	/* 保存はするが画面には出さない項目 (色調の設定が使う) */
	if(!empty($setup_constant['hidden']))
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
	/*
	 * 保存されている値が「いまの見た目」と食い違う項目のための差し替え。
	 * 個別色 (色調がプリセットのときは使われない) がこれを使う。
	 */
	if(isset($setup_constant['value_function']))
	    $value = call_user_func($setup_constant['value_function'], $key, $value);
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
	case 'tone':
	    /*
	     * 保存されている値は上流の古い名前 ('beige/green') かもしれない。
	     * 読み替えないと、選択肢のどれにも一致せず先頭が選ばれてしまう。
	     */
	    $append_element = dom_append_selector($dd, $name, tone_get_options(),
						  tone_normalize_id($value));
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
	    if(!empty($setup_constant['allowempty'])) {
		/*
		 * 画像を上げ直す以外にできること。「標準に戻す」が無いと、
		 * 一度差し替えた画像から元に戻す手段が無くなる。
		 *
		 * 先頭は空にしておく。隣で画像を選んだのに「そのまま」と
		 * 書いてあると、どちらが効くのか分からない。
		 *
		 * 既定の画像を持たない項目 (見出しや本文の背景画像) では
		 * 「標準に戻す」と「表示しない」が同じことになるので出さない。
		 */
		$actions = array('' => '');
		if(default_value($setup_constant['default'], '') !== '')
		    $actions[ADMIN_SETUP_IMAGE_DEFAULT] = l('Restore the default image');
		$actions[ADMIN_SETUP_IMAGE_NONE] = l('Do not show');
		dom_append_text($dd, ' ');
		dom_append_selector($dd, $name . '_action', $actions);
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

    if($form_foot !== '')
	call_user_func($form_foot, $form);

    $p = dom_append_child($form, 'p', 'apply');
    $submit = dom_append_child($p, 'input', '', array('type' => 'submit', 'value' => l('Apply')));

    if($note !== '') {
	$small = dom_append_child($p, 'small');
	dom_append_text($small, '(' . $note . ')');
    }

    return false;
}
?>
