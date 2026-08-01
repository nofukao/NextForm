<?php
$TOOLS['page']['summary'] = array(
    'order' => 30,
    'show' => 'summary_tool'
    );

function summary_tool(&$page, $dom) {
    tools_add_link(
	$dom,
	array('name' => l('Summary'),
	      'class' => 'summary',
	      'accesskey' => 'i',
	      'disable' => empty($page),
	      'uri' => url_make($page['name'], array('option' => 'summary'))));
}

$OPTIONS['summary'] = array(
    'show' => 'summary_show',
    'only' => 'summary_only',
    );

function summary_show(&$args, $dom) {
    $page = &target_page($args);
    $page['args']['summarize_link'] = true;
    handle($page, $dom, 'summary');
    return false;
}

function summary_only(&$args, $dom) {
    $page = &target_page($args);
    $page['args']['summarize_link'] = true;
    $summary_dom = dom_create_document();
    handle($page, $summary_dom, 'summary');
    //dom_append_class($summary_dom->firstChild, 'only');
    //$summary = summary_dom_to_array($summary_dom);
    
    //header("Content-Type: application/json; charset=utf-8");
    echo(dom_save_html($summary_dom));
    exit(0);
    //return false;
}


?>