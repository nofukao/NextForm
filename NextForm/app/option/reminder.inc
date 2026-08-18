<?php
$OPTIONS['reminder'] = array(
    'show' => 'reminder_show',
    'write' => 'reminder_write',
    );

function reminder_show(&$args, $dom) {
    title_set_l('Reset password');
    page_stop_processing();

    message_info(l('Send email to reset password if you input correct user name and email address.'));

    $form = dom_append_form($dom, 
			    array(
				'option' => 'reminder',
				'action' => 'write',
				'back' => default_value($args['back'], '')), '', 'POST');
    $dl = dom_append_child($form, 'dl', 'form');

    $dt = dom_append_child($dl, 'dt');
    dom_append_text($dt, l('User name'));
    $dd = dom_append_child($dl, 'dd');
    dom_append_child($dd, 'input', '', array('type' => 'text', 'name' => 'user'));

    $dt = dom_append_child($dl, 'dt');
    dom_append_text($dt, l('Email address'));
    $dd = dom_append_child($dl, 'dd');
    dom_append_child($dd, 'input', '', array('type' => 'email', 'name' => 'email'));

    $button = dom_append_child($form, 'input', '', array('type' => 'submit', 'value' => l('Send')));
}

function reminder_write(&$args, $dom) {
    global $AUTH_DIGESTS;
    title_set_l('Reset password');
    page_stop_processing();

    if(!isset($args['user']) || !auth_is_valid_username($args['user']) ||
       !isset($args['email']) || !email_is_valid($args['email'])) {
	message_error(l('Invalid parameter.'));
	return false;
    }

    $email = setup_read('user_email_' . $args['user']);
    if($email !== false && $email === $args['email'] && !isset($AUTH_DIGESTS[$args['user']])) {
	$limit = time() + 60 * 60;
	$reminder_key = md5('reminder_' . $args['user'] . '_' . $limit . rand());
	$reminder_ticket = array(
	    'limit' => $limit,
	    'user' => $args['user'],
	    );
	$reminder_ticket = serialize($reminder_ticket);
	cache_delete('', 'reminder_ticket_' . $args['user'] . '_');
	if(cache_create('', 'reminder_ticket_' . $args['user'] . '_' . $reminder_key, $reminder_ticket) === false) {
	    message_error(l('Cannot create reminder data.'));
	    return false;
	}

	$subject = lf('Password reset was requested on $sitename$', array('sitename' => SITE_NAME));
	if(PASSWORD_REMINDER_EMAIL_FROM !== '') {
	    $from = PASSWORD_REMINDER_EMAIL_FROM;
	} else  {
	    $from = 'nextform-reminder-noreply';
	    if(isset($_SERVER['HTTP_HOST']))
		$from = $from . '@' . $_SERVER['HTTP_HOST']; 
	}
	$headers = 'From: ' . $from;

	$url = url_get_site_url() . '?' .
	    url_make_query(array(
			       'option' => 'password',
			       'user' => $args['user'],
			       'rkey' => $reminder_key,
			       ));
	$body = $subject . "\n\n" .
	    l('User name') . ': ' . $args['user'] . "\n" .
	    "\n" . 
	    l('Click this URL and reset your password.') . "\n" .
	    $url . "\n";
	
	if(function_exists('mb_send_mail')) {
	    if(LANG === 'ja')
		mb_language(LANG);
	    mb_send_mail($email, $subject, $body, $headers);
	} else {
	    mail($email, $subject, $body, $headers);
	}
	message_notice(l('A mail has just been sent.'));
    } else {
	message_error(l('Invalid user name or email address.'));
    }


    return false;
}
?>