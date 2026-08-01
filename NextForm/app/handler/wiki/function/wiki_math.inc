<?php
$WIKI_OPERATORS_FUNCTION['math'] = true;
function wiki_operator_math(&$context, $dom, $tree) {
    $tex_string = wiki_get_target_string($tree);

    if(!empty($context['texts_only'])) {
	dom_append_text($dom, $tex_string);
	return;
    }

    $dpi = 120;
    $display = 'display';
    wiki_parse_parameters(
	$context, $tree,
	array(
	    'number' => &$dpi,
	    'equalities' => array(
		'dpi' => &$dpi,
		),
	    'long_flags' => array(
		'inline' => array(&$display, 'inline'),
		),
	    )
	);
    $dpi = (int)$dpi;
    if($dpi <= 0)
	$dpi = 120;

    $uri = url_make('',
		    array(
			'option' => 'wiki_math',
			'math' => $tex_string,
			'display' => $display,
			'dpi' => $dpi,
			));
    $img = dom_append_child($dom, 'img', 'math',
			    array(
				'src' => $uri,
				'alt' => $tex_string,
				));
    dom_append_class($img, $display);
    dom_set_position($img, $tree);
}

$OPTIONS['wiki_math'] = array(
    'show' => 'wiki_option_math_show',
    );

function wiki_option_math_show(&$args, $dom) {
    if(!defined('WIKI_MATH_ENABLE') || WIKI_MATH_ENABLE !== 'true')
	return false;

    if(empty($args['math'])) {
	message_error(l('Invalid parameter.'));
	return false;
    }
    $tex_string = $args['math'];
    $dpi = max(2, min(default_value($args['dpi'], 72), 300));
    if(!empty($args['display']) && $args['display'] == 'inline')
	$display = 'inline';
    else
	$display = 'display';
    $is_debug = !empty($args['math_debug']);

    $cachekey = 'math' . $tex_string . '-dpi' . $dpi . '-' . $display;
    if(($contents = cache_get('', $cachekey)) === false || $is_debug) {
	$descriptorspec = array(
	    0 => array("pipe", "r"),
	    1 => array("pipe", "w"),
	    2 => array("pipe", "w"),
	    );
	$command = template_replace_variables(WIKI_MATH_COMMAND,
					      array(
						  'DPI' => escapeshellarg($dpi)
						  ));

	$envs = array();
	if(defined('WIKI_MATH_COMMAND_ENVS')) {
	    $es = explode(';', WIKI_MATH_COMMAND_ENVS);
	    foreach($es as $e) {
		list($n, $v) = explode('=', $e);
		if(!empty($n))
		    $envs[$n] = $v;
	    }
	}

	$process = proc_open($command, $descriptorspec, $pipes, NULL, $envs);
	if(!is_resource($process))
	    return;
	$tex_source = 
	    '\documentclass{jarticle}' . "\n".
	    '\pagestyle{empty}' . "\n" .
	    '\begin{document}' . "\n";
	if($display == 'inline')
	    $tex_source .= '$' . $tex_string . '$' . "\n";
	else
	    $tex_source .= '\[' . $tex_string . '\]' . "\n";
	$tex_source .= '\end{document}';

	if($is_debug) {
	    print("tex source ------- \n");
	    print($tex_source);
	    print("\n------------------ \n\n");
	}

        fwrite($pipes[0], $tex_source);
	fclose($pipes[0]);

	$contents = stream_get_contents($pipes[1]);
	$error = stream_get_contents($pipes[2]);
	fclose($pipes[1]);
	fclose($pipes[2]);
	proc_close($process);

	if($is_debug) {
	    print("stderr output ---- \n");
	    print($error);
	    print("\n------------------ \n");
	}

	if($contents !== false)
	    cache_create('', $cachekey, $contents);
	else
	    return;
    }

    if(!$is_debug) {
	header('Content-type: image/png');
	header('X-Content-Type-Options: nosniff');
	echo $contents;
	exit(0);
    } else {
	header('Content-type: text/plain');
	exit(0);
    }

    return false;
}

/**
@manual $manualpage$/ja/Wiki/CheatSheet/Function
|[[数式:../../Function/math]]|&&math(inline, dpi=画像のDPI){TeX表記の数式}|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/Function
|mathematical formula|&&math(inline, dpi=dpi of image){tex format}|
*/
/**
@manual $manualpage$/ja/Wiki/Function/math
&title &&math

*概要
 &math(inline, dpi=画像のDPI){ TeX表記の数式 }
数式を表示します．数式はTeXの数式として処理され，画像に変換されて表示されます．
このファンクションはサイトの管理者によって有効化されていないと使用できません．
有効化するには設定項目WIKI_MATH_ENABLEを'true'にします．有効化の際は注意が必要です．

*パラメータ
:{inline|数式の高さを文中に収まるようにします．文中に数式を書く場合に使用します．
&&mathの代わりに&&mathiと書くことでinlineを指定することもできます．
}
:dpi=画像のDPI|数式を画像に変換する際のDPIを設定します．数式の大きさを決定します．デフォルトは120です．

*対象
対象には数式を書きます．数式はTeXの表記方法です．

*例

&wikiexample{
&math{ F(x) = \int_a^{a + b} \frac{f(x)}{x} dx}

&math(dpi=144){ F(x) = \int_a^{a + b} \frac{f(x)}{x} dx}

&math(inline) {f_N(x) = \sum_{i=0}^N \sqrt\frac{x^2 + 4x + 10}{x + 1}}
}
*/
$WIKI_ALIASES['mathi'] = '&math(inline,<PARAMS>){<TARGET>}';


?>
