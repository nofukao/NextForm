<?php
$OPTIONS['admin_permissions'] = array(
    'show' => 'admin_permissions_show',
    'write' => 'admin_permissions_write',
    );

function admin_permissions_show(&$args, $dom) {
    global $AUTH_PERMISSIONS;
    auth_check(false, 'admin');
    title_set_l('Permission settings');

    if(!(isset($args['user']) && (auth_is_valid_username($args['user']) || $args['user'] === AUTH_GUEST_USERNAME))) {
	message_error(l('Invalid parameter.'));
	return false;
    }
    $target_username = $args['user'];

    title_set_lf('Permission settings for $user$',
		 array('user' => $args['user'] === AUTH_GUEST_USERNAME ? l('User not logged in') : $target_username));

    $noscript = dom_append_child($dom, 'noscript');
    dom_append_text($noscript, l('Turn JavaScript on.'));

    $form = dom_append_form($dom, 
			    array(
				'option' => 'admin_permissions',
				'action' => 'write',
				'redirect' => default_value($args['redirect'], ''),
				'user' => $target_username), 'permissions', 'POST');
    dom_set_attribute($form, 'id', 'permissions_form');

    $button = dom_append_child($form, 'button', '', array('type' => 'button', 'id' => 'submit_button'));
    dom_append_text($button, l('Apply'));

    $ul = dom_append_child($form, 'ul', 'comment');
    $li = dom_append_child($ul, 'li');
    dom_append_text($li, l('You can use * or ? wildcards in the page name.'));
    $li = dom_append_child($ul, 'li');
    dom_append_text($li, l('e.g. SomePage/* means child and descendant sub pages of SomePage (without SomePage itself).'));
    $li = dom_append_child($ul, 'li');
    dom_append_text($li, l('Rules are evaluated from top to bottom.'));

    $user_permissions = $AUTH_PERMISSIONS;
    $target_user_permissions = array(array('*', 'keepout'));
    if(isset($user_permissions[$target_username])) {
	if(is_string($user_permissions[$target_username]))
	    $target_user_permissions = array(array('*', $user_permissions[$target_username]));
	else if(!empty($user_permissions[$target_username])) {
	    $target_user_permissions = array();
	    foreach($user_permissions[$target_username] as $pattern => $permission) {
		$target_user_permissions[] = array($pattern, $permission);
	    }
	}
    }

    $options = hash_to_pairs(auth_get_permission_options());
    dom_append_text(head_tags_add_javascript(), 
		    'var targetUserPermissions=' . json_encode($target_user_permissions) . ';' .
		    'var permissionOptions=' . json_encode($options) . ';');
    head_tags_add_javascript('resource/script/permissions.js');
}

function admin_permissions_write(&$args, $dom) {
    global $AUTH_PERMISSIONS;
    global $AUTH_PERMISSIONS_ORIGINAL;

    auth_check(false, 'admin');
    title_set_l('Permission settings');

    if(!(isset($args['user']) && (auth_is_valid_username($args['user']) || $args['user'] === AUTH_GUEST_USERNAME)) ||
       !isset($args['permissions']) || ($arg_permissions = json_decode($args['permissions'])) === NULL ||
       !is_array($arg_permissions)) {
	message_error(l('Invalid parameter.'));
	return false;
    }
    
    $target_username = $args['user'];

    title_set_lf('Permission settings for $user$',
		 array('user' => $args['user'] === AUTH_GUEST_USERNAME ? l('User not logged in') : $target_username));

    $permissions = array();
    foreach($arg_permissions as $permission) {
	if(is_array($permission) && count($permission) == 2 &&
	   is_string($permission[0]) && $permission[0] !== '' &&
	   is_string($permission[1]) && $permission[1] !== '')
	    $permissions[$permission[0]] = $permission[1];
    }
    if(count($permissions) === 1) {
	$keys = array_keys($permissions);
	if($keys[0] === '*')
	    $permissions = $permissions['*'];
    }
    if(empty($permissions))
	$permissions = 'keepout';
    
    $user_permissions = $AUTH_PERMISSIONS;
    $user_permissions[$target_username] = $permissions;

    if(!(auth_exists_admin($AUTH_PERMISSIONS_ORIGINAL) || auth_exists_admin($user_permissions))) {
	message_error(l('Can not delete all administrators.'));
	return false;
    }

    if(!auth_save_permissions($user_permissions)) {
	message_error(l('Could not save user preferences.'));
	return false;
    }

    if(!empty($args['redirect'])) {
	header('Location: ' . $args['redirect']);
	exit(0);
    }

    return false;
}


?>