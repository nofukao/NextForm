<?php
$SENTENCE_HANDLERS['search'] = array(
    'init' => 'sentence_search_init',
    'word' => 'sentence_search_word',
    );

function sentence_search_init(&$page) {
    if(!isset($page['query']))
	return;
    $query = $page['query'];
    $page['sentence']['search']['ignorecase'] = true;
    $page['sentence']['search']['words'] = $query;
    $page['sentence']['search']['word_to_no'] = array_flip(array_map('strtolower', $query));
    $page['sentence']['search']['mark_num'] = 0;
}

/*
 * 1 ページ (または 1 つのスニペット) に付ける印の上限。正規表現 re:. のような
 * 問い合わせで全部の文字に印が付き、HTML が何十倍にもなるのを防ぐ。
 * 上限を超えた所は、印を付けずに同じ文字を出す (入れ子の処理は通す)。
 */
define('SENTENCE_SEARCH_MARK_MAX', 500);

function sentence_search_word($dom, $word, &$page, $sentence_context) {
    if($page['sentence']['search']['mark_num'] >= SENTENCE_SEARCH_MARK_MAX) {
	sentence_append($dom, $word, $page, $sentence_context);
	return true;
    }
    $page['sentence']['search']['mark_num']++;
    $lword = strtolower($word);
    $mark = dom_append_child($dom, 'mark', 'keyword_' . ($page['sentence']['search']['word_to_no'][$lword] % 5));
    sentence_append($mark, $word, $page, $sentence_context);
    return true;
}
?>
