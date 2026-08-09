<?php
define('APP_DIR_PATH', dirname(__FILE__));

$INSTALL_INFO = array();
$main_page_dom = null;
if(!defined('INSTALL_INFO_FILE_PATH'))
    define('INSTALL_INFO_FILE_PATH', 'install-info.dat');
define('SITE_NAME', 'ToraToraWiki Initial setup');
/*
 * ここで SIDE_PAGENAME を '' に上書きしてはいけない。
 *
 * install_theme() が同じプロセスで theme_convert() を呼んで静的 CSS を生成する。
 * SIDE_PAGENAME が '' だと .theme.css の optional layout ブロックが
 * article.main { margin-left: 0; width: 100% } を出力し、「サイドページ無し」の
 * レイアウトが焼き付く。あとから Side ページを作ってもサイドバーが横に並ばず、
 * 本文の下に回り込んでしまう。
 *
 * インストーラ画面でサイドページのリンクが出る心配は無い。インストール中は
 * $TOOLS が空で、main.inc の sideapge_tool() は一度も呼ばれない。
 */

function install_info_load() {
    global $INSTALL_INFO;
    $info = @file_get_contents(INSTALL_INFO_FILE_PATH);
    if($info === false)
	$INSTALL_INFO = array();
    else
	$INSTALL_INFO = unserialize($info);
}

function install_info_save() {
    global $INSTALL_INFO;
    $dir_path = dirname(INSTALL_INFO_FILE_PATH);
    if(!file_exists($dir_path))
	mkdir($dir_path, 0777, true);

    $INSTALL_INFO['SERVER'] = $_SERVER;
    
    if(!@file_put_contents(INSTALL_INFO_FILE_PATH, serialize($INSTALL_INFO))) {
	echo("Cannot write file " . INSTALL_INFO_FILE_PATH . ". Check directory permission.");
    }
}

function install_reload() {
    header('Location: ' . url_make());
    install_info_save();
    exit(0);
}

function install_check_ticket($args) {
    global $INSTALL_INFO;
    if(!isset($INSTALL_INFO['iticket'])) {
	$INSTALL_INFO['iticket'] = md5(__FILE__ . time() . rand());
	setcookie('iticket', $INSTALL_INFO['iticket']);
	return true;
    }

    $iticket = '';
    if(isset($_COOKIE['iticket']) && $_COOKIE['iticket'] !== '')
	$iticket = $_COOKIE['iticket'];
    if(isset($args['iticket']) && $args['iticket'] !== '') {
	$iticket = $args['iticket'];
	setcookie('iticket', $args['iticket']);
    }
    
    if($iticket !==  $INSTALL_INFO['iticket']) {
	echo('Enable cookies and refresh browser. / クッキーを有効にして再読み込みして下さい．'); 
	return false;
    }
    return true;
} 

function install_app_directory() {
    if(!defined('APP_DIR_PATH'))
	define('APP_DIR_PATH', './app');
    if(is_file(APP_DIR_PATH . '/boot.inc'))
	return true;
    header('Content-type: text/plain');
    echo("Directory '" . APP_DIR_PATH . "' is not ToraToraWiki app directory.\n\n");
    echo <<<EOT
Please insert the following text in index.php. 
---------------------------
define('APP_DIR_PATH', 'path/to/app/directory');
EOT;
    return false;
}

function install_theme($dom, $args) {
    $result_theme = @theme_convert(THEME);
    $result_htaccess_contents = <<<EOT
<IfVersion >= 2.3>
    Require all granted
</IfVersion>

<IfVersion < 2.3>
    Allow from all
</IfVersion>

EOT;
    $result_htaccess = @file_put_contents(THEME_DIR_PATH . '/.htaccess', $result_htaccess_contents);
    if($result_theme && $result_htaccess)
	return true;
    header('Content-type: text/plain');
    echo(l('Could not apply theme preferences.') ."\n");
    echo(lf('Check directory \'$dir$\' permission.', array('dir' => THEME_DIR_PATH)) ."\n");
    return false;
}

function install_resource($dom, $args) {
    global $INSTALL_INFO;

    title_set_l('Check \'resource\' directory');
    
    if(isset($INSTALL_INFO['resource_check']))
	return true;

    if(!empty($args['resource_check'])) {
	$INSTALL_INFO['resource_check'] = true;
	install_reload();
    }
    
    $noscript = dom_append_child($dom, 'noscript');
    dom_append_text($noscript, l('Turn JavaScript on.'));
    $script = dom_append_child($dom, 'script', '', array('type' => 'text/javascript'));
    dom_append_text($script, <<<EOT
if(typeof config == 'object') {
    window.location = '?resource_check=true';
}
EOT
);
    $p = dom_append_child($dom, 'p');
    dom_append_text($p, l('Copy or symlink \'resource\' directory to here. And refresh the current page.'));
    return false;
}

function install_language($dom, $args) {
    global $INSTALL_INFO;
    global $LANGUAGE_OPTIONS;

    title_set_l('Language');
    
    if(!empty($args['lang']) && isset($LANGUAGE_OPTIONS[$args['lang']])) {
	$INSTALL_INFO['LANG'] = $args['lang'];
	install_reload();
    }
    if(isset($INSTALL_INFO['LANG']))
	return true;

    $form = dom_append_form($dom);
    dom_append_selector($form, 'lang', $LANGUAGE_OPTIONS);
    $submit = dom_append_child($form, 'input', '', array('type' => 'submit', 'value' => 'Enter'));
    
    return false;
}

function install_storage($dom, $args) {
    global $INSTALL_INFO;
    if(!empty($INSTALL_INFO['storage_check']))
	return true;
    
    if(!storage_install())
	return false;

    if(!empty($INSTALL_INFO['avoid_storage_check'])) {
	$INSTALL_INFO['storage_check'] = true;
	return true;
    }

    if(defined('STORAGE_FILE_DIR_PATH')) {
	title_set_l('Checking Web server settings');

	if(!file_put_contents(STORAGE_FILE_DIR_PATH . '/check.txt', l('The answer is yes.')))
	    $result = false;

	if(isset($args['storage_check_see']) && $args['storage_check_see'] === 'no') {
	    $INSTALL_INFO['storage_check'] = true;
	    return true;
	}

	$path = relative_path($_SERVER['SCRIPT_FILENAME'], realpath(STORAGE_FILE_DIR_PATH));
	$check_url = $path . '/check.txt';

	$ldom = dom_create_document();
	$link = dom_append_link($ldom, $check_url, l('this secret file'), 'storage_check');
	dom_set_attribute($link, 'target', '_blank');

	$p = dom_append_child($dom, 'p');
	dom_append_text($p, l('You must hide \'storage\' directory and publicize \'resource\', \'theme\' directory.'));

	if(isset($args['storage_check_see']) && $args['storage_check_see'] !== 'no') {
	    $p = dom_append_child($dom, 'p');
	    ld($p, 'Please change the settings of your Web server $link$ should not be displayed.', 
	       array('link' => $link));
	    $p = dom_append_child($dom, 'p');
	    dom_append_text($p, l('If you\'re running Apache HTTP server, enable .htaccess or insert the following types of text in httpd.conf.'));
	    $pre = dom_append_child($dom, 'pre');
	    dom_append_text($pre, INSTALL_APACHE_EXAMPLE);
	}

	$p = dom_append_child($dom, 'p');
	ld($p, 'Can you see $link$?',  array('link' => $link));

	$form = dom_append_form($dom);
	dom_append_selector($form, 'storage_check_see', array('yes' => l('Yes'), 'no' => l('No')));
	$submit = dom_append_child($form, 'input', '', array('type' => 'submit', 'value' => l('Enter')));
    }
    return false;
}

function install_admin($dom, $args) {
    global $INSTALL_INFO;
    global $AUTH_DIGESTS;
    global $AUTH_PERMISSIONS;
    global $AUTH_GET_USER_CACHE;
    global $main_page_output;

    if(auth_exists_admin($AUTH_PERMISSIONS))
	return true;

    $user = auth_get_user();
    if($user !== false && $user['method'] === 'other') {
	$INSTALL_INFO['admin_user'] = $user['name'];
    }

    if(!isset($INSTALL_INFO['admin_user'])) {
	title_set_l('Administrator user name');
	if(isset($args['user']) && $args['user'] !== '') {
	    if(auth_is_valid_username($args['user'])) {
		$INSTALL_INFO['admin_user'] = $args['user'];
		install_reload();
	    } else {
		message_error(l('Being usable for a user name only as for the alphanumeric character.'));
	    }
	}
	$form = dom_append_form($dom);
	dom_append_child($form, 'input', '', array('type' => 'text', 'name' => 'user'));
	$submit = dom_append_child($form, 'input', '', array('type' => 'submit', 'value' => l('Enter')));
	return false;
    }

    if(!isset($AUTH_PERMISSIONS[$INSTALL_INFO['admin_user']])) {
	$AUTH_PERMISSIONS[$INSTALL_INFO['admin_user']] = 'admin';
	if(auth_save_permissions())
	    install_reload();
	else
	    return false;
    }
    
    if(!auth_user_has_password($INSTALL_INFO['admin_user'])) {
	$AUTH_GET_USER_CACHE = array('name' => $INSTALL_INFO['admin_user'], 'method' => 'self');
	$args['option'] = 'password';
	$main_page_output .= option($args);
	if(auth_user_has_password($INSTALL_INFO['admin_user']))
	    install_reload();
	return false;
    }
    return false;
}

function install_admin_email($dom, $args) {
    global $INSTALL_INFO;
    global $AUTH_GET_USER_CACHE;
    global $main_page_output;

    $user = auth_get_user();
    if($user !== false && $user['method'] === 'other')
	return true;

    if(!isset($INSTALL_INFO['admin_user']))
	return true;
    
    if(isset($INSTALL_INFO['admin_email']))
	return true;
    
    $AUTH_GET_USER_CACHE = array('name' => $INSTALL_INFO['admin_user'], 'method' => 'self');
    $args['option'] = 'email';
    $args['first'] = 'true';
    $is_done = false;
    if(isset($args['action']) && $args['action'] === 'write' &&
       isset($args['email']) && ($args['email'] === '' || email_is_valid($args['email'])))
	$is_done = true;
    $main_page_output .= option($args);
    if($is_done) {
	$INSTALL_INFO['admin_email'] = true;
	install_reload();
    }
    return false;
}

function install_guest($dom, $args) {
    global $AUTH_PERMISSIONS;

    title_set_l('Permission level of user not logged in');

    if(isset($AUTH_PERMISSIONS[AUTH_GUEST_USERNAME]))
	return true;

    $options = auth_get_permission_options();
    if(EXPORT_ENABLE !== 'true')
	unset($options['export']);
    unset($options['admin']);

    if(isset($args['guest_permission']) && 
       isset($options[$args['guest_permission']])) {
	$AUTH_PERMISSIONS[AUTH_GUEST_USERNAME] = $args['guest_permission'];
	if(auth_save_permissions())
	    install_reload();
	else
	    return false;
    }
    
    $form = dom_append_form($dom);
    dom_append_selector($form, 'guest_permission', $options, 'read');
    $submit = dom_append_child($form, 'input', '', array('type' => 'submit', 'value' => l('Enter')));
    
}

function install_done($dom, $args) {
    global $INSTALL_INFO;
    title_set_l('Install');

    $filepath = $_SERVER['SCRIPT_FILENAME'];
    $contents = file_get_contents($filepath);
    $contents = preg_replace('|(require_once\(.*)install\.inc|m', '$1boot.inc', $contents);
    if(!@file_put_contents($filepath, $contents)) {
	message_error(lf('Cannot write file $filename$.', array('filename' => $filepath)));
	return false;
    }
    
    $values = array();
    $contents = setup_read('site');
    if($contents !== false)
	$values = unserialize($contents);
    if(isset($INSTALL_INFO['LANG']))
	$values['LANG'] = $INSTALL_INFO['LANG'];
    $contents = serialize($values);
    if(!setup_write('site', $contents)) {
	message_error(l('Could not save site preferences.'));
	return false;
    }

    title_set_l('Initial setup done.');

    $ldom = dom_create_document();
    $link = dom_append_link($ldom, url_make('', array('option' => 'admin_setup_site', 'first' => 'true')), l('here'));

    $p = dom_append_child($dom, 'p');
    ld($p, 'Log in to $link$ and configure site preferences.', array('link' => $link));
    
    return true;
}

define('INSTALL_APACHE_EXAMPLE', <<<'EOT'
# version 2.2
<Directory "/path/to/toratorawiki/">
    Order Allow,Deny
    <Files ".">
        Allow from all
    </Files>
    <Files "index.php">
        Allow from all
    </Files>
    Deny from all
</Directory>
<Directory ~ "^/path/to/toratorawiki/(resource|theme)/">
    Allow from all
</Directory>

# version 2.4
<Directory "/path/to/toratorawiki/">
     Require all denied
    <Files ".">
        Require all granted
    </Files>
    <Files "index.php">
        Require all granted
    </Files>
</Directory>
<Directory ~ "^/path/to/toratorawiki/(resource|theme)/">
    Require all granted
</Directory>

EOT
);

install_info_load();
if(isset($INSTALL_INFO['LANG']))
    define('LANG', $INSTALL_INFO['LANG']);

install_app_directory() || exit(1);
require_once(APP_DIR_PATH . '/main.inc');

$LANGUAGE['ja'] += array(
    'Initial setup' => '初期設定',
    'here' => 'ここ',
    'Administrator user name' => '管理者のユーザー名',
    'Permission level of user not logged in' => 'ログインしていないユーザーの権限',
    'Check directory \'$dir$\' permission.' => 'ディレクトリ $dir$ の権限を確認して下さい．',
    'Check \'resource\' directory' => '\'resource\'ディレクトリの確認',
    'Copy or symlink \'resource\' directory to here. And refresh the current page.' => 
    '\'resource\'ディレクトリをこのディレクトリにコピーするかシンボリックリンクを張って下さい．'.
    'そしてブラウザを再読み込みして下さい．',
    'Initial setup done.' => '初期設定が完了しました．',
    'Log in to $link$ and configure site preferences.' =>
    '$link$をクリックし，ログインしてサイト設定を行って下さい．',
    'Checking Web server settings' => 'Webサーバー設定の確認',
    'You must hide \'storage\' directory and publicize \'resource\', \'theme\' directory.' =>
    '\'storage\'ディレクトリは外部から閲覧できないようになっている必要があります．'.
    'また\'resource\'と\'theme\'ディレクトリは外部から閲覧できる必要があります．',
    'Please change the settings of your Web server $link$ should not be displayed.' =>
    'Webサーバーの設定を変更して$link$が閲覧できないようにして下さい．',
    'this secret file' => 'このファイル',
    'If you\'re running Apache HTTP server, enable .htaccess or insert the following types of text in httpd.conf.' =>
    'もしApache HTTP serverを使用しているなら.htaccessを有効化するか以下のような設定をhttpd.confに追記して下さい．',
    'Can you see $link$?' => '$link$が見えますか?',
    'The answer is yes.' => 'はい と回答して下さい．',
    );

$main_page_dom = dom_create_document();
theme_simple_screen_init();
theme_avoid_cache(true);
title_set_l('Initial setup');

$args = args_get();

install_check_ticket($args) || exit(1);
install_theme($main_page_dom, $args) || exit(1);
install_language($main_page_dom, $args) && 
install_resource($main_page_dom, $args) &&
install_storage($main_page_dom, $args) &&
install_admin($main_page_dom, $args) &&
install_admin_email($main_page_dom, $args) &&
install_guest($main_page_dom, $args) &&
install_done($main_page_dom, $args);

$main_page_output .= dom_save_html($main_page_dom);
theme_simple_screen();

install_info_save();
?>
