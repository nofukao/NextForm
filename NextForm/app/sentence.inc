<?php
$SENTENCE_HANDLERS = array();

foreach(glob_array(APP_DIR_PATH . '/sentence/*.inc') as $file) require_once($file);

function sentence_init(&$page) {
    global $SENTENCE_HANDLERS;
    foreach($SENTENCE_HANDLERS as $id => $handler) {
	if(isset($handler['init'])) {
	    $handler['init']($page);
	    if(isset($page['sentence'][$id]['words'])) {
		foreach($page['sentence'][$id]['words'] as $word) {
		    $page['sentence'][$id]['words_length'][] = strlen($word);
		}
	    }
	}
    }
}

function sentence_term(&$page) {
    global $SENTENCE_HANDLERS;
    foreach($SENTENCE_HANDLERS as $id => $handler) {
	if(isset($handler['term']))
	    $handler['term']($page);
    }
}

function sentence_append($dom, $text, &$page, $sentence_context = array()) {
    global $SENTENCE_HANDLERS;
    if(!isset($page['sentence']))
	return dom_append_text($dom, $text);

    if(isset($sentence_context['nests'])) {
	$match_words = $sentence_context['nests'];
    } else {
	$match_words = array();
	foreach($page['sentence'] as $id => $sentence) {
	    if(isset($sentence_context['only']) && $id !== $sentence_context['only'])
		continue;
	    if(isset($sentence_context['excludes']) && in_array($id, $sentence_context['excludes'])) {
		continue;
	    }
	    if(isset($sentence['words'])) {
		$strposfunc = empty($page['sentence'][$id]['ignorecase']) ? 'strpos' : 'stripos';
		$word_index = 0;
		foreach($sentence['words'] as $word) {
		    $offset = 0;
		    $word_length = $sentence['words_length'][$word_index];
		    while(($pos = $strposfunc($text, $word, $offset)) !== false) {
			$match_words[] = 
			    array(
				'position' => $pos,
				'id' => $id,
				'word' => $word,
				'length' => $word_length,
				);
			$offset = $pos + $word_length;
		    }
		    $word_index++;
		}
	    }
	    if(isset($sentence['regexs'])) {
		foreach($sentence['regexs'] as $regex) {
		    if(preg_match_all($regex['pattern'], $text, $matches, PREG_OFFSET_CAPTURE) > 0) {
			foreach($matches[0] as $m) {
			    $match_words[] =
				array(
				    'position' => $m[1],
				    'id' => $id,
				    'word' => $m[0],
				    'length' => strlen($m[0]),
				    'source' => $regex['source'],
				    );
			}
		    }
		}
	    }
	}
	usort($match_words, 'sentence_match_compare');
    }
    //pprint_r($match_words);

    reset($match_words);
    $offset = 0;
    while(($match_word = current($match_words)) !== false) {
	if($match_word['position'] < $offset) {
	    next($match_words);
	    continue;
	}
	if($offset < $match_word['position']) {
	    dom_append_text($dom, substr($text, $offset, $match_word['position'] - $offset));
	    $offset = $match_word['position'];
	}
	$sentence_context['nests'] = array();
	$next_match_word = next($match_words);
	while($next_match_word !== false && empty($sentence_context['nonests'])) {
	    if($match_word['position'] <= $next_match_word['position'] &&
	       $match_word['position'] + $match_word['length'] >=
	       $next_match_word['position'] + $next_match_word['length']) {
		$next_match_word['position'] -= $offset;
		$sentence_context['nests'][] = $next_match_word;
		$next_match_word = next($match_words);
	    } else {
		break;
	    }
	}
	if(isset($match_word['source']))
	    $sentence_context['source'] = $match_word['source'];
	if($SENTENCE_HANDLERS[$match_word['id']]['word'](
	       $dom, substr($text, $offset, $match_word['length']), $page, $sentence_context)) {
	    $offset = $match_word['position'] + $match_word['length'];
	}
    }
    dom_append_text($dom, substr($text, $offset, strlen($text) - $offset));
}

function sentence_match_compare($a, $b) {
    if($b['position'] + $b['length'] < $a['position'])
	return 1;
    if($b['length'] > $a['length'] && $b['position'] <= $a['position'] + $a['length'])
	return 1;
    if($b['length'] == $a['length'] && $b['position'] < $a['position'])
	return 1;
    return -1;
}

?>
