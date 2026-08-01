<?php
define('APP_DIR_PATH', dirname(__FILE__));
require_once(APP_DIR_PATH . '/main.inc');
main();
require_once(APP_DIR_PATH . '/theme/' . THEME . '/' . THEME_HTML_FILE_NAME);
?>
