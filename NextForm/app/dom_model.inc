<?php
$DOM_PHRASING_CONTENT_TAGNAMES = array(
    'a' => true,
    'abbr' => true,
    'area' => true,
    'audio' => true,
    'b' => true,
    'bdi' => true,
    'bdo' => true,
    'br' => true,
    'button' => true,
    'canvas' => true,
    'cite' => true,
    'code' => true,
    'command' => true,
    'datalist' => true,
    'del' => true,
    'dfn' => true,
    'em' => true,
    'embed' => true,
    'i' => true,
    'iframe' => true,
    'img' => true,
    'input' => true,
    'ins' => true,
    'kbd' => true,
    'keygen' => true,
    'label' => true,
    'map' => true,
    'mark' => true,
    'math' => true,
    'meter' => true,
    'noscript' => true,
    'object' => true,
    'output' => true,
    'progress' => true,
    'q' => true,
    'ruby' => true,
    's' => true,
    'samp' => true,
    'script' => true,
    'select' => true,
    'small' => true,
    'span' => true,
    'strong' => true,
    'sub' => true,
    'sup' => true,
    'svg' => true,
    'textarea' => true,
    'time' => true,
    'u' => true,
    'var' => true,
    'video' => true,
    'wbr' => true,
    );

function dom_is_phrasing_content($dom) {
    global $DOM_PHRASING_CONTENT_TAGNAMES;
    if($dom instanceof DOMText)
	return true;
    else if($dom instanceof DOMElement)
	return !empty($DOM_PHRASING_CONTENT_TAGNAMES[$dom->tagName]);
    return false;
}

function dom_has_no_phrasing_content($dom, $without_myself = false) {
    if(!$without_myself && !dom_is_phrasing_content($dom))
	return true;
    if(!empty($dom->childNodes)) {
	foreach($dom->childNodes as $childNode) {
	    if(dom_has_no_phrasing_content($childNode))
		return true;
	}
    }
    return false;
}


?>