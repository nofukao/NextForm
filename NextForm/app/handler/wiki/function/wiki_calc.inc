<?php
$WIKI_OPERATORS_FUNCTION['calc'] = true;
$WIKI_OPERATORS_LINE_HEAD['calclist'] =
    array('symbol' => '=:?', 'group' => true);

$WIKI_CALC_PARSE_2OPS = array(
    array('left',  array('&')),
    array('right', array('=')),
    array('left',  array('+', '-')),
    array('left',  array('%')),
    array('left',  array('*', '/')),
    array('right', array('^')),
    null,
    );

function wiki_operator_calc(&$context, $dom, $tree) {
    $span = dom_append_child($dom, 'span', 'calc');
    $new_context = $context;
    wiki_operator_calc_internal($new_context, $span, $tree);
    //dom_set_attribute($dom, 'data-value', dom_get_attribute($span, 'data-value'));
}

function wiki_operator_calclist(&$context, $dom, $tree) {
    if(!empty($context['cell'])) {
	foreach($tree->children as $child) {
	    wiki_operator_calc($context, $dom, $child);
	}
    } else {
	$ul = dom_append_child($dom, 'ul', 'calc');
	dom_set_position($ul, $tree);
	foreach($tree->children as $child) {
	    $li = dom_append_child_with_position($ul, 'li', $child);
	    $new_context = $context;
	    wiki_operator_calc_internal($new_context, $li, $child);
	}
    }
}

function wiki_operator_calc_internal(&$context, $dom, $tree) {
    $args = wiki_get_parameters_string($tree);
    $original_string = wiki_get_target_string($tree);
    $uncomment_string = trim(preg_replace('/(\[.*?\])/', '', $original_string));

    $replace = true;
    $equal_position = '';
    if(preg_match('/^\s*=(.*)/s', $uncomment_string, $maches)) {
	$string = $maches[1];
	$equal_position = 'left';
    } else if(preg_match('/(.*)=\s*$/s', $uncomment_string, $maches)) {
	$string = $maches[1];
	$equal_position = 'right';
	$replace = false;
    } else {
	$string = $uncomment_string;
    }

    if(!isset($context['page']['calc']))
	$context['page']['calc'] = array('vars' => array());
    if(!isset($context['page']['calc']['vars']))
	$context['page']['calc']['vars'] = array();

    $result = wiki_calc($string, $context);

    $round = 2;
    $output_result = true;

    if($tree instanceof WikiApplyTree && $tree->symbol == '=:') {
	$output_result = false;
    }

    $formats = default_value($context['formats'], array());

    wiki_parse_parameters(
	$context, $tree,
	array(
	    'equalities' => array(
		'round' => &$round,
		),
	    'long_flags' => array(
		'replace' => &$replace,
		'result' => &$output_result,
		'comma' => &$formats['comma'],
		),
	    )
	);

    $context['calc_result'] = $result;

    if($result !== false) {
	scalar_walk($result, 'round', $round);
	scalar_walk($result, 'wiki_calc_format', $formats);
	if(is_array($result))
	    $result = implode(' & ', $result);
    } else {
	$result = 'ERROR';
    }

    if($output_result) {
	if($replace) {
	    dom_append_text($dom, $result);
	} else {
	    if($equal_position == 'left') {
		$output = preg_replace('/^([^=]*)=/', $result . '$1=', $original_string);
	    } else if($equal_position == 'right') {
		$output = preg_replace('/=([^=]*)$/', '= '. $result . '$1', $original_string);
	    } else {
		$output = $original_string . ' = ' . $result;
	    }
	    dom_append_text($dom, $output);
	}
    } else {
	dom_append_text($dom, $original_string);
    }
}

function wiki_calc($string, &$context) {
    $tokens = wiki_calc_lex($string, $context);
    $tree = wiki_calc_parse($tokens);
    if(!empty($tokens)) {
	return false;
    }
    $result = wiki_calc_eval($tree, $context);
    return $result;
}

function wiki_calc_lex($string, &$context) {
    preg_match_all(
	'/([\d\.]+(?:,[\d\.]+)*|[\*\/\-\+=\(\)\^%&]|[a-zA-Z]+\d+:[a-zA-Z]+\d+|[a-zA-Z][\w\d]*)/',
	$string, $maches);
    $tokens = $maches[1];
    foreach($tokens as &$token) {
	if(strpos($token, ',') !== false) {
	    if(!isset($context['formats']['comma']) || is_null($context['formats']['comma']))
		$context['formats']['comma'] = true;
	    $token = str_replace(',', '', $token);
	}
    }
    return $tokens;
}

function wiki_calc_parse(&$tokens) {
    return wiki_calc_parse_2op($tokens, 0);
}

function wiki_calc_parse_2op(&$tokens, $opi) {
    global $WIKI_CALC_PARSE_2OPS;
    if($WIKI_CALC_PARSE_2OPS[$opi] === null)
	return wiki_calc_parse_prim($tokens);
    else {
	$lower = wiki_calc_parse_2op($tokens, $opi + 1);
	return wiki_calc_parse_2op_d($tokens, $opi, $lower);
    }
}

function wiki_calc_parse_2op_d(&$tokens, $opi, $child) {
    global $WIKI_CALC_PARSE_2OPS;
    if(count($tokens) == 0)
	return $child;

    if(in_array($tokens[0], $WIKI_CALC_PARSE_2OPS[$opi][1])) {
	$token = array_shift($tokens);
	if($WIKI_CALC_PARSE_2OPS[$opi][0] == 'right') {
	    return array($token, $child, wiki_calc_parse_2op($tokens, $opi));
	} else {
	    $t = array($token, $child, wiki_calc_parse_2op($tokens, $opi + 1));
	    return wiki_calc_parse_2op_d($tokens, $opi, $t);
	}
    } else
	return $child;
}

function wiki_calc_parse_prim(&$tokens) {
    if(!isset($tokens[0])) {
	return false;
    } else if($tokens[0] == '(') {
	array_shift($tokens);
	$e = wiki_calc_parse_2op($tokens, 0);
	if(!isset($tokens[0]) || $tokens[0] != ')')
	    return false;
	array_shift($tokens);
	return $e;
    } else if($tokens[0] == '-' && isset($tokens[1])) {
	array_shift($tokens);
	return array('~', array_shift($tokens));
    } else if(ctype_alpha($tokens[0]) && isset($tokens[1]) && $tokens[1] == '(') {
	$name = array_shift($tokens);
	array_shift($tokens);
	$call = array($name, wiki_calc_parse_2op($tokens, 0));
	if(array_shift($tokens) != ')')
	    return false;
	return $call;
    } else {
	return array_shift($tokens);
    }
}

function wiki_calc_eval(&$tree, &$context) {
    $env = &$context['page']['calc'];
    if(is_array($tree)) {
	$operand_num = count($tree) - 1;
	if($operand_num == 2) {
	    if($tree[0] === '=') {
		if(is_array($tree[1]) || !preg_match('/^[a-zA-Z][\w\d]*$/', $tree[1]))
		    return false;
		$val = wiki_calc_eval($tree[2], $context);
		$env['vars'][$tree[1]] = $val;
		return $val;
	    }
	    
	    $operand1 = wiki_calc_eval($tree[1], $context);
	    $operand2 = wiki_calc_eval($tree[2], $context);
	    if($operand1 === false || $operand2 === false)
		return false;

	    if($tree[0] == '&') {
		if(!is_array($operand1))
		    $operand1 = array($operand1);
		if(!is_array($operand2))
		    $operand2 = array($operand2);
		return array_merge($operand1, $operand2);
	    } else {
		if(is_array($operand1)) {
		    if(count($operand1) == 1)
			$operand1 = $operand1[0];
		    else
			return false;
		}
		if(is_array($operand2)) {
		    if(count($operand2) == 1)
			$operand2 = $operand2[0];
		    else
			return false;
		}
		switch($tree[0]) {
		case '+': return $operand1 + $operand2;
		case '-': return $operand1 - $operand2;
		case '*': return $operand1 * $operand2;
		case '^': return pow($operand1, $operand2);
		case '%':
		    if($operand2 == 0)
			return false;
		    return (int)$operand1 % (int)$operand2;
		case '/':
		    if($operand2 == 0)
			return false;
		    return $operand1 / $operand2;
		}
	    }
	} else if($operand_num == 1) {
	    $operand1 = wiki_calc_eval($tree[1], $context);
	    if($operand1 === false)
		return false;


	    switch(strtolower($tree[0])) {

	    case '~':
	    case 'sin':
	    case 'cos':
	    case 'tan':
	    case 'ceil':
	    case 'floor':
		if(is_array($operand1))
		    return false;
		switch(strtolower($tree[0])) {
		case '~': return - $operand1;
		case 'sin': return sin($operand1);
		case 'cos': return cos($operand1);
		case 'tan': return tan($operand1);
		case 'ceil': return ceil($operand1);
		case 'floor': return floor($operand1);
		}

	    case 'log':
		if(is_array($operand1)) {
		    if(isset($operand1[1]))
			return log($operand1[1], $operand1[0]);
		    return log($operand1[0]);
		}
		return log($operand1);
	    case 'round':
		if(is_array($operand1)) {
		    if(isset($operand1[1]))
			return round($operand1[1], $operand1[0]);
		    return round($operand1[0]);
		}
		return round($operand1);
	    case 'min':
		if(is_array($operand1)) {
		    if(empty($operand1))
			return false;
		    return min($operand1);
		}
		return $operand1;
	    case 'max':
		if(is_array($operand1)) {
		    if(empty($operand1))
			return false;
		    return max($operand1);
		}
		return $operand1;
	    case 'sum':
		if(is_array($operand1))
		    return array_sum($operand1);
		return $operand1;
	    case 'average':
		if(is_array($operand1)) {
		    $sum = array_sum($operand1);
		    $n = count($operand1);
		    if($n != 0)
			return $sum / $n;
		    else
			return false;
		}
		return $operand1;

	    case 'up':
	    case 'down':
	    case 'left':
	    case 'right':
		$skip = 1;
		$num = 0;
		if(is_array($operand1)) {
		    if(isset($operand1[0]))
			$skip += (int)$operand1[0];
		    if(isset($operand1[1]))
			$num = (int)$operand1[1];
		} else {
		    $skip += (int)$operand1;
		}
		switch(strtolower($tree[0])) {
		case 'up': return wiki_calc_eval_cell($context, $context['cell']['x'], $context['cell']['y'], 0, -$skip, 0, -1, 0, $num);
		case 'down': return wiki_calc_eval_cell($context, $context['cell']['x'], $context['cell']['y'], 0, $skip, 0, 1, 0, $num);
		case 'left': return wiki_calc_eval_cell($context, $context['cell']['x'], $context['cell']['y'], -$skip, 0, -1, 0, $num, 0);
		case 'right': return wiki_calc_eval_cell($context, $context['cell']['x'], $context['cell']['y'], $skip, 0, 1, 0, $num, 0);
		}
		
	    }
	} else
	    return false;
    } else if($tree === false) {
	return false;
    } else if(preg_match('/^([A-Z]+)(\d+):([A-Z]+)(\d+)$/', $tree, $matches)) {
	$ax1 = alpha_to_int($matches[1]);
	$ay1 = (int)$matches[2] - 1;
	$ax2 = alpha_to_int($matches[3]);
	$ay2 = (int)$matches[4] - 1;
	$x1 = min($ax1, $ax2);
	$y1 = min($ay1, $ay2);
	$x2 = max($ax1, $ax2);
	$y2 = max($ay1, $ay2);
	$values = wiki_calc_eval_cell($context, $x1, $y1, 0, 0, 1, 1, $x2 - $x1 + 1, $y2 - $y1 + 1);
	return $values;

    } else if(preg_match('/^([A-Z]+)(\d+)$/', $tree, $matches)) {
	$x = alpha_to_int($matches[1]);
	$y = (int)$matches[2] - 1;
	return wiki_calc_eval_cell($context, $x, $y, 0, 0, 0, 0, 1, 1);
    } else if(preg_match('/^[a-zA-Z][\w\d]*$/', $tree)) {
	if(isset($env['vars'][$tree]))
	    return $env['vars'][$tree];

	switch(strtolower($tree)) {
	case 'pi': return M_PI;
	case 'e': return M_E;
	case 'up': return wiki_calc_eval_cell($context, $context['cell']['x'], $context['cell']['y'], 0, -1, 0, -1);
	case 'down': return wiki_calc_eval_cell($context, $context['cell']['x'], $context['cell']['y'], 0, 1, 0, 1);
	case 'left': return wiki_calc_eval_cell($context, $context['cell']['x'], $context['cell']['y'], -1, 0, -1, 0);
	case 'right': return wiki_calc_eval_cell($context, $context['cell']['x'], $context['cell']['y'], 1, 0, 1, 0);
	}
	return false;
    } else if(preg_match('/^[\d\.]+$/', $tree)) {
	return $tree;
    }
    return false;
}

function wiki_calc_eval_cell(&$context, &$asx, &$asy, $ssx, $ssy, $dx, $dy, $ux = 0, $uy = 0) {
    if(!isset($asx) || !isset($asy))
	return false;
    $sx = $asx + $ssx;
    $sy = $asy + $ssy;
    $env = &$context['page']['calc'];
    $values = array();
    for($y = $sy, $cy = 0; $uy == 0 || $cy < $uy; $y += $dy, $cy++) {
	for($x = $sx, $cx = 0; $ux == 0 || $cx < $ux ; $x += $dx, $cx++) {
	    if(isset($env['cells'][$y][$x])) {
		$value = wiki_operator_table_cell($context, $env['cells'][$y][$x]);
		if($value === false)
		    return false;
		if($value !== 'IGNORE') {
		    if(is_array($value))
			$values = array_merge($values, $value);
		    else
			$values[] = $value;
		}
	    } else if($ux * $uy == 0) {
		$ux = $cx;
		$uy = $cy;
	    }
	    if($dx == 0)
		break;
	}
	if($dy == 0)
	    break;
    }
    return $values;
}

function wiki_calc_format($value, &$formats) {
    if(empty($formats))
	return $value;
    if(!empty($formats['comma']) && preg_match('/^\-?[\d\.]+$/', $value)) {
	$value = (string)$value;
	$value = preg_replace('/^\-?\d+(\..*)?$/', number_format($value) . '$1', $value);
    }
    return $value;
}

/**
@manual $manualpage$/ja/Wiki/Calc

&title 数値計算

*概要
NextFormにはWiki記法中に式を書くことでその式の値を計算する数値計算機能があります．
数値計算機能を使うには以下のように行頭に&=を書く方法と&&calcを使う方法があります．

 = 式

 &calc{式}

行頭に&=を書く方法は箇条書きとして扱われ，複数続けて書くことができます．
&calcの方は文中で計算を行う際に使用します．

*式

式の中の数値は1,000のように&,で桁の区切りがされていてもかまいません．
式の中では以下の演算子, 関数, 定数 が使えます．
関数に複数の引数を渡す場合は式を&&で区切ります．

- 演算子
-- 四則演算 &+, &-, &*, &/
-- 剰余算 &%
-- べき乗 &^
-- 代入 &=
- 関数
-- 三角関数 sin(x), cos(x), tan(x)
-- 対数 log(x), log(base & x) 
-- 四捨五入 round(x), round(x & 桁数) 
-- 切り上げ ceil(x)
-- 切り捨て floor(x)
-- 最小 min(a & b & c & ..)
-- 最大 max(a & b & c & ..)
-- 合計 sum(a & b & c & ..)
-- 平均 average(a & b & c & ..)
- 定数
-- 円周率 pi
-- 自然対数の底 e

例えば以下のように使用できます．

&wikiexample{
= (1 + 2) * 3 ^ 2 / 4
&calc{ sin(pi/4) + cos(pi/4) + log(2 & 1024) }
}

数式内の&[と&]で囲まれた部分は無視されます．
これは以下のように単位や説明を記載するために使用できます．

&wikiexample{= 1,000[ml] * 20[本] / 1000[L換算]}

通常は式を書くとその式の計算結果に置き換わります．
式の最後に&=を書くと結果を置き換えずに数式と結果の両方を表示します．

&wikiexample{
= (1 + 2) * 3 ^ 2 / 4 =
= 1,000[ml] * 20[本] / 1000[L換算] = [L]
}

*変数
以下のように記述することで変数が使用できます．変数は代入後，同一ページ内で有効です．
&wikiexample{
= foo = 2 + 5
= foo * 20 =
}

代入の演算は代入した値を演算結果とするので上記の最初の式は7と表示されます．
式の値が重要でない計算(副作用が重要な計算)をする場合は&=に続けて&:を記述すると結果が表示されなくなります．
&wikiexample{
=: foo = 10
= foo * 20 =
}

*パラメータ
以下のように書くことで数値計算の挙動をカスタマイズすることもできます．
 =(パラメータ) 式
 &calc(パラメータ){ 式 }

パラメータには以下の指定ができます．複数指定する場合はカンマで区切ります．
:round=桁数|指定された桁数で小数点以下の四捨五入をします．デフォルトは2です．
:{comma|結果をコンマ区切りの数値で表示します．
式中にコンマ区切りの数値が使用されていた場合は自動で指定されます．}
:!comma|結果をコンマ区切りの数値で表示しないようにします．
:replace|式を結果で置き換えます．デフォルトで指定されています．
:!replace|式を結果で置き換えずに式と結果を表示するようにします．
:result|結果を表示します．デフォルトで指定されています．
:{!result|結果を表示しません．変数への代入等の副作用のみが必要な場合に指定します．
行頭に&=&:とした場合と同等の効果が得られます．}

*表計算

表のセル内で最初に&=を書き，続いて式を書くと
表計算のように使用することができます．
表の中でも通常の演算子や関数が使えますが，さらに以下のような変数，関数が使用できます．

:A1, B2のようなセルの番号|大文字のみ有効，セルの番号はセルの上でマウスカーソルを止めておくと表示されます
:A1:C3のようなセル番号での範囲|大文字のみ有効
:up|現在のセルから見て上方向のセルすべて
:up(n & m)|上方向にn個のセルをとばしてm個のセル(mが省略された場合はmは無限扱い)
upと同様にdown,left,rightも同様に使用できます．

&wikiexample{
|名前    |国語        |数学        |英語        |合計           |hc
|鈴木一郎|90          |70          |80          |=sum(left(0&3))|lr
|佐藤二郎|50          |75          |100         |=sum(left(0&3))|
|田中三郎|60          |100         |30          |=sum(left(0&3))|
|平均    |=average(up)|=average(up)|=average(up)|=average(up)   |f
|最高得点|=max(up(1)) |=max(up(1)) |=max(up(1)) |=max(up(1))    |f
}

*/

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[数値計算(文中):../../Function/calc]]|&&calc{式} or &&calc(round=桁数, comma){式}|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|calculation (in sentence)|&&calc{expression} or &&calc(round=number of decimals, comma){expression}|
*/
/**
@manual $manualpage$/ja/Wiki/Function/calc

&title &&calc

*概要
 &calc(round=桁数, comma, replace, result){ 式 [=] }

 =(round=桁数, comma, replace, result) 式 [=]

式の計算を行い，その結果を表示します．詳細は[[数値計算>../../Calc]]のページを参照して下さい．

*パラメータ
:round=桁数|指定された桁数で小数点以下の四捨五入をします．デフォルトは2です．
:{comma|結果をコンマ区切りの数値で表示します．
式中にコンマ区切りの数値が使用されていた場合は自動で指定されます．}
:!comma|結果をコンマ区切りの数値で表示しないようにします．
:replace|式を結果で置き換えます．デフォルトで指定されています．
:!replace|式を結果で置き換えずに式と結果を表示するようにします．
:result|結果を表示します．デフォルトで指定されています．
:{!result|結果を表示しません．変数への代入等の副作用のみが必要な場合に指定します．
行頭に&=&:とした場合と同等の効果が得られます．}

*対象
対象には計算したい式を書きます．

*例

&wikiexample{
= 30 / 4 + 1
= 1,024 * 2 ^ 20 =
=: x = 10 & y = 2
= x + y * x =

1ギガバイトは&calc(comma){10^(3*3)}バイトのことです．
}
*/

?>
