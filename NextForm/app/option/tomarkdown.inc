<?php
/*
 * Wiki 記法のページを Markdown のページに変換する (?ページ&option=tomarkdown)。
 *
 * 種別 wiki のページのページメニューに出す。確認の画面で、変換後の本文と、落ちるもの・
 * 原文の枠で残るもの・飛び先 (id) が変わる見出しを見せ、「変換する」で保存する。
 * 変換の規則は handler/wiki/wiki_markdown.inc、設計は
 * docs/development/design-wiki-to-markdown.md の 3e。
 *
 * 変換前の版は、子ページを作らず履歴に残す (利用者の判断 2026-10-18)。同じ人が続けて
 * 保存したときでも必ず残し (backup_now)、その版の時刻を本文の末尾のリンクに書く。
 * 元に戻すのは、その版の画面の「この版に戻す」(option/restore.inc)。
 * 種別は page_write() の中で変える (write_meta。古い ngram を古い種別で数えるため)。
 */

$TOOLS['page']['tomarkdown'] = array(
    'order' => 85,
    'show' => 'tomarkdown_tool'
    );

$OPTIONS['tomarkdown'] = array(
    'show' => 'tomarkdown_show',
    'write' => 'tomarkdown_write',
    );

/* 種別 wiki のページにだけ出す。書けない・ロック中は押せなくする (編集と同じ) */
function tomarkdown_tool(&$page, $dom) {
    if(empty($page) || empty($page['is_exists']) || page_get_type($page) !== 'wiki')
	return;
    tools_add_link(
	$dom,
	array('name' => l('Convert to Markdown'),
	      'disable' => !restore_can_restore($page),
	      'uri' => url_make($page['name'], array('option' => 'tomarkdown'))));
}

function tomarkdown_show(&$args, $dom) {
    $page = &main_page();
    tomarkdown_append_form($dom, $page);
    return false;
}

function tomarkdown_write(&$args, $dom) {
    $page = &main_page();
    auth_check($page['name'], 'write');
    $ticket = default_value($args['ticket'], '');
    if(!empty($page['is_exists']) && page_get_type($page) === 'wiki' &&
       tomarkdown_convert_page($page, $ticket)) {
	header('Location: ' . url_make($page['name']));
	exit(0);
    }
    $page = page_read($page['name']);
    tomarkdown_append_form($dom, $page);
    return false;
}

/*
 * 変換して保存する。変換前の版を時刻 $backup_time で必ず残し、末尾にその版への
 * リンクを付ける。題名とタグは Markdown の normalize が本文 (先頭の # と
 * フロントマター) から取る。ロック・権限・ticket は page_write() が確かめる。
 */
function tomarkdown_convert_page(&$page, &$ticket) {
    $result = wiki_markdown_convert($page, (string)page_get_contents($page));
    $backup_time = tomarkdown_backup_time($page);
    $contents = $result['markdown'] . ($result['markdown'] === '' ? '' : "\n") .
	tomarkdown_footer($page['name'], $backup_time) . "\n";

    /* normalize は種別で引くので、Markdown をそのあいだだけ当てる (meta は page_write() が書く) */
    $page['meta']['type'] = 'markdown';
    $contents = markdown_normalize($page, $contents);
    $page['meta']['type'] = 'wiki';

    $page['write_meta'] = array('type' => 'markdown');
    $page['backup_now'] = $backup_time;
    $is_written = page_write($page, $contents, $ticket);
    unset($page['write_meta'], $page['backup_now']);
    return $is_written;
}

/* 変換前の版の時刻。同じ秒の版が既にあればずらす (同じ時刻の版は上書きされない) */
function tomarkdown_backup_time(&$page) {
    $time = time();
    $times = array_flip(page_get_backup_times($page));
    while(isset($times[$time]))
	$time++;
    return $time;
}

/*
 * 本文の末尾の一文。変換前の版へのリンクは ? で始まるので、Markdown では
 * ページ名ではなく URL のまま扱われる (handler/markdown/link.inc)。
 * ページの名前を変えるとこのリンクは切れる (履歴はページに付いていく)。
 */
function tomarkdown_footer($pagename, $backup_time) {
    $url = '?' . url_path_encode($pagename) . '&' .
	url_make_query(array('option' => 'history', 'action' => 'page', 'backuptime' => $backup_time));
    $url = str_replace(array('(', ')', ' '), array('%28', '%29', '%20'), $url);
    return l('This page was converted from a wiki page.') . '[' . l('(The version before conversion)') . '](' . $url . ')';
}

/* 確認の画面 */
function tomarkdown_append_form($dom, &$page) {
    title_set_pagename('Convert $pagename$ to Markdown', $page['name']);
    if(empty($page['is_exists'])) {
	message_error(l('Page not found.'));
	return;
    }
    if(page_get_type($page) !== 'wiki') {
	message_error(l('Only pages of type wiki can be converted to Markdown.'));
	return;
    }
    $has_permission = auth_warn($page['name'], 'write') && lock_warn($page);
    $source = (string)page_get_contents($page);
    $result = wiki_markdown_convert($page, $source);

    $p = dom_append_child($dom, 'p');
    dom_append_text($p, l('This page is converted to Markdown notation as below. The version before conversion is kept in the history, and a link to it is added at the end of the text. To undo, open that version and press "Restore this version".'));

    tomarkdown_append_list($dom, l('Things that are dropped'), $result['lost']);
    tomarkdown_append_list($dom, l('Functions kept as their source in code blocks'), $result['kept']);
    $headings = tomarkdown_changed_headings($page, $source, $result['markdown']);
    if(!empty($headings)) {
	$section = dom_append_child($dom, 'section', 'tomarkdown_headings');
	$h = dom_append_child($section, 'h2');
	dom_append_text($h, l('Headings whose links change ([[page#heading]] from other pages stops reaching them)'));
	$ul = dom_append_child($section, 'ul');
	foreach($headings as $heading) {
	    $li = dom_append_child($ul, 'li');
	    dom_append_text($li, $heading);
	}
    }

    $form = dom_append_page_form($page, $dom, array('option' => 'tomarkdown', 'action' => 'write'), 'tomarkdown');
    $label = dom_append_child($form, 'label');
    dom_append_text($label, l('Text after conversion'));
    $textarea = dom_append_child($label, 'textarea', '', array('readonly' => 'readonly', 'rows' => '20', 'cols' => '80'));
    dom_append_text($textarea, $result['markdown'] . "\n" . tomarkdown_footer($page['name'], tomarkdown_backup_time($page)) . "\n");
    $submit = dom_append_child($form, 'input', '', array('type' => 'submit', 'value' => l('Convert')));
    if(!$has_permission)
	dom_set_attribute($submit, 'disabled', 'disabled');
}

function tomarkdown_append_list($dom, $title, $items) {
    if(empty($items))
	return;
    $section = dom_append_child($dom, 'section', 'tomarkdown_report');
    $h = dom_append_child($section, 'h2');
    dom_append_text($h, $title);
    $ul = dom_append_child($section, 'ul');
    foreach($items as $label => $count) {
	$li = dom_append_child($ul, 'li');
	dom_append_text($li, lf('$label$ ($count$)', array('label' => $label, 'count' => $count)));
    }
}

/*
 * 飛び先 (id) が変わる見出し。id は見出しの文字から作るので、文字が同じなら他のページ
 * からの [[ページ#見出し]] は届き続ける。変換で文字が変わるもの (記法の落ちた見出しなど)
 * だけを出す。両方を本文として描いて、振られた id を比べる。
 */
function tomarkdown_changed_headings(&$page, $source, $markdown) {
    $wiki_page = $page;
    $wiki_page['is_main'] = true;
    $wiki_dom = dom_create_document();
    wiki_convert($wiki_page, $wiki_dom, $source);
    $wiki_ids = array();
    $xpath = new DOMXPath(dom_owner($wiki_dom));
    foreach($xpath->query('.//section[@id]', $wiki_dom) as $section) {
	$heading = $xpath->query('h1', $section)->item(0);
	$wiki_ids[$section->getAttribute('id')] = is_null($heading) ? '' : trim($heading->textContent);
    }

    $markdown_page = $page;
    $markdown_page['meta']['type'] = 'markdown';
    $markdown_page['is_main'] = true;
    $markdown_dom = dom_create_document();
    markdown_convert($markdown_page, $markdown_dom, $markdown);
    $markdown_ids = array();
    $xpath = new DOMXPath(dom_owner($markdown_dom));
    foreach($xpath->query('.//*[@id]', $markdown_dom) as $element)
	$markdown_ids[$element->getAttribute('id')] = true;

    $changed = array();
    foreach($wiki_ids as $id => $text)
	if(!isset($markdown_ids[$id]))
	    $changed[] = $text;
    return $changed;
}
?>
