<?php
$OPTIONS['email'] = array(
    'show' => 'email_show',
    'write' => 'email_write',
    );

$OPTION_PATHS['email'] = array('title' => 'Email address settings',
			       'parent_function' => 'email_title_parent');

/* 自分の分しか扱わない画面なので、ログインしていれば親は個人設定 */
function email_title_parent($args) {
    return auth_get_user() === false ? false : 'config';
}

function email_show(&$args, $dom) {
    title_set_l('Email address settings');
    $user = auth_get_user();
    if($user === false)
	return false;
    $target_username = $user['name'];

    title_set_lf('Email address settings for $user$', array('user' => $target_username));

    $email = setup_read('user_email_' . $target_username);
    if($email === false || $email === '')
	$email = l('not set');

    message_info(l('Email address will be used for password reminder. Blank input allowed.'));

    $form = dom_append_form($dom, 
			    array(
				'option' => 'email',
				'action' => 'write',
				'back' => default_value($args['back'], ''),
				'user' => $target_username), '', 'POST');
    $dl = dom_append_child($form, 'dl', 'form');

    if(empty($args['first'])) {
	$dt = dom_append_child($dl, 'dt');
	dom_append_text($dt, l('Current email address'));
	$dd = dom_append_child($dl, 'dd');
	dom_append_text($dd, $email);
    }

    $dt = dom_append_child($dl, 'dt');
    dom_append_text($dt, empty($args['first']) ? l('New email address') : l('Email address'));
    
    $dd = dom_append_child($dl, 'dd');
    dom_append_child($dd, 'input', '', array('type' => 'email', 'name' => 'email'));

    $button = dom_append_child($form, 'input', '', array('type' => 'submit', 'value' => l('Enter')));
}

function email_write(&$args, $dom) {
    title_set_l('Email address settings');
    if(!isset($args['user'])) {
	message_error(l('Invalid parameter.'));
	return false;
    }
    $user = auth_get_user();
    if($user === false || $user['name'] !== $args['user']) {
	message_error(l('Invalid parameter.'));
	return false;
    }
    $target_username = $args['user'];

    title_set_lf('Email address settings for $user$', array('user' => $target_username));

    if(isset($args['email']) && $args['email'] === '') {
	if(setup_read('user_email_' . $target_username) === false || setup_delete('user_email_' . $target_username)) {
	    message_notice(lf('$user$ email address successfully deleted.', array('user' => $target_username)));
	} else {
	    message_error(lf('Cannot delete $user$ email address.', array('user' => $target_username)));
	}
    } else {
	if(!isset($args['email']) || !email_is_valid($args['email'])) {
	    message_error(lf('Invalid $term$.', array('term' => l('email address'))));
	    return false;
	}

	if(setup_write('user_email_' . $target_username, $args['email'])) {
	    message_notice(lf('$user$ email address successfully changed.', array('user' => $target_username)));
	} else {
	    message_error(lf('Cannot change $user$ email address.', array('user' => $target_username)));
	}
    }
    if(isset($args['back']) && $args['back'] !== '') {
	dom_append_link($dom, $args['back'], l('Return'));
    }

    return false;
}
?>