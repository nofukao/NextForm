<?php
function lock_has_permission(&$page) {
    $username = '';
    if(($user = auth_get_user()) !== false)
	$username = $user['name'];
    return auth_has_permission($page['name'], 'admin') ||
	(auth_has_permission($page['name'], 'lock') &&
	 (!isset($page['meta']['lock_user']) || $page['meta']['lock_user'] === $username));
}

function lock_warn(&$page, $message = '') {
    if(!empty($page['meta']['lock'])) {
	if(lock_has_permission($page)) {
	    message_notice(l('This page was locked.'));
	    return true;
	}

	if(isset($page['meta']['lock_user'])) {
	    message_error(lf('This page was locked by $user$.', array('user' => $page['meta']['lock_user'])));
	} else {
	    message_error(l('This page was locked.'));
	}
	return false;
    }
    return true;
}

?>