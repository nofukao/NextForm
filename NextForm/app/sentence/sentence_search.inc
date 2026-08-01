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
}

function sentence_search_word($dom, $word, &$page, $sentence_context) {
    $lword = strtolower($word);
    $mark = dom_append_child($dom, 'mark', 'keyword_' . ($page['sentence']['search']['word_to_no'][$lword] % 5));
    sentence_append($mark, $word, $page, $sentence_context);
    return true;
}
?>
