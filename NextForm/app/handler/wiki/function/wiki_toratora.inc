<?php
$WIKI_OPERATORS_FUNCTION['taro'] = true;
function wiki_operator_taro(&$context, $dom, $tree) {
    $b = dom_append_child($dom, 'b', 'toratora taro');
    dom_append_text($b, 'taro');
}
$WIKI_OPERATORS_FUNCTION['toro'] = true;
function wiki_operator_toro(&$context, $dom, $tree) {
    $b = dom_append_child($dom, 'b', 'toratora toro');
    dom_append_text($b, 'toro');
}
$WIKI_ALIASES['toratora'] = '&taro;&toro;';
?>
