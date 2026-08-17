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

/*
 * ダイジェスト認証の nonce。
 *
 * 上流はクライアントが送ってきた nonce をそのまま応答の計算に使っており、
 * **自分が発行したものかを一度も確かめていなかった**。nc (再送カウンタ) も
 * 見ておらず、期限も無い。そのため Authorization ヘッダを 1 度捕まえれば、
 * 同じメソッドと URI に対して無期限に再利用できた。
 *
 * 発行した nonce を storage/cache に控え、期限と nc の進みを見る。
 * 期限切れや使い回しには 401 に stale=true を付けて返す。こうすると
 * ブラウザは利用者に問い合わせずに認証をやり直すので、画面は変わらない。
 */
define('AUTH_NONCE_KEY_PREFIX', 'auth_nonce_');
define('AUTH_NONCE_LIFETIME', 60 * 60);
define('AUTH_NONCE_SECRET_NAME', 'auth_nonce_secret');

/*
 * nonce の署名鍵。無ければ作る。
 *
 * 保存できないサイトでは鍵が毎回変わり、発行した nonce を自分で
 * 「知らない」と言い続けて 401 を繰り返す。ログインできない wiki に
 * するより、検査を諦めて上流と同じ挙動に戻す方がましなので、
 * 書けなかったことを記録したうえで false を返す。
 */
function auth_nonce_secret() {
    $secret = setup_read(AUTH_NONCE_SECRET_NAME);
    if($secret !== false && strlen($secret) >= 64)
	return $secret;

    $secret = bin2hex(random_bytes(32));
    if(!setup_write(AUTH_NONCE_SECRET_NAME, $secret)) {
	error_log('cannot save the digest nonce secret; nonce check is disabled');
	return false;
    }
    return $secret;
}

/*
 * nonce を作る。**発行しただけでは何も保存しない。**
 *
 * 保存する作りにすると、認証していない相手に 401 を返すたびに
 * ファイルが 1 つ増える。ログインを必須にしたサイトは巡回ボットの
 * 相手をするだけで storage/cache が埋まってしまう。
 * 代わりに「発行時刻 + その署名」を nonce 自体に入れて、
 * 自分が出したものかどうかを保存なしで確かめられるようにする。
 */
function auth_nonce_create() {
    $secret = auth_nonce_secret();
    if($secret === false)
	return bin2hex(random_bytes(16));
    /*
     * 発行時刻だけで作ってはいけない。同じ秒に 401 を受けた別々の
     * クライアントが同じ nonce を持ってしまい、片方が使った nc を
     * もう片方が「戻っている」と判定されて弾かれる。乱数を混ぜて
     * 発行ごとに別のものにする。
     */
    $body = sprintf('%08x', time()) . bin2hex(random_bytes(8));
    return $body . hash_hmac('sha256', $body, $secret);
}

/*
 * 戻り値は 'ok' か 'stale'。
 *
 * 知らない nonce も期限切れも使い回しも、まとめて 'stale' にする。
 * stale は「資格情報は正しいが nonce をやり直せ」の意味なので、
 * パスワードを知らない相手にはやり直しようがない。区別して返すと、
 * どの nonce が生きているかを外から探れてしまう。
 */
function auth_nonce_check($nonce, $nc) {
    $secret = auth_nonce_secret();
    if($secret === false)
	return 'ok';

    if(!preg_match('/^([0-9a-f]{8}[0-9a-f]{16})([0-9a-f]{64})$/', (string)$nonce, $matches))
	return 'stale';

    if(!hash_equals(hash_hmac('sha256', $matches[1], $secret), $matches[2]))
	return 'stale';
    $issued = hexdec(substr($matches[1], 0, 8));
    /* 未来の時刻は時計のずれの範囲だけ許す */
    if($issued + AUTH_NONCE_LIFETIME < time() || $issued > time() + 60)
	return 'stale';

    /* nc は 8 桁までの 16 進。必ず前回より進んでいなければならない */
    if(!preg_match('/^[0-9a-fA-F]{1,8}$/', (string)$nc))
	return 'stale';
    $count = hexdec($nc);

    $cachekey = AUTH_NONCE_KEY_PREFIX . $nonce;
    $entry = cache_serialized_get('', $cachekey);
    if($entry === false || !isset($entry['nc'])) {
	/*
	 * この nonce で認証が通るのは初めて。ここに来られるのは
	 * パスワードを知っている相手だけなので、掃除の回数も実際の
	 * 利用者の数で頭打ちになる。
	 */
	auth_nonce_sweep();
	$last = 0;
    } else {
	$last = $entry['nc'];
    }
    if($count <= $last)
	return 'stale';

    /* cache_serialized_create() は第 3 引数を参照で取るので変数に入れる */
    $new_entry = array('nc' => $count, 'time' => time());
    cache_serialized_create('', $cachekey, $new_entry);
    return 'ok';
}

/* 期限の切れた nc の記録を捨てる */
function auth_nonce_sweep() {
    $limit = time() - AUTH_NONCE_LIFETIME;
    foreach(cache_get_keys_prefix('', AUTH_NONCE_KEY_PREFIX) as $cachekey) {
	$entry = cache_serialized_get('', $cachekey);
	if($entry === false || !isset($entry['time']) || $entry['time'] < $limit)
	    cache_delete('', $cachekey);
    }
}

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

    /* 突き合わせは定数時間で行う */
    if(!hash_equals($valid_response, (string)$data['response']))
	return false;

    /*
     * 応答が正しいことを確かめてから nonce を見る。
     *
     * 順序が大事で、パスワードが違う要求にまで 401 stale を返すと、
     * ブラウザが延々と再挑戦してしまう。ここに来るのは資格情報が
     * 合っている要求だけなので、nonce をやり直させれば済む。
     */
    if(auth_nonce_check($data['nonce'], $data['nc']) !== 'ok')
	auth_digest_auth(true);

    return array('name' => $data['username'], 'method' => 'self');
}

/*
 * $is_stale が true のときは「パスワードは合っているが nonce をやり直せ」の意味。
 * ブラウザは利用者に問い合わせずに認証をやり直す。
 */
function auth_digest_auth($is_stale = false) {
    setcookie('logout', '');
    header('HTTP/1.1 401 Unauthorized');
    header('WWW-Authenticate: Digest realm="' . AUTH_DIGEST_REALM .
	   '",qop="auth",nonce="' . auth_nonce_create() . '",opaque="' . md5(AUTH_DIGEST_REALM) . '"' .
	   ($is_stale ? ',stale=true' : ''));

    /*
     * stale のときは画面を出さない。ブラウザが黙って再挑戦するので、
     * 利用者が「認証に失敗しました」を見ることは無い。
     */
    if($is_stale)
	exit(1);

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
