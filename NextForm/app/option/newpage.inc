<?php
$TOOLS['site']['newpage'] = array(
    'order' => 20,
    'show' => 'newpage_tool'
    );

function newpage_tool(&$page, $dom) {
    tools_add_link(
	$dom,
	array('name' => l('New page'),
	      'uri' => url_make('', array('option' => 'newpage'))));
}

$OPTIONS['newpage'] = array(
    'show' => 'newpage_show',
    );

function newpage_show(&$args, $dom) {
    title_set_l('New page');
    newpage_dom_append_form($dom);
    return false;
}

function newpage_dom_append_form($dom, $default_name = '') {
    global $HANDLERS;

    $form = dom_append_form($dom, array());

    $label = dom_append_child($form, 'label', 'newpage');
    dom_append_text($label, l('New page name'));
    
    $text = dom_append_child($label, 'input', '', array('type' => 'text', 'name' => 'page'));
    if($default_name !== '')
	dom_set_attribute($text, 'value', $default_name);

    $options = array();
    foreach($HANDLERS as $type => &$handler) {
	$options[$type] = $type;
    }
    
    $label = dom_append_child($form, 'label');
    dom_append_text($label, l('Type'));
    dom_append_selector($label, 'newpage_type', $options, DEFAULT_PAGE_TYPE);

    $submit = dom_append_child($form, 'input', '', array('type' => 'submit', 'value' => l('Create')));
}
?>
