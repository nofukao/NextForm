<?php
define('AUTH_DIGEST_REALM', 'User login');
define('AUTH_GUEST_USERNAME', '$default$');

$AUTH_PRESET_PERMISSIONS = array(
    'admin'   => array('read' => true,  'insert' => true,  'write' => true,  'lock' => true,  'export' => true,  'admin' => true),
    'export'  => array('read' => true,  'insert' => true,  'write' => true,  'lock' => true,  'export' => true,  'admin' => false),
    'lock'    => array('read' => true,  'insert' => true,  'write' => true,  'lock' => true,  'export' => false, 'admin' => false),
    'write'   => array('read' => true,  'insert' => true,  'write' => true,  'lock' => false, 'export' => false, 'admin' => false),
    'insert'  => array('read' => true,  'insert' => true,  'write' => false, 'lock' => false, 'export' => false, 'admin' => false),
    'read'    => array('read' => true,  'insert' => false, 'write' => false, 'lock' => false, 'export' => false, 'admin' => false),
    'keepout' => array('read' => false, 'insert' => false, 'write' => false, 'lock' => false, 'export' => false, 'admin' => false));

function auth_digest_parse($txt) {
    $needed_parts = array('nonce'=>1, 'nc'=>1, 'cnonce'=>1, 'qop'=>1, 'username'=>1, 'uri'=>1, 'response'=>1);
    $data = array();
    $keys = implode('|', array_keys($needed_parts));
    preg_match_all('@(' . $keys . ')=(?:([\'"])([^\2]+?)\2|([^\s,]+))@', $txt, $matches, PREG_SET_ORDER);
    foreach($matches as $m) {
        $data[$m[1]] = $m[3] !== '' ? $m[3] : $m[4];
        unset($needed_parts[$m[1]]);
    }
    return $needed_parts ? false : $data;
}

function auth_digest_get_digest($username) {
    global $AUTH_DIGESTS;
    $digest = false;
    if(isset($AUTH_DIGESTS[$username]))
	return $AUTH_DIGESTS[$username];
    else
	$digest = setup_read('auth_digest_' . $username);
    return $digest;
}

function auth_user_has_password($username) {
    return auth_digest_get_digest($username) !== false || 
	(($user = auth_get_user()) !== false && $user['method'] === 'other');
}

function auth_digest_get_user() {
    if(empty($_SERVER['PHP_AUTH_DIGEST'])) {
	if(isset($_COOKIE['login']) && $_COOKIE['login'] == '1')
	    auth_digest_auth();
	return false;
    }
    if(!($data = auth_digest_parse($_SERVER['PHP_AUTH_DIGEST'])))
	return false;

    $digest = auth_digest_get_digest($data['username']);
    if($digest === false)
	return false;

    $a1 = $digest;
    $a2 = md5($_SERVER['REQUEST_METHOD'].':'.$data['uri']);
    $valid_response = md5($a1.':'.$data['nonce'].':'.$data['nc'].':'.$data['cnonce'].':'.$data['qop'].':'.$a2);

    if($data['response'] != $valid_response)
	return false;

    return array('name' => $data['username'], 'method' => 'self');
}

function auth_digest_auth() {
    setcookie('logout', '');
    header('HTTP/1.1 401 Unauthorized');
    header('WWW-Authenticate: Digest realm="' . AUTH_DIGEST_REALM .
	   '",qop="auth",nonce="' . uniqid() . '",opaque="' . md5(AUTH_DIGEST_REALM) . '"');

    $p = dom_create('p');
    dom_append_link($p, url_make('', array('option' => 'reminder')), l('Reset password'));
    auth_simple_message(l('User authentication failed.'), $p);
    exit(1);
}

function auth_other_get_user() {
    if(isset($_SERVER['PHP_AUTH_USER']))
	return array('name' => $_SERVER['PHP_AUTH_USER'], 'method' => 'other');
    return false;
}

$AUTH_GET_USER_CACHE = null;
function auth_get_user() {
    global $AUTH_GET_USER_CACHE;
    if(!is_null($AUTH_GET_USER_CACHE))
	return $AUTH_GET_USER_CACHE;
    if(isset($_COOKIE['logout']) && $_COOKIE['logout'] == '1')
	return ($AUTH_GET_USER_CACHE = false);
    $AUTH_GET_USER_CACHE = auth_other_get_user();
    if($AUTH_GET_USER_CACHE === false)
	$AUTH_GET_USER_CACHE = auth_digest_get_user();
    return $AUTH_GET_USER_CACHE;
}

function auth_permission_can_action($permission, $pagename, $action) {
    global $AUTH_PRESET_PERMISSIONS;
    if(is_string($permission)) {
	$permission = 
	    array('*' => &$AUTH_PRESET_PERMISSIONS[$permission]);
    }
    if($pagename === false) {
	if(empty($permission))
	    return false;
	foreach($permission as $pagepattern => $actions) {
	    if(is_string($actions) && isset($AUTH_PRESET_PERMISSIONS[$actions]))
		$actions = $AUTH_PRESET_PERMISSIONS[$actions];
	    if(empty($actions[$action]))
		return false;
	}
	if(!isset($permission['*']))
	    return false;
	return true;
    } else {
	foreach($permission as $pagepattern => $actions) {
	    if(is_string($actions) && isset($AUTH_PRESET_PERMISSIONS[$actions]))
		$actions = $AUTH_PRESET_PERMISSIONS[$actions];
	    if(fnmatch($pagepattern, $pagename)) {
		return !empty($actions[$action]);
	    }
	}
    }
    return false;
}

function auth_get_permission() {
    global $AUTH_PERMISSIONS;
    $user = auth_get_user();
    if($user === false || !isset($AUTH_PERMISSIONS[$user['name']]))
	$username = AUTH_GUEST_USERNAME;
    else
	$username = $user['name'];
    if(!isset($AUTH_PERMISSIONS[$username]))
	return false;
    return $AUTH_PERMISSIONS[$username];
}

function auth_has_permission($pagename, $action) {
    $permission = auth_get_permission();
    if($permission === false)
	return false;
    return auth_permission_can_action($permission, $pagename, $action);
}

function auth_check($pagename, $action) {
    if(auth_has_permission($pagename, $action)) {
	return true;
    } else if(($user = auth_get_user()) !== false) {
	header('HTTP/1.1 403 Forbidden');
	auth_simple_message(
		 lf('User \'$user$\' has no permission to perform the requested action.',
		    array('user' => $user['name'])));

	if(DEBUG_MODE == 'true')
	    debug_log_backtrace('auth_check failure');

	exit(1);
    } else {
	auth_digest_auth();
    }
}

function auth_warn($pagename, $action, $message = '') {
    if(!auth_has_permission($pagename, $action)) {
	if(empty($message)) {
	    $dom = dom_create_document();
	    $a = dom_append_link($dom, url_make('', array('option' => 'login')), l('Login'));
	    ld(message_error(), 'You have no permission to perform this action. (Please $login$)',
	       array('login' => $a));
	} else {
	    message_error($message);
	}
	return false;
    }
    return true;
}

function auth_simple_message($message, $dom = false) {
    global $main_page_output;
    theme_simple_screen_init();
    title_set_l('User authentication');
    message_error($message);
    if(!empty($dom))
	$main_page_output = dom_save_html($dom);
    theme_simple_screen();
}

function auth_is_valid_username($username) {
    return preg_match('/^[-_0-9a-zA-Z]+$/', $username);
}

function auth_exists_admin($permissions) {
    foreach($permissions as $username => &$permission) {
	if(auth_permission_can_action($permission, false, 'admin') && auth_user_has_password($username))
	    return true;
    }
    return false;
}

function auth_get_permission_options() {
    return array(
	'keepout' => l('keep out'),
	'read' => l('read only'),
	'insert' => l('text insertion available'),
	'write' => l('page write available'),
	'lock' => l('page lock available'),
	'export' => l('export available'),
	'admin' => l('administrator'),
	);
}

function auth_save_permissions($target_permissions = false) {
    global $AUTH_PERMISSIONS;
    if($target_permissions === false)
	$target_permissions = $AUTH_PERMISSIONS;
    $contents = serialize($target_permissions);
    if(setup_write('auth_permissions', $contents)) {
	return true;
    } else {
	message_error(l('Could not save user preferences.'));
	return false;
    }
}

function auth_setup() {
    global $AUTH_DIGESTS;
    global $AUTH_PERMISSIONS;
    global $AUTH_PERMISSIONS_ORIGINAL;

    $AUTH_PERMISSIONS_ORIGINAL = $AUTH_PERMISSIONS;

    $user_permissions = setup_read('auth_permissions');
    if($user_permissions === false)
	$user_permissions = array();
    else    
	$user_permissions = unserialize($user_permissions);
    foreach($user_permissions as $username => $permission) {
	if(!isset($AUTH_PERMISSIONS[$username]))
	    $AUTH_PERMISSIONS[$username] = $permission;
    }
}

?>
