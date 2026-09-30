<?php
/*
 * 添付した HTML (text/html) を、開いたときに動かす。
 *
 * **既定では動かさない** (FILE_ALLOW_HTML)。無効のときは種類を登録しないので、
 * これまでどおり application/octet-stream のダウンロードになる
 * (file_get_content_type())。
 *
 * 動かすときは Content-Security-Policy: sandbox で隔離する。
 * wiki と同じオリジンの text/html として返すと、中のスクリプトは開いた人の
 * 資格情報で wiki にリクエストを送れる (ダイジェスト認証はブラウザが自動で
 * 付ける)。管理者が開けば、ページも設定も書き換えられる。sandbox の文書は
 * wiki とは別の (不透明な) オリジンになるので、
 *   - wiki のページや localStorage には触れない
 *   - wiki への POST は Origin: null になり、csrf_is_same_origin() が弾く
 * 許すのはスクリプトだけ。フォームの送信・ポップアップ・alert / prompt・
 * 最上位のページの移動は許さない (prompt はパスワードを尋ねる偽の画面に使える)。
 * **allow-same-origin を足してはいけない。** 足した瞬間に隔離が無くなる。
 *
 * 埋め込み (ファイルのページ・&include・Markdown の ![..](x.html)) は
 * 「開く」リンクだけにする。枠 (iframe) の中では動かさない。
 */
if(defined('FILE_ALLOW_HTML') && FILE_ALLOW_HTML === 'true') {
    $FILE_CONTENT_TYPE_HANDLER_HTML = array(
	'embed' => 'file_html_embed',
	'raw' => 'file_html_raw',
	);
    $FILE_CONTENT_TYPE_HANDLERS['text/html'] = &$FILE_CONTENT_TYPE_HANDLER_HTML;
}

function file_html_embed(&$page, $dom, $params = null) {
    $queries = array('action' => 'raw');
    if(!empty($page['backuptime']))
	$queries['backuptime'] = $page['backuptime'];

    $a = dom_append_link($dom, url_make($page['name'], $queries), l('Open'), 'open_html');
    dom_set_attribute($a, 'data-link-pagename', $page['name']);
    return $a;
}

/*
 * HTML の中で宣言した文字コード (<meta charset> / http-equiv の charset)。
 * 無ければ false。
 *
 * PHP は text/html に default_charset (UTF-8) を付けて送る。HTTP の charset は
 * <meta> より強いので、Shift_JIS などで書いた HTML が文字化けする。
 * ブラウザと同じく先頭の 1024 バイトだけを見る。値は文字コード名に使える
 * 文字だけを拾うので、改行などで見出しを増やされることはない。
 */
function file_html_charset(&$page) {
    $fp = page_open_contents($page);
    if($fp === false)
	return false;
    $head = fread($fp, 1024);
    fclose($fp);
    if(is_string($head) &&
       preg_match('/<meta[^>]*charset\s*=\s*["\']?\s*([A-Za-z0-9_.:-]+)/i', $head, $matches))
	return $matches[1];
    return false;
}

/*
 * Content-type / nosniff / inline は file_raw() が出している。
 * ここでは宣言された文字コードと隔離の指定を足してから中身を出す。
 */
function file_html_raw(&$page) {
    if(($charset = file_html_charset($page)) !== false)
	header('Content-type: text/html; charset=' . $charset);
    header('Content-Security-Policy: sandbox allow-scripts');
    page_output_contents($page);
}
?>
