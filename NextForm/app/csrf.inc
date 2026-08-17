<?php
/*
 * クロスサイトリクエストフォージェリ (CSRF) 対策。
 *
 * 上流には対策が 1 つも無かった。Referer / Origin / SameSite の検査は
 * コード全体に存在せず、さらに args_get() が $_GET + $_POST なので
 * GET だけでページを作ったり消したりできた。HTTP ダイジェスト認証は
 * ブラウザが資格情報を自動で付けるため、ログイン中の利用者が罠サイトを
 * 開くだけで成立する。<img src="…?ページ名&option=delete&action=write">
 * を 1 つ貼れば、それを見た人のページが消えた。
 *
 * 対策は 2 段:
 *
 *   1. 状態を変える action は POST でしか受け付けない
 *   2. その POST が同じサイトから来たことを Origin / Referer で確かめる
 *
 * 隠しトークンを全フォームに入れる方式は採らなかった。ダイジェスト認証には
 * セッションが無く置き場所に困ることと、42 個の option すべてに手を入れる
 * 必要があり、**1 つ書き漏らすとそこだけ無防備になる**ため。
 * ここはディスパッチの手前 1 箇所で、option を数え漏らしても守られる。
 */

/*
 * 状態を変えない action の一覧。
 *
 * 許可リストにしてあるのは、あとから option や handler を足したときに
 * 守り漏らさないため。**知らない action は既定で保護される**。
 * 新しい読み取り専用の action を足したら、ここにも足すこと。
 */
$CSRF_SAFE_ACTIONS = array(
    'show'         => true,	// 既定。表示
    'edit'         => true,	// 編集フォームを出すだけ。保存は write
    'source'       => true,	// 原文表示
    'summary'      => true,	// 要約表示
    'texts'        => true,	// 検索用のテキスト抽出
    'raw'          => true,	// 添付ファイルの実体
    'diff'         => true,	// 版の差分表示
    'page'         => true,	// 履歴のページ送り
    'only'         => true,	// 要約の絞り込み表示
    'list'         => true,	// 添付一覧 (JSON)
    'autocomplete' => true,	// 入力補完
    'check'        => true,	// 索引の検査。索引は書き換えない
    );

function csrf_action_is_safe($action) {
    global $CSRF_SAFE_ACTIONS;
    if($action === '' || $action === false || is_null($action))
	return true;
    return !empty($CSRF_SAFE_ACTIONS[$action]);
}

function csrf_request_method() {
    return isset($_SERVER['REQUEST_METHOD']) ? strtoupper($_SERVER['REQUEST_METHOD']) : '';
}

/*
 * URL からホスト部 (host:port) を取り出す。
 *
 * scheme は比べない。リバースプロキシの後ろだと $_SERVER['HTTPS'] が
 * 立たず、こちらが http と思い込んでブラウザの送る https の Origin と
 * 食い違う。scheme の不一致で守れるのは「同じホストの http と https」で、
 * それは既に経路を握られている状況なので、比べても得るものが無い。
 */
function csrf_url_host($url) {
    $host = @parse_url($url, PHP_URL_HOST);
    if($host === false || is_null($host) || $host === '')
	return false;
    $port = @parse_url($url, PHP_URL_PORT);
    return strtolower($host) . (empty($port) ? '' : ':' . $port);
}

/* このサイト自身のホスト部 */
function csrf_site_host() {
    if(BASE_URI !== '') {
	$host = csrf_url_host(BASE_URI);
	if($host !== false)
	    return $host;
    }
    if(empty($_SERVER['HTTP_HOST']))
	return false;
    return strtolower($_SERVER['HTTP_HOST']);
}

/*
 * この要求が同じサイトから来たか。
 *
 * Origin があればそれだけを見る (ブラウザが必ず正しく付けるため)。
 * 無ければ Referer で代替する。**どちらも無いものは通さない**。
 * 通してしまうと、Referer を落とす設定にした罠サイトから素通りになる。
 */
function csrf_is_same_origin() {
    $site_host = csrf_site_host();
    if($site_host === false)
	return false;

    if(!empty($_SERVER['HTTP_ORIGIN'])) {
	/* Origin: null は sandbox された iframe などから来る。通さない */
	$origin_host = csrf_url_host($_SERVER['HTTP_ORIGIN']);
	return $origin_host !== false && $origin_host === $site_host;
    }

    if(!empty($_SERVER['HTTP_REFERER'])) {
	$referer_host = csrf_url_host($_SERVER['HTTP_REFERER']);
	return $referer_host !== false && $referer_host === $site_host;
    }

    return false;
}

/*
 * 状態を変える要求を弾く。通らないときは 403 を返してその場で終わる。
 *
 * ディスパッチの直前 (option() と handle()) から呼ぶ。ここより後ろで
 * 弾いても、既に書き込みが始まっている可能性がある。
 */
function csrf_check($action) {
    if(CSRF_PROTECTION !== 'true')
	return true;
    if(csrf_action_is_safe($action))
	return true;

    if(csrf_request_method() !== 'POST') {
	csrf_reject(lf('\'$action$\' must be sent by POST.', array('action' => $action)));
	return false;
    }
    if(!csrf_is_same_origin()) {
	csrf_reject(l('The request did not come from this site.'));
	return false;
    }
    return true;
}

function csrf_reject($reason) {
    global $main_page_output;

    header('HTTP/1.1 403 Forbidden');

    theme_simple_screen_init();
    title_set_l('Request rejected');
    message_error($reason);

    $dom = dom_create_document();
    $p = dom_append_child($dom, 'p');
    dom_append_text($p, l('If you reached this page from a bookmark or a link, open the page and use the button on it.'));
    $main_page_output = dom_save_html($dom);

    theme_simple_screen();
    exit(0);
}
?>
