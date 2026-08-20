<?php
/*
 * tests/theme-switch.sh と tests/theme-tone.sh からサイトの中で実行される
 * 検証ヘルパ。
 *
 *   php theme-helper.php <index.php> <検査名> [引数..]
 *
 * 検査名:
 *   guest-admin        ログインしていない利用者に admin 権限を与える
 *   values <カテゴリ>   設定の項目を `名前=base64(値)` で並べる (site / theme)
 *   theme              保存されているテーマ名 (theme=...)
 *   set-tone <識別子>   保存されている色調を差し替える (theme=... と同じ保存先)
 *
 * 結果は `key=value` の行で出す。判定は呼び出し側の shell が行う。
 *
 * 権限を書き換えるので、必ず複製したサイトに対して実行すること。
 */
$argv = $_SERVER['argv'];
if(count($argv) < 3) {
    fprintf(STDERR, "Usage: php theme-helper.php <index.php> <check> [args..]\n");
    exit(2);
}
$index_path = $argv[1];
$check      = $argv[2];
$rest       = array_slice($argv, 3);

require_once(dirname(realpath($index_path)) . '/app/tool/common');
eval_index_php($index_path);
define('APP_DIR_PATH', getcwd() . '/app');
require_once(APP_DIR_PATH . '/main.inc');

switch($check) {

case 'guest-admin':
    /*
     * 資格情報をテストに置かずに済ませる。テーマの生成は認証の後ろにあるので、
     * 誰として通ったかは経路に影響しない。
     */
    global $AUTH_PERMISSIONS;
    $AUTH_PERMISSIONS[AUTH_GUEST_USERNAME] = 'admin';
    printf("saved=%d\n", auth_save_permissions() ? 1 : 0);
    break;

case 'values':
    /*
     * 設定のフォームは同じカテゴリの全項目を一度に送る。1 項目でも欠けると
     * 「入力して下さい」で保存そのものが失敗するので、いまの値を全部出す。
     * 改行を含む値 (サイトの説明) があるため base64 で 1 行にする。
     */
    global $SETUP_CONSTANTS;
    $target = default_value($rest[0], 'site');
    foreach($SETUP_CONSTANTS as $key => $setup_constant) {
	$category = default_value($setup_constant['category'], 'site');
	if($category !== $target)
	    continue;
	if(default_value($setup_constant['type'], 'string') === 'theme_file_image')
	    continue;
	printf("%s=%s\n", $key, base64_encode(defined($key) ? constant($key) : ''));
    }
    break;

case 'set-tone':
    /*
     * 色調の設定は常に 'custom' を書き込む。上流から引き継いだサイトのように
     * 色調の名前が入っている状態を作るために、直接書き換える。
     */
    $contents = setup_read('site');
    $values = ($contents === false) ? array() : unserialize($contents);
    $values['THEME_TONE'] = $rest[0];
    $contents = serialize($values);
    printf("saved=%d\n", setup_write('site', $contents) ? 1 : 0);
    break;

case 'theme':
    $values = setup_read('site');
    $values = ($values === false) ? array() : unserialize($values);
    printf("theme=%s\n", default_value($values['THEME'], ''));
    break;

default:
    fprintf(STDERR, "unknown check: %s\n", $check);
    exit(2);
}
?>
