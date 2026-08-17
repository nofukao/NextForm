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

function search(&$query, $search_options = array()) {
    if(SEARCH_ENGINE === 'builtin')
	return search_builtin($query, $search_options);
    else if(($engine = search_engine()) !== false)
	return $engine['search']($query, $search_options);
}

function search_builtin(&$query, $search_options = array()) {
    global $SEARCH_SUBSTR, $SEARCH_STRLEN;

    $query_num = count($query);

    $candidate_pagenames = array();
    foreach($query as $word) {
	search_word($word, $candidate_pagenames);
    }

    $search_results = array();
    foreach($candidate_pagenames as $pagename => $count) {
	if(!empty($search_options['is_ignore_system_page']) && page_is_system_page($pagename))
	    continue;
	
	if($count == $query_num) {
	    $result = search_page_match($pagename, $query);
	    if($result !== false) {
		$search_results[$pagename] = $result;
	    }
	}
    }
    if(isset($search_options['order']) && $search_options['order'] === 'mtime')
	uasort($search_results, 'search_result_compare_mtime');
    else
	uasort($search_results, 'search_result_compare_score');

    return $search_results;
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

function search_word($word, &$candidate_pagenames) {
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
    foreach($candidate1_pagenames as $pagename => $count) {
	if($count == $match_page_count) {
	    if(empty($candidate_pagenames[$pagename]))
		$candidate_pagenames[$pagename] = 1;
	    else
		$candidate_pagenames[$pagename]++;
	}
    }
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
function search_index_check($is_deep = false) {
    $check = search_index_collect($is_deep);

    $real_pages = array();
    foreach(page_find('', array('is_pagename_only' => true)) as $page)
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

function search_page_match($pagename, $query) {
    global $SEARCH_STRLEN;
    if(!auth_has_permission($pagename, 'read'))
	return false;

    $page = page_read($pagename);
    $page_texts_info = search_page_texts($page);
    $page_texts = $page_texts_info['body'];
    $matches = array();
    $score = 0;
    $count = 0;
    $match_title = false;
    $match_description_num = 0;
    $match_tag_num = 0;
    $title = $page_texts_info['title'];
    
    $snippets = array();
    $texts = array();

    if(isset($page_texts_info['title']) && $page_texts_info['title'] !== '') {
	$title = $page_texts_info['title'];
	if(search_text_match($page_texts_info['title'], $query, $matches)) {
	    $snippets[] = l('Title') . ': ' . $page_texts_info['title'];
	    $match_title = true;
	}
    }

    if(isset($page_texts_info['description']) && $page_texts_info['description'] !== '') {
	if(search_text_match($page_texts_info['description'], $query, $matches, $match_description_num)) {
	    $snippets[] = l('Description') . ': ' . $page_texts_info['description'];
	}
    }

    if(isset($page_texts_info['tag'])) {
	if(search_text_match($page_texts_info['tag'], $query, $matches, $match_tag_num)) {
	    $snippets[] = l('Tags') . ': ' . $page_texts_info['tag'];
	}
    }

    foreach($page_texts as &$text) {
	$words = preg_split(LANGUAGE_SEARCH_SPLIT_REGEX, $text, -1, PREG_SPLIT_NO_EMPTY);
	$texts = array_merge($texts, $words);
	$texts[] = ' ';
    }

    $texts_num = count($texts);
    $position = 0; 
    $positions = array();
    for($i = 0; $i < $texts_num; $i++) {
	if(search_text_match($texts[$i], $query, $matches, $count, $position, $positions)) {
	    $snippet = $pre_snippet = $texts[$i];
	    $j = 1;
	    $s = $e = $i;
	    while($SEARCH_STRLEN($snippet) < SEARCH_SNIPPET_LENGTH) {
		$s = $i - $j;
		$e = $i + $j;
		if($s >= 0)
		    $snippet = $texts[$s] . $snippet;
		if($e < $texts_num) {
		    search_text_match($texts[$e], $query, $matches, $count, $position, $positions);
		    $snippet = $snippet . $texts[$e];
		}
		$j++;
		if($pre_snippet === $snippet)
		    break;
		$pre_snippet = $snippet;
	    }
	    $snippets[] = $snippet;
	    $i = $e;
	}
    }

    if(count($matches) == count($query)) {
	/*
	$deviation = 0;
	$positions_num = count($positions);
	if($positions_num > 1) {
	    $mean_position = array_sum($positions) / $positions_num;
	    foreach($positions as $position)
		$deviation += pow($position - $mean_position, 2);
	    $deviation /= $positions_num;
	    $deviation = sqrt($deviation);
	}
	$score = ($match_title ? 1 : 0) * 100 + $count - min($deviation / 1000, 1);
	*/

	$score = ($match_title ? 1 : 0) * 100 + $match_description_num * 5 + $match_tag_num * 10 + $count;
	$result = array(
	    'pagename' => $pagename,
	    'mtime' => $page['mtime'],
	    'snippets' => $snippets,
	    'match_title' => $match_title,
	    'count' => $count,
	    'score' => $score);
	if(isset($page['meta']['title']))
	    $result['title'] = $page['meta']['title'];
	return $result;
    } else {
	return false;
    }
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

function search_parse_query($query_text) {
    preg_match_all(LANGUAGE_SEARCH_QUERY_REGEX, language_sanitize($query_text), $matches);
    $query = array();
    foreach($matches[1] as $m) {
	$m = trim($m, '"');
	if($m !== '') {
	    $query[] = $m;
	}
    }
    return $query;
}

function search_notice_query(&$page, $dom) {
    $query = &$page['query'];
    $p = dom_append_child($dom, 'p', 'search_query');
    $a = dom_append_link($p, url_make($page['name']), '', 'button');
    dom_append_ref($a, 'times');
    dom_append_text($p, l('Search keywords') . ':');
    dom_append_hightlighted_text($p, implode(' ', $query), $query);
}
?>
