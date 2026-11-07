<?php
$TOOLS['page']['meta'] = array(
    'order' => 110,
    'show' => 'meta_tool'
    );

function meta_tool(&$page, $dom) {
    $pagename = empty($page) ? '' : $page['name'];
    tools_add_link(
	$dom,
	array('name' => l('Meta info'),
	      'disable' => empty($page),
	      'uri' => url_make($pagename, array('option' => 'meta'))));
}

$OPTIONS['meta'] = array(
    'show' => 'meta_show',
    'write' => 'meta_write',
    );

function meta_show(&$args, $dom) {
    $page = &main_page();
    $has_permission = auth_has_permission($page['name'], 'write') && lock_warn($page);
    title_set_pagename('Meta info of $pagename$', $page['name']);

    $form = dom_append_page_form($page, $dom,
				 array('option' => 'meta',
				       'action' => 'write',
				     ), 'meta');

    $dl = dom_append_child($form, 'dl', 'form');
    $dt = dom_append_child($dl, 'dt');
    dom_append_text($dt, l('Title'));
    $dd = dom_append_child($dl, 'dd');
    $default_value = isset($page['meta']['title']) ? $page['meta']['title'] : '';
    dom_append_child($dd, 'input', 'title',
		     array('type' => 'text', 'name' => 'title', 'value' => $default_value));
    /*
     * 本文が題名を決める種別では、ここで付けた題名が次の本文保存で
     * 置き換わる。入力欄のすぐ横で断っておく。
     */
    $title_notices = array('wiki'     => 'overwritten with &title wiki function',
			   'markdown' => 'overwritten with front matter title');
    $type = page_get_type($page);
    if(isset($title_notices[$type])) {
	$small = dom_append_child($dd, 'small');
	dom_append_text($small, l($title_notices[$type]));
    }

    /*
     * 短いタイトル (カレンダーのセルなど、場所で分かることを除いて出すところに使う)。
     * 題名と同じく、本文に書く場所がある種別では本文が優先される。
     */
    $dt = dom_append_child($dl, 'dt');
    dom_append_text($dt, l('Short title'));
    $dd = dom_append_child($dl, 'dd');
    $default_value = isset($page['meta']['short_title']) ? $page['meta']['short_title'] : '';
    dom_append_child($dd, 'input', 'short_title',
		     array('type' => 'text', 'name' => 'short_title', 'value' => $default_value));
    $short_title_notices = array('wiki'     => 'overwritten with &title(short) wiki function',
				 'markdown' => 'overwritten with front matter short_title');
    if(isset($short_title_notices[$type])) {
	$small = dom_append_child($dd, 'small');
	dom_append_text($small, l($short_title_notices[$type]));
    }

    $dt = dom_append_child($dl, 'dt');
    dom_append_text($dt, l('Description'));
    $dd = dom_append_child($dl, 'dd');
    $textarea = dom_append_child($dd, 'textarea', 'description', array('name' => 'description'));
    $default_value = isset($page['meta']['description']) ? $page['meta']['description'] : '';
    dom_append_text($textarea, $default_value);
    $small = dom_append_child($dd, 'small');
    dom_append_text($small, l('referred by search engines'));
    
    $submit = dom_append_child($form, 'input', '', array('type' => 'submit', 'value' => l('Save')));
    if(!$has_permission)
	dom_set_attribute($submit, 'disabled', 'disabled');

    return false;
}

function meta_write(&$args, $dom) {
    $page = &target_page($args);
    title_set_pagename('Meta info of $pagename$', $page['name']);
   
    /* 本文が題名・短いタイトルを決めているかを、表示の経路を走らせて見る */
    if(isset($args['title']) || isset($args['short_title'])) {
	$dummy = dom_create_document();
	handle($page, $dummy, 'show');
    }
    if(isset($args['title']) && !isset($page['title_dom'])) {
	/*
	 * 空文字は「消す」(page_write)。meta を直に落とすと、page_write() が古い ngram を
	 * 数える前に題名が消え、消した題名の語が検索の索引に残る。
	 */
	$page['title'] = trim($args['title']);
    }
    if(isset($args['short_title']) && !isset($page['short_title'])) {
	/*
	 * 本文が決めていないときだけ使う。空文字は「消す」(page_write)。
	 * meta を直に落とさない (題名の注意と同じ)。
	 */
	$page['short_title'] = trim(preg_replace('/\s+/u', ' ', $args['short_title']));
    }
    if(isset($args['description'])) {
	/* 空文字は「消す」(page_write。題名と同じ約束) */
	$page['description'] = $args['description'];
    }

    $page['keep_mtime'] = true;
    $fp = page_open_contents($page);
    if(page_write($page, $fp, $page['meta']['ticket'])) {
	header('Location: ' . url_make($page['name']));
	exit(0);
	/*
	message_notice(lf('Save meta info of $pagename$.',
			  array('pagename' => $page['name'])));
	*/
    } else {
	message_error(l('Page write failed.'));
    }

    if(isset($args['back_page'])) {
	dom_append_link($dom, url_make($args['back_page']), l('Return'));
    }

    return false;
}
?>
