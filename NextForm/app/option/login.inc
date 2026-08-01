<?php
$TOOLS['user']['login'] = array(
    'order' => 10,
    'show' => 'login_tool'
    );

function login_tool(&$page, $dom) {
    if(($user = auth_get_user()) !== false) {
	$li = dom_append_child($dom, 'li');
	dom_append_text($li, lf('User: $user$', array('user' => $user['name'])));
	if($user['method'] == 'self') {
	    $li = dom_append_child($dom, 'li');
	    $a = dom_append_link($li, url_make('', array('option' => 'logout')),
			    l('Logout'), 'logout');
	    dom_set_attribute($a, 'rel', 'nofollow');
	}
    } else {
	$li = dom_append_child($dom, 'li');
	$a = dom_append_link($li, url_make('', array('option' => 'login')),
			     l('Login'));
	dom_set_attribute($a, 'rel', 'nofollow');
    }
}

$OPTIONS['login'] = array('show' => 'login_show');
$OPTIONS['logout'] = array('show' => 'logout_show');

function login_show(&$args, $dom) {
    $user = auth_get_user();
    if($user === false) {
	auth_digest_auth();
    } else {
	setcookie('login', '1');
	header('Location: ' . url_make());
	exit(0);
    }
}

function logout_show(&$args, $dom) {
    title_set_l('Logout');
    page_stop_processing();
    $user = auth_get_user();
    if($user === false) {
	setcookie('login', '');
	message_notice(l('You are now logged out.'));
    } else
	message_error(l('Your browser does not support logout function. Plese close the browser or remove password.'));
    return false;
}
?>
