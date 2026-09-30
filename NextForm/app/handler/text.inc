<?php
$HANDLERS['text'] = array(
    'show' => 'text_show',
    'source' => 'text_show',
    'summary' => 'text_summary',
    'edit' => 'text_edit',
    'write' => 'text_write',
    'texts' => 'text_texts');
$PAGE_TYPES_TEXTBASE[] = 'text';

function text_show(&$page, $dom) {
    $pre = dom_append_child($dom, 'pre', 'text');
    sentence_append($pre, page_get_contents($page), $page, array('excludes' => array('pagename')));
}

function text_source(&$page, $dom) {
    title_set_pagename('Source of $pagename$', $page['name']);
    text_show($page, $dom);
}

function text_summary(&$page, $dom) {
    $contents = page_get_contents($page);
    $words = preg_split(LANGUAGE_SEARCH_SPLIT_REGEX, $contents, -1, PREG_SPLIT_NO_EMPTY);
    if(!empty($words))
	dom_append_text($dom, $words[0]);
}

function text_edit(&$page, $dom, $type = 'text', $contents = null, $ticket = null) {
    global $PAGE_TYPES_PREVIEWABLE;
    $has_permission = auth_warn($page['name'], 'write') && lock_warn($page);
    title_set_pagename('Editing $pagename$', $page['name']);
    $source_contents = page_get_contents($page);
    if(is_null($contents))
	$contents = $source_contents;
    else 
	$page['meta']['ticket'] = $ticket;

    $hiddens = array('action' => 'write');
    if(!$page['is_exists'])
	$hiddens['newpage_type'] = $type;
    $hiddens['source_contents'] = $source_contents;

    $form = dom_append_page_form($page, $dom, $hiddens, 'text_edit type_' . $type);
    if(!empty($page['args']['preview']))
	dom_append_class($form, 'preview');

    text_edit_append_notation($form, $type);
    $textarea = dom_append_child($form, 'textarea', '', array('name' => 'contents', /*'autofocus' => ''*/));
    dom_append_text($textarea, "\n" . $contents);

    $ul = dom_append_child($form, 'ul');

    $li = dom_append_child($ul, 'li');
    $submit = dom_append_child($li, 'input', '',
			       array('type' => 'submit', 'name' => 'save_and_quit',
				     'value' => l('Save and quit') . '(S)', 'accesskey' => 's'));
    if(!$has_permission)
	dom_set_attribute($submit, 'disabled', 'disabled');
    $li = dom_append_child($ul, 'li');
    $submit = dom_append_child($li, 'input', '',
			       array('type' => 'submit', 'name' => 'save_and_edit',
				     'value' => l('Save and edit') . '(C)' , 'accesskey' => 'c'));
    if(!$has_permission)
	dom_set_attribute($submit, 'disabled', 'disabled');

    if(in_array($type, $PAGE_TYPES_PREVIEWABLE)) {
	$li = dom_append_child($ul, 'li');
	$submit = dom_append_child($li, 'input', '',
				   array('type' => 'submit', 'name' => 'preview',
					 'value' => l('Preview') . '(P)', 'accesskey' => 'p'));
    }

    $li = dom_append_child($ul, 'li');
    $label = dom_append_child($li, 'label');
    $check_attrs = array('type' => 'checkbox', 'name' => 'keep_mtime', 'value' => 'on');
    if(!empty($page['args']['keep_mtime']))
	$check_attrs['checked'] = 'checked';
    dom_append_child($label, 'input', '', $check_attrs);
    dom_append_text($label, l('Keep modify time'));

    text_edit_append_cheat_sheet($ul, $type);
}

/*
 * 記法の早見表へのリンク。
 *
 * マニュアルは組み込みになったので、公式サイトへ逃げる経路は要らない。
 * ただし言語によっては訳が無い (英語版に Markdown のページはまだ無い) ので、
 * 実在を確かめてから出す。無いページへのリンクを編集画面に置かない。
 */
function text_edit_append_cheat_sheet($ul, $type) {
    global $PAGE_TYPE_CHEAT_SHEETS;
    $manual_page_name = text_edit_cheat_sheet_pagename($type);
    if($manual_page_name === false)
	return;

    $li = dom_append_child($ul, 'li');
    $a = dom_append_link($li, url_make($manual_page_name),
			 l($PAGE_TYPE_CHEAT_SHEETS[$type]['label']));
    dom_set_attribute($a, 'target', '_blank');
}

/* その種別の早見表のページ名。無ければ false */
function text_edit_cheat_sheet_pagename($type) {
    global $PAGE_TYPE_CHEAT_SHEETS;
    if(empty($PAGE_TYPE_CHEAT_SHEETS[$type]))
	return false;
    $manual_page_name = MANUAL_PAGENAME . '/' . LANG . $PAGE_TYPE_CHEAT_SHEETS[$type]['page'];
    if(!page_is_exists($manual_page_name))
	return false;
    return $manual_page_name;
}

/*
 * 入力欄の上の札。いま書いているのが Wiki 記法か Markdown かを示す
 * ($PAGE_TYPE_NOTATIONS。ほかの種別には出さない)。
 *
 * 入力欄の左の線 (.colors-map.css。その記法のページへのリンクの色) と違って
 * 色に頼らないので、リンクの色を同じにしていても読める。早見表があれば
 * そこへのリンクを兼ねる。部分編集の欄には nextform.js が同じ札を出す。
 */
function text_edit_append_notation($dom, $type) {
    global $PAGE_TYPE_NOTATIONS;
    if(empty($PAGE_TYPE_NOTATIONS[$type]))
	return;

    $p = dom_append_child($dom, 'p', 'notation');
    $label = l($PAGE_TYPE_NOTATIONS[$type]);
    $manual_page_name = text_edit_cheat_sheet_pagename($type);
    if($manual_page_name === false) {
	dom_append_text(dom_append_child($p, 'span'), $label);
	return;
    }
    $a = dom_append_link($p, url_make($manual_page_name), $label);
    dom_set_attribute($a, 'target', '_blank');
}

function text_write(&$page, $dom, $type = 'text') {
    title_set_pagename('Editing $pagename$', $page['name']);

    $page['meta']['type'] = $type;
    $contents = default_value($page['args']['contents'], '');
    if(!empty($page['args']['keep_mtime']))
	$page['keep_mtime'] = true;

    if($contents === '') {
	if(page_delete($page))
	    message_notice(l('Page deleted.'));
    } else {
	if(page_write($page, $contents, $page['args']['ticket'], $error)) {
	    if(!empty($page['args']['save_and_edit'])) {
		$page = page_read($page['name']);
		ld(message_info(), 'Saved. ($time$)', array(
		       'time' => dom_append_time(null, time(), TIME_FORMAT)
		       ));
		text_edit($page, $dom, $type);
	    } else {
		header('Location: ' . url_make($page['name']));
		exit(0);
	    }
	} else {
	    if($error == PAGE_WRITE_ERROR_CONFLICT) {
		$source_contents = default_value($page['args']['source_contents'], '');
		$article1 = dom_append_child($dom, 'article', 'conflict1');
		$article2 = dom_append_child($dom, 'article', 'conflict2');
		$h = dom_append_child($article1, 'h1');
		dom_append_text($h, l('Current source'));
		$h = dom_append_child($article2, 'h1');
		dom_append_text($h, l('Your changes'));

		dom_append_compare_line($article2, $source_contents, $contents);
		$reread_page = page_read($page['name']);
		text_edit($reread_page, $article1, $type);
	    } else {
		message_error(l('Page write failed.'));
	    }
	}
    }
}

function text_texts(&$page) {
    return array('body' => explode("\n", page_get_contents($page)));
}

?>
