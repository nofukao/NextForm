<?php
$TOOLS['site']['admin'] = array(
    'order' => 1000,
    'show' => 'admin_tool'
    );

function admin_tool(&$page, $dom) {
    if(auth_has_permission(false, 'admin')) {
	tools_add_link(
	    $dom,
	    array('name' => l('Admin tools'),
		  'uri' => url_make('', array('option' => 'admin'))));
    }
}

$OPTIONS['admin'] = array(
    'show' => 'admin_show',
    );

/*
 * 管理ツールの下にある画面。見出しの後ろに出す道筋 (option.inc) に使う。
 * 下の一覧とは別に持つ。表示名が違う画面 (検索インデックス) があり、
 * 一覧は「押す操作の名前」、道筋は「場所の名前」を出すため。
 * 画面を足したら両方に足す。
 */
$OPTION_PATHS['admin']                   = array('title' => 'Admin tools');
$OPTION_PATHS['admin_setup_site']        = array('title' => 'Site preferences',
						 'parent' => 'admin');
$OPTION_PATHS['admin_setup_theme']       = array('title' => 'Appearance theme preferences',
						 'parent' => 'admin');
$OPTION_PATHS['admin_setup_tone']        = array('title' => 'Tone preferences',
						 'parent' => 'admin');
$OPTION_PATHS['admin_user']              = array('title' => 'User Manager',
						 'parent' => 'admin');
$OPTION_PATHS['admin_info']              = array('title' => 'System information',
						 'parent' => 'admin');
$OPTION_PATHS['search_index']            = array('title' => 'Search index',
						 'parent' => 'admin');
$OPTION_PATHS['admin_manual']            = array('title' => 'Generate manual pages',
						 'parent' => 'admin');
$OPTION_PATHS['admin_import_otherwiki']  = array('title' => 'Import pages from other wiki data',
						 'parent' => 'admin');
$OPTION_PATHS['admin_analyze']           = array('title' => 'Access analyzer',
						 'parent' => 'admin');
/* 権限設定はユーザーの管理から入る */
$OPTION_PATHS['admin_permissions']       = array('title' => 'Permission settings',
						 'parent' => 'admin_user');

function admin_show(&$args, $dom) {
    title_set_l('Admin tools');

    $tools = array();

    $tools = array(
	array('name' => l('Site preferences'),
	      'uri' => url_make('', array('option' => 'admin_setup_site'))),
	array('name' => l('Appearance theme preferences'),
	      'uri' => url_make('', array('option' => 'admin_setup_theme'))),
	array('name' => l('Tone preferences'),
	      'uri' => url_make('', array('option' => 'admin_setup_tone'))),
	array('name' => l('User Manager'),
	      'uri' => url_make('', array('option' => 'admin_user'))),
	array('name' => l('System information'),
	      'uri' => url_make('', array('option' => 'admin_info'))),
	array('name' => l('Rebuild search index'),
	      'uri' => url_make('', array('option' => 'search_index'))),
	array('name' => l('Generate manual pages'),
	      'uri' => url_make('', array('option' => 'admin_manual'))),
	//array('name' => l('Make user setting'),
	//    'uri' => url_make('', array('option' => 'admin_user_client')))
	array('name' => l('Import pages from other wiki data'),
	      'uri' => url_make('', array('option' => 'admin_import_otherwiki'))),
	array('name' => l('Access analyzer'),
	      'uri' => url_make('', array('option' => 'admin_analyze'))),
	);

    $ul = dom_append_child($dom, 'ul');
    foreach($tools as &$tool) {
	tools_add_link($ul, $tool);
    }
}
?>
