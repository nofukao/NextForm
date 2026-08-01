<?php
$messages_doc = null;
$messages_dom = null;
$messages_level_to_class = array('info', 'notice', 'error');

define('MESSAGE_LEVEL_INFO',   0);
define('MESSAGE_LEVEL_NOTICE', 1);
define('MESSAGE_LEVEL_ERROR',  2);

function message_init() {
    global $messages_doc;
    global $messages_dom;
    $messages_doc = dom_create_document();
    $messages_dom = dom_append_child($messages_doc, 'ul', 'messages');
}

function message_add($level, $msg) {
    global $messages_dom;
    global $messages_level_to_class;
    $li = dom_append_child($messages_dom, 'li', $messages_level_to_class[$level]);
    if(is_string($msg))
	dom_append_text($li, $msg);
    return $li;
}

function message_info($msg = null) {
    return message_add(MESSAGE_LEVEL_INFO, $msg);
}

function message_notice($msg = null) {
    return message_add(MESSAGE_LEVEL_NOTICE, $msg);
}

function message_error($msg = null) {
    return message_add(MESSAGE_LEVEL_ERROR, $msg);
}

function message_html() {
    global $messages_doc;
    global $messages_dom;
    if(!$messages_dom->hasChildNodes())
	dom_append_text($messages_dom, '');
    return dom_save_html($messages_doc);
}
?>
