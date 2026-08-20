<?php
$OPTIONS = array();

/*
 * 画面の親子関係。見出しの後ろに「(管理ツール/サイト設定)」と道筋を出すのに使う。
 * 中身は各 option のファイルが入れる (管理ツールの下は option/admin.inc)。
 *
 *   $OPTION_PATHS['admin_setup_site'] =
 *       array('title' => 'Site preferences', 'parent' => 'admin');
 *
 * 入口が複数ある画面は 'parent' の代わりに 'parent_function' を持つ。
 * 引数はその要求の $args で、親の名前か false (道筋を出さない) を返す。
 *
 * $OPTIONS に混ぜていないのは、$OPTIONS のキーが action の名前として
 * そのまま呼ばれるため (handle_is_dispatchable)。
 */
$OPTION_PATHS = array();

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
	option_append_title_path($args['option'], $args);
	unset($args['action']);
	return dom_save_html($dom);
    } else {
	message_error(l('Unkown parameters'));
	$is_continue = false;
	return '';
    }
}

/*
 * 見出しの後ろに、その画面までの道筋を出す。
 * $OPTION_PATHS の parent を根までたどる。登録の無い画面では何もしない。
 */
function option_append_title_path($name, $args = array()) {
    global $OPTION_PATHS;

    $links = array();
    $seen = array();
    while(!empty($OPTION_PATHS[$name]['title']) && !isset($seen[$name])) {
	$seen[$name] = true;
	array_unshift($links,
		      array('name' => l($OPTION_PATHS[$name]['title']),
			    'uri' => url_make('', array('option' => $name))));
	if(!isset($OPTION_PATHS[$name]['parent_function'])) {
	    $name = default_value($OPTION_PATHS[$name]['parent'], '');
	    continue;
	}
	/*
	 * 入口が複数ある画面 (パスワード設定) は、どこから来たのかを
	 * 画面自身に決めさせる。false が返ったら道筋を出さない。
	 */
	$parent = call_user_func($OPTION_PATHS[$name]['parent_function'], $args);
	if($parent === false) {
	    $links = array();
	    break;
	}
	$name = $parent;
    }
    title_append_path($links);
}
?>
