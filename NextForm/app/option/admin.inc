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

function admin_show(&$args, $dom) {
    title_set_l('Admin tools');

    $tools = array();

    $tools = array(
	array('name' => l('Site preferences'),
	      'uri' => url_make('', array('option' => 'admin_setup_site'))),
	array('name' => l('Appearance theme preferences'),
	      'uri' => url_make('', array('option' => 'admin_setup_theme'))),
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
