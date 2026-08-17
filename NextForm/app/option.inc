<?php
$OPTIONS = array();

foreach(glob_array(APP_DIR_PATH . '/option/*.inc') as $file) require_once($file);

function option(&$args, $action = '', &$is_continue = true) {
    global $OPTIONS;

    if(empty($action) && !empty($args['action']))
	$action = $args['action'];
    if(empty($action))
	$action = 'show';

    if(empty($args['option']) || empty($OPTIONS[$args['option']])) {
	$is_continue = true;
	return '';
    }

    /*
     * 実体の無い登録は「知らないパラメータ」として扱う。
     * $OPTIONS['listedit']['insert'] は上流から存在しない関数を指しており、
     * 呼ぶと Fatal error になっていた。詳細は handle.inc の
     * handle_is_dispatchable() の説明を参照。
     */
    $option = &$OPTIONS[$args['option']];
    if(handle_is_dispatchable($option, $action)) {
	/* 状態を変える action は POST かつ同一オリジンのときだけ通す */
	csrf_check($action);
	$dom = dom_create_document();
	$is_continue = $option[$action]($args, $dom);
	unset($args['action']);
	return dom_save_html($dom);
    } else {
	message_error(l('Unkown parameters'));
	$is_continue = false;
	return '';
    }
}
?>
