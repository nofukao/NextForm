<?php
$TOOLS['site']['config'] = array(
    'order' => 900,
    'show' => 'config_tool'
    );

function config_tool(&$page, $dom) {
    tools_add_link(
	$dom,
	array('name' => l('Preferences'),
	      'uri' => url_make('', array('option' => 'config'))));
}

$OPTIONS['config'] = array(
    'show' => 'config_show',
    'write' => 'config_write',
    );

function config_show(&$args, $dom) {
    title_set_l('Preferences');
    $section = dom_append_child($dom, 'section', 'config');
    dom_append_text($section, '');

    $user = auth_get_user();
    if($user !== false && password_has_change_permission($user['name'])) {
	$account_config_section = dom_append_child($section, 'section', 'section account_config');
	$h = dom_append_child($account_config_section, 'h1');
	dom_append_text($h, l('Account preferences'));

	$ul = dom_append_child($account_config_section, 'ul');
	$li = dom_append_child($ul, 'li');
	dom_append_link(
	    $li, 
	    url_make('',
		     array('option' => 'password',
			   'back' => url_make('', array('option' => 'config')))),
	    l('Password settings'));
	$li = dom_append_child($ul, 'li');
	dom_append_link(
	    $li, 
	    url_make('',
		     array('option' => 'email',
			   'back' => url_make('', array('option' => 'config')))),
	    l('Email address settings'));
    }

    if(USER_HISTORY_ENABLE == 'true') {
	$user_history_section = dom_append_child($section, 'section', 'section user_history_config');
	$h = dom_append_child($user_history_section, 'h1');
	dom_append_text($h, l('Browsing history'));
	
	$form = dom_append_form($user_history_section,
				array('option' => 'config',
				      'action' => 'write',
				      'mode' => 'user_history_clear',
				    ), 'user_history');
	$submit = dom_append_child($form, 'input', '',
				   array('type' => 'submit',
					 'value' => l('Clear browsing history')));

	$form = dom_append_form($user_history_section,
				array('option' => 'config',
				      'action' => 'write',
				      'mode' => 'user_history_basetime',
				    ), 'user_history');
	$submit = dom_append_child($form, 'input', '',
				   array('type' => 'submit',
					 'value' => l('Mark all pages as read')));
	
    }

    return false;
}

function config_write(&$args, $dom) {
    if(!empty($args['mode'])) {
	if(USER_HISTORY_ENABLE == 'true') {
	    if($args['mode'] == 'user_history_basetime') {
		$dummy = false;
		user_history_load($dummy);
		user_history_set_basetime(time());
		user_history_save();
	    } else if($args['mode'] == 'user_history_clear') {
		user_history_clear();
	    }
	}
    }
    
    header('Location: ' .  url_make('', array('option' => 'config')));
    exit(0);
    
    return false;
}

?>
