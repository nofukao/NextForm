<?php
/*
 * 履歴の「この版に戻す」(?ページ&option=restore&backuptime=時刻)。
 *
 * 履歴の一覧と旧版の画面から来る。確認の画面を出し、「この版に戻す」で
 * その版の本文と種別で今のページを書き直す (page_restore())。種別の違う版にも
 * 戻せる。戻す前の版は履歴に残るので、戻したこと自体もまた戻せる。
 *
 * 旧版の画面 (option=history&action=page) の internal_action は使わない。
 * あの経路は旧版のページに対して action を走らせるので、ロックの判定も
 * 旧版の meta で行われてしまう。ここでは今のページで確かめる。
 */

$OPTIONS['restore'] = array(
    'show' => 'restore_show',
    'write' => 'restore_write',
    );

function restore_show(&$args, $dom) {
    $page = &main_page();
    restore_append_form($dom, $page, default_value($args['backuptime'], ''));
    return false;
}

function restore_write(&$args, $dom) {
    $page = &main_page();
    auth_check($page['name'], 'write');
    $backuptime = default_value($args['backuptime'], '');
    $ticket = default_value($args['ticket'], '');

    if(!restore_is_backuptime($backuptime)) {
	message_error(l('Backup not found.'));
    } else if(page_restore($page, (int)$backuptime, $ticket)) {
	header('Location: ' . url_make($page['name']));
	exit(0);
    }
    $page = page_read($page['name']);
    restore_append_form($dom, $page, $backuptime);
    return false;
}

/*
 * 版の時刻は数字だけ (保存先のファイル名に使われる)。0 は受け付けない。
 * storage_page_get_filepath() は空の時刻 (empty("0")) を今のページとして扱う。
 */
function restore_is_backuptime($backuptime) {
    return is_string($backuptime) && preg_match('/^[1-9][0-9]*$/', $backuptime);
}

/*
 * 戻すかどうかを確かめる画面。戻す先の版と、それを見るリンクを出す。
 * 書く権限が無い・ロック中は、ボタンを押せなくする (削除の画面と同じ)。
 */
function restore_append_form($dom, &$page, $backuptime) {
    title_set_pagename('Restore an old version of $pagename$', $page['name']);
    if(empty($page['is_exists'])) {
	message_error(l('Page not found.'));
	return;
    }
    if(!restore_is_backuptime($backuptime)) {
	message_error(l('Backup not found.'));
	return;
    }
    $backup_page = page_read_backup($page['name'], $backuptime);
    if(empty($backup_page['is_exists'])) {
	message_error(l('Backup not found.'));
	return;
    }
    $has_permission = auth_warn($page['name'], 'write') && lock_warn($page);

    $p = dom_append_child($dom, 'p');
    ld($p, 'Overwrite the current $pagename$ with the text and the type ($type$) of the version at $time$. The current content is kept in the history, so this can be undone in the same way.',
       array('pagename' => dom_pagename($page['name']),
	     'type' => page_type_label(default_value($backup_page['meta']['type'], DEFAULT_PAGE_TYPE)),
	     'time' => dom_append_time(null, $backuptime, TIME_FORMAT)));

    $ul = dom_append_child($dom, 'ul');
    $li = dom_append_child($ul, 'li');
    dom_append_link($li, url_make($page['name'], array('option' => 'history', 'action' => 'page',
							'backuptime' => $backuptime)),
		    l('Show this version'));
    $li = dom_append_child($ul, 'li');
    dom_append_link($li, url_make($page['name'], array('option' => 'history')), l('History'));

    $form = dom_append_page_form($page, $dom,
				 array('option' => 'restore', 'action' => 'write', 'backuptime' => $backuptime),
				 'restore');
    $submit = dom_append_child($form, 'input', '', array('type' => 'submit', 'value' => l('Restore this version')));
    if(!$has_permission)
	dom_set_attribute($submit, 'disabled', 'disabled');
}

/*
 * 履歴の一覧と旧版の画面に出す「この版に戻す」のリンク。編集できる人だけに出す
 * (ロック中は、編集と同じく管理者とロックした本人だけ)。
 */
function restore_can_restore(&$page) {
    return !empty($page['is_exists']) && auth_has_permission($page['name'], 'write') &&
	(empty($page['meta']['lock']) || lock_has_permission($page));
}

function restore_append_link($dom, &$page, $backuptime) {
    return dom_append_link($dom, url_make($page['name'], array('option' => 'restore', 'backuptime' => $backuptime)),
			   l('Restore this version'));
}
?>
