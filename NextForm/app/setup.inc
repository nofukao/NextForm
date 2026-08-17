<?php
$SETUP_CONSTANTS['SITE_NAME'] = array(
    'description' => 'Site name',
    'default' => 'Some Wiki');
$SETUP_CONSTANTS['SITE_DESCRIPTION'] = array(
    'description' => 'Site description',
    'type' => 'multilinestring',
    'allowempty' => true,
    'default' => '',
    'note' => 'referred by search engines');
$SETUP_CONSTANTS['DEFAULT_PAGENAME'] = array(
    'description' => 'Default page name',
    'default' => 'Top',
    'type' => 'pagename');
$SETUP_CONSTANTS['SIDE_PAGENAME'] = array(
    'description' => 'Side page name',
    'default' => 'Side',
    'type' => 'pagename',
    'allowempty' => true,
    'note' => 'blank means no side page');
$SETUP_CONSTANTS['SENTENCE_PAGENAME_ENABLE'] = array(
    'description' => 'Automatic page link generation',
    'default' => 'false',
    'type' => 'select',
    'options' => array('true' => 'Enable', 'false' => 'Disable'),
    'note' => 'the word same as page name will be link');
$SETUP_CONSTANTS['LANG'] = array(
    'description' => 'Language',
    'default' => 'en',
    'type' => 'select',
    'options' => array('ja' => 'Japanese', 'en' => 'English'));
$SETUP_CONSTANTS['DEFAULT_TIMEZONE'] = array(
    'description' => 'Time Zone',
    'default' => 'Asia/Tokyo',
    'type' => 'timezone');
$SETUP_CONSTANTS['THEME'] = array(
    'description' => 'Theme',
    'default' => 'basic',
    'type' => 'theme');
$SETUP_CONSTANTS['THEME_DEBUG_MODE'] = array(
    'category' => 'debug',
    'description' => 'Theme debug mode',
    'default' => 'false',
    'type' => 'select',
    'options' => array('true' => 'Enable', 'false' => 'Disable'),
    'note' => 'it enables no cache theme file');
$SETUP_CONSTANTS['GOOGLE_API_KEY'] = array(
    'description' => 'Google API key',
    'type' => 'string',
    'allowempty' => true,
    'default' => '',
    'note' => 'used by &calender');

set_const('BASE_URI', '');

set_const('PASSWORD_REMINDER_EMAIL_FROM', '');

set_const('MANUAL_PAGENAME', 'ToraToraWikiManual');

set_const('TIME_FORMAT', '%Y-%m-%d %H:%M:%S');
set_const('DATE_FORMAT', '%Y-%m-%d');

set_const('APP_DIR_PATH', './app');
set_const('STORAGE_FILE_DIR_PATH', './storage');
set_const('PLUGIN_DIR_PATH', './plugin');

set_const('THEME_DIR_PATH', 'theme');
set_const('THEME_URI', THEME_DIR_PATH);

set_const('BACKUP_SKIP_TIME_SECONDS', 15 * 60);

/*
 * 外部のサーバーから取ってくるときの待ち時間の上限 (秒)。
 * &calendar の Google カレンダー連携と Elasticsearch がここを見る。
 * どちらもページの描画の途中で同期的に取りに行くので、相手が応答しないと
 * その時間だけページが出てこない。実測で 21 秒待たされた例がある。
 */
set_const('EXTERNAL_FETCH_TIMEOUT', 5);

set_const('PAGE_FIND_CREATE_CACHE_TIME', 0.01);

set_const('PAGE_FILE_IMAGE_RESIZE', 'max 640x640');
set_const('PAGE_FILE_IMAGE_RESIZE_MAX_SIZE', 524288);
set_const('PAGE_FILE_SHOW_GPS', 'false');

set_const('CONTENTS_READ_BUFFER_SIZE', 4096);
set_const('CONTENTS_READ_MAX_SIZE', 1024 * 1024);

set_const('DEFAULT_PAGE_TYPE', 'wiki');

set_const('FFMPEG_PATH', 'ffmpeg');

set_const('SEARCH_SNIPPET_LENGTH', 30);
set_const('SEARCH_DEFAULT_ORDER', 'score');
set_const('SEARCH_ENGINE', 'builtin');
set_const('SEARCH_ELASTICSEARCH_URL', 'http://localhost:9200/toratorawiki');

set_const('FEED_DEFAULT_PAGE_NUM', 20);

set_const('USER_HISTORY_ENABLE', 'false');
set_const('USER_HISTORY_UNREAD', 'false');

set_const('WIKI_INCLUDE_URI_DEFAULT_VIDEO_WIDTH', 480);
set_const('WIKI_INCLUDE_URI_DEFAULT_VIDEO_HEIGHT', 480 * 9 / 16);

set_const('WIKI_TIME_DEFAULT_NEW_HOUR', 24);

set_const('WIKI_SUMMARY_IMAGE_RESIZE', 'max 100x100');

set_const('WIKI_CALENDAR_RULES_DEFAULT',
	  "outid3el0qkcrsuf89fltf7a4qbacgt9@import.calendar.google.com holiday\n");

set_const('WIKI_MATH_ENABLE', 'false');
set_const('WIKI_MATH_COMMAND',
	  'DIR=$(mktemp -d /tmp/wiki-math.XXXXXX); ' .
	  'export shell_escape=f openin_any=p openout_any=p;' .
	  'platex -no-shell-escape -output-directory="$DIR" -jobname=math 1>&2;' .
	  'dvipdfmx -o "$DIR/math.pdf" "$DIR/math.dvi";'.
	  'convert -density $DPI$x$DPI$ "$DIR/math.pdf" -trim png:- ;' .
	  'rm -rf "$DIR"');
set_const('WIKI_MATH_COMMAND_ENVS', 'PATH=/usr/local/bin:/usr/bin:/bin');

set_const('EXPORT_ENABLE', 'false');
set_const('EXPORT_DIR_PATH', './export');
set_const('EXPORT_POST_COMMAND', '');
set_const('EXPORT_DEFAULT_TEMPLATE_WIKI',  <<<'EOF'
<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8">
    <title>$title$</title>
  </head>
  <body>
    <h1>$title$</h1>	  
    $contents$
  </body>
</html>
EOF
);

set_const('IMPORT_PUKIWIKI_ENABLE', 'false');
set_const('IMPORT_ATWIKIBACKUP_ENABLE', 'true');

set_const('ANALYZE_STORAGE_LIFE_DAYS', '7');

if(!isset($SETUP_CONSTANTS))
    $SETUP_CONSTANTS = array();

if(!isset($WIKI_ALIASES))
    $WIKI_ALIASES = array();
if(!isset($WIKI_TEMPLATES))
    $WIKI_TEMPLATES = array();

if(!isset($POST_HANDLERS))
    $POST_HANDLERS = array();

if(!isset($AUTH_DIGESTS))
    $AUTH_DIGESTS = array();
if(!isset($AUTH_PERMISSIONS))
    $AUTH_PERMISSIONS = array();

set_const('DEBUG_MODE', 'false');

set_const('OFFICIAL_URL', 'http://toratora.wiki/');

set_const('OS_IS_WINDOWS', strtoupper(substr(PHP_OS, 0, 3)) == 'WIN' ? 'true' : 'false');

function set_const($constname, $defaultvalue) {
    if(!defined($constname)) {
	$envval = false;
	if(function_exists('apache_getenv'))
	    $envval = apache_getenv('TORATORAWIKI_' . $constname);
	if($envval !== false && $envval !== NULL)
	    define($constname, $envval);
	else
	    define($constname, $defaultvalue);
    }
}

function setup_local() {
    global $SETUP_CONSTANTS;
    $values = setup_read('site');
    if($values !== false) 
	$values = unserialize($values);
    
    foreach($SETUP_CONSTANTS as $key => &$setup_constant) {
	if(!defined($key)) {
	    if(isset($values[$key]))
		define($key, $values[$key]);
	    else
		define($key, $setup_constant['default']);
	}
    }
}

function setup_write($name, &$contents) {
    return storage_setup_write($name, $contents);
}

function setup_delete($name) {
    return storage_setup_delete($name);
}

function setup_read($name) {
    return storage_setup_read($name);
}

/**
@manual $manualpage$/ja/Administration/Settings
&title 設定項目

-設定方法は[[サイトの管理:..]]参照．
|定数名|初期値|説明|h
|{BASE_URI||サイトのURL(通常は変更の必要なし)|}
|{PASSWORD_REMINDER_EMAIL_FROM|(自動)|パスワード再設定メールの送信者メールアドレス|}
|{MANUAL_PAGENAME|ToraToraWikiManual|マニュアルページのページ名|}
|{TIME_FORMAT|%Y-%m-%d %H:%M:%S|標準の日時のフォーマット(strftime形式)|}
|{DATE_FORMAT|%Y-%m-%d|標準の日付のフォーマット(strftime形式)|}
|{APP_DIR_PATH|./app(自動で設定されます)|ToraToraWikiのappディレクトリ|}
|{STORAGE_FILE_DIR_PATH|./storage|サイトの各種情報の保存ディレクトリ|}
|{PLUGIN_DIR_PATH|./plugin|プラグインを読み込むディレクトリ|}
|{THEME_DIR_PATH|theme|テーマをキャッシュしておくディレクトリ|}
|{THEME_URI|theme|THEME_DIR_PATHにアクセスするためのURL|}
|{BACKUP_SKIP_TIME_SECONDS|15 * 60|この秒数以内の編集はバックアップをしない|}
|{EXTERNAL_FETCH_TIMEOUT|5|外部のサーバーから取ってくるときの待ち時間の上限(秒)|}
|{PAGE_FIND_CREATE_CACHE_TIME|0.01|内部で使用するページの一覧取得処理でこの秒数以上かかった結果はキャッシュする|}
|{PAGE_FILE_IMAGE_RESIZE|max 640x640|種別fileのページで画像を表示する際のサイズ|}
|{PAGE_FILE_IMAGE_RESIZE_MAX_SIZE|1048576|メモリ保護のためこのバイト数以上の画像ファイルはリサイズ処理を行わない|}
|{PAGE_FILE_SHOW_GPS|false|種別fileのページでGPS情報を表示するか|}
|{CONTENTS_READ_BUFFER_SIZE|4096|ページの内容を表示するときのバッファサイズ|}
|{CONTENTS_READ_MAX_SIZE|1024 * 1024|メモリに読み込めるページ容量の最大サイズ|}
|{DEFAULT_PAGE_TYPE|wiki|デフォルトのページ種別|}
|{FFMPEG_PATH|ffmpeg|ffmpegコマンドの名称|}
|{SEARCH_SNIPPET_LENGTH|30|検索結果のスニペットの文字数|}
|{SEARCH_DEFAULT_ORDER|score|検索結果の順序 score:単語の出現頻度(スコア)順, mtime:更新日時順|}
|{SEARCH_ENGINE|builtin|使用する検索エンジン builtin:ToraToraWiki内臓の検索エンジン, elasticsearch: Elasticsearchを使用|}
|{SEARCH_ELASTICSEARCH_URL|http://localhost:9200/toratorawiki|ElasticsearchのURL|}
|{FEED_DEFAULT_PAGE_NUM|20|フィードのデフォルトの件数|}
|{USER_HISTORY_ENABLE|false|ユーザーが閲覧したページを記録する(試験的機能,非推奨)|}
|{USER_HISTORY_UNREAD|false|未読ページへのリンクにマークをつける(USER_HISTORY_ENABLEをtrueにする必要あり)|}
|{WIKI_ICON_IMAGE_WIDTH|16|顔文字等のアイコンの幅|}
|{WIKI_ICON_IMAGE_HEIGHT|16|顔文字等のアイコンの高さ|}
|{WIKI_INCLUDE_URI_DEFAULT_VIDEO_WIDTH|600|動画サイトの埋め込みの際の標準の幅|}
|{WIKI_INCLUDE_URI_DEFAULT_VIDEO_HEIGHT|600 * 9 / 16|動画サイトの埋め込みの際の標準の高さ|}
|{WIKI_TIME_DEFAULT_NEW_HOUR|24|[[&&time:../../Wiki/Function/time]]でNEW!を表示するデフォルトの時間|}
|{WIKI_SUMMARY_IMAGE_RESIZE|max 100x100|画像ファイルのページが要約されるときの大きさ|}
|{WIKI_CALENDAR_RULES_DEFAULT|省略(setup.inc参照)|[[&&calendar:../../Wiki/Function/calendar]]で常に表示されるイベント情報|}
|{WIKI_MATH_ENABLE|false|[[&&math:../../Wiki/Function/math]]を有効にするか(有効にするには注意が必要)|}
|{WIKI_MATH_COMMAND|省略(setup.inc参照)|LaTeXソースを画像にするコマンド|}
|{WIKI_MATH_COMMAND_ENVS|PATH=/usr/local/bin:/usr/bin:/bin|LaTeXソースを画像にするコマンドを実行する際の環境変数|}
|{EXPORT_ENABLE|false|エクスポート機能を有効にするか|}
|{EXPORT_DIR_PATH|./export|エクスポートのディレクトリ|}
|{EXPORT_POST_COMMAND||エクスポート後に実行されるコマンド(エクスポート先ディレクトリがコマンド引数として渡される)|}
|{EXPORT_DEFAULT_TEMPLATE_WIKI|省略|種別wikiのページをエクスポートする際のデフォルトのテンプレート|}
|{IMPORT_PUKIWIKI_ENABLE|false|Pukiwikiからページをインポートする機能を有効にするか|}
|{IMPORT_ATWIKIBACKUP_ENABLE|true|@Wikiのデータバックアップからページをインポートする機能を有効にするか|}
|{ANALYZE_STORAGE_LIFE_DAYS|7|アクセスカウンタの記録保存日数|}
|{$WIKI_ALIASES (配列)|省略|サイト全体で有効なエイリアスの定義|}
|{DEBUG_MODE|false|(開発者向け) デバッグモード|}
|{GOOGLE_API_KEY||&&calendarで使用するGoole APIキー|}
*/
?>
