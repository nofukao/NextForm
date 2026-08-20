<?php
/*
 * 組み込みマニュアル。
 *
 * マニュアルの正本は app/ の中のソースコードにある。manual_parse() が
 * manual/manual-before.txt・setup.inc・handler/wiki/function/*.inc・
 * plugin/*.inc に埋め込まれた注釈を集めて組み立てる。
 *
 * 0.6.0 までは、これを ?option=admin_manual で storage の一般ページとして
 * 焼き付けていた。焼き付け先が storage なのでアップグレードの対象外になり、
 * 古いマニュアルが残り続けた。管理者が 1 回押さないと存在せず、
 * アンロックすれば編集もできてしまった。
 *
 * 0.7.0 で焼き付けをやめた。MANUAL_PAGENAME 以下は実体を持たない
 * 読み取り専用の仮想ページで、page.inc が解決する。正本は app/ の 1 つだけに
 * なり、アップグレードで app/ が入れ替わればマニュアルも自動的に新しくなる。
 * 取り残される生成物が存在しない。
 */
$TOOLS['site']['manual'] = array(
    'order' => 50,
    'show' => 'manual_tool'
    );

/* 解決したマニュアル一式の保存先 (storage/cache/) */
define('MANUAL_CACHE_KEY', 'manual_pages');
/* 索引に載せた ngram の保存先。次に作り直すときの「更新前」になる */
define('MANUAL_INDEX_CACHE_KEY', 'manual_index');

/*
 * リクエスト内のメモ。
 *
 * page_is_exists() はリンク 1 本ごとに呼ばれるので、そのたびに
 * storage のキャッシュを読み直すわけにいかない。
 * sentence/sentence_pagename.inc と同じやり方。
 */
$MANUAL_PAGES_CACHE = null;
$MANUAL_SOURCE_MTIME_CACHE = null;

function manual_tool(&$page, $dom) {
    $manual_page_name = MANUAL_PAGENAME . '/' . LANG;
    if(page_is_exists($manual_page_name)) {
	tools_add_link(
	    $dom,
	    array('name' => l('Manual'),
		  'target' => '_blank',
		  'seo_follow' => true,
		  'rel' => 'help',
		  'uri' => url_make($manual_page_name)));
    } else {
	/*
	 * その言語のマニュアルが無いときの行き先。組み込みになったので
	 * 通常は起こらないが、LANG に対応する節がまだ書かれていない場合に残る。
	 */
	tools_add_link(
	    $dom,
	    array('name' => l('Manual') . ' (' . l('official site') .')',
		  'target' => '_blank',
		  'seo_follow' => true,
		  'rel' => 'help',
		  'uri' => OFFICIAL_URL));
    }
}

/*
 * マニュアルの元になるファイルの一覧。
 * 組み立てる順がそのままページ内の並び順になる。
 */
function manual_source_files() {
    $manual_files = array();
    $manual_files[] = APP_DIR_PATH . '/manual/manual-before.txt';
    $manual_files[] = APP_DIR_PATH . '/setup.inc';
    $manual_files = array_merge($manual_files, glob_array(APP_DIR_PATH . '/handler/wiki/function/*.inc'));
    $manual_files = array_merge($manual_files, glob_array(PLUGIN_DIR_PATH . '/*.inc'));
    $manual_files[] = APP_DIR_PATH . '/manual/manual-after.txt';
    return $manual_files;
}

/*
 * 元になるファイルのうち最も新しい更新時刻。
 *
 * キャッシュを作り直すかどうかの判定に使う。アップグレードで app/ が
 * 入れ替わればここが動くので、利用者が何もしなくても追随する。
 */
function manual_source_mtime() {
    global $MANUAL_SOURCE_MTIME_CACHE;
    if($MANUAL_SOURCE_MTIME_CACHE !== null)
	return $MANUAL_SOURCE_MTIME_CACHE;

    $mtime = 0;
    foreach(manual_source_files() as $manual_file) {
	$file_mtime = @filemtime($manual_file);
	if($file_mtime !== false && $file_mtime > $mtime)
	    $mtime = $file_mtime;
    }
    $MANUAL_SOURCE_MTIME_CACHE = $mtime;
    return $mtime;
}

/*
 * 解決したマニュアル一式を返す。`ページ名 => array('body', 'mtime')`。
 *
 * 元のファイルを読んで正規表現と eval を回す処理なので、storage/cache/ に
 * 持たせる。元のファイルの更新時刻か NextForm の版数が変わったら作り直す。
 */
function manual_get_pages() {
    global $MANUAL_PAGES_CACHE;
    if($MANUAL_PAGES_CACHE !== null)
	return $MANUAL_PAGES_CACHE;

    if(MANUAL_PAGENAME === '') {
	$MANUAL_PAGES_CACHE = array();
	return $MANUAL_PAGES_CACHE;
    }

    $source_mtime = manual_source_mtime();
    $cached = cache_serialized_get('', MANUAL_CACHE_KEY);
    if(is_array($cached) && isset($cached['pages']) &&
       isset($cached['source_mtime']) && $cached['source_mtime'] == $source_mtime &&
       isset($cached['version']) && $cached['version'] === NEXTFORM_VERSION) {
	$MANUAL_PAGES_CACHE = $cached['pages'];
	return $MANUAL_PAGES_CACHE;
    }

    $pages = array();
    foreach(manual_parse() as $pagename => $manual_page) {
	$pages[$pagename] = array(
	    'body' => $manual_page['body'],
	    'mtime' => $manual_page['mtime']);
    }

    /*
     * 索引を作る前にメモへ入れる。**順番を入れ替えてはいけない。**
     * search_page_ngram() は本文を変換するので、その途中で [[...]] の
     * 判定のために page_is_exists() が呼ばれ、ここへ戻ってくる。
     * メモが空のままだと無限に再入する。ページ名の集合はこの時点で
     * 確定しているので、再入しても答えは正しい。
     */
    $MANUAL_PAGES_CACHE = $pages;

    /*
     * 焼き付けていた頃は、ここで wiki_normalize() を通していた
     * (表の桁を揃える整形)。もう通さない。
     *
     * 整形が直すのは**本文の見た目**で、描画される HTML は変わらない。
     * 直す相手は「あとで編集する人」だが、マニュアルは編集されない。
     *
     * それに、通すと警告が出る。マニュアルのチートシートは他のマニュアル
     * ページを &include しており、wiki_normalize() は取り込んだ側の表の
     * 位置を自分の本文に当ててしまう (handler/wiki.inc の
     * //table[@data-twp] の走査)。焼き付けていた頃は取り込み先の
     * ページがまだ無く、&include が空振りしていたので表面化しなかった。
     */

    /* cache_serialized_create() は第 3 引数を参照で取るので、変数に入れて渡す */
    $entry = array('version' => NEXTFORM_VERSION,
		   'source_mtime' => $source_mtime,
		   'pages' => $pages);
    cache_serialized_create('', MANUAL_CACHE_KEY, $entry);

    /*
     * 編集画面の入力補助はチートシートのページから作られている
     * (option/helper.inc)。有効期限を持たないキャッシュなので、
     * マニュアルを作り直したらここで落とす。
     * cache_delete() は前方一致なので 'wiki-helper-info-completion' に届く。
     */
    cache_delete('', 'wiki-helper-info');

    manual_index_update($pages);

    return $MANUAL_PAGES_CACHE;
}

/*
 * 検索の索引を、マニュアルの分だけ差し替える。
 *
 * 組み込み検索の索引は page_write() のときにしか更新されない。
 * マニュアルは書き込まれないので、キャッシュを作り直したこの瞬間が
 * 唯一の更新の機会になる。やっていることは page_write() と同じで、
 * 「更新前の ngram」と「更新後の ngram」の差分を取る。
 * 更新前の ngram は前回ここで保存したものを使う。
 */
function manual_index_update(&$pages) {
    /*
     * 索引を作るには全ページの本文を読む。読む権限が無い利用者の
     * リクエストで走ると auth_check() がその場で終了させてしまうので、
     * 読めないときは何もしない。?option=search_index の再構築が拾う。
     */
    if(!auth_has_permission(MANUAL_PAGENAME, 'read'))
	return;

    $old_ngrams = cache_serialized_get('', MANUAL_INDEX_CACHE_KEY);
    if(!is_array($old_ngrams))
	$old_ngrams = array();

    $new_ngrams = array();
    foreach($pages as $pagename => $manual_page) {
	$page = manual_page_create($pagename, $manual_page);
	if(SEARCH_ENGINE === 'builtin') {
	    $ngrams = search_page_ngram($page);
	    $old = isset($old_ngrams[$pagename]) ? $old_ngrams[$pagename] : array();
	    search_index_update($pagename, $old, $ngrams);
	    $new_ngrams[$pagename] = $ngrams;
	} else {
	    search_engine_index_update($page);
	}
    }

    /* 前回はあって今回は無いページ (節ごと消えた場合) を索引から落とす */
    foreach($old_ngrams as $pagename => $ngrams) {
	if(isset($new_ngrams[$pagename]))
	    continue;
	if(SEARCH_ENGINE === 'builtin')
	    search_index_remove($pagename, $ngrams);
    }

    if(SEARCH_ENGINE === 'builtin')
	cache_serialized_create('', MANUAL_INDEX_CACHE_KEY, $new_ngrams);
}

/*
 * 0.6.0 以前に焼き付けたマニュアルのページを片付ける。
 *
 * 焼き付けた写しは storage に残るが、表示には組み込みの方が使われる。
 * 中身が出ないのに全ページ一覧には名前が出るので、アップグレードのときに
 * 消す。消えるのは storage の写しだけで、組み込みのマニュアルは残る。
 *
 * 消すのは**生成されたときの印が残っているものだけ**。manual_build() は
 * lock と lock_user=admin を書いていた。ロックを外して手を入れたページは
 * 利用者のものなので残し、名前を $kept に入れて返す。
 * 利用者が MANUAL_PAGENAME の下に自分で作ったページ (組み込みに同名が
 * 無いもの) は、この一覧に出てこないので初めから触らない。
 *
 * page_delete() は通せない。CLI には利用者がおらず、その先の auth_check()
 * がその場で終了させてしまう。ロックの確認も要らない (ロックされている
 * ことが、まさに焼き付けた印なので)。storage の論理削除を直接呼ぶ。
 * 版は残るので「削除されたページ」から戻せる。
 *
 * 索引はページ名で引く。写しと組み込みは同じページ名なので、写しの分だけを
 * 索引から抜くことはできない。組み込みの分は manual_index_update() が
 * 入れ直すので、残るのは「昔のマニュアルにあって今は無い語」だけ。
 * 気になるなら ?option=search_index で作り直す。
 */
function manual_cleanup_generated(&$kept = array()) {
    $kept = array();
    $removed = 0;
    foreach(array_keys(manual_get_pages()) as $pagename) {
	if(!storage_page_is_exists($pagename))
	    continue;
	$page = page_create($pagename);
	storage_page_read($page);
	if(empty($page['meta']['lock']) ||
	   default_value($page['meta']['lock_user'], '') !== 'admin') {
	    $kept[] = $pagename;
	    continue;
	}
	if(storage_page_delete($page)) {
	    cache_delete($pagename);
	    cache_delete($pagename, 'page_find_', true);
	    $removed++;
	}
    }
    if($removed > 0)
	cache_delete('', 'page_find_');
    return $removed;
}

/*
 * マニュアルのページを 1 件、$page の形に組み立てる。
 * 形は storage_page_read_meta() が作るものに合わせる。
 */
function manual_page_create($pagename, $manual_page) {
    $page = page_create($pagename);
    $page['meta']['type'] = 'wiki';
    /*
     * 施錠されているものとして見せる。編集の UI が今までと同じ見え方になる。
     * 実際の書き込みは page.inc が名前空間で弾く。
     */
    $page['meta']['lock'] = true;
    $page['meta']['lock_user'] = 'admin';
    /*
     * ticket は入れない。編集フォームが往復できなくなり、
     * 書き込みの経路が 1 つ塞がる。
     */
    $page['manual_body'] = $manual_page['body'];
    $page['meta_size'] = 0;
    $page['contents_size'] = strlen($manual_page['body']);
    $page['mtime'] = $manual_page['mtime'];
    $page['is_exists'] = true;
    return $page;
}

function manual_page_is_exists($pagename) {
    $pages = manual_get_pages();
    return isset($pages[$pagename]);
}

function manual_page_read(&$page) {
    $pages = manual_get_pages();
    if(!isset($pages[$page['name']]))
	return false;
    $page = manual_page_create($page['name'], $pages[$page['name']]);
    return true;
}

/*
 * 本文を読み出すストリーム。
 * 実体のファイルが無いので、メモリ上のストリームを渡す。
 */
function manual_page_open_contents(&$page) {
    if(!isset($page['manual_body'])) {
	$pages = manual_get_pages();
	if(!isset($pages[$page['name']]))
	    return false;
	$page['manual_body'] = $pages[$page['name']]['body'];
    }
    $fp = fopen('php://memory', 'r+b');
    if($fp === false)
	return false;
    fwrite($fp, $page['manual_body']);
    rewind($fp);
    return $fp;
}

function manual_page_get_mtime($pagename) {
    $pages = manual_get_pages();
    if(!isset($pages[$pagename]))
	return 0;
    return $pages[$pagename]['mtime'];
}

/*
 * マニュアルのページ名を page_find() と同じ形で返す。
 *
 * page_find() 本体には混ぜない。混ぜると ?option=allpage や &pages や
 * ページ名の自動リンクにマニュアルが全部出てきてしまう。
 * これを使うのは検索の 2 箇所 (索引の再構築と整合検査) だけ。
 */
function manual_page_find() {
    $found = array();
    foreach(manual_get_pages() as $pagename => $manual_page)
	$found[] = array('name' => $pagename, 'mtime' => $manual_page['mtime']);
    return $found;
}

function manual_parse() {
    $manual_files = manual_source_files();

    $manual_pages = array();

    $variables = array(
	'manualpage' => MANUAL_PAGENAME,
	'comment_begin' => '/*',
	'comment_end' => '*/',
	'bq' => '`');
    
    foreach($manual_files as $manual_file) {
	$contents = file_get_contents($manual_file);
	$mtime = filemtime($manual_file);
	if(preg_match_all('|/\*\*(.*?)\*/|s', $contents, $matches) > 0) {
	    foreach($matches[1] as $text) {
		$text = preg_replace_callback('/`(.*)`/', 'manual_replace', $text);
		$text = template_replace_variables($text, $variables);

		$body = '';
		$vars = array();
		$lines = explode("\n", $text);
		foreach($lines as $line) {
		    if(preg_match('|^\s*[@\\\\](\w+)\s+(.*)\s*$|', $line, $matches)) {
			$vars[$matches[1]] = $matches[2];
		    } else {
			$body .= $line . "\n";
		    }
		}
		$body = trim($body) . "\n";
		if(!empty($vars['manual'])) {
		    if(empty($manual_pages[$vars['manual']])) {
			$manual_pages[$vars['manual']] = array(
			    'vars' => $vars,
			    'body' => $body);
		    } else {
			$manual_pages[$vars['manual']]['body'] .= $body;
			$manual_pages[$vars['manual']]['vars'] =
			    array_merge($manual_pages[$vars['manual']]['vars'], $vars);
		    }

		    if(isset($manual_pages[$vars['manual']]['mtime'])) {
			if($mtime > $manual_pages[$vars['manual']]['mtime'])
			    $manual_pages[$vars['manual']]['mtime'] = $mtime;
		    } else {
			$manual_pages[$vars['manual']]['mtime'] = $mtime;
		    }
		}
	    }
	}
    }

    return $manual_pages;
}

function manual_replace($var) {
    global $WIKI_ALIASES;
    return eval('return ' . $var[1] . ';');
}

?>
