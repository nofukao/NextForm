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
$SETUP_CONSTANTS['MARKDOWN_ALLOW_HTML'] = array(
    'description' => 'Raw HTML in Markdown pages',
    'default' => 'false',
    'type' => 'select',
    'options' => array('false' => 'Escape', 'true' => 'Allow'),
    'note' => 'allowing it lets anyone who can edit run scripts');
$SETUP_CONSTANTS['FILE_ALLOW_HTML'] = array(
    'description' => 'Attached HTML files',
    'default' => 'false',
    'type' => 'select',
    'options' => array('false' => 'Download', 'true' => 'Run in a sandbox'),
    'note' => 'running them lets anyone who can edit show any page under the URL of this site');
/*
 * フォームの送り方 (util.inc の post_args_get()、nextform.js の postEncodingSetup())。
 *
 * ConoHa WING などの共用サーバーの WAF は、本文の `'--` や `<script>` を攻撃と
 * みなして保存を遮断する。base64 にすると、ブラウザが POST の値を包んで送り、
 * サーバーが戻すので WAF に当たらない。代わりに、この wiki に送る内容を
 * WAF が検査しなくなるので、要るサイトだけが選ぶ。既定は改修前と同じ。
 */
$SETUP_CONSTANTS['POST_ENCODING'] = array(
    'description' => 'How forms are sent',
    'default' => 'none',
    'type' => 'select',
    'options' => array('none' => 'Send as is', 'base64' => 'Wrap in Base64 (for a WAF)'),
    'note' => 'choose wrapping on a shared server with a WAF, such as ConoHa WING, where saving a page with \'-- or <script> in it stops at "Forbidden access". The WAF then no longer inspects what is sent to this wiki');
$SETUP_CONSTANTS['GOOGLE_API_KEY'] = array(
    'description' => 'Google API key',
    'type' => 'string',
    'allowempty' => true,
    'default' => '',
    'note' => 'used by &calender');

set_const('BASE_URI', '');

set_const('PASSWORD_REMINDER_EMAIL_FROM', '');

set_const('MANUAL_PAGENAME', 'NextFormManual');

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

/*
 * CSRF 対策を効かせるか。
 *
 * 'true' のとき、状態を変える action は POST かつ同一オリジンからのものだけ
 * 受け付ける (app/csrf.inc)。判定は Origin、無ければ Referer のホスト部で行う。
 *
 * 切れるようにしてあるのは、ブラウザ以外から POST している運用
 * (自作のスクリプトなど) を止めてしまう可能性があるため。
 * **切ると罠サイトからページを消せる状態に戻る。** 切る前に、
 * 呼び出し側に Origin ヘッダを付けられないかを先に検討すること。
 */
set_const('CSRF_PROTECTION', 'true');

set_const('PAGE_FIND_CREATE_CACHE_TIME', 0.01);

set_const('PAGE_FILE_IMAGE_RESIZE', 'max 640x640');
set_const('PAGE_FILE_IMAGE_RESIZE_MAX_SIZE', 524288);
set_const('PAGE_FILE_SHOW_GPS', 'false');

set_const('CONTENTS_READ_BUFFER_SIZE', 4096);
set_const('CONTENTS_READ_MAX_SIZE', 1024 * 1024);

/*
 * ページ種別の既定値は 2 つある。役割が違うので分けてある。
 *
 *   NEWPAGE_DEFAULT_TYPE  画面から新規作成するときに種別の欄で選ばれている種別
 *   DEFAULT_PAGE_TYPE     種別がどこにも書かれていないときの落とし先
 *
 * 後者を markdown にしてはいけない。&insert &listadd &writepage (&newpage) が
 * 作るページは種別を指定せずに書き込むので、ここに落ちる。これらは wiki 記法の
 * ファンクションで、書き込む中身も wiki 記法なので、wiki のままにしておく。
 * 種別の読めないページの落とし先でもある。
 */
set_const('NEWPAGE_DEFAULT_TYPE', 'markdown');
set_const('DEFAULT_PAGE_TYPE', 'wiki');

set_const('FFMPEG_PATH', 'ffmpeg');

set_const('SEARCH_SNIPPET_LENGTH', 30);
/*
 * 検索結果を 1 画面に何件出すか。0 で全件。
 * 全件出すと、ヒットが多いときに応答が巨大になる (1077 件で 9.6MB)。
 */
set_const('SEARCH_RESULTS_PER_PAGE', 50);
/*
 * 1 件の検索結果に、本文からのスニペットを何個まで並べるか。0 で無制限。
 * 一致した回数だけ出るので、上限が無いと 1 ページで何十個にもなる。
 */
set_const('SEARCH_SNIPPET_MAX', 5);
set_const('SEARCH_DEFAULT_ORDER', 'score');
set_const('SEARCH_ENGINE', 'builtin');
set_const('SEARCH_ELASTICSEARCH_URL', 'http://localhost:9200/nextform');

set_const('FEED_DEFAULT_PAGE_NUM', 20);

set_const('USER_HISTORY_ENABLE', 'false');
set_const('USER_HISTORY_UNREAD', 'false');


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
/* 種別 markdown も同じ枠で書き出す。中身は変換後の HTML */
set_const('EXPORT_DEFAULT_TEMPLATE_MARKDOWN', EXPORT_DEFAULT_TEMPLATE_WIKI);

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

set_const('OFFICIAL_URL', 'https://github.com/nofukao/NextForm');

set_const('OS_IS_WINDOWS', strtoupper(substr(PHP_OS, 0, 3)) == 'WIN' ? 'true' : 'false');

function set_const($constname, $defaultvalue) {
    if(!defined($constname)) {
	$envval = false;
	if(function_exists('apache_getenv')) {
	    /*
	     * NEXTFORM_ を先に見て、無ければ上流の TORATORAWIKI_ を見る。
	     * 既存サイトの SetEnv を書き換えさせないため、古い接頭辞も残す。
	     */
	    $envval = apache_getenv('NEXTFORM_' . $constname);
	    if($envval === false || $envval === NULL)
		$envval = apache_getenv('TORATORAWIKI_' . $constname);
	}
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
|{MANUAL_PAGENAME|NextFormManual|マニュアルページのページ名|}
|{MARKDOWN_ALLOW_HTML|false|種別Markdownのページで生のHTMLをそのまま通すか。trueにすると編集できる人が誰でもスクリプトを埋め込める|}
|{FILE_ALLOW_HTML|false|添付したHTMLファイルを開いたときに動かすか。falseならダウンロードになる。trueにするとwikiから隔離して動かすが，編集できる人は誰でもこのサイトのURLの下に好きな画面を出せる|}
|{TIME_FORMAT|%Y-%m-%d %H:%M:%S|標準の日時のフォーマット(strftime形式)|}
|{DATE_FORMAT|%Y-%m-%d|標準の日付のフォーマット(strftime形式)|}
|{APP_DIR_PATH|./app(自動で設定されます)|NextFormのappディレクトリ|}
|{STORAGE_FILE_DIR_PATH|./storage|サイトの各種情報の保存ディレクトリ|}
|{PLUGIN_DIR_PATH|./plugin|プラグインを読み込むディレクトリ|}
|{THEME_DIR_PATH|theme|テーマをキャッシュしておくディレクトリ|}
|{THEME_URI|theme|THEME_DIR_PATHにアクセスするためのURL|}
|{BACKUP_SKIP_TIME_SECONDS|15 * 60|この秒数以内の編集はバックアップをしない|}
|{EXTERNAL_FETCH_TIMEOUT|5|外部のサーバーから取ってくるときの待ち時間の上限(秒)|}
|{CSRF_PROTECTION|true|状態を変える操作をPOSTかつ同一オリジンからのものに限る(切ると罠サイトからページを消せる)|}
|{PAGE_FIND_CREATE_CACHE_TIME|0.01|内部で使用するページの一覧取得処理でこの秒数以上かかった結果はキャッシュする|}
|{PAGE_FILE_IMAGE_RESIZE|max 640x640|種別fileのページで画像を表示する際のサイズ|}
|{PAGE_FILE_IMAGE_RESIZE_MAX_SIZE|1048576|メモリ保護のためこのバイト数以上の画像ファイルはリサイズ処理を行わない|}
|{PAGE_FILE_SHOW_GPS|false|種別fileのページでGPS情報を表示するか|}
|{CONTENTS_READ_BUFFER_SIZE|4096|ページの内容を表示するときのバッファサイズ|}
|{CONTENTS_READ_MAX_SIZE|1024 * 1024|メモリに読み込めるページ容量の最大サイズ|}
|{NEWPAGE_DEFAULT_TYPE|markdown|新規作成の画面で最初に選ばれているページ種別|}
|{DEFAULT_PAGE_TYPE|wiki|種別が指定されなかったときのページ種別。&&insertなどが作るページが該当する|}
|{FFMPEG_PATH|ffmpeg|ffmpegコマンドの名称|}
|{SEARCH_SNIPPET_LENGTH|30|検索結果のスニペットの文字数|}
|{SEARCH_RESULTS_PER_PAGE|50|検索結果を1画面に出す件数(0で全件)|}
|{SEARCH_SNIPPET_MAX|5|検索結果1件に出す本文スニペットの上限(0で無制限)|}
|{SEARCH_DEFAULT_ORDER|score|検索結果の順序 score:単語の出現頻度(スコア)順, mtime:更新日時順|}
|{SEARCH_ENGINE|builtin|使用する検索エンジン builtin:NextForm内蔵の検索エンジン, elasticsearch: Elasticsearchを使用|}
|{SEARCH_ELASTICSEARCH_URL|http://localhost:9200/nextform|ElasticsearchのURL|}
|{FEED_DEFAULT_PAGE_NUM|20|フィードのデフォルトの件数|}
|{USER_HISTORY_ENABLE|false|ユーザーが閲覧したページを記録する(試験的機能,非推奨)|}
|{USER_HISTORY_UNREAD|false|未読ページへのリンクにマークをつける(USER_HISTORY_ENABLEをtrueにする必要あり)|}
|{WIKI_ICON_IMAGE_WIDTH|16|顔文字等のアイコンの幅|}
|{WIKI_ICON_IMAGE_HEIGHT|16|顔文字等のアイコンの高さ|}
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
|{EXPORT_DEFAULT_TEMPLATE_MARKDOWN|EXPORT_DEFAULT_TEMPLATE_WIKIと同じ|種別Markdownのページをエクスポートする際のデフォルトのテンプレート|}
|{IMPORT_PUKIWIKI_ENABLE|false|Pukiwikiからページをインポートする機能を有効にするか|}
|{IMPORT_ATWIKIBACKUP_ENABLE|true|@Wikiのデータバックアップからページをインポートする機能を有効にするか|}
|{ANALYZE_STORAGE_LIFE_DAYS|7|アクセスカウンタの記録保存日数|}
|{$WIKI_ALIASES (配列)|省略|サイト全体で有効なエイリアスの定義|}
|{DEBUG_MODE|false|(開発者向け) デバッグモード|}
|{GOOGLE_API_KEY||&&calendarで使用するGoole APIキー|}
*/

/**
@manual $manualpage$/en/Administration/Settings
&title Settings

-For how to set them, see [[Site administration:..]].
|constant|default|description|h
|{BASE_URI||the URL of the site (normally no need to change)|}
|{PASSWORD_REMINDER_EMAIL_FROM|(automatic)|the sender address of the password reset mail|}
|{MANUAL_PAGENAME|NextFormManual|the page name of the manual pages|}
|{MARKDOWN_ALLOW_HTML|false|whether raw HTML passes through in pages of type Markdown. Setting it true lets anyone who can edit embed a script|}
|{FILE_ALLOW_HTML|false|whether an attached HTML file runs when it is opened. With false it is downloaded. Setting it true runs it isolated from the wiki, but lets anyone who can edit show any page under the URL of this site|}
|{TIME_FORMAT|%Y-%m-%d %H:%M:%S|the standard format for date and time (strftime form)|}
|{DATE_FORMAT|%Y-%m-%d|the standard format for dates (strftime form)|}
|{APP_DIR_PATH|./app (set automatically)|the app directory of NextForm|}
|{STORAGE_FILE_DIR_PATH|./storage|the directory that stores the various data of the site|}
|{PLUGIN_DIR_PATH|./plugin|the directory plugins are loaded from|}
|{THEME_DIR_PATH|theme|the directory themes are cached in|}
|{THEME_URI|theme|the URL for reaching THEME_DIR_PATH|}
|{BACKUP_SKIP_TIME_SECONDS|15 * 60|no backup is taken for edits within this many seconds|}
|{EXTERNAL_FETCH_TIMEOUT|5|the limit on how long to wait when fetching from an external server (seconds)|}
|{CSRF_PROTECTION|true|limit operations that change state to POST from the same origin (turn it off and a trap site can delete pages)|}
|{PAGE_FIND_CREATE_CACHE_TIME|0.01|cache the result of the internal page listing when it took at least this many seconds|}
|{PAGE_FILE_IMAGE_RESIZE|max 640x640|the size at which images are shown in pages of type file|}
|{PAGE_FILE_IMAGE_RESIZE_MAX_SIZE|1048576|to protect memory, image files of at least this many bytes are not resized|}
|{PAGE_FILE_SHOW_GPS|false|whether GPS data is shown in pages of type file|}
|{CONTENTS_READ_BUFFER_SIZE|4096|the buffer size used when showing the contents of a page|}
|{CONTENTS_READ_MAX_SIZE|1024 * 1024|the largest page that can be read into memory|}
|{NEWPAGE_DEFAULT_TYPE|markdown|the page type selected first on the new page screen|}
|{DEFAULT_PAGE_TYPE|wiki|the page type used when no type was given. Pages created by &&insert and the like fall under this|}
|{FFMPEG_PATH|ffmpeg|the name of the ffmpeg command|}
|{SEARCH_SNIPPET_LENGTH|30|the number of characters in a snippet of a search result|}
|{SEARCH_RESULTS_PER_PAGE|50|how many search results are shown on one screen (0 for all)|}
|{SEARCH_SNIPPET_MAX|5|the limit on snippets shown for one search result (0 for no limit)|}
|{SEARCH_DEFAULT_ORDER|score|the order of search results. score: by how often the words appear, mtime: by the time of modification|}
|{SEARCH_ENGINE|builtin|the search engine to use. builtin: the search engine inside NextForm, elasticsearch: use Elasticsearch|}
|{SEARCH_ELASTICSEARCH_URL|http://localhost:9200/nextform|the URL of Elasticsearch|}
|{FEED_DEFAULT_PAGE_NUM|20|the default number of entries in a feed|}
|{USER_HISTORY_ENABLE|false|record the pages a user has read (experimental, not recommended)|}
|{USER_HISTORY_UNREAD|false|mark links to unread pages (USER_HISTORY_ENABLE has to be true)|}
|{WIKI_ICON_IMAGE_WIDTH|16|the width of icons such as emoticons|}
|{WIKI_ICON_IMAGE_HEIGHT|16|the height of icons such as emoticons|}
|{WIKI_TIME_DEFAULT_NEW_HOUR|24|the default number of hours [[&&time:../../Wiki/Function/time]] shows NEW! for|}
|{WIKI_SUMMARY_IMAGE_RESIZE|max 100x100|the size at which a page of an image file is summarized|}
|{WIKI_CALENDAR_RULES_DEFAULT|omitted (see setup.inc)|the event data always shown by [[&&calendar:../../Wiki/Function/calendar]]|}
|{WIKI_MATH_ENABLE|false|whether [[&&math:../../Wiki/Function/math]] is enabled (enabling it calls for care)|}
|{WIKI_MATH_COMMAND|omitted (see setup.inc)|the command that turns LaTeX source into an image|}
|{WIKI_MATH_COMMAND_ENVS|PATH=/usr/local/bin:/usr/bin:/bin|the environment variables for running the command that turns LaTeX source into an image|}
|{EXPORT_ENABLE|false|whether the export function is enabled|}
|{EXPORT_DIR_PATH|./export|the directory exported to|}
|{EXPORT_POST_COMMAND||the command run after an export (the directory exported to is passed as a command argument)|}
|{EXPORT_DEFAULT_TEMPLATE_WIKI|omitted|the default template for exporting pages of type wiki|}
|{EXPORT_DEFAULT_TEMPLATE_MARKDOWN|the same as EXPORT_DEFAULT_TEMPLATE_WIKI|the default template for exporting pages of type Markdown|}
|{IMPORT_PUKIWIKI_ENABLE|false|whether importing pages from PukiWiki is enabled|}
|{IMPORT_ATWIKIBACKUP_ENABLE|true|whether importing pages from an @Wiki data backup is enabled|}
|{ANALYZE_STORAGE_LIFE_DAYS|7|how many days the records of the access counter are kept|}
|{$WIKI_ALIASES (array)|omitted|the definitions of aliases valid across the whole site|}
|{DEBUG_MODE|false|(for developers) debug mode|}
|{GOOGLE_API_KEY||the Google API key used by &&calendar|}
*/
?>
