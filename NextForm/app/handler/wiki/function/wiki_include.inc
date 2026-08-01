<?php
$WIKI_OPERATORS_FUNCTION['include'] = true;

function wiki_operator_include(&$context, $dom, $tree) {
    global $WIKI_OPERATOR_INCLUDE_URI_HANDLERS;
    
    $pagename = '';
    $include_params = array();
    $uri = '';
    $name = '';
    $gravity = false;
    wiki_parse_parameters(
	$context, $tree,
	array('pagename' => &$pagename,
	      'uri' => &$uri,
	      'long_flags' => array(
		  'left' => array(&$gravity, 'left'),
		  'right' => array(&$gravity, 'right'),
		  ),
	      'values' => &$include_params));

    $figcaption = false;
    if(!wiki_tokens_is_whitespace($tree->target)) {
	$figure = dom_append_child($dom, 'figure');
	$dom = $figure;
	$figcaption = dom_create('figcaption');
	wiki_convert_from_tokens($context, $figcaption, $tree->target);
    }

    if(!empty($context['texts_only'])) {
	if($figcaption !== false)
	    dom_append_dom($dom, $figcaption);
	return;
    }
    $is_external = false;
    $handler_dom = false;
    if(!empty($uri)) {
	$name = $uri;
	foreach($WIKI_OPERATOR_INCLUDE_URI_HANDLERS as &$uri_handler) {
	    if(preg_match($uri_handler['regex'], $uri)) {
		$handler_dom = $uri_handler['function']($dom, $uri, $include_params);
		break;
	    }
	}
	$is_external = true;
    } else {
	if($pagename === '') {
	    dom_append_error($dom, l('&include pagename not specified'));
	    return;
	}

	if(!auth_has_permission($pagename, 'read')) {
	    dom_append_error($dom, l('&include not permitted.'));
	    return;
	}
	
	$name = $pagename;
	if(page_is_exists($pagename)) {
	    $context['page']['depend_pages'][$pagename] = true;
	    $uri = url_make($pagename);
	    $page = page_read($pagename);
	    $type = page_get_type($page);
	    $page['include_pages'] = default_value($context['page']['include_pages'], array());
	    $page['include_pages'][] = $context['page']['name'];
	    $page['wiki_context'] = &$context;
	    if(in_array($page['name'], $page['include_pages'])) {
		dom_append_error($dom, l('&include loop detected.'));
		return;
	    }
	    if($type == 'file') {
		user_history_add_page_view($page['name']);
		$content_type_handler = &file_get_content_type_handler($page);
		if(function_exists($content_type_handler['embed'])) {
		    $handler_dom = $content_type_handler['embed']($page, $dom, $include_params);
		}
	    } else {
		$handler_dom = page_handle($page, $dom);
	    }
	} else {
	    $uri = url_make($pagename, array('newpage_type' => 'file'));
	}
    }

    if($handler_dom === false) {
	$a = dom_append_link($dom, $uri, $name);
	dom_append_class($a, 'uri');
	if($is_external)
	    dom_append_class($a, 'external');
	if(in_array('open', $include_params))
	    dom_set_attribute($a, 'target', '_blank');
    }

    $outside_dom = $handler_dom;
    if($figcaption !== false) {
	dom_append_dom($dom, $figcaption);
	$outside_dom = $dom;
    }

    if($outside_dom !== false) {
	dom_append_class($outside_dom, 'figure');
	if($gravity === 'left' || $gravity === 'right') {
	    dom_append_class($outside_dom, 'gravity_' . $gravity);
	}
    }

}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[コンテンツの埋め込み:../../Function/include]]|&&include(&[&[ページ名&]&]); or &&include(URL);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|Embedding content|&&include(&[&[page name&]&]); or &&include(URL);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/include

&title &&include

*概要
 &include([[ページ名]], 幅x高さ, 拡大率%, open);
 &include(URL , 幅x高さ, 拡大率%, open);
指定されたページまたはURLの内容を埋め込みます．指定するページはwiki,text,file等どの種別でもかまいません．
ページ内に画像や動画を埋め込むために使用できます．

画像のURLを指定するとその画像を埋め込むことができます．
またYouTubeやニコニコ動画のURLを書くとその動画が埋め込まれます．
''URLに続いて,(カンマ)を書く場合はスペースを入れて下さい．そうしないとカンマもURLとして認識されます．''

添付ファイル(子ページとしてアップロードしたファイル)を埋め込む際は
編集画面で埋め込む位置にカーソルを移動させ，添付ボタンを押してファイルを選択すると便利です．

*パラメータ
:{幅x高さ|幅と高さの間の記号は半角英字のx(エックス)です．
640x400のようにピクセル単位で指定します．640xやx400のように幅や高さだけを指定することもできます．
最後に&!記号をつけると縦横の比率を無視してサイズ変更を行います．max 640x400のように最初にmaxを表記すると，
指定したサイズを越えない場合はサイズの変更を行わなくなります．}
:{拡大率%|元の大きさを基準としてどれぐらい拡大するかをパーセント表記で指定します．
元の大きさが取得できないコンテンツの場合は無視されます．}
:{!adjust|画像のリサイズをサーバー側ではなくブラウザで行うようにします．
画質が向上する場合がありますが，通信量が増えます．添付ファイルの画像にのみ有効です．
}
:open|画像をクリックした際に新しいウィンドウ(タブ)で開くようにします．
:!link|画像を表示するだけでリンクにしないようにします．
:left|左側に独立して配置します．
:right|右側に独立して配置します．

*対象
対象は必須ではありません．
対象を書いた場合は埋め込んだコンテンツのキャプションとして表示されます．

*例

 &include([[./some.jpg]], 600x300);
 &include([[./foo.png]], 600x, open) { fooの様子 }
 &include(http://www.youtube.com/watch?v=vdQj2ohqCBk);

*/
?>
