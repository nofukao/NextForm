<?php
$WIKI_OPERATORS_FUNCTION['alias'] = true;
function wiki_operator_alias(&$context, $dom, $tree) {
    $params = wiki_get_parameters_string($tree);
    $target = wiki_get_target_string($tree);
    $alias_name = end($params);

    if(!empty($target) && $target[0] == "\n")
	$target = substr($target, 1);

    $context['aliases'][$alias_name] = $target;
}

function wiki_alias_expand(&$context, $dom, $tree) {
    global $wiki_operator_alias_expand_replace_args;

    $alias =& $context['aliases'][$tree->id];

    $new_context = wiki_modify_context($context, array('source_position' => &$tree, 'display' => 'inline'), 0);
    unset($new_context['aliases']);
    $new_context['aliases'] = $context['aliases'];
    unset($new_context['aliases'][$tree->id]);

    $use_equalities = array();
    if(preg_match_all('/<([A-Za-z_-]+)(=[^>]*)?>/', $alias, $matches)) {
	foreach($matches[1] as $name) {
	    if($name !== 'PARAMS' && $name !== 'TARGET')
		$use_equalities[$name] = true;
	}
    }

    $params = wiki_get_parameters_string($tree);
    $values = array();
    $equalities = array();
    foreach($params as $param) {
	if(preg_match('/^([^=]+)\s*=\s*(.*)$/', $param, $matches) && !empty($use_equalities[$matches[1]]))
	    $equalities[$matches[1]] = $matches[2];
	else
	    $values[] = $param;
    }

    $wiki_operator_alias_expand_replace_args['target'] = wiki_tokens_to_string_without_escape($tree->target);
    $wiki_operator_alias_expand_replace_args['params'] =& $values;
    $wiki_operator_alias_expand_replace_args['equalities'] =& $equalities;

    $body = preg_replace_callback('/<([A-Za-z_-]+(=[^>]+)?)>/', 'wiki_alias_expand_replace', $alias);
    //pprint_r($body);

    $adom = dom_create('section');
    wiki_convert_from_source($new_context, $adom, $body, array());
    wiki_alias_remove_position($adom);
    if($adom->childNodes->length == 1 && $adom->firstChild instanceof DOMElement)
	dom_set_position($adom->firstChild, $tree);
    dom_append_dom_children($dom, $adom);
}

function wiki_alias_expand_replace($var) {
    global $wiki_operator_alias_expand_replace_args;
    if(preg_match('/^([^=]+)(?:=(.*))?$/s', $var[1], $matches)) {
	$value = '';
	$is_keep = true;
	if(isset($wiki_operator_alias_expand_replace_args['equalities'][$matches[1]])) {
	    $value = $wiki_operator_alias_expand_replace_args['equalities'][$matches[1]];
	    $is_keep = false;
	} else if($matches[1] == 'PARAMS') {
	    $value = implode(',', $wiki_operator_alias_expand_replace_args['params']);
	} else if($matches[1] == 'TARGET') {
	    $value = $wiki_operator_alias_expand_replace_args['target'];
	}
	if($is_keep && empty($value) && !empty($matches[2]))
	    $value = $matches[2];
	return $value;
    }
    return $var[0];
}

function wiki_alias_remove_position($dom) {
    if(!($dom instanceof DOMElement))
	return;
    if($dom->hasAttribute('data-pagename'))
	return;
    $dom->removeAttribute('data-twl');
    $dom->removeAttribute('data-twp');
    foreach($dom->childNodes as $child) {
	wiki_alias_remove_position($child);
    }
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/FunctionAdvanced
|[[エイリアス(別名)の定義:../../Function/alias]]|&&alias(エイリアス名){エイリアスの定義}|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/FunctionAdvanced
|define alias|&&alias(alias name){alias definition}|
*/
/**
@manual $manualpage$/ja/Wiki/Function/alias
&title &&alias

*概要
 &alias(エイリアス名){エイリアスの定義}

よく使用するWiki記法に名前をつけておき，後で簡単に使用できるように準備します．
これをエイリアスを定義しているといいます．
定義を行うのみでこの記法自体は何の表示も行いません．

定義されたエイリアスは
 &エイリアス名
という形でファンクションと同じように使用できます．
パラメータや対象もファンクションと同様に使用できます．

エイリアスは定義された位置より後ろでのみ使用できます．
定義された位置より後ろであれば [[&&include:../include]] で挿入されたページ内でも使用できます．
全ページ共通で有効なエイリアスは管理者のみ設定できます．

*パラメータ
:エイリアス名|定義したいエイリアスの名前を指定します．&&記号は不要です．

*対象
エイリアスの定義をWiki記法で書きます．

エイリアスの定義内で

<PARAMS> と記述すると，エイリアスが使用された際のパラメータに置き換わります．

<パラメータ名> と記述すると，エイリアスが使用された際のパラメータのうち''パラメータ名=値''形式のものの値に置き換わります．

<TARGET> と記述すると，エイリアスが使用された際の対象に置き換わります．

<PARAMS=初期値> や <TARGET=初期値> のように記述するとパラメータや対象が省略された場合の初期値を設定できます．

定義の先頭を半角スペースにすると行頭記法の整形済みテキストとして解釈されるので注意して下さい．

*例

[[&&color:../color]]と[[&&size:../size]]を同時に使うことが頻繁にある場合，
&&aliasを使用すると簡素に記述することができます．
ここではこの二つの組み合わせをlargecolorというエイリアス名で定義して使用しています．

&wikiexample{
&alias(largecolor){&size(200%){&color(<PARAMS>){<TARGET>}}}

&largecolor(red){大きいし，色もつけたい．けど長い記述はめんどう．}
&largecolor(green){何度も似た記述をする場合はエイリアスが有用．}
}
*/
?>
