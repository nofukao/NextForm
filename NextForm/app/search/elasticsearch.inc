<?php
$SEARCH_ENGINES['elasticsearch'] = array(
    'index_update' => 'elasticsearch_index_update',
    'index_delete' => 'elasticsearch_index_delete',
    'index_clear' => 'elasticsearch_index_clear',
    'search' => 'elasticsearch_search',
    );

function elasticsearch_request($path, $method, $body = false) {
    $url = SEARCH_ELASTICSEARCH_URL . $path;
    $headers = array(
	'Content-Type: application/json'
	);
    /* 検索も索引更新もリクエストの途中で呼ばれる。待ち時間に上限を掛ける */
    $options = array('http' => array(
			 'ignore_errors' => true,
			 'method' => $method,
			 'header' => implode("\r\n", $headers),
			 'timeout' => EXTERNAL_FETCH_TIMEOUT,
			 ));
    if($body !== false)
	$options['http']['content'] = $body;
    return @file_get_contents($url, false, stream_context_create($options));
}

function elasticsearch_page_path($page) {
    return '/page/page_' . urlencode($page['name']);
}

function elasticsearch_index_update($page) {
    $texts_info = search_page_texts($page);
    $data = array(
	'name' => $page['name'],
	'title' => default_value($page['meta']['title'], ''),
	'description' => default_value($texts_info['description'], ''),
	'tag' => default_value($texts_info['tag'], ''),
	'mtime' => $page['mtime'],
	'is_system_page' => page_is_system_page($page['name']),
	'contents' => default_value($texts_info['body'], array())
	);
    elasticsearch_request(elasticsearch_page_path($page), 'PUT', json_encode($data));
}

function elasticsearch_index_delete($page) {
    elasticsearch_request(elasticsearch_page_path($page), 'DELETE');
}

function elasticsearch_index_clear() {
    elasticsearch_request('/page/_query', 'DELETE', '{"query":{"match_all":{}}}');
}

function elasticsearch_search($query, $search_options) {
    global $SEARCH_STRLEN;
    $search_results = array();

    $phrase_data = array();
    foreach($query as $q) {
	if($SEARCH_STRLEN($q) <= 1) {
	    $phrase_data[] = array(
		'term' => array('_all' => $q));
	} else {
	    $phrase_data[] = array(
		'match_phrase' => array(
		    '_all' => array(
			'query' => $q,
			'minimum_should_match' => '100%',
			)
		    )
		);
	}
    }

    $query_data = array(
	'size' => 100,
	'fields' => array('name', 'mtime', 'title'),
	'highlight' => array(
	    'pre_tags' => array(''),
	    'post_tags' => array(''),
	    'order' => 'score',
	    'fields' => array(
		'name' => array("number_of_fragments" => 1),
		'title' => array("number_of_fragments" => 1),
		'description' => array("number_of_fragments" => 1),
		'tag' => array("number_of_fragments" => 1),
		'contents' => array("number_of_fragments" => 10),
		)
	    ),
	'query' => array('bool' => array('must' => $phrase_data)),
	);

    if(isset($search_options['order']) && $search_options['order'] === 'mtime')
	$query_data['sort'] = array(array('mtime' => 'desc'), '_score');
    else
	$query_data['sort'] = array('_score');
    if(!empty($search_options['is_ignore_system_page']))
	$query_data['query']['bool']['must'][] = array('term' => array('is_system_page' => false));


    $results = elasticsearch_request('/page/_search', 'GET', json_encode($query_data));
    $results = json_decode($results, true);

    if(!isset($results['hits']['hits']))
	return $search_results;

    foreach($results['hits']['hits'] as $result) {
	$pagename = $result['fields']['name'][0];
	if(!auth_has_permission($pagename, 'read'))
	    continue;

	$snippets = array();
	if(isset($result['highlight']['title'][0]))
	    $snippets[] = l('Title') . ': ' . $result['highlight']['title'][0];
	else if(isset($result['highlight']['name'][0]))
	    $snippets[] = l('Title') . ': ' . $result['highlight']['name'][0];
	if(isset($result['highlight']['description'][0]))
	    $snippets[] = l('Description') . ': ' . $result['highlight']['description'][0];
	if(isset($result['highlight']['tag'][0]))
	    $snippets[] = l('Tags') . ': ' . $result['highlight']['tag'][0];
	if(isset($result['highlight']['contents']))
	    $snippets = array_merge($snippets, $result['highlight']['contents']);

	$search_result = array(
	    'pagename' => $pagename,
	    'mtime' => $result['fields']['mtime'][0],
	    'snippets' => $snippets,
	    'score' => $result['_score'],
	    );
	if(isset($result['fields']['title'][0]) && $result['fields']['title'][0] !== '')
	    $search_result['title'] = $result['fields']['title'][0];
	$search_results[$search_result['pagename']] = $search_result;
    }

    return $search_results;
}

?>
