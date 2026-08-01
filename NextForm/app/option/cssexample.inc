<?php
$TOOLS['site']['cssexample'] = array(
    'order' => 2000,
    'show' => 'cssexample_tool'
    );

function cssexample_tool(&$page, $dom) {
    if(DEBUG_MODE == 'true') {
	tools_add_link(
	    $dom,
	    array('name' => '(' . l('CSS Examples') . ')',
		  'uri' => url_make($page['name'], array('option' => 'cssexample'))));
    }
}

$OPTIONS['cssexample'] = array(
    'show' => 'cssexample_show',
    );

function cssexample_show(&$args, $dom) {
    title_set_l('CSS Examples');

    $ldom = dom_create_document();
    $a = dom_append_link($ldom, url_make(''), 'link');
    ld(message_error(), 'This is error message, $link$ here.', array('link' => $a));
    ld(message_notice(), 'This is notice message, $link$ here.', array('link' => $a));
    ld(message_info(), 'This is info message, $link$ here.', array('link' => $a));
    
    $form = dom_append_form($dom,array('option' => 'cssexample'), '', 'POST');

    $attributes = array('type' => 'text',
			'value' => 
			l('Your browser does not support logout function. Plese close the browser or remove password.'));
    if(empty($setup_constant['allowempty']))
	$attributes['required'] = '';
    dom_append_child($form, 'input', '', $attributes);

    $submit = dom_append_child($form, 'input', '', array('type' => 'submit', 'value' => l('Apply') .' submit'));
    dom_append_link($form, '', l('Apply').' a.button', 'button');

    $button = dom_append_child($form, 'button', '');
    dom_append_text($button, l('Apply').' button');

    $button = dom_append_child($form, 'button', '');
    dom_append_ref($button, 'times');

    $label = dom_append_child($form, 'label');
    $check = dom_append_child($label, 'input', '',
			      array(
				  'type' => 'checkbox',
				  'value' => 'on',
				  'checked' => 'checked'
				  ));
    dom_append_text($label, 'Delete');

    $label = dom_append_child($form, 'label');
    $check = dom_append_child($label, 'input', '',
			      array(
				  'type' => 'checkbox',
				  'value' => 'on',
				  'checked' => 'checked'
				  ));
    dom_append_text($label, l('Delete'));
    
    $options = time_get_timezones();
    ksort($options);
    dom_append_selector($form, '', $options);
    $options = theme_get_themes();
    ksort($options);
    dom_append_selector($form, '', $options);

    /*
    $progress = dom_append_child($form, 'progress', '', array('max' => 100, 'value' => 25));
    dom_append_text($progress, '');
    $progress = dom_append_child($form, 'progress', '', array('max' => 100, 'value' => 100));
    dom_append_text($progress, '');
    */

    $section = dom_append_child($dom, 'section', 'list');
    $ul = dom_append_child($section, 'ul');
    $li = dom_append_child($ul, 'li');
    dom_append_link($li, url_make(), 'visited link');
    $li = dom_append_child($ul, 'li');
    dom_append_link($li, url_make('', array('random' => md5(time()))), 'unvisited link');
    $li = dom_append_child($ul, 'li');
    dom_append_link($li, url_make(), 'unread link', 'unread');
    $li = dom_append_child($ul, 'li');
    dom_append_link($li, url_make(), 'not exists link', 'not_exists');
    
    //$textarea = dom_append_child($form, 'textarea');
    //dom_append_text($textarea, '');
    
    return false;
}
?>
