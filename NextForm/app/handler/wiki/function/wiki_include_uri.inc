<?php
/*
 * &include(URL) に URL の種類ごとの埋め込みを足す口。
 *
 * いま登録しているのは画像だけ。**動画サイト (YouTube / ニコニコ動画) の
 * 埋め込みは v0.9.1 で外した。**
 *
 *   - ニコニコの埋め込み先は http:// 固定で、https のページに出すと
 *     ブラウザが混在コンテンツとして遮断する。当たる URL も
 *     http://www.nicovideo.jp/watch/ の形だけで、2015 年の URL 体系のまま
 *   - YouTube は幅と高さを決めるために oembed を**ページの描画中に同期で**
 *     取りに行っていた。しかもそこに EXTERNAL_FETCH_TIMEOUT が渡っておらず、
 *     default_socket_timeout (既定 60 秒) 任せになっていた。YouTube が
 *     応答しないと、その URL を貼ったページの表示がそのぶん止まる
 *
 * 外しても壊れない。どのハンドラにも当たらない URL は wiki_include.inc が
 * 外部リンクに落とす ($handler_dom === false の分岐)。既に貼ってあるページは
 * 埋め込みが動画サイトへのリンクに変わるだけで、本文は失われない。
 *
 * 経緯は docs/development/project-overview.md §7 の「動画サイトの埋め込み」。
 */
$WIKI_OPERATOR_INCLUDE_URI_HANDLERS['image'] = array(
    'regex' => '/\.(jpe?g|png|gif)$/i',
    'function' => 'wiki_operator_include_uri_image');
function wiki_operator_include_uri_image($dom, $uri, $params) {
    $width = $height = 0;

    if(!empty($params)) {
	if(function_exists('getimagesize')) {
	    $cachekey = 'imagedata-' . $uri;
	    $data = cache_serialized_get('', $cachekey, time() - 60 * 60 * 3);
	    if($data !== false) {
		$width = $data['width'];
		$height = $data['height'];
	    } else {
		$size = @getimagesize($uri);
		if(isset($size[0]) && isset($size[1])) {
		    $width = $size[0];
		    $height = $size[1];
		}
		$data = array('width' => $width, 'height' => $height);
		cache_serialized_create('', $cachekey, $data);
	    }
	}
	
	foreach($params as $param) {
	    image_parse_size($param, $width, $height);
	}
    }

    if(in_array('!link', $params) || dom_in_tag_name($dom, 'a')) {
	$e = $dom;
    } else {
	$e = dom_append_link($dom, $uri);
	if(in_array('open', $params))
	    dom_set_attribute($e, 'target', '_blank');
    }
    $img = dom_append_child($e, 'img', '',
			    array(
				'src' => $uri,
				'alt' => $uri,
				));
    if(!empty($width))
	dom_set_attribute($img, 'width', $width);
    if(!empty($height))
	dom_set_attribute($img, 'height', $height);

    return $e;
}
?>
