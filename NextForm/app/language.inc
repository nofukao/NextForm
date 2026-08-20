<?php
$LANGUAGE = array();
$LANGUAGE_OPTIONS = array('ja' => 'Japanese', 'en' => 'English');
$LANGUAGE['ja'] = array(
    'Sun' => '日',
    'Mon' => '月',
    'Tue' => '火',
    'Wed' => '水',
    'Thu' => '木',
    'Fri' => '金',
    'Sat' => '土',

    'Yes' => 'はい',
    'No' => 'いいえ',
    'days' => '日',
    'hours' => '時間',
    'minutes' => '分',
    'seconds' => '秒',
    '%b %Y' => OS_IS_WINDOWS === 'true' ? '%Y年%#m月' : '%Y年%-m月',

    'Page' => 'ページ',
    'Save' => '保存',
    'Change' => '変更',
    'Run' => '実行',
    'Apply' => '適用',
    'Enter' => '決定',
    'Send' => '送信',
    'Edit' => '編集',
    'Close' => '閉じる',
    'Now' => '現在',
    'Compare' => '比較',
    'Delete' => '削除',
    'Show' => '表示',
    'Display' => '表示',
    'Type' => '種別',
    'Create' => '作成',
    'Page type' => 'ページ種別',
    'Raw HTML in Markdown pages' => '種別markdownのページの生HTML',
    'Escape' => 'そのまま文字として出す',
    'Allow' => 'HTMLとして解釈する',
    'allowing it lets anyone who can edit run scripts' => 'HTMLとして解釈すると、編集できる人は誰でもスクリプトを埋め込めます',
    'Markdown library is missing. Showing the source instead.' => 'Markdownの変換に必要なファイル (app/vendor/) がありません。原文を表示します。',
    'Source' => 'ソース',
    'Page name' => 'ページ名',
    'Size' => 'サイズ',
    'Last modified' => '最終更新',
    'Last updated by' => '最終更新者',
    'Birth time' => '作成日時',
    'Creator' => '作成者',
    'Last updated' => '更新日時',
    'Post' => '投稿',
    'Poster' => '投稿者',
    'Posted' => '投稿日時',
    'Date' => '日付',
    'Move' => '移動',
    'Add' => '追加',
    'Name' => '名前',
    'Comment' => 'コメント',
    'Title' => 'タイトル',
    'Text' => '本文',
    'Content' => '内容',
    'Top page' => 'トップページ',
    'Page creation time' => 'ページ生成時間',
    'Back' => '戻す',
    'Return' => '戻る',
    'Monthly' => '月',
    'Weekly' => '週',
    'Download' => 'ダウンロード',

    'First' => '最初',
    'Prev' => '前',
    'Next' => '次',
    'Last' => '最後',

    'Enable' => '有効',
    'Disable' => '無効',
    'not set' => '未設定',

    'Login' => 'ログイン',
    'Logout' => 'ログアウト',
    'User: $user$' => 'ユーザー: $user$',
    'Admin tools' => '管理ツール',

    'For this site' => 'サイトに対して',
    'For this page' => 'このページに対して',

    'Invalid parameter.' => '不正なパラメータです．',
    'Invalid $term$.' => '不正な $term$ です．',
    'Invalid page.' => '不正なページ指定です．',
    'Invalid page type.' => '不正なページ種別です．',
    'Cannot create tmpdir.' => '一時ディレクトリが作成できません．',
    'Page not found.' => 'ページがありません．',
    '$pagename$ not found.' => '$pagename$ がありません．',

    'User authentication' => 'ユーザー認証',
    'User authentication failed.' => 'ユーザーの認証に失敗しました．',
    'User \'$user$\' has no permission to perform the requested action.' =>
    'ユーザ $user$ には要求された処理を行う権限はありません．',
    'You have no permission to perform this action. (Please $login$)' =>
    '現在のユーザーではこの操作をする権限はありません．($login$して下さい)',
    'You are now logged out.' => 'ログアウトしました．',
    'Your browser does not support logout function. Plese close the browser or remove password.' =>
    'ブラウザにログアウト機能がないためログアウトできませんでした．ブラウザを閉じるか記憶されたパスワードを削除して下さい．',

    'Page name is too long.' => 'ページ名が長すぎます．',
    'mkdir failed. Check $dir$ permission.' => 'ディレクトリ $dir$ の作成に失敗しました．',
    'Cannot write file $filename$.' => 'ファイル $filename$ に書き込みができません．',

    'Editing $pagename$' => '$pagename$ の編集',
    'Page write failed.' => 'ページの保存に失敗しました．',
    'Page deleted.' => 'ページを削除しました．',
    'Page delete failed.' => 'ページの削除に失敗しました．',
    'Save and quit' => '保存して編集終了',
    'Save and edit' => '保存して編集続行',
    'Keep modify time' => '更新時刻はそのまま',
    'Preview' => 'プレビュー',
    'Saved. ($time$)' => '保存しました．($time$)',
    'Confliction detected.' => '編集の衝突が起きました．',
    'The search index could not be read. Rebuild it from the admin page.' =>
    '検索インデックスを読めませんでした．管理画面から再構築してください．',
    'Installation' => '設置',
    'not found' => '見つかりません',
    'writable by the web server' => 'Web サーバーから書き込めます',
    'not writable by the web server' => 'Web サーバーからは書き込めません',
    'The web server can rewrite the program. See installation.md.' =>
    'Web サーバーがプログラムを書き換えられる状態です．installation.md の「コードを Web サーバーに書かせない」を参照してください．',
    'Request rejected' => '要求を受け付けませんでした',
    '\'$action$\' must be sent by POST.' =>
    '$action$ は POST でしか受け付けません．',
    'The request did not come from this site.' =>
    'このサイトから送られた要求ではありません．',
    'If you reached this page from a bookmark or a link, open the page and use the button on it.' =>
    'ブックマークやリンクからここに来た場合は，対象のページを開いて，そのページ上のボタンから操作してください．',

    '($start$-$end$)' => '($start$〜$end$件目)',
    'Logging in may widen what you can search.' =>
    'ログインすると検索できる範囲が広がる場合があります．',

    'Search index' => '検索インデックス',
    'Index files: $num$ ($size$)' => 'インデックスのファイル: $num$ 個 ($size$)',
    'Indexed pages: $num1$ / Actual pages: $num2$' =>
    'インデックス上のページ: $num1$ / 実際のページ: $num2$',
    'Unreadable index files: $num$' => '読めないインデックスのファイル: $num$ 個',
    'Pages not in the index: $num$' =>
    'インデックスに載っていないページ: $num$ 件 (検索に出てきません)',
    'Index entries for pages that no longer exist: $num$' =>
    '実在しないページの登録: $num$ 件',
    'Pages missing some of their index entries: $num$' =>
    'インデックスの登録が欠けているページ: $num$ 件 (一部の語で検索に出てきません)',
    'Pages with index entries no longer in their text: $num$' =>
    '本文に無い登録が残っているページ: $num$ 件 (検索結果は正しいままです)',
    'Check in detail' => '詳しく検査する',
    'Repair' => '直す',
    'Repaired $pagename$.' => '$pagename$ のインデックスを入れ直しました．',
    'The index matches the pages.' => 'インデックスとページは一致しています．',
    'Rebuild the search index to fix this.' =>
    '検索インデックスを再構築してください．',
    'This check cannot see entries missing from a page that is otherwise indexed.' =>
    'この検査では、ページ単位で一部の登録だけが欠けている状態は分かりません．「詳しく検査する」で調べられます．',
    'Current source' => '最新のソース',
    'Your changes' => 'あなたの行った編集',

    'Source of $pagename$' => '$pagename$ のソース',

    'Summary' => '目次',

    'New page' => '新規ページ作成',
    'New page name' => '新規ページ名',
    'Page already exists.' => '既に存在するページです．',
    'Subpage' => '子ページ',
    'No subpage' => '子ページはありません．',
    'Create page $pagename$' => '$pagename$ を作成して下さい．',
    'Pagename required.' => 'ページ名を入力して下さい．',

    'Delete' => '削除',
    'Delete $pagename$' => '$pagename$ を削除',
    'Delete $pagename$.' => '$pagename$ を削除しました．',
    'Delete follow subpages' => '以下の子ページも削除する',
    'Delete $pagename$ failed.' => '$pagename$ の削除に失敗しました．',
    'Delete history too' => '履歴も削除する',
    'Are you OK?' => '本当によろしいですか?',

    'Tags' => 'タグ',
    'Tags for $pagename$' => '$pagename$ のタグ',
    'Space separated tags' => '半角スペース区切りでタグを記入',
    'Tag follow subpages' => '以下の子ページも同じタグにする',
    'Tagged $pagename$.' => '$pagename$ にタグをつけました．',
    'Tag $pagename$ failed.' => '$pagename$ にタグをつけれませんでした．',
    
    'All tags' => 'タグ一覧',
    'Number of pages' => 'ページ数',
    '$tag$ tagged pages' => 'タグ $tag$ がつけられたページ',
    'List of $tag$ tagged pages' => 'タグ $tag$ のページのみ表示中',
    'Tagged: $tag$' => 'タグ: $tag$',

    'Lock' => 'ロック',
    'Locked by' => 'ロックしているユーザー',
    'Lock $pagename$' => '$pagename$ をロック',
    'Lock $pagename$.' => '$pagename$ をロックしました．',
    'Lock follow subpages' => '以下の子ページもロックする',
    'Lock $pagename$ failed.' => '$pagename$ のロックに失敗しました．',
    'Enable only $user$ unlocking' => '$user$ だけがアンロックできるようにする',
    'This page was locked.' => 'このページはロックされています．',
    'This page was locked by $user$.' => 'このページは $user$ によってロックされています．',

    'Unlock' => 'アンロック',
    'Unlock $pagename$' => '$pagename$ をアンロック',
    'Unlock $pagename$.' => '$pagename$ をアンロックしました．',
    'Unlock follow subpages' => '以下の子ページもアンロックする',
    'Unlock $pagename$ failed.' => '$pagename$ のアンロックに失敗しました．',
    'Only $user$ can unlock this page.' => 'このページは $user$ だけがアンロックできます．',

    'Rename' => 'リネーム',
    'Rename $pagename$' => '$pagename$ をリネーム',
    'Rename follow subpages' => '以下の子ページもリネームする',
    '$pagename$ already exists.' => '$pagename$ はすでに存在します．',
    '$pagename$ is not exist.' => '$pagename$ が存在しません．',
    'Rname $pagename1$ to $pagename2$.' => '$pagename1$ から $pagename2$ にリネームしました．', 
    'Rname $pagename1$ to $pagename2$ failed.' => '$pagename1$ から $pagename2$ へのリネームに失敗しました．',
    'Rename aborted.' => 'リネームを中止しました．',

    'Attach files in $pagename$' => '$pagename$ にファイルを添付',
    'Uploaded files will be subpage of $pagename$.' =>
    'アップロードしたファイルは $pagename$ の子ページになります．',
    'Subpages of $pagename$' => '$pagename$ の子ページ',
    'Attachment files' => '添付ファイル',

    'Meta info' => 'メタ情報',
    'Meta info of $pagename$' => '$pagename$ のメタ情報',
    'Keywords' => 'キーワード',
    'Description' => '説明',
    'referred by search engines' => '検索エンジンに利用されます',
    'overwritten with &title wiki function' =>
    'wikiの&titleを使用している場合は&titleが優先されます．',

    'All pages' => '全ページ一覧',
    'Display table style' => '表形式で表示する',
    'Display tree style' => 'ツリー形式で表示する',
    'Deleted pages' => '削除されたページ',
    'Deleted pages' => '削除されたページ',
    'Deleted at' => '削除した日時',

    'Modification' => '変更点',
    'Modification of $pagename$' => '$pagename$ の変更点',
    'Compare $pagename$ with the last view' => '$pagename$ の最後の閲覧からの変更点',

    'History' => '履歴',
    'Modification time' => '更新日時',
    'History of $pagename$' => '$pagename$ の履歴',
    'Old version of $pagename$ at $time$' => '$time$時点の $pagename$',
    'Source of old version of $pagename$ at $time$' => '$time$時点の $pagename$ のソース',
    'Backup not found.' => 'バックアップが見つかりません．',
    'Backup failed.' => 'バックアップに失敗しました．',

    'Difference between $pagename1$ (at $compare1time$) and $pagename2$ (at $compare2time$)' =>
    '$pagename1$ ($compare1time$) と $pagename2$ ($compare2time$)の比較',
    'Difference between $pagename1$ and $pagename2$' => '$pagename1$ と $pagename2$ の比較',
    '$pagename$ (at $time$)' => '$pagename$ ($time$)',
    'Newest modification of $pagename$' => '$pagename$ の最近の変更点',
    'Meta info difference' => 'メタ情報の違い',

    'Added line' => '追加された行',
    'Deleted line' => '削除された行',
    'Added part' => '追加された部分',
    'Deleted part' => '削除された部分',
    'Compare source' => 'ソースを比較',

    'Attach' => '添付',
    'Upload' => 'アップロード',
    'Upload files' => 'アップロードするファイル(複数可)',
    'Upload file' => 'アップロードするファイル',
    'Overwrite file' => '上書きするファイル',
    'Change Content-type' => 'Content-typeを変更',
    'File size limit' => 'サイズ上限',
    'No upload files.' => 'アップロードされたファイルがありません．',
    'Upload failed.' => 'アップロードに失敗しました．',
    'Upload $filename$.' => '$filename$ をアップロードしました．',
    'Upload $filename$ failed.' => '$filename$ のアップロードに失敗しました．',

    'Download' => 'ダウンロード',
    'Original file name' => 'アップロード時のファイル名',
    'File type' => 'ファイルの種類',
    'File size' => 'ファイルのサイズ',
    'byte(s)' => 'バイト',
    'Image width' => '画像の幅',
    'Image height' => '画像の高さ',
    'Video duration' => '動画の長さ',
    'Video width' => '動画の幅',
    'Video height' => '動画の高さ',
    'Audio duration' => '音声の長さ',

    'Pagename not specified.' => 'ページ名が指定されていません．',

    'This page will redirect to $pagename$.' => 'このページは $pagename$ に転送されます．',
    'Redirected from $pagename$' => '$pagename$ より転送されました．',

    'Debug' => 'デバッグ',
    'Debug info' => 'デバッグ情報',
    'Debug info of $pagename$' => '$pagename$ のデバッグ情報',
    'Syntax tree' => '構文木',

    'Your text was merged.' => '他からの変更と統合しました．',
    'Text insertion failed.' => 'テキストの挿入に失敗しました．',
    'URL is not allowed.' => 'URLは許可されていません．',
    'Posted text is not include Japanese.' => '日本語が含まれていません．',

    'Show hidden $num$ pages' => '非表示の$num$件も表示',

    'Search' => '検索',
    'Ignore manual pages' => 'マニュアルは除く',
    'Search keywords' => '検索キーワード',
    '$num$ result(s)' => '$num$件の検索結果',
    'score: $score$' => 'スコア: $score$',
    'Sort' => '順序',
    'Frequency' => '出現頻度',

    '($start$-$end$)' => '($start$〜$end$件目)',
    'Logging in may widen what you can search.' =>
    'ログインすると検索できる範囲が広がる場合があります．',

    'Search index' => '検索インデックス',
    'Rebuild search index' => '検索インデックスを再構築',
    '$num$ / $totalnum$ pages ($percent$%)' => '$num$ / $totalnum$ ページ ($percent$%)',
    'Rebuilding search index..' => '検索インデックスを再構築中..',
    'Elapsed time $time$' => '経過時間 $time$',
    'Done.' => '完了',

    'Make user setting' => 'ユーザー情報の生成',
    'User name' => 'ユーザー名',
    'Password' => 'パスワード',
    'Make' => '生成',
    'Permission' => '権限',
    'Permission level' => '権限レベル',
    'keep out' => '利用不可',
    'read only' => '閲覧のみ',
    'text insertion available' => 'テキストの挿入のみ',
    'page write available' => '編集可能',
    'page lock available' => 'ロック可能',
    'export available' => 'エクスポート可能',
    'administrator' => '管理者',
    'Insert below line into index.php.' =>
    '以下の行をindex.phpに追記して下さい．',

    '$sitename$ updating' => '$sitename$ の更新',
    'Subpages of $pagename$ updating' => '$pagename$ の子ページの更新',

    'System information' => 'システム情報',

    'Site preferences' => 'サイト設定',
    'You can reconfigure these items on Admin tools > Site preferences.' =>
    '以下の項目は 管理ツール > サイト設定 で後から変更することができます．',
    'Basic' => '基本',
    'Appearance' => '外観',
    'Site name' => 'サイト名',
    'Site description' => 'サイトの説明',
    
    'Default page name' => 'トップページの名前',
    'Side page name' => '横に表示するページの名前',
    'blank means no side page' => '空欄は横のページを非表示にします',
    'Automatic page link generation' => '自動リンク',
    'the word same as page name will be link' => 'ページ名と同じ単語をそのページへのリンクにします',
    'Language' => '言語',
    'Time Zone' => 'タイムゾーン',
    'Theme' => 'テーマ',
    'Theme debug mode' => 'テーマ開発モード',
    'it enables no cache theme file' => '有効にするとテーマのファイルがキャッシュされなくなります',
    'Current image' => '現在の画像',
    'Restore the default image' => '標準に戻す',
    '$term$: cannot choose an image and an action at the same time.' =>
    '$term$ : 画像を選ぶ操作と「標準に戻す」「表示しない」は同時にできません．',
    'Do not show' => '表示しない',
    'used by &calender' => '&calenderで予定の取得に使用します',

    'You must enter a $term$' => '$term$ が入力されていません．',
    'Could not save preferences.' => '設定の保存に失敗しました．',

    'Appearance theme preferences' => '外観の設定',
    'You can reconfigure these items on Admin tools > Appearance theme preferences.' =>
    '以下の項目は 管理ツール > 外観の設定 で後から変更することができます．',
    'Could not apply theme preferences.' => 'テーマの反映に失敗しました．',
    'Need to refresh browser applying changes' => '適用後ブラウザを再読み込みして下さい',

    'Tone preferences' => '色調の設定',
    'Load a tone' => '色調の読み込み',
    'Load' => '読み込む',
    'Could not load the tone.' => '色調の読み込みに失敗しました．',
    'Loaded the tone \'$tone$\'. Press apply to use it.' =>
    '色調 \'$tone$\' を読み込みました．使うには適用を押して下さい．',
    'Save this tone' => 'この色調の保存',
    'Delete a saved tone' => '保存した色調の削除',
    'Identifier' => '識別子',
    'Display name' => '表示名',
    'the identifier becomes the file name' => '識別子はファイル名になります',
    'Use half-width alphanumerics, dot, underscore and hyphen for the identifier.' =>
    '識別子は半角の英数字と . _ - で、32 文字までです．',
    'Could not save the tone.' => '色調の保存に失敗しました．',
    'The tone \'$tone$\' is gone. $default$ is used instead.' =>
    '色調 \'$tone$\' が見つかりません．$default$ が使われます．',
    'Could not delete the tone.' => '色調の削除に失敗しました．',
    'Deleted the tone.' => '色調を削除しました．',
    'deleting a tone that came with NextForm brings the original back' =>
    'はじめから入っている色調を上書きした場合、消すと元に戻ります',

    'User Manager' => 'ユーザーの管理',
    'New user' => '新しいユーザー',
    'User name' => 'ユーザー名',
    'Configured' => '設定済み',
    'Not set' => '未設定',
    'Settings for each page' => 'ページごとに設定',
    'User not logged in' => 'ログインしていないユーザー',
    'Turn JavaScript on.' => 'JavaScriptを有効にして下さい．',
    'Account preferences' => 'アカウント設定',
    'Password settings' => 'パスワード設定',
    'Password settings for $user$' => '$user$ のパスワード設定',
    'Permission settings' => '権限設定',
    'Permission settings for $user$' => '$user$ の権限設定',
    'New password' => '新しいパスワード',
    'Verify password' => 'パスワード確認',
    '$user$ password successfully changed.' => '$user$ のパスワードを変更しました．',
    'Cannot change $user$ password.' => '$user$ のパスワードを変更できませんでした．',
    'User $user$ already exists.' => '$user$ は既に存在するユーザーです．',
    'Can not delete all administrators.' => '管理者がいなくなってしまいます．',
    'Being usable for a user name only as for the alphanumeric character.' =>
    'ユーザー名に使用できるのは英数字のみです．',
    'Please $action$.' => '$action$して下さい',
    're login' => '再度ログイン',
    'Could not save user preferences.' => 'ユーザーの管理情報を保存できませんでした．',
    'You can use * or ? wildcards in the page name.' => 'ページ名には * と ? のワイルドカードが使用できます．',
    'e.g. SomePage/* means child and descendant sub pages of SomePage (without SomePage itself).' =>
    '例えば，SomePage/* は SomePageの子や孫ページを意味します(SomePage自身は含みません)．',
    'Rules are evaluated from top to bottom.' => 'ルールは上から順に評価されます．',

    'email address' => 'メールアドレス',
    'Email address' => 'メールアドレス',
    'Email address settings' => 'メールアドレス設定',
    'Email address settings for $user$' => '$user$ のメールアドレス設定',
    'Current email address' => '現在のメールアドレス',
    'New email address' => '新しいメールアドレス',
    '$user$ email address successfully deleted.' => '$user$ のメールアドレスを削除しました．',
    'Cannot delete $user$ email address.' => '$user$ のメールアドレスを削除できませんでした．',
    '$user$ email address successfully changed.' => '$user$ のメールアドレスを変更しました．',
    'Cannot change $user$ email address.' => '$user$ のメールアドレスを変更できませんでした．',
    'Email address will be used for password reminder. Blank input allowed.' =>
    'メールアドレスはパスワード忘れてしまった場合に使用します．必要なければ空欄でも構いません．',
    'Reset password' => 'パスワード再設定',
    'Send email to reset password if you input correct user name and email address.' =>
    '正しいユーザー名とメールアドレスが入力された場合のみ再設定用のメールが送信されます．',
    'A mail has just been sent.' => 'メールを送信しました．',
    'Invalid user name or email address.' => 'ユーザー名またはメールアドレスが異なります．',
    'Password reset was requested on $sitename$' => '$sitename$でのパスワード再設定',
    'Click this URL and reset your password.' => '下記URLをクリックしてパスワードを再設定して下さい．',
    
    'Manual' => 'マニュアル',
    'official site' => '公式サイト',
    'Wiki Cheat Sheet' => 'Wiki記法早見表',
    'Generate manual pages' => 'マニュアルページの生成',
    'Generate manual in under $pagename$ pages.' => '$pagename$ 以下のページにマニュアルページを生成します．', 
    'Manual pages under $pagename$ was generated.' => '$pagename$ 以下にマニュアルページを生成しました．',

    'Subpage name' => '子ページ名',
    'Subpage name (optional)' => '子ページ名(省略可)',

    'Continue reading' => '続きを読む',

    'Preferences' => '個人設定',
    'Browsing history' => '閲覧履歴',
    'Clear browsing history' => '閲覧履歴を消去',
    'Mark all pages as read' => 'すべてのページを既読にする',

    'Export' => 'エクスポート',
    'Export result' => 'エクスポートの結果',
    'Export finished.' => 'エクスポートが完了しました.',
    'Export function disabled.' => 'エクスポート機能は無効になっています．',
    'Invalid file name $filename$.' => '$filename$ は不正なファイル名です．',
    'Invalid directory name $filename$.' => '$filename$ は不正なディレクトリ名です．',
    'Export configuration table not found in $pagename$.' => '$pagename$ にエクスポート設定の表がありません．',
    'Invalid template page $template_pagename$ for $pagename$.' =>
    '$pagename$用のテンプレート $template_pagename$ はテンプレートとして使用できるページ種別ではありません．',
    'Template $template_pagename$ too large for $pagename$.' =>
    '$pagename$用のテンプレート $template_pagename$ はサイズが大きすぎます．',
    '$pagename$ too large.' => '$pagename$ のサイズが大きすぎます．',
    '$pagename$ is circular reference. ($pages$)' => '$pagename$ が循環参照されています．($pages$)',
    'File name' => 'ファイル名',
    'Template' => 'テンプレート',
    'Result' => '結果',
    'success' => '成功',
    'skip' => 'スキップ',
    'error' => '失敗',

    'Import pages from other wiki data' => '他のwikiからページをインポート',
    'Wiki type' => 'Wikiの種類',
    'Pages and histories will be overwritten by other wiki pages.' => '他のwikiのページによってページと履歴が上書きされます．',
    'Access analyzer' => 'アクセス解析',
    'Number of hits per page' => 'ページごとのアクセス数',
    'Number of hits' => 'アクセス数',
    'Total' => '合計',
    'Aggregate total' => '総計',
    'Today' => '今日',
    'Yesterday' => '昨日',
    '$num$ days ago' => '$num$日前',

    'Page URL' => 'ページURL',
    'Shorten URL' => '短縮URL',
    'Page shorten URL' => 'ページ短縮URL',
    
    'Cannot write page, disk usage exceeded.' => 'サイトの容量オーバーのためページの保存ができません．' // use in external script
    );

//define('LANGUAGE_SEARCH_SPLIT_REGEX', '/(\n|\b|\.|\,|．|，|。|、)/');
define('LANGUAGE_SEARCH_SPLIT_REGEX', '/(\n|．|，|。|、)/');
define('LANGUAGE_SEARCH_QUERY_REGEX',  '/(".*?"|[^\s]+)/');

function l($key) {
    global $LANGUAGE;
    if(isset($LANGUAGE[LANG][$key]))
	return $LANGUAGE[LANG][$key];
    else
	return $key;
}

function lf($key, $vars = array()) {
    return template_replace_variables(l($key), $vars);
}

function ld($dom, $key, $vars = array()) {
    template_replace_variables_by_dom($dom, l($key), $vars);
}

function language_sanitize($str) {
    return str_replace('　', ' ', $str);
}

?>
