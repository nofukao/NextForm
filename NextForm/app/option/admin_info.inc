<?php
$OPTIONS['admin_info'] = array(
    'show' => 'admin_info_show',
    );

function admin_info_dom_append_section($dom, $title, $list = false) {
    $section = dom_append_child($dom, 'section', 'section');
    $h = dom_append_child($section, 'h1');
    dom_append_text($h, $title);

    if(!empty($list)) {
	$ul = dom_append_child($section, 'ul');
	foreach($list as $value) {
	    $li = dom_append_child($ul, 'li');
	    dom_append_text($li, $value);
	}
    }
    return $section;
}

function admin_info_show(&$args, $dom) {
    global $HANDLERS;
    global $OPTIONS;
    global $WIKI_OPERATORS_INLINE;
    global $WIKI_OPERATORS_LINE_HEAD;
    global $WIKI_OPERATORS_FUNCTION;
    global $WIKI_ALIASES;
    global $WIKI_OPERATOR_INCLUDE_URI_HANDLERS;
    global $FILE_CONTENT_TYPE_HANDLERS;

    //auth_check(false, 'admin');
    title_set_l('System information');

    if(TORATORAWIKI_REVISION !== '$REVISION$')
	$revision = TORATORAWIKI_REVISION;
    else {
	$revision = shell_exec('svnversion -n ' . escapeshellarg(APP_DIR_PATH . '/..'));
    }

    $dom = dom_append_child($dom, 'section', 'page');

    // NextForm 自身の版数と、fork 元である上流の版数を並べて出す。
    // アップグレードが反映されたかを画面から確認できるようにするため。
    admin_info_dom_append_section($dom, l('Version'),
				  array('NextForm ' . NEXTFORM_VERSION,
					'ToraToraWiki ' . TORATORAWIKI_VERSION . ' (Revision: ' . $revision . ')'));
    
    admin_info_dom_append_section($dom, l('Page type'), array_keys($HANDLERS));
    
    admin_info_dom_append_section($dom, l('Options'), array_keys($OPTIONS));

    $section = admin_info_dom_append_section($dom, l('Wiki'));
    admin_info_dom_append_section($section, l('Wiki inline operator'), array_keys($WIKI_OPERATORS_INLINE));
    admin_info_dom_append_section($section, l('Wiki line head operator'), array_keys($WIKI_OPERATORS_LINE_HEAD));
    admin_info_dom_append_section($section, l('Wiki function operator'), array_keys($WIKI_OPERATORS_FUNCTION));
    admin_info_dom_append_section($section, l('Wiki aliases'), array_keys($WIKI_ALIASES));
    admin_info_dom_append_section($section, l('Wiki &include handler'), array_keys($WIKI_OPERATOR_INCLUDE_URI_HANDLERS));

    $section = admin_info_dom_append_section($dom, l('File'));
    admin_info_dom_append_section($section, l('Content-type handler'), array_keys($FILE_CONTENT_TYPE_HANDLERS));
}
?>
