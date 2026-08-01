<?php
$OPTIONS['admin_manual'] = array(
    'show' => 'admin_manual_show',
    'write' => 'admin_manual_write',
    );

function admin_manual_show(&$args, $dom) {
    auth_check(false, 'admin');
    title_set_l('Generate manual pages');
    $form = dom_append_form($dom, 
			    array('option' => 'admin_manual', 
				  'action' => 'write'), '', 'GET');
    ld($form, 'Generate manual in under $pagename$ pages.', array('pagename' => dom_pagename(MANUAL_PAGENAME)));
    
    $submit = dom_append_child($form, 'input', 'run', array('type' => 'submit', 'value' => l('Run')));
}

function admin_manual_write(&$args, $dom) {
    auth_check(false, 'admin');
    title_set_l('Generate manual pages');
    set_time_limit(120);
    manual_build();

    //dom_append_link(message_notice(), url_make(), l());
    ld(message_notice(), 'Manual pages under $pagename$ was generated.',
       array('pagename' => dom_pagename(MANUAL_PAGENAME)));
    
    return false;
}
?>
