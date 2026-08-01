<?php
function diff($aa, $ab){
    $ca = count($aa);
    $cb = count($ab);

    $is_swap = false;
    if($cb > $ca) {
	$is_swap = true;
	$m = $cb;
	$n = $ca;
	$a = &$ab;
	$b = &$aa;
    } else {
	$m = $ca;
	$n = $cb;
	$a = &$aa;
	$b = &$ab;
    }

    $offset = $n;
    $delta = $m - $n;
    $size = $m + $n + 1;

    $fp = array();
    for($i = 0; $i < $size; $i++) {
	$fp[] = array(
	    'y' => null,
	    'tree' => null,
	    );
    }

    $p = -1;
    $k = 0;

    while($fp[$delta + $offset]['y'] < $n){
	$p = $p + 1;
	for($k = -$p; $k < $delta; ++$k){
	    diff_snake($k, $m, $n, $fp, $offset, $is_swap, $a, $b);
	}
	for($k = $delta + $p; $k > $delta; --$k){
	    diff_snake($k, $m, $n, $fp, $offset, $is_swap, $a, $b);
	}

	diff_snake($delta, $m, $n, $fp, $offset, $is_swap, $a, $b);
    }

    $current = &$fp[$delta + $offset];

    $current['d'] = $delta + 2 * $p;
    $list = array();
    $type = null;
    $a_index = $m - 1;
    $b_index = $n - 1;
    $a_length = 0;
    $b_length = 0;
    foreach($a as $t)
	$a_length += strlen($t);
    foreach($b as $t)
	$b_length += strlen($t);
    $a_position = $a_length + $m;
    $b_position = $b_length + $n;

    for($i = &$current['tree']; !empty($i); $i = &$i['prev']){
	$type = $i['type'];
	if($type == '+'){
	    $length = strlen($b[$b_index]);
	    $b_position -= $length + 1;
	    $result = array('change' => ($is_swap ?  'delete' : 'add'),
			    'source' => ($is_swap ?  'l' : 'r'),
			    'index' => $b_index,
			    'position' => $b_position,
			    'length' => $length,
			    'string' => $b[$b_index]);
	    $b_index--;
	} else if($type == '-'){
	    $length = strlen($a[$a_index]);
	    $a_position -= $length + 1;
	    $result = array('change' => ($is_swap ?  'add' : 'delete'),
			    'source' => ($is_swap ?  'r' : 'l'),
			    'index' => $a_index,
			    'position' => $a_position,
			    'length' => $length,
			    'string' => $a[$a_index]);
	    $a_index--;
	} else {
	    $length = strlen($a[$a_index]);
	    $a_position -= $length + 1;
	    $b_position -= $length + 1;
	    $result = array('change' => 'same',
			    'source' => 'r',
			    'index' => ($is_swap ?  $a_index : $b_index),
			    'position' => ($is_swap ?  $a_position : $b_position),
			    'length' => $length,
			    'string' => $a[$a_index]);
	    $a_index--;
	    $b_index--;
	}
	array_unshift($list, $result);
    }

    return $list;
}

function diff_snake($k, $m, $n, &$fp, $offset, $is_swap, &$a, &$b) {
    if(($k < -$n) || ($m < $k)){
	return;
    } else {
	$current = &$fp[$k + $offset];
	if($k == -$n) {
	    $down = &$fp[$k + 1 + $offset];
	    $current['y'] = $down['y'] + 1;
	    $current['tree'] = array(
		'type' => '+',
		'prev' => $down['tree']);
	} else if($k == $m) {
	    $slide = &$fp[$k - 1 + $offset];
	    $current['y'] = $slide['y'];
	    $current['tree'] = array(
		'type' => '-',
		'prev' => $slide['tree']);
	} else {
	    $slide = &$fp[$k - 1 + $offset];
	    $down  = &$fp[$k + 1 + $offset];
	    if($slide['y'] === null && $down['y'] === null) {
		$current['y'] = 0;
	    } else if($down['y'] === null || $slide['y'] === null) {
		if($down['y'] === null){
		    $current['y'] = $slide['y'];
		    $current['tree'] = array(
			'type' => '-',
			'prev' => $slide['tree']);
		} else {
		    $current['y'] = $down['y'] + 1;
		    $current['tree'] = array(
			'type' => '+',
			'prev' => $down['tree']);
		}
	    } else {
		if($slide['y'] > ($down['y'] + ($is_swap ? 0 : 1))){
		    $current['y'] = $slide['y'];
		    $current['tree'] = array(
			'type' => '-',
			'prev' => $slide['tree']);
		} else {
		    $current['y'] = $down['y'] + 1;
		    $current['tree'] = array(
			'type' => '+',
			'prev' => $down['tree']);
		}
	    }
	}
	$y = $current['y'];
	$x = $y + $k;
	while($x < $m && $y < $n && $a[$x] == $b[$y]){
	    $current['tree'] = array(
		'type' => '|',
		'prev' => $current['tree']);
	    $x++;
	    $y++;
	}
	$current['y'] = $y;
    }
}
/* bibliography: http://d.hatena.ne.jp/Constellation/20091021/1256112978 */
?>
