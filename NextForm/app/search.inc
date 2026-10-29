<?php
$SEARCH_SUBSTR = function_exists('mb_substr') ? 'mb_substr' : 'substr';
$SEARCH_STRLEN = function_exists('mb_strlen') ? 'mb_strlen' : 'strlen';
define('SEARCH_NGRAM_NUMBER', 2);

/*
 * 索引のファイルは storage/cache/ にこの接頭辞 + 1 バイトの名前で置かれる。
 * 再構築中は別の接頭辞に書き、最後にまとめて付け替える。こうすると
 * 再構築が途中で止まっても、それまでの索引がそのまま使われ続ける。
 * 2 つの接頭辞は、片方がもう片方の前方一致にならないようにすること
 * (cache_get_keys_prefix() が前方一致で拾うため)。
 */
define('SEARCH_INDEX_KEY_PREFIX', 'search_index_');
define('SEARCH_INDEX_REBUILD_KEY_PREFIX', 'search_rebuild_');

$SEARCH_ENGINES = array();
require_once(APP_DIR_PATH . '/search/elasticsearch.inc');

/*
 * 利用者が書いた正規表現を 1 回呼ぶときの pcre.backtrack_limit。
 * (.*)*x のような式は、既定の 100 万では 1 回で数ミリ秒〜数十ミリ秒かかり、
 * 全ページに掛けると数秒になる。上限に当たった塊は「一致しない」とする。
 */
define('SEARCH_REGEX_BACKTRACK_LIMIT', 20000);
/* 開いたページで、正規表現の一致から作る印の語の数 (種類) の上限 */
define('SEARCH_REGEX_HIGHLIGHT_MAX', 100);

/*
 * 検索する。$query は search_query_parse() の戻り値。
 * 今までの平らな語の配列 (search_parse_query() の戻り値) も受け取る。
 *
 * $search_options:
 *   manual  'without' (マニュアルを含めない) / 'with' (含める、既定) / 'only' (マニュアルだけ)
 *           画面は既定で 'without' を渡す。ここの既定が 'with' なのは、
 *           今まで (is_ignore_system_page を渡さないと含めた) と揃えるため
 *   is_ignore_system_page  今までの書き方。真なら 'without'
 *   order   'score' / 'mtime'
 *
 * 問い合わせの誤り・案内は $query['errors'] に、打ち切ったことは
 * $query['is_partial'] に入る (平らな配列で呼んだときは入らない)。
 */
function search(&$query, $search_options = array()) {
    if(isset($query['clauses']))
	$parsed = &$query;
    else
	$parsed = search_query_from_words($query);

    if(SEARCH_ENGINE === 'builtin')
	return search_builtin($parsed, $search_options);
    else if(($engine = search_engine()) !== false) {
	/*
	 * 新しい書き方は builtin だけ。記号を落として渡すと「含まない」が
	 * 「含む」になるなど、意味が変わるので、問い合わせずに知らせる。
	 */
	if(!empty($parsed['operators'])) {
	    $parsed['errors'][] = l('This search engine does not support this way of writing.');
	    return array();
	}
	$words = search_query_words($parsed);
	return $engine['search']($words, $search_options);
    }
}

/*
 * マニュアルを探す範囲。'without' / 'with' / 'only'。
 */
function search_manual_scope($search_options) {
    if(isset($search_options['manual']) &&
       in_array($search_options['manual'], array('without', 'with', 'only'), true))
	return $search_options['manual'];
    if(!empty($search_options['is_ignore_system_page']))
	return 'without';
    return 'with';
}

function search_scope_includes($scope, $pagename) {
    if($scope === 'with')
	return true;
    $is_manual = page_is_system_page($pagename);
    return $scope === 'only' ? $is_manual : !$is_manual;
}

/* 正規表現 (re:) を使えるのはログインした人だけ (設計メモ §4 の 2) */
function search_regex_is_allowed() {
    return auth_get_user() !== false;
}

function search_builtin(&$query, $search_options = array()) {
    $search_results = array();
    $scope = search_manual_scope($search_options);

    if(!empty($query['errors']))
	return $search_results;
    if(!empty($query['has_regex']) && !search_regex_is_allowed()) {
	$query['errors'][] = l('Regular expressions can be used after logging in.');
	return $search_results;
    }
    if(empty($query['clauses']) && $query['after'] === null && $query['before'] === null)
	return $search_results;

    /*
     * 候補を索引で絞る。「含む」語だけで正規表現を含まない節は、語ごとの
     * 候補の和 (OR) を取り、そういう節どうしは交差 (AND) を取る。今の
     * 書き方 (語を並べるだけ) は必ずここを通り、今までと同じ候補になる。
     */
    $candidates = null;
    foreach($query['clauses'] as $clause) {
	if(!search_clause_is_indexable($clause))
	    continue;
	$union = array();
	foreach($clause as $term)
	    $union += search_word_pages($term['value']);
	$candidates = $candidates === null ? $union : array_intersect_key($candidates, $union);
    }

    /*
     * 絞れる節が無いとき (含まない・正規表現・日時だけ) は全ページを読む。
     * 実測 (926 ページ) でキャッシュがあれば 50 ms 前後。大きなサイトや
     * 重い正規表現に備えて時間の上限を掛ける。
     */
    $deadline = null;
    if($candidates === null) {
	$candidates = array();
	foreach(search_index_target_pages() as $found)
	    $candidates[$found['name']] = true;
	$deadline = microtime(true) + (float)SEARCH_TIME_LIMIT;
    }

    foreach($candidates as $pagename => $dummy) {
	$pagename = (string)$pagename;
	if(!search_scope_includes($scope, $pagename))
	    continue;
	if($deadline !== null && microtime(true) > $deadline) {
	    $query['is_partial'] = true;
	    break;
	}
	$result = search_page_match($pagename, $query, $deadline);
	if($result === null) {
	    $query['is_partial'] = true;
	    break;
	}
	if($result !== false)
	    $search_results[$pagename] = $result;
    }
    if(isset($search_options['order']) && $search_options['order'] === 'mtime')
	uasort($search_results, 'search_result_compare_mtime');
    else
	uasort($search_results, 'search_result_compare_score');

    return $search_results;
}

function search_clause_is_indexable($clause) {
    foreach($clause as $term) {
	if($term['not'] || $term['type'] !== 'text')
	    return false;
    }
    return true;
}

function search_result_compare_score($result_a, $result_b) {
    if($result_a['score'] > $result_b['score'])
	return -1;
    if($result_a['score'] < $result_b['score'])
	return 1;
    return strcmp($result_a['pagename'], $result_b['pagename']);
}

function search_result_compare_mtime($result_a, $result_b) {
    if($result_a['mtime'] > $result_b['mtime'])
	return -1;
    if($result_a['mtime'] < $result_b['mtime'])
	return 1;
    return strcmp($result_a['pagename'], $result_b['pagename']);
}

/*
 * その語の 2-gram が全部入っているページ (候補)。`ページ名 => true`。
 * 1 文字の語は、その文字で始まる 2-gram のどれかを持つページ。
 */
function search_word_pages($word) {
    global $SEARCH_SUBSTR, $SEARCH_STRLEN;
    
    $candidate1_pagenames = array();

    $word = strtolower($word);
    $word_length = $SEARCH_STRLEN($word);
    if($word_length >= SEARCH_NGRAM_NUMBER) {
	$ngrams = array();
	$group = array();
	search_word_to_ngrams_for_query($word, $ngrams);
	search_ngrams_group_by_key($ngrams, $group);
	$match_page_count = count($ngrams);
	foreach($group as $key => &$ngrams) {
	    $cachekey = SEARCH_INDEX_KEY_PREFIX . $key;
	    $index = cache_serialized_get('', $cachekey);
	    if($index !== false) {
		foreach($ngrams as $ngram) {
		    if(!empty($index[$ngram])) {
			foreach($index[$ngram] as $pagename => &$dummy) {
			    if(empty($candidate1_pagenames[$pagename]))
				$candidate1_pagenames[$pagename] = 1;
			    else
				$candidate1_pagenames[$pagename]++;
			}
		    }
		}
	    }
	}
    } else {
	$key = search_ngram_key($word);
	$cachekey = SEARCH_INDEX_KEY_PREFIX . $key;
	$match_page_count = 1;
	$index = cache_serialized_get('', $cachekey);
	if($index !== false) {
	    foreach($index as $ingram => &$ipages) {
		if($SEARCH_SUBSTR($ingram, 0, $word_length) == $word) {
		    foreach($ipages as $pagename => &$dummy) {
			$candidate1_pagenames[$pagename] = 1;
		    }
		}
	    }
	}
    }
    $pages = array();
    foreach($candidate1_pagenames as $pagename => $count) {
	if($count == $match_page_count)
	    $pages[$pagename] = true;
    }
    return $pages;
}

function search_page_ngram(&$page) {
    $texts_info = search_page_texts($page);
    $texts = $texts_info['body'];
    if(isset($texts_info['title']) && $texts_info['title'] !== '')
	$texts[] = $texts_info['title'];
    if(isset($texts_info['description']) && $texts_info['description'] !== '')
	$texts[] = $texts_info['description'];
    if(isset($texts_info['tag']) && $texts_info['tag'] !== '')
	$texts[] = $texts_info['tag'];
    return search_texts_to_ngrams($texts);
}

function search_page_index_add(&$page, $key_prefix = SEARCH_INDEX_KEY_PREFIX) {
    //auth_check(false, 'admin');
    $ngrams = search_page_ngram($page);
    search_index_add($page['name'], $ngrams, $key_prefix);
}

/*
 * 索引のファイルを 1 つ読む。
 *
 * 戻り値 false は「ファイルはあるのに中身が取れなかった」ことだけを表す。
 * このとき呼び出し側は絶対に書き戻してはいけない。読めなかったものを空と
 * みなして書き戻すと、そのファイルに入っていた全ページ分の登録が、
 * 保存した 1 ページ分に置き換わって消える。
 *
 * cache_serialized_get() は「ファイルが無い」も「読めない」も同じ false を
 * 返してしまうので、ここではファイルの有無を別に見て区別する。
 * ファイルが無いのは正常 (まだ 1 度も書かれていないだけ)。
 */
function search_index_read($cachekey, &$index) {
    $index = array();
    $data = cache_get('', $cachekey);
    if($data === false)
	return cache_get_as_filepath('', $cachekey) === false;
    $index = @unserialize($data);
    if($index === false) {
	$index = array();
	return false;
    }
    return true;
}

/*
 * 読めない索引に出会ったことを知らせる。黙って進むと、検索結果から
 * ページが減っていることに誰も気付けない。画面には 1 回だけ出す。
 */
function search_index_warn_broken($cachekey) {
    global $SEARCH_INDEX_IS_WARNED;
    error_log(sprintf('search index is unreadable: cachekey=%s', bin2hex($cachekey)));
    if(!empty($SEARCH_INDEX_IS_WARNED))
	return;
    $SEARCH_INDEX_IS_WARNED = true;
    message_error(l('The search index could not be read. Rebuild it from the admin page.'));
}

function search_index_add($pagename, &$ngrams, $key_prefix = SEARCH_INDEX_KEY_PREFIX) {
    $group = array();
    search_ngrams_group_by_key($ngrams, $group);
    foreach($group as $key => &$ngrams) {
	$cachekey = $key_prefix . $key;
	if(!search_index_read($cachekey, $index)) {
	    search_index_warn_broken($cachekey);
	    continue;
	}
	foreach($ngrams as $ngram) {
	    $index[$ngram][$pagename] = true;
	}
	cache_serialized_create('', $cachekey, $index);
    }
}

function search_index_remove($pagename, &$ngrams) {
    $group = array();
    search_ngrams_group_by_key($ngrams, $group);
    foreach($group as $key => &$ngrams) {
	$cachekey = SEARCH_INDEX_KEY_PREFIX . $key;
	if(!search_index_read($cachekey, $index)) {
	    search_index_warn_broken($cachekey);
	    continue;
	}
	foreach($ngrams as $ngram) {
	    unset($index[$ngram][$pagename]);
	}
	cache_serialized_create('', $cachekey, $index);
    }
}

function search_index_update($pagename, &$old_ngrams, &$new_ngrams) {
    $remove_ngrams = array_diff_assoc($old_ngrams, $new_ngrams);
    $add_ngrams = array_diff_assoc($new_ngrams, $old_ngrams);
    search_index_add($pagename, $add_ngrams);
    search_index_remove($pagename, $remove_ngrams);
}

/*
 * 再構築で作った索引を、使われている索引と入れ替える。
 *
 * rename(2) は置き換えなので、入れ替えの途中で索引が空になる瞬間が無い。
 * 先に全部消してから作り直すやり方だと、再構築が中断された時点で
 * 索引が空のまま残り、何もしなかった場合より悪い状態になっていた。
 *
 * 今回の再構築で 1 件も入らなかった古いファイル (収録していた語がすべて
 * ページから消えた場合) は、入れ替えの後で消す。ただし 1 つも入れ替えて
 * いないときは何もしない。再構築が空振りしただけの状況で、生きている
 * 索引を消してしまわないため。
 */
function search_index_rebuild_swap() {
    $swapped = array();
    foreach(cache_get_keys_prefix('', SEARCH_INDEX_REBUILD_KEY_PREFIX) as $cachekey) {
	$to_cachekey = SEARCH_INDEX_KEY_PREFIX .
	    substr($cachekey, strlen(SEARCH_INDEX_REBUILD_KEY_PREFIX));
	if(cache_rename('', $cachekey, $to_cachekey))
	    $swapped[$to_cachekey] = true;
    }
    if(empty($swapped))
	return 0;

    foreach(cache_get_keys_prefix('', SEARCH_INDEX_KEY_PREFIX) as $cachekey)
	if(!isset($swapped[$cachekey]))
	    cache_delete('', $cachekey);
    return count($swapped);
}

/*
 * 索引を 1 回なめて集計する。$is_deep が真ならページごとの ngram 集合も作る。
 * ページ名が数字だけだと PHP の配列キーが整数になるので文字列に戻している。
 */
function search_index_collect($is_deep = false) {
    $collected = array(
	'file_num' => 0,
	'bytes' => 0,
	'ngram_num' => 0,
	'posting_num' => 0,
	'indexed_pages' => array(),
	'broken' => array(),
	'page_ngrams' => array(),
	);
    foreach(cache_get_keys_prefix('', SEARCH_INDEX_KEY_PREFIX) as $cachekey) {
	$collected['file_num']++;
	$filepath = cache_get_as_filepath('', $cachekey);
	if($filepath !== false)
	    $collected['bytes'] += filesize($filepath);

	$index = array();
	if(!search_index_read($cachekey, $index)) {
	    $collected['broken'][$cachekey] = true;
	    continue;
	}
	foreach($index as $ngram => $pagenames) {
	    $collected['ngram_num']++;
	    foreach($pagenames as $pagename => $dummy) {
		$pagename = (string)$pagename;
		$collected['indexed_pages'][$pagename] = true;
		if($is_deep)
		    $collected['page_ngrams'][$pagename][$ngram] = true;
		$collected['posting_num']++;
	    }
	}
    }
    return $collected;
}

/*
 * 索引と実際のページを突き合わせる。
 *
 * $is_deep が偽のときはページ名の集合だけを比べる。速い (実測 967 ページで
 * 0.13 秒) が、**部分的な欠けは見つけられない**。1 ページ分の ngram は複数の
 * ファイルに分かれて入るので、1 ファイルを失ってもページ自体は他のファイルに
 * 残り、集合としては一致してしまう。実際に起きる壊れ方はこの部分的な欠けなので、
 * 「壊れている」ことは言えても「壊れていない」ことは言えない。
 *
 * $is_deep が真なら全ページの本文から ngram を作り直して 1 件ずつ比べる。
 * 実測で wiki ページ 1 件あたり約 1.8ms (967 ページで 0.77 秒)。
 * 再構築と違って索引を書かないぶん、再構築よりずっと安い。
 */
/*
 * 索引に載せるページの一覧。
 *
 * storage のページに、組み込みマニュアル (実体を持たないので page_find() には
 * 出ない) を足したもの。**再構築と整合検査は必ず同じ一覧を見ること。**
 * 別々に組み立てると、片方がマニュアルを忘れた瞬間に、もう片方が
 * 「索引にあるのに実在しない」あるいは「実在するのに索引に無い」と報告する。
 */
function search_index_target_pages() {
    return array_merge(page_find('', array('is_pagename_only' => true)),
		       manual_page_find());
}

function search_index_check($is_deep = false) {
    $check = search_index_collect($is_deep);

    $real_pages = array();
    foreach(search_index_target_pages() as $page)
	$real_pages[$page['name']] = true;

    $check['is_deep'] = $is_deep;
    $check['page_num'] = count($real_pages);
    $check['missing'] = array_diff_key($real_pages, $check['indexed_pages']);
    $check['ghosts']  = array_diff_key($check['indexed_pages'], $real_pages);
    $check['partial'] = array();
    $check['stale']   = array();

    if($is_deep) {
	foreach($real_pages as $pagename => $dummy) {
	    $page = page_read($pagename);
	    $should = search_page_ngram($page);
	    $actual = isset($check['page_ngrams'][$pagename]) ?
		$check['page_ngrams'][$pagename] : array();
	    $lack = count(array_diff_key($should, $actual));
	    $over = count(array_diff_key($actual, $should));
	    if($lack > 0)
		$check['partial'][$pagename] = $lack;
	    if($over > 0)
		$check['stale'][$pagename] = $over;
	}
    }

    $check['is_ok'] =
	empty($check['broken']) && empty($check['missing']) &&
	empty($check['ghosts']) && empty($check['partial']) && empty($check['stale']);
    return $check;
}

/*
 * 1 ページ分の ngram を入れ直す。差分ではなく全部を入れるので、
 * 更新が失われて欠けたページを、全体の再構築なしに直せる。
 * 余分な登録は消さない (検索結果は本文で照合し直すので実害が無い)。
 */
function search_index_repair($pagename) {
    $page = page_read($pagename);
    if(empty($page['is_exists']))
	return false;
    search_page_index_add($page);
    return true;
}

function search_engine() {
    global $SEARCH_ENGINES;
    if(isset($SEARCH_ENGINES[SEARCH_ENGINE]))
	return $SEARCH_ENGINES[SEARCH_ENGINE];
    else
	return false;
}

function search_engine_index_update($page) {
    if(($engine = search_engine()) !== false)
	$engine['index_update']($page);
}
function search_engine_index_delete($page) {
    if(($engine = search_engine()) !== false)
	$engine['index_delete']($page);
}
function search_engine_index_clear() {
    if(($engine = search_engine()) !== false)
	$engine['index_clear']();
}


function search_page_texts(&$page) {
    auth_check($page['name'], 'read');
    /*
     * 実在しないページでは 'title' も入れない。
     *
     * ここに 'title' => ページ名 を足したくなるが、**足すと索引が壊れる**。
     * 索引の更新は「保存前の ngram」と「保存後の ngram」の差分で行う
     * (search_index_update)。新規ページの「保存前」はこの経路を通るので、
     * 名前の ngram が両側に現れて相殺され、索引に入らなくなる。
     * 読む側が 'title' の無い配列に備えること (search_page_match を参照)。
     */
    if(empty($page['is_exists']))
	return array('body' => array());

    unset($page['title']);
    unset($page['title_dom']);
    $texts_func = handler_function($page, 'texts');
    if($texts_func !== false)
	$texts = $texts_func($page);
    else
	$texts = array('body' => array());
    if(isset($page['meta']['title']))
	$texts['title'] = trim(str_replace("\n", ' ', $page['meta']['title'])) . ' ' . $page['name'];
    else
	$texts['title'] = $page['name'];

    if(isset($page['meta']['description']))
	$texts['description'] = $page['meta']['description'];

    $tags = tag_get_page_tags($page);
    if(!empty($tags))
	$texts['tag'] = list_to_string($tags);

    /*
     * 全角スペースを問い合わせ側と揃える。
     *
     * search_parse_query() は language_sanitize() を通すのに、索引を作る側
     * (search_page_ngram) と照合する側 (search_page_match) は通していなかった。
     * そのため "設定　方法" のように全角スペースを挟んだフレーズは、本文に
     * そのとおり書いてあっても ngram が一致せず、必ず 0 件になっていた。
     *
     * この関数の出力が索引側と照合側の両方の入力なので、ここで 1 度揃えれば
     * 3 者が同じ文字列を見ることになる。
     *
     * 既にある索引は全角スペースのまま作られているため、この変更の効果が
     * 出るのは再構築のあと。それまでは search_index_check --deep が
     * ずれとして報告する。
     */
    foreach($texts as $key => &$value) {
	if(is_array($value)) {
	    foreach($value as &$text)
		$text = language_sanitize($text);
	    unset($text);
	} else {
	    $value = language_sanitize($value);
	}
    }
    unset($value);
    return $texts;
}

/*
 * 検索の照合に使うテキストを、ページごとにキャッシュして返す。
 *
 * search_page_match() は候補ページごとに wiki_texts() で本文をフルパース
 * している (字句解析 → 構文解析 → DOM 構築)。実測 (1372 ページ) で
 * 検索時間の 92〜95% がここに使われており、索引の引き込みは 0.1〜2.9ms
 * でしかなかった。**遅いのは索引ではなく、そのあとの本文の再パース。**
 *
 * 有効かどうかはページの mtime だけで決める。page_read() が入れた値を
 * そのまま使うので、判定のための追加の読み込みは無い。
 * 本文の md5 を指紋にする案は、判定のためだけに全候補の本文を読み直す
 * ことになり、試作で最悪ケースが 1011ms → 3833ms と**元より遅くなった**。
 * export.inc も同じく mtime で判定している。
 *
 * 無効化は要らない。page_write() と page_delete() は先頭で
 * cache_delete($page['name']) を呼び、そのページのキャッシュを全部消す。
 *
 * **外れても壊れない。** 見つからない・読めない・古い、のどれでも
 * 作り直すだけで、結果は常に本文と一致する。間違えると検索結果が
 * 変わる検索インデックスとは性質が違う。
 */
define('SEARCH_TEXTS_CACHE_KEY', 'search_texts');

function search_page_texts_cached(&$page) {
    if(empty($page['is_exists']) || empty($page['mtime']))
	return search_page_texts($page);

    /* 権限の確認はキャッシュの有無に関わらず行う */
    auth_check($page['name'], 'read');

    /*
     * キャッシュがページより新しいときだけ使う。cache_open() は
     * 「mtime が limit_time 未満なら false」なので、+1 して
     * 「ページと同じ秒に作られたキャッシュ」を弾く。同じ秒に保存と
     * 検索が重なったとき、古い本文のキャッシュを掴まないようにする。
     */
    $texts = cache_serialized_get($page['name'], SEARCH_TEXTS_CACHE_KEY, $page['mtime'] + 1);
    if(is_array($texts) && isset($texts['body']))
	return $texts;

    $texts = search_page_texts($page);
    cache_serialized_create($page['name'], SEARCH_TEXTS_CACHE_KEY, $texts);
    return $texts;
}

/*
 * 1 ページを問い合わせと照合する。当たればスニペットとスコアを返し、
 * 当たらなければ false、時間の上限 ($deadline) を過ぎたら null を返す。
 *
 * 確かめる順は「読む権限 → 日時 → 文字」。権限の無いページは中身を読まない。
 */
function search_page_match($pagename, $query, $deadline = null) {
    global $SEARCH_STRLEN;
    if(!isset($query['clauses']))
	$query = search_query_from_words($query);
    if(!auth_has_permission($pagename, 'read'))
	return false;

    $page = page_read($pagename);
    if(empty($page['is_exists']))
	return false;
    if($query['after'] !== null && $page['mtime'] < $query['after'])
	return false;
    if($query['before'] !== null && $page['mtime'] > $query['before'])
	return false;

    $page_texts_info = search_page_texts_cached($page);
    $page_texts = $page_texts_info['body'];
    $matches = array();
    $score = 0;
    $count = 0;
    $match_title = false;
    $match_description_num = 0;
    $match_tag_num = 0;
    /*
     * 実在しないページ (索引に残っているだけ) では search_page_texts() が
     * 'title' を返さない。無条件に読むと、索引が古いあいだ検索のたびに
     * 警告がログに積まれる。検索は誰でも叩ける経路なので放置できない。
     */
    $title = default_value($page_texts_info['title'], $pagename);
    
    $snippets = array();
    $texts = array();

    foreach($page_texts as &$text) {
	$words = preg_split(LANGUAGE_SEARCH_SPLIT_REGEX, $text, -1, PREG_SPLIT_NO_EMPTY);
	$texts = array_merge($texts, $words);
	$texts[] = ' ';
    }
    unset($text);

    /*
     * 節を全部確かめる。1 つでも外れたら当たらない。正規表現の一致した
     * 文字は、スニペットと印のために集める (「含まない」語のものは除く)。
     */
    $regex_words = array();
    foreach($query['clauses'] as $clause) {
	$is_clause_match = false;
	foreach($clause as $term) {
	    $hit = search_term_match($term, $page_texts_info, $texts, $regex_words, $deadline);
	    if($hit === null)
		return null;
	    if($hit !== $term['not'])
		$is_clause_match = true;
	}
	if(!$is_clause_match)
	    return false;
    }

    /*
     * スニペットとスコアは今までと同じ作り。印を付ける語は「含む」語の
     * 並び (今までの平らな配列と同じ) に、正規表現の一致した文字を足したもの。
     */
    $highlights = array_merge(search_query_words($query), array_keys($regex_words));

    if(isset($page_texts_info['title']) && $page_texts_info['title'] !== '') {
	$title = $page_texts_info['title'];
	if(search_text_match($page_texts_info['title'], $highlights, $matches)) {
	    $snippets[] = l('Title') . ': ' . $page_texts_info['title'];
	    $match_title = true;
	}
    }

    if(isset($page_texts_info['description']) && $page_texts_info['description'] !== '') {
	if(search_text_match($page_texts_info['description'], $highlights, $matches, $match_description_num)) {
	    $snippets[] = l('Description') . ': ' . $page_texts_info['description'];
	}
    }

    if(isset($page_texts_info['tag'])) {
	if(search_text_match($page_texts_info['tag'], $highlights, $matches, $match_tag_num)) {
	    $snippets[] = l('Tags') . ': ' . $page_texts_info['tag'];
	}
    }

    /*
     * 本文からのスニペットは一致した回数だけ増える。1 ページに何十回も
     * 一致すると、その数だけ結果に並ぶ。ページングを入れても 50 件で
     * 547KB あったのはこれが理由なので、件数に上限を掛ける。
     *
     * 打ち切るのは**並べる分だけ**で、ループ自体は最後まで回す。
     * $count はスコアの材料 (一致回数) で、途中で抜けると検索結果の
     * 並び順が変わってしまう。
     */
    $body_snippet_num = 0;
    $texts_num = count($texts);
    $position = 0; 
    $positions = array();
    for($i = 0; $i < $texts_num; $i++) {
	if(search_text_match($texts[$i], $highlights, $matches, $count, $position, $positions)) {
	    $snippet = $pre_snippet = $texts[$i];
	    $j = 1;
	    $s = $e = $i;
	    while($SEARCH_STRLEN($snippet) < SEARCH_SNIPPET_LENGTH) {
		$s = $i - $j;
		$e = $i + $j;
		if($s >= 0)
		    $snippet = $texts[$s] . $snippet;
		if($e < $texts_num) {
		    search_text_match($texts[$e], $highlights, $matches, $count, $position, $positions);
		    $snippet = $snippet . $texts[$e];
		}
		$j++;
		if($pre_snippet === $snippet)
		    break;
		$pre_snippet = $snippet;
	    }
	    if(SEARCH_SNIPPET_MAX <= 0 || $body_snippet_num < SEARCH_SNIPPET_MAX)
		$snippets[] = $snippet;
	    $body_snippet_num++;
	    $i = $e;
	}
    }

    $score = ($match_title ? 1 : 0) * 100 + $match_description_num * 5 + $match_tag_num * 10 + $count;
    $result = array(
	'pagename' => $pagename,
	'mtime' => $page['mtime'],
	'snippets' => $snippets,
	'highlights' => $highlights,
	'match_title' => $match_title,
	'count' => $count,
	'score' => $score);
    if(isset($page['meta']['title']))
	$result['title'] = $page['meta']['title'];
    return $result;
}

/*
 * 語 1 つを、場所の文字と照合する。当たれば true、当たらなければ false、
 * 時間の上限を過ぎたら null。「含まない」の反転は呼び出し側で行う。
 *
 * 本文は句読点と改行で分けた塊ごとに見る ($pieces)。今までの語と同じく、
 * 正規表現も「。、．，」と改行をまたがない。
 */
function search_term_match($term, $texts_info, $pieces, &$regex_words, $deadline) {
    $fields = array();
    if($term['field'] === '' || $term['field'] === 'title') {
	if(isset($texts_info['title']) && $texts_info['title'] !== '')
	    $fields[] = $texts_info['title'];
    }
    if($term['field'] === '') {
	if(isset($texts_info['description']) && $texts_info['description'] !== '')
	    $fields[] = $texts_info['description'];
	if(isset($texts_info['tag']))
	    $fields[] = $texts_info['tag'];
    }
    if($term['field'] === 'tag') {
	/* タグは 1 つずつ。文字の語は完全一致 (&pages(tag=..) と同じ) */
	$tags = isset($texts_info['tag']) ? explode(' ', $texts_info['tag']) : array();
	if($term['type'] === 'text') {
	    foreach($tags as $tag) {
		if($tag !== '' && strtolower($tag) === strtolower($term['value']))
		    return true;
	    }
	    return false;
	}
	$fields = $tags;
    }
    if($term['field'] === '' || $term['field'] === 'body')
	$fields = array_merge($fields, $pieces);

    $is_match = false;
    foreach($fields as $text) {
	if($term['type'] === 'text') {
	    if(stripos($text, $term['value']) !== false)
		return true;
	    continue;
	}
	if($deadline !== null && microtime(true) > $deadline)
	    return null;
	foreach(search_regex_matches($term['pattern'], $text) as $matched) {
	    $is_match = true;
	    if(!$term['not'])
		$regex_words[$matched] = true;
	}
    }
    return $is_match;
}

/*
 * 利用者の正規表現を 1 回掛け、一致した空でない文字を返す。
 * 手数の上限は**この呼び出しの前後だけ**下げる。検索全体で下げると、
 * キャッシュの無いページの本文の変換 (wiki・Markdown の部品も正規表現を
 * 使う) まで途中で失敗し、誤った中身がキャッシュに残る。
 * 上限に当たったとき・文字が UTF-8 でないときは「一致しない」。
 */
function search_regex_matches($pattern, $text) {
    $limit = ini_get('pcre.backtrack_limit');
    ini_set('pcre.backtrack_limit', (string)SEARCH_REGEX_BACKTRACK_LIMIT);
    $num = @preg_match_all($pattern, $text, $matches);
    ini_set('pcre.backtrack_limit', $limit);
    if(!$num)
	return array();
    $found = array();
    foreach($matches[0] as $matched) {
	if($matched !== '')
	    $found[] = $matched;
    }
    return $found;
}

function search_text_match($text, $query, &$matches = array(), &$count = 0, &$position = 0, &$positions = array()) {
    $i = 0;
    $is_match = false;
    foreach($query as $word) {
	$offset = 0;
	while(($offset = stripos($text, $word, $offset)) !== false) {
	    $matches[$i] = true;
	    $is_match = true;
	    //$positions[] = $position + $offset;
	    $positions[$i][] = $position + $offset;
	    $count++;
	    $offset += strlen($word);
	}
	$i++;
    }
    $position += strlen($text);
    return $is_match;
}

function search_texts_to_ngrams(&$texts) {
    $ngrams = array();
    foreach($texts as &$text) {
	$text = strtolower($text);
	$ws = preg_split(LANGUAGE_SEARCH_SPLIT_REGEX, $text, -1, PREG_SPLIT_NO_EMPTY);
	foreach($ws as $w) {
	    $w = trim($w);
	    if($w !== '') {
		search_word_to_ngrams($w, $ngrams);
	    }
	}
    }
    return $ngrams;
}

function search_word_to_ngrams(&$word, &$ngrams) {
    global $SEARCH_SUBSTR;
    $i = 0;
    while(($ngram = $SEARCH_SUBSTR($word, $i, SEARCH_NGRAM_NUMBER)) !== '') {
	$ngrams[$ngram] = true;
	$i++;
    }
}

function search_word_to_ngrams_for_query(&$word, &$ngrams) {
    global $SEARCH_SUBSTR, $SEARCH_STRLEN;
    $i = 0;
    $c = 0;
    $length = $SEARCH_STRLEN($word);
    $n = floor($length / SEARCH_NGRAM_NUMBER) + 1;
    while($c < $n) {
	$ngram = $SEARCH_SUBSTR($word, $i, SEARCH_NGRAM_NUMBER);
	$ngrams[$ngram] = true;
	$i = min($i + SEARCH_NGRAM_NUMBER, $length - SEARCH_NGRAM_NUMBER);
	$c++;
    }
}

function search_ngrams_group_by_key(&$ngrams, &$group) {
    foreach($ngrams as $ngram => $dummy) {
	$key = search_ngram_key($ngram);
	$group[$key][] = $ngram;
    }    
}

function search_ngram_key($ngram) {
    global $SEARCH_SUBSTR;
    $head = $SEARCH_SUBSTR($ngram, 0, 1);
    sscanf(bin2hex($head), '%x', $hex);
    $k1 = ($hex & 0x3f) << 2;
    $hex = $hex >> 8;
    $k2 = ($hex & 0x3f);
    $hex = $hex >> 8;
    $k3 = ($hex & 0x3f);
    return chr(($k1 + $k2 + $k3) & 0xff);
}

/*
 * 検索欄の問い合わせを読む (docs/development/design-search-query.md §3.1)。
 *
 *   語1 語2              両方を含む (今までと同じ)
 *   "..."                空白ごと 1 つの語 (今までと同じ)
 *   語1 OR 語2           どちらかを含む。隣どうしだけで、括弧は無い
 *   -語                  含まない。ほかに条件 (含む語か日時) が無ければ文字として探す
 *   title: body: tag:    探す場所 (題名とページ名 / 本文 / タグの完全一致)
 *   re:式                正規表現 (大文字小文字を区別しない)
 *   after: before:       更新日時 (YYYY-MM-DD。その日を含む)
 *
 * 頭に付けるものの順は - → 場所 → re:。値は "..." でもよい。演算子の記号は
 * 半角だけ。知らない名前の xxx: や、値の無いものはふつうの語。
 *
 * 戻り値:
 *   text       入力 (全角の空白を揃えたもの)
 *   clauses    節 (AND) の並び。節は語 (OR) の並び。語は
 *              type ('text' / 'regex')・value・field ('' / 'title' / 'body' / 'tag')・
 *              not・raw (入力の 1 語)・pattern (正規表現のとき)
 *   after / before  更新日時の範囲 (UNIX 時刻、無ければ null)
 *   errors     画面に出す誤り (読めない日付・読めない式)
 *   operators  新しい書き方を使ったか
 *   has_regex  正規表現を含むか
 */
function search_query_parse($query_text) {
    $text = language_sanitize($query_text);
    $query = array(
	'text' => $text,
	'clauses' => array(),
	'after' => null,
	'before' => null,
	'errors' => array(),
	'operators' => false,
	'has_regex' => false,
	);

    /*
     * 切り出しは今までの LANGUAGE_SEARCH_QUERY_REGEX と同じく "..." か空白を
     * 含まない並び。演算子の頭 (-・場所・re:) の後ろの "..." も 1 つの語にする。
     */
    preg_match_all('/((?:-?(?:(?:title|body|tag):)?(?:re:)?)?".*?"|[^\s]+)/', $text, $matches);
    $items = array();
    foreach($matches[1] as $token) {
	if($token === 'OR') {
	    $items[] = 'OR';
	    continue;
	}
	if(preg_match('/^(after|before):(.*)$/s', $token, $m)) {
	    $time = search_query_date($m[2], $m[1] === 'before');
	    if($time === false) {
		$query['errors'][] = l('Write dates as YYYY-MM-DD.');
		continue;
	    }
	    if($m[1] === 'after')
		$query['after'] = $query['after'] === null ? $time : max($query['after'], $time);
	    else
		$query['before'] = $query['before'] === null ? $time : min($query['before'], $time);
	    $query['operators'] = true;
	    continue;
	}
	$term = search_query_term($token);
	if($term['value'] === '')
	    continue;
	$items[] = $term;
    }

    /* OR は前後の語を 1 つの節にまとめる。端や続いた OR はふつうの語 */
    $item_num = count($items);
    for($i = 0; $i < $item_num; $i++) {
	$item = $items[$i];
	if($item === 'OR') {
	    if($i > 0 && $items[$i - 1] !== 'OR' && !empty($query['clauses']) &&
	       $i + 1 < $item_num && $items[$i + 1] !== 'OR') {
		$i++;
		$query['clauses'][count($query['clauses']) - 1][] = $items[$i];
		$query['operators'] = true;
		continue;
	    }
	    $item = search_query_term('OR');
	}
	$query['clauses'][] = array($item);
    }

    /*
     * ほかに条件が無いときの「含まない」は、今までどおり文字として探す。
     * `-rf` や `--force` だけで探すと、ほぼ全ページが出てしまうため。
     */
    $has_positive = $query['after'] !== null || $query['before'] !== null;
    foreach($query['clauses'] as $clause)
	foreach($clause as $term)
	    if(!$term['not'])
		$has_positive = true;
    if(!$has_positive) {
	foreach($query['clauses'] as &$clause)
	    foreach($clause as &$term)
		$term = search_query_literal($term['raw']);
	unset($clause, $term);
    }

    foreach($query['clauses'] as &$clause) {
	foreach($clause as &$term) {
	    if($term['is_operator'])
		$query['operators'] = true;
	    if($term['type'] !== 'regex')
		continue;
	    $query['has_regex'] = true;
	    /*
	     * 区切りは制御文字 (\x01)。/ を含む式 (/api/v1/) をそのまま書ける。
	     * 読めない式は、警告を出さずに 1 回だけ確かめる (誰でもログを
	     * 埋められないように)。
	     */
	    $term['pattern'] = "\x01" . $term['value'] . "\x01iu";
	    if(@preg_match($term['pattern'], '') === false) {
		$error = error_get_last();
		error_clear_last();
		$detail = $error !== null ? preg_replace('/^preg_match\(\): /', '', $error['message']) : '';
		$query['errors'][] = lf('Could not read the regular expression: $error$',
					array('error' => $detail));
	    }
	}
    }
    unset($clause, $term);
    return $query;
}

/* 入力の 1 語を、場所・含まない・正規表現に分ける */
function search_query_term($token) {
    $term = search_query_literal($token);
    $rest = $token;
    $is_operator = false;
    $not = false;
    $field = '';
    $type = 'text';
    if(strlen($rest) > 1 && $rest[0] === '-') {
	$not = true;
	$rest = substr($rest, 1);
	$is_operator = true;
    }
    if(preg_match('/^(title|body|tag):(.+)$/s', $rest, $m)) {
	$field = $m[1];
	$rest = $m[2];
	$is_operator = true;
    }
    if(strlen($rest) > 3 && substr($rest, 0, 3) === 're:') {
	$type = 'regex';
	$rest = substr($rest, 3);
	$is_operator = true;
    }
    if(!$is_operator)
	return $term;
    if(strlen($rest) >= 2 && $rest[0] === '"' && substr($rest, -1) === '"')
	$rest = substr($rest, 1, -1);
    if($rest === '')
	return $term;
    $term['type'] = $type;
    $term['value'] = $rest;
    $term['field'] = $field;
    $term['not'] = $not;
    $term['is_operator'] = true;
    return $term;
}

/* 今までと同じ、文字として探す語。前後の " は外す */
function search_query_literal($token) {
    return array(
	'type' => 'text',
	'value' => trim($token, '"'),
	'field' => '',
	'not' => false,
	'raw' => $token,
	'is_operator' => false,
	);
}

/*
 * YYYY-MM-DD を UNIX 時刻にする (サイトのタイムゾーン)。$is_end なら
 * その日の終わり。読めない日付 (2026/10/01、2026-02-30) は false。
 */
function search_query_date($text, $is_end) {
    if(!preg_match('/^(\d{4})-(\d{2})-(\d{2})$/', $text, $m) ||
       !checkdate((int)$m[2], (int)$m[3], (int)$m[1]))
	return false;
    if($is_end)
	return mktime(23, 59, 59, (int)$m[2], (int)$m[3], (int)$m[1]);
    return mktime(0, 0, 0, (int)$m[2], (int)$m[3], (int)$m[1]);
}

/* 今までの平らな語の配列を、問い合わせの形にする */
function search_query_from_words($words) {
    $query = array(
	'text' => implode(' ', $words),
	'clauses' => array(),
	'after' => null,
	'before' => null,
	'errors' => array(),
	'operators' => false,
	'has_regex' => false,
	);
    foreach($words as $word) {
	$term = search_query_literal($word);
	$term['value'] = $word;
	$query['clauses'][] = array($term);
    }
    return $query;
}

/*
 * 印を付ける語の並び。「含む」文字の語を、書いた順に。今までの書き方では
 * 今までの平らな配列と同じになる (印の色 keyword_N の番号もこの順)。
 */
function search_query_words($query) {
    $words = array();
    foreach($query['clauses'] as $clause)
	foreach($clause as $term)
	    if(!$term['not'] && $term['type'] === 'text')
		$words[] = $term['value'];
    return $words;
}

/*
 * 今までの、平らな語の配列を返す読み方。新しい書き方の語は「含む」文字の
 * 語だけが残る。
 */
function search_parse_query($query_text) {
    return search_query_words(search_query_parse($query_text));
}

/*
 * 結果を開いたページで印を付ける語。「含む」文字の語に、正規表現が
 * このページで一致した文字を足す (ログインしているときだけ。種類に上限)。
 */
function search_page_highlights(&$page, $query) {
    $words = search_query_words($query);
    if(empty($query['has_regex']) || !empty($query['errors']) || !search_regex_is_allowed())
	return $words;
    if(empty($page['is_exists']) || !auth_has_permission($page['name'], 'read'))
	return $words;
    $texts_info = search_page_texts_cached($page);
    $texts = array();
    foreach(array('title', 'description', 'tag') as $key)
	if(isset($texts_info[$key]) && $texts_info[$key] !== '')
	    $texts[] = $texts_info[$key];
    foreach($texts_info['body'] as $text)
	$texts = array_merge($texts, preg_split(LANGUAGE_SEARCH_SPLIT_REGEX, $text, -1, PREG_SPLIT_NO_EMPTY));
    $found = array();
    foreach($query['clauses'] as $clause) {
	foreach($clause as $term) {
	    if($term['not'] || $term['type'] !== 'regex')
		continue;
	    foreach($texts as $text) {
		foreach(search_regex_matches($term['pattern'], $text) as $matched) {
		    $found[$matched] = true;
		    if(count($found) >= SEARCH_REGEX_HIGHLIGHT_MAX)
			break 3;
		}
	    }
	}
    }
    return array_merge($words, array_keys($found));
}

/*
 * 「検索キーワード:」。今までの書き方では語を空白でつないだもの (今までと
 * 同じ)、新しい書き方では入力をそのまま出し、「含む」文字の語に印を付ける
 * (正規表現がページで一致した文字には付けない。式の中の文字に当たるだけ)。
 */
function search_notice_query(&$page, $dom) {
    $query = isset($page['query_notice_words']) ? $page['query_notice_words'] : $page['query'];
    $text = isset($page['query_text']) ? $page['query_text'] : implode(' ', $query);
    $p = dom_append_child($dom, 'p', 'search_query');
    $a = dom_append_link($p, url_make($page['name']), '', 'button');
    dom_append_ref($a, 'times');
    dom_append_text($p, l('Search keywords') . ':');
    dom_append_hightlighted_text($p, $text, $query);
}
?>
