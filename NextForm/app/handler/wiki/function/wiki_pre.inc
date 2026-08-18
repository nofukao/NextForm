<?php
$WIKI_OPERATORS_FUNCTION['pre'] = true;
$WIKI_OPERATORS_LINE_HEAD['pre'] =
    array('symbol' => ' ', 'group' => true);

function wiki_operator_pre(&$context, $dom, $block) {
    wiki_operator_pre_append($context, $dom, $block, array());
}

function wiki_operator_pre_append(&$context, $dom, $block, $options) {
    $langs = array();
    $is_code = !empty($options['is_code']);
    $is_proportional_ascii_art = !empty($options['is_proportional_ascii_art']);
    $linenum = 0;
    $is_function = false;
    $text = '';
    foreach($block->children as &$tree) {
	if($tree->symbol == '&')
	    $is_function = true;
	if(!empty($tree->parameters)) {
	    wiki_parse_parameters($context, $tree, 
				  array('values' => &$langs,
					'long_flags' => array(
					    'code' => &$is_code,
					    'linenum' => array(&$linenum, 1),
					    'paa' => &$is_proportional_ascii_art,
					    ),
					'equalities' => array(
					    'linenum' => &$linenum
					    ),
				      ));
	    
	    if(count(wiki_tokens_trim_space($tree->target)) > 0) {
		$text .= wiki_tokens_to_string_without_escape($tree->target);
	    }
	} else {
	    $text .= wiki_tokens_to_string_without_escape($tree->target);
	}
    }
    if($is_function) {
	if(!empty($text) && $text[0] == "\n")
	    $text = substr($text, 1);
    }

    if($is_code) {
	head_tags_add_javascript('resource/script/prettify.js');
	// 言語指定が無いと array_pop() は null を返す。そのまま class に渡すと
	// setAttribute(null) で非推奨警告が出るうえ、意味の無い class="" が付く
	$lang = array_pop($langs);
	if(!empty($lang))
	    $lang = 'language-' . $lang;
	else
	    $lang = '';
	$pre = dom_append_child($dom, 'pre', 'code prettyprint linenums:' . $linenum);
	dom_set_position($pre, $block);
	$code = dom_append_child($pre, 'code', $lang);
	sentence_append($code, $text, $context['page'], array('excludes' => array('pagename')));
    } else {
	$pre = dom_append_child_with_position($dom, 'pre', $block);
	dom_append_text($pre, "\n");
	sentence_append($pre, $text, $context['page'], array('excludes' => array('pagename')));
    }
    if($is_proportional_ascii_art)
	dom_append_class($pre, 'paa');
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[整形済みテキスト(ファンクション):../../Function/pre]]|&&pre(code, linenum){対象テキスト}|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|preformatted text (function)|&&pre(code, linenum){text}|
*/
/**
@manual $manualpage$/ja/Wiki/Function/pre
&title &&pre

*概要
 &pre(code, paa, linenum, linenum=開始行番号) {対象テキスト}
対象テキストを整形済みテキストとしてそのまま表示します．
行頭に半角スペースを書く記法と機能は同等ですが，
行頭に半角スペースを入れる代わりに&{と&}で囲むだけで指定ができます。

*パラメータ
:code|対象テキストをプログラム言語としてハイライト表示します．
:paa|プロポーショナルフォントを用いたアスキーアートに適した表示を行います．
:linenum|codeを指定した際に行番号を表示します．
:{linenum=行番号|codeを指定した際に行番号を表示します．行番号は指定された番号から開始されます．}

*対象
対象テキストには整形済みテキストやプログラム言語を書きます．

対象テキストの中に対になっていない ''&{'' や ''&}'' が入っていて，
対象テキストの終わりであると誤解されてしまうことがあります．
その場合， ''&{&{&{'' と ''&}&}&}'' のように連続した ''&{記号'' と ''&}記号'' で囲んで下さい(連続する回数は任意)．

&wikiexample{{{{{{
&pre{{{
}や{のはいった対象テキスト
}}}
}}}}}}

*例

&wikiexample{
&pre{
 +------------+
 | ASCII Art  |====>
 +------------+
}

&pre(paa) {
ここにアスキーアート
}

&pre(code, linenum) {
#include<stdio.h>

int main(int argc, char **argv) {
	printf("Hello NextForm.\n");
	return 0;
}
}

}
*/

$WIKI_OPERATORS_FUNCTION['code'] = true;
function wiki_operator_code(&$context, $dom, $tree) {
    $block = new WikiBlockTree(array($tree), 'pre');
    $block->position = $tree->position;
    $block->length = $tree->length;
    wiki_operator_pre_append($context, $dom, $block, array('is_code' => true));
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[ソースコード:../../Function/code]]|&&code(linenum){ソースコード}|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|source code|&&code(linenum){source code}|
*/
/**
@manual $manualpage$/ja/Wiki/Function/code
&title &&code

*概要
 &code(linenum, linenum=開始行番号) {ソースコード}
プログラミング言語等のソースコードをハイライト表示します．

*パラメータ
:linenum|行番号を表示します．
:{linenum=行番号|行番号を表示します．行番号は指定された番号から開始されます．}

*対象
対象テキストにはプログラミング言語等のソースコードを書きます．

ソースコードの中に対になっていない ''&{'' や ''&}'' が入っていて，
ソースコードの終わりであると誤解されてしまうことがあります．
その場合， ''&{&{&{'' と ''&}&}&}'' のように連続した ''&{記号'' と ''&}記号'' で囲んで下さい(連続する回数は任意)．

&wikiexample{{{{{{
&code{{{
}や{のはいったソースコード
}}}
}}}}}}

*例

&wikiexample{{{
&code() {
#include<stdio.h>
int main(int argc, char **argv) {
	printf("Hello NextForm.\n");
	return 0;
}
}

&code(linenum) {{{
main :: IO ()
main = do
  putStrLn "Hello NextForm."
}}}
*/


$WIKI_OPERATORS_FUNCTION['paa'] = true;
function wiki_operator_paa(&$context, $dom, $tree) {
    $block = new WikiBlockTree(array($tree), 'pre');
    $block->position = $tree->position;
    $block->length = $tree->length;
    wiki_operator_pre_append($context, $dom, $block, array('is_proportional_ascii_art' => true));
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[アスキーアート:../../Function/paa]]|&&paa{アスキーアート}|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|proportional ascii art|&&paa{proportional ascii art}|
*/
/**
@manual $manualpage$/ja/Wiki/Function/paa
&title &&paa

*概要
 &paa{アスキーアート}
プロポーショナルフォントによるアスキーアートを表示します．
通常のアスキーアートは[[&&pre:../pre]]を使用して下さい．

*パラメータ
パラメータは指定しません．

*対象
対象にアスキーアートを書きます．

アスキーアートの中に対になっていない ''&{'' や ''&}'' が入っていて，
アスキーアートの終わりであると誤解されてしまうことがあります．
その場合， ''&{&{&{'' と ''&}&}&}'' のように連続した ''&{記号'' と ''&}記号'' で囲んで下さい(連続する回数は任意)．

*例

&wikiexample{{{{{{
&paa{{{

　　 ／￣￣＼
　／　　 _ノ　　＼
　|　　　（ ●）（●）　＜おっと、それ以上は言うなよ…
.　|　　　　 （__人__）＿＿＿_
　 |　　　　　｀ ⌒／ ─'　'ー＼
.　 |　　 　 　　／（ ○） 　（○）＼
.　 ヽ　　 　 ／　　⌒（n_人__）⌒ ＼
　　 ヽ　　　|、　　　　（　　ヨ　　　　|
　　　/　　 　`ー─－　　厂　　　／
　　　|　　 ､ ＿ 　　__,,／　　　　 ＼
}}}
}}}}}}
*/
?>
