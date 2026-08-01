<?php
$OPTIONS['admin_import_otherwiki'] = array(
    'show' => 'admin_import_otherwiki_show',
    'write' => 'admin_import_otherwiki_write',
    'import' => 'admin_import_otherwiki_import',
    );
$IMPORT_OTHERWIKI_TYPE = array();
if(IMPORT_PUKIWIKI_ENABLE === 'true') $IMPORT_OTHERWIKI_TYPE['pukiwiki'] = 'PukiWiki';
if(IMPORT_ATWIKIBACKUP_ENABLE === 'true') $IMPORT_OTHERWIKI_TYPE['atwikibackup'] = '@wikiデータバックアップ';

function admin_import_otherwiki_show(&$args, $dom) {
    global $IMPORT_OTHERWIKI_TYPE;
    auth_warn(false, 'admin');
    title_set_l('Import pages from other wiki data');

    message_notice(l('Pages and histories will be overwritten by other wiki pages.'));

    $form = dom_append_form($dom, 
			    array('option' => 'admin_import_otherwiki', 
				  'action' => 'write'), '', 'GET');
    $label = dom_append_child($form, 'label');
    dom_append_text($label, l('Wiki type'));
    dom_append_selector($label, 'otherwiki_type', $IMPORT_OTHERWIKI_TYPE);
    $submit = dom_append_child($form, 'input', '', array('type' => 'submit', 'value' => l('Enter')));
}

function admin_import_otherwiki_write(&$args, $dom) {
    global $IMPORT_OTHERWIKI_TYPE;
    
    auth_check(false, 'admin');
    title_set_l('Import pages from other wiki data');

    if(!isset($args['otherwiki_type']) || !isset($IMPORT_OTHERWIKI_TYPE[$args['otherwiki_type']])) {
	message_error(l('Invalid parameter.'));
	return;
    }
    $module_path = APP_DIR_PATH . '/otherwiki/' . $args['otherwiki_type'] . '.inc';
    if(!is_file($module_path)) {
	message_error(l('Invalid parameter.'));
	return;
    }
    include($module_path);

    $new_args = $args;
    $new_args['action'] = 'import';
    $form = dom_append_form($dom, $new_args, '', 'GET');
    dom_append_input_hidden($form, array('firsttime' => 'true'));
    import_otherwiki_form($form);
    $submit = dom_append_child($form, 'input', '', array('type' => 'submit', 'value' => l('Run')));
}

function admin_import_otherwiki_import(&$args, $dom) {
    global $IMPORT_OTHERWIKI_TYPE;
    page_stop_processing();
    auth_check(false, 'admin');
    title_set_l('Import pages from other wiki data');

    if(!isset($args['otherwiki_type']) || !isset($IMPORT_OTHERWIKI_TYPE[$args['otherwiki_type']])) {
	message_error(l('Invalid parameter.'));
	return;
    }
    $module_path = APP_DIR_PATH . '/otherwiki/' . $args['otherwiki_type'] . '.inc';
    if(!is_file($module_path)) {
	message_error(l('Invalid parameter.'));
	return;
    }
    include($module_path);

    if(!import_otherwiki_form_check($args))
	return;

    if(isset($args['firsttime']) && import_otherwiki_prepare($args, $dom) === false)
	return;
    unset($args['firsttime']);

    $queue = import_otherwiki_create_queue($args);
    if($queue === false) {
	return;
    }

    $show_options = array(
	'progress' => '$num$ / $totalnum$ pages ($percent$%)',
	'no_progress' => 'Rebuilding search index..'
	);

    $func_arg = false;
    run_queue($args, $dom, $show_options, $queue,
	      'admin_import_otherwiki_processor',
	      'admin_import_otherwiki_initializer',
	      'admin_import_otherwiki_finalizer', $func_arg);
}

function admin_import_otherwiki_initializer($args, $dom, &$func_arg) {
    return import_otherwiki_initializer($args, $dom, $func_arg);
}

function admin_import_otherwiki_processor($args, $dom, &$func_arg, &$job) {
    return import_otherwiki_processor($args, $dom, $func_arg, $job);
}

function admin_import_otherwiki_finalizer($args, $dom, &$func_arg) {
    $result = import_otherwiki_finalizer($args, $dom, $func_arg);
    $p = dom_append_child($dom, 'p');
    dom_append_text($p, l('Done.'));
    $p = dom_append_child($dom, 'p');
    dom_append_link($p, url_make('', array('option' => 'admin')), l('Return'));
    return $result;
}

?>
