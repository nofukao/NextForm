<?php
$SENTENCE_HANDLERS['pagename'] = array(
    'init' => 'sentence_pagename_init',
    'word' => 'sentence_pagename_word',
    );

$SENTENCE_PAGENAME_PAGENAMES_CACHE = false;
function sentence_pagename_init(&$page) {
    global $SENTENCE_PAGENAME_PAGENAMES_CACHE;
    if(SENTENCE_PAGENAME_ENABLE !== 'true')
	return;

    if($SENTENCE_PAGENAME_PAGENAMES_CACHE === false) {
	$SENTENCE_PAGENAME_PAGENAMES_CACHE = page_find('', array('is_pagename_only' => true));
    }
    $page['sentence']['pagename']['words'] = array();
    foreach($SENTENCE_PAGENAME_PAGENAMES_CACHE as $one_page) {
	if(preg_match('/^[a-z]+$/i', $one_page['name'])) {
	    $page['sentence']['pagename']['regexs'][] = array(
		'pattern' => '/\b' . preg_quote($one_page['name'], '/') . '\b/i',
		'source' => $one_page['name']
		);
	} else {
	    $page['sentence']['pagename']['words'][] = $one_page['name'];
	}
    }    
}

function sentence_pagename_word($dom, $word, &$page, $sentence_context) {
    if(dom_in_tag_name($dom, 'a'))
	return false;
    if(isset($sentence_context['source']))
	$pagename = $sentence_context['source'];
    else
	$pagename = $word;
    $link = dom_append_empty_page_link($dom, $pagename);
    dom_append_class($link, 'auto');
    sentence_append($link, $word, $page, $sentence_context);
    return true;
}

?>
