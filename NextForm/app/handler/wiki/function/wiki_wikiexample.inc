<?php
$WIKI_OPERATORS_FUNCTION['wikiexample'] = true;
function wiki_operator_wikiexample(&$context, $dom, $tree) {
    $dl = dom_append_child_with_position($dom, 'dl', $tree);
    $dt = dom_append_child($dl, 'dt');
    dom_append_text($dt, l('Source'));
    $dd = dom_append_child($dl, 'dd');
    $pre = dom_append_child($dd, 'pre');
    dom_append_text($pre, wiki_tokens_to_string_without_escape($tree->target));
    
    $dt = dom_append_child($dl, 'dt');
    dom_append_text($dt, l('Display'));
    $dd = dom_append_child($dl, 'dd');
    $tokens = wiki_tokens_trim_space($tree->target);
    wiki_convert_from_tokens($context, $dd, $tokens);
}
?>
