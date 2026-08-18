<?php
$OPTIONS['admin_manual'] = array(
    'show' => 'admin_manual_show',
    'write' => 'admin_manual_write',
    );

function admin_manual_show(&$args, $dom) {
    auth_check(false, 'admin');
    title_set_l('Generate manual pages');
    /*
     * POST で送る。0.4.0 から、安全と分かっている action 以外は POST かつ
     * 同一オリジンでないと弾かれる。GET のままだと「実行」を押しても 403 で
     * 何も起きない (v0.4.0 から v0.4.1 の間、この画面は動かなかった)。
     */
    $form = dom_append_form($dom, 
			    array('option' => 'admin_manual', 
				  'action' => 'write'));
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
