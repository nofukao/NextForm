<?php
$SEARCH_SUBSTR = function_exists('mb_substr') ? 'mb_substr' : 'substr';
$SEARCH_STRLEN = function_exists('mb_strlen') ? 'mb_strlen' : 'strlen';
define('SEARCH_NGRAM_NUMBER', 2);

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

function search_result_compare_score(&$result_a, &$result_b) {
    if($result_a['score'] > $result_b['score'])
	return -1;
    if($result_a['score'] < $result_b['score'])
	return 1;
    return strcmp($result_a['pagename'], $result_b['pagename']);
}

function search_result_compare_mtime(&$result_a, &$result_b) {
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
	    $cachekey = 'search_index_' . $key;
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
	$cachekey = 'search_index_' . $key;
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

function search_page_index_add(&$page) {
    //auth_check(false, 'admin');
    $ngrams = search_page_ngram($page);
    search_index_add($page['name'], $ngrams);
}

function search_index_add($pagename, &$ngrams) {
    $group = array();
    search_ngrams_group_by_key($ngrams, $group);
    foreach($group as $key => &$ngrams) {
	$cachekey = 'search_index_' . $key;
	$index = cache_serialized_get('', $cachekey);
	if($index === false)
	    $index = array();
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
	$cachekey = 'search_index_' . $key;
	$index = cache_serialized_get('', $cachekey);
	if($index === false)
	    $index = array();
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
