<?php
$OPTIONS['admin_user'] = array(
    'show' => 'admin_user_show',
    );

function admin_user_show(&$args, $dom) {
    global $AUTH_DIGESTS;
    global $AUTH_PERMISSIONS;
    global $AUTH_PERMISSIONS_ORIGINAL;

    auth_check(false, 'admin');
    title_set_l('User Manager');

    $user_permissions = $AUTH_PERMISSIONS;
    uksort($user_permissions, 'admin_user_compare');

    $options = auth_get_permission_options();

    if(isset($args['apply']) && $args['apply'] === 'true') {
	$is_success = true;
	$index = 0;
	$delete_usernames = array();
	foreach($user_permissions as $username => &$permission) {
	    if((string)$username === AUTH_GUEST_USERNAME)
		continue;

	    if(isset($args['username_' . $index]) && (string)$username === $args['username_' . $index]) {
		if(isset($args['delete_' . $index]) && $args['delete_' . $index] === 'on') {
		    $delete_usernames[] = $username;
		} else if(isset($args['permission_' . $index]) && isset($options[$args['permission_' . $index]]))
		    $permission = $args['permission_' . $index];
	    }
	    $index++;
	}
	foreach($delete_usernames as $username)
	    unset($user_permissions[$username]);
	if(isset($args['username_new']) && $args['username_new'] !== '') {
	    if(isset($user_permissions[$args['username_new']])) {
		message_error(lf('User $user$ already exists.', array('user' => $args['username_new'])));
		$is_success = false;
	    } else if(!auth_is_valid_username($args['username_new'])) {
		message_error(l('Being usable for a user name only as for the alphanumeric character.'));
		$is_success = false;
	    } else {
		$user_permissions[$args['username_new']] = default_value($args['permission_new'], 'read');
	    }
	}

	if(isset($args['permission_guest']) && isset($options[$args['permission_guest']])) {
	    $user_permissions[AUTH_GUEST_USERNAME] = $args['permission_guest'];
	}

	if(!(auth_exists_admin($AUTH_PERMISSIONS_ORIGINAL) || auth_exists_admin($user_permissions))) {
	    message_error(l('Can not delete all administrators.'));
	    $is_success = false;
	}
	
	if($is_success) {
	    if(auth_save_permissions($user_permissions)) {
		foreach($delete_usernames as $username) {
		    setup_delete('auth_digest_' . $username);
		}
	    } else {
		message_error(l('Could not save user preferences.'));
		$is_success = false;
	    }
	}

	if($is_success) {
	    header('Location: ' . url_make('', array('option' => 'admin_user')));
	    exit(0);
	} else {
	    $user_permissions = $AUTH_PERMISSIONS;
	}
    }

    uksort($user_permissions, 'admin_user_compare');

    $form = dom_append_form($dom, array('option' => 'admin_user', 'apply' => 'true'), '', 'POST');
    
    $table = dom_append_child($form, 'table', 'user nosort');
    $thead = dom_append_child($table, 'thead');
    $tbody = dom_append_child($table, 'tbody');

    $tr = dom_append_child($thead, 'tr');
    $th = dom_append_child($tr, 'th');
    dom_append_text($th, l('User name'));
    $th = dom_append_child($tr, 'th');
    dom_append_text($th, l('Password'));
    $th = dom_append_child($tr, 'th');
    dom_append_text($th, l('Permission'));
    $th = dom_append_child($tr, 'th');
    dom_append_text($th, l('Delete'));

    $tr = dom_append_child($tbody, 'tr');
    $td = dom_append_child($tr, 'td');
    dom_append_text($td, l('User not logged in'));

    $td = dom_append_child($tr, 'td');
    dom_append_text($td, '');

    $td = dom_append_child($tr, 'td');
    admin_user_append_permissions($td, 'guest', AUTH_GUEST_USERNAME, 
				  isset($user_permissions[AUTH_GUEST_USERNAME]) ?
				  $user_permissions[AUTH_GUEST_USERNAME] : 'keepout', $options);

    $td = dom_append_child($tr, 'td');
    dom_append_text($td, '');

    if(EXPORT_ENABLE !== 'true')
	unset($options['export']);

    $index = 0;
    foreach($user_permissions as $username => &$permission) {
	if((string)$username === AUTH_GUEST_USERNAME)
	    continue;
	$tr = dom_append_child($tbody, 'tr');
	$td = dom_append_child($tr, 'td');
	dom_append_text($td, $username);
	
	$td = dom_append_child($td, 'input', '', array(
				   'type' => 'hidden', 'name' => 'username_' . $index, 'value' => $username));

	$td = dom_append_child($tr, 'td');
	if(isset($AUTH_DIGESTS[$username]) || !password_has_change_permission($username)) {
	    dom_append_text($td, l('Configured'));
	} else {
	    $linkname = l('Change');
	    if(!auth_user_has_password($username)) {
		$linkname = l('Not set');
		dom_append_class($td, 'error');
	    }
	    dom_append_link(
		$td, url_make(
		    '', array(
			'option' => 'password',
			'user' => $username,
			'back' => url_make('', array('option' => 'admin_user')))),
		$linkname);
	}

	$td = dom_append_child($tr, 'td');
	admin_user_append_permissions($td, $index, $username, $permission, $options);

	$td = dom_append_child($tr, 'td');
	if(!isset($AUTH_PERMISSIONS_ORIGINAL[$username])) {
	    $label = dom_append_child($td, 'label');
	    $check = dom_append_child($label, 'input', '',
				      array(
					  'type' => 'checkbox',
					  'name' => 'delete_' . $index,
					  'value' => 'on',
					  ));
	    dom_append_text($label, l('Delete'));
	}
	$index++;
    }

    $tr = dom_append_child($tbody, 'tr');
    $td = dom_append_child($tr, 'td');
    dom_append_child($td, 'input', '', array(
			 'type' => 'text',
			 'name' => 'username_new',
			 'placeholder' => l('New user'),
			 ));

    $td = dom_append_child($tr, 'td');
    dom_append_text($td, '');
    
    $td = dom_append_child($tr, 'td');
    $select = dom_append_selector($td, 'permission_new', $options, 'read');
    dom_set_attribute($select, 'id', 'admin_user_perm');

    $td = dom_append_child($tr, 'td');
    dom_append_text($td, '');

    $submit = dom_append_child($form, 'input', '', array('type' => 'submit', 'value' => l('Apply')));

    dom_append_text(head_tags_add_javascript(), 'var permissionsIsAdminUser=true');
    head_tags_add_javascript('resource/script/permissions.js');
}

function admin_user_append_permissions($td, $index, $username, $permission, $options) {
    global $AUTH_PERMISSIONS_ORIGINAL;

    if(isset($AUTH_PERMISSIONS_ORIGINAL[$username])) {
	if(is_string($AUTH_PERMISSIONS_ORIGINAL[$username]) &&
	   isset($options[$AUTH_PERMISSIONS_ORIGINAL[$username]]))
	    dom_append_text($td, $options[$AUTH_PERMISSIONS_ORIGINAL[$username]]);
	else
	    dom_append_text($td, l('Configured'));
    } else {
	$permission_url = url_make('', array(
				       'option' => 'admin_permissions',
				       'user' => $username,
				       'redirect' => url_make('', array('option' => 'admin_user'))));
	if(is_string($permission)) {
	    $select = dom_append_selector($td, 'permission_' . $index, $options, $permission);
	    dom_set_attribute($select, 'data-url', $permission_url);
	} else {
	    dom_append_link($td, $permission_url, l('Settings for each page'), 'button');
	}
    }
}

function admin_user_compare($a, $b) {
    return strcmp((string)$a, (string)$b);
}

?>
