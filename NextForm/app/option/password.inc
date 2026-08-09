<?php
$OPTIONS['password'] = array(
    'show' => 'password_show',
    'write' => 'password_write',
    );

function password_has_change_permission($target_username) {
    global $AUTH_DIGESTS;
    return ($user = auth_get_user()) !== false &&
	$user['method'] !== 'other' &&
	($target_username === $user['name'] || auth_has_permission(false, 'admin')) &&
	!isset($AUTH_DIGESTS[$target_username]);
}

function password_get_reminder_ticket($args) {
    if(!isset($args['rkey']) || !preg_match('/^[0-9a-f]{32}$/', $args['rkey']) ||
       !isset($args['user']) || !auth_is_valid_username($args['user']))
	return false;
    $reminder_ticket = cache_get('', 'reminder_ticket_' . $args['user'] . '_' . $args['rkey']);
    if($reminder_ticket === false)
	return false;
    return unserialize($reminder_ticket);
}

function password_check_reminder_ticket($reminder_ticket) {
    return isset($reminder_ticket['user']) && $reminder_ticket['user'] !== '' &&
	isset($reminder_ticket['limit']) && time() < $reminder_ticket['limit'];
}

function password_show(&$args, $dom) {
    title_set_l('Password settings');
    $user = auth_get_user();
    if(isset($args['user']) && auth_is_valid_username($args['user']))
	$target_username = $args['user'];
    else
	$target_username = $user === false ? '' : $user['name'];

    $reminder_ticket = password_get_reminder_ticket($args);
    if(!password_has_change_permission($target_username) && !password_check_reminder_ticket($reminder_ticket)) {
	message_error(lf('User \'$user$\' has no permission to perform the requested action.',
			 array('user' => $target_username)));
	return false;
    }

    title_set_lf('Password settings for $user$', array('user' => $target_username));

    $noscript = dom_append_child($dom, 'noscript');
    dom_append_text($noscript, l('Turn JavaScript on.'));

    $form = dom_append_form($dom, 
			    array(
				'option' => 'password',
				'action' => 'write',
				'back' => default_value($args['back'], ''),
				'user' => $target_username), '', 'POST');
    if(isset($args['rkey']))
	dom_append_input_hidden($form, array('rkey' => $args['rkey']));
    dom_set_attribute($form, 'id', 'password_form');
    
    $dl = dom_append_child($form, 'dl', 'form');

    $dt = dom_append_child($dl, 'dt');
    dom_append_text($dt, auth_user_has_password($target_username) ? l('New password') : l('Password'));
    $dd = dom_append_child($dl, 'dd');
    dom_append_child($dd, 'input', '',
		     array('type' => 'password', 'id' => 'password', 'autocomplete' => 'off'));

    $dt = dom_append_child($dl, 'dt');
    dom_append_text($dt, l('Verify password'));
    $dd = dom_append_child($dl, 'dd');
    dom_append_child($dd, 'input', '',
		     array('type' => 'password', 'id' => 'password_confirm', 'autocomplete' => 'off'));

    $button = dom_append_child($form, 'button', '', array('type' => 'button', 'id' => 'submit_button'));
    dom_append_text($button, l('Enter'));

    head_tags_add_javascript('resource/script/md5.js');
    dom_append_text(head_tags_add_javascript(), "var AUTH_DIGEST_REALM='" . AUTH_DIGEST_REALM . "';");
    head_tags_add_javascript('resource/script/password.js');
}

function password_write(&$args, $dom) {
    global $AUTH_DIGESTS;
    title_set_l('Password settings');
    if(!isset($args['user']) || !auth_is_valid_username($args['user'])) {
	message_error(l('Invalid parameter.'));
	return false;
    }
    $user = auth_get_user();
    $target_username = $args['user'];
    $reminder_ticket = password_get_reminder_ticket($args);
    if(!password_has_change_permission($target_username) && !password_check_reminder_ticket($reminder_ticket)) {
	message_error(lf('User \'$user$\' has no permission to perform the requested action.',
			 array('user' => $user['name'])));
	return false;
    }

    title_set_lf('Password settings for $user$', array('user' => $target_username));

    if(isset($args['digest']) && 
       preg_match('/^[0-9a-f]{32}$/', $args['digest']) &&
       setup_write('auth_digest_' . $target_username, $args['digest'])) {
	message_notice(lf('$user$ password successfully changed.', array('user' => $target_username)));

	cache_delete('', 'reminder_ticket_' . $target_username . '_');

	if($user['name'] === $target_username || !empty($reminder_ticket)) {
	    $adom = dom_create_document();
	    $a = dom_append_link($adom, url_make('', array('option' => 'login')), l('re login'));
	    ld(message_notice(), 'Please $action$.', array('action' => $a));
	}
	
    } else {
	message_error(lf('Cannot change $user$ password.', array('user' => $target_username)));
    }

    if(isset($args['back']) && $args['back'] !== '') {
	dom_append_link($dom, $args['back'], l('Return'));
    }

    return false;
}


?>