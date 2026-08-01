<?php
$OPTIONS['partial'] = array(
    'show' => 'partial_show',
    );

function partial_show(&$args, $dom) {
    $page = &target_page($args);
    $response = array();

    if(!isset($args['position']) || !isset($args['length'])) {
	$response['status'] = 'params';
    } else if(!page_type_is_text_base(page_get_type($page))) {
	$response['status'] = 'type';
    } else {
	if(empty($page['is_exists']) && $args['length'] != -1) {
	    $response['status'] = 'exists';
	} else {
	    $ticket = default_value($args['ticket'], '');
	    $page_ticket = default_value($page['meta']['ticket'], '');
	    if($ticket === $page_ticket) {
		$contents = page_get_contents($page);
		if($args['length'] >= 0)
		    $contents = substr($contents, $args['position'], $args['length']);
		if($contents === false)
		    $contents = '';
		$response['source'] = $contents;
		$response['lines'] = max(1, substr_count($contents, "\n"));
		$response['status'] = 'ok';
		if(!auth_has_permission($page['name'], 'write'))
		    $response['status'] = 'permission';
		else if(!empty($page['meta']['lock'])) {
		    if(lock_has_permission($page))
			$response['status'] = 'ignorelock';
		    else
			$response['status'] = 'lock';
		}
	    } else
		$response['status'] = 'ticket';
	}
    }
    
    $json_text = json_encode($response);
    header("Content-Type: application/json; charset=utf-8");
    echo($json_text);
    exit(0);
}
?>
