<?php
$OPTIONS['admin_user_client'] = array(
    'show' => 'admin_user_client_show',
    );

function admin_user_client_show(&$args, $dom) {
    global $AUTH_PRESET_PERMISSIONS;
    title_set_l('Make user setting');

    $ul = dom_append_child($dom, 'ul', 'form');

    $li = dom_append_child($ul, 'li');
    $label = dom_append_child($li, 'label');
    dom_append_text($label, l('User name'));
    dom_append_child($label, 'input', '', array('type' => 'text', 'id' => 'admin_user_name'));
    
    $li = dom_append_child($ul, 'li');
    $label = dom_append_child($li, 'label');
    dom_append_text($label, l('Password'));
    dom_append_child($label, 'input', '', array('type' => 'password', 'id' => 'admin_user_pass'));

    $li = dom_append_child($ul, 'li');
    $label = dom_append_child($li, 'label');
    dom_append_text($label, l('Permission level'));
    $perms = array(
	'keepout' => l('keep out'),
	'read' => l('read only'),
	'insert' => l('text insertion available'),
	'write' => l('page write available'),
	'lock' => l('page lock available'),
	'export' => l('export available'),
	'admin' => l('administrator'),
	);
    $select = dom_append_selector($label, 'admin_user_perm', $perms, 'write');
    dom_set_attribute($select, 'id', 'admin_user_perm');

    $li = dom_append_child($ul, 'li');
    $button = dom_append_child($li, 'button', '', array('type' => 'button', 'id' => 'admin_user_button'));
    dom_append_text($button, l('Make'));

    $p = dom_append_child($dom, 'p');
    dom_set_attribute($p, 'id', 'admin_user_msg');

    dom_append_text($p, l('Insert below line into index.php.'));
    
    $pre = dom_append_child($dom, 'pre', 'admin_user_output', array('id' =>'admin_user_output'));
    dom_append_text($pre, '');

    head_tags_add_javascript('resource/script/md5.js');
    $script = dom_append_child($dom, 'script', '', array('type' => 'text/javascript'));
    $realm = AUTH_DIGEST_REALM;
    dom_append_text($script, <<<EOT
var msg = $('admin_user_msg');
msg.hide();
$('admin_user_button').observe('click', function(event) {
    var user = $('admin_user_name').value;
    var pass = $('admin_user_pass').value;
    var perm = $('admin_user_perm').value;
    var pre = $('admin_user_output');
    var digest = MD5_hexhash(user + ':' + '$realm' + ':' + pass);
    var output = "\$AUTH_PERMISSIONS['" + user.replace("'", "\\\\\'") + "'] =  '" + perm + "';\\n";
    output += "\$AUTH_DIGESTS['" + user.replace("'", "\\\\\'") + "'] =  '" + digest + "';";
    while(pre.hasChildNodes()) pre.removeChild(pre.firstChild);
    pre.appendChild(document.createTextNode(output.unescapeHTML()));
    msg.show();
});
EOT
);
}
?>
