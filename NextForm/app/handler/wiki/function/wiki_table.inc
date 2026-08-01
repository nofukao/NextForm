<?php
$WIKI_OPERATORS_LINE_HEAD['table'] =
    array('symbol' => '\|', 'group' => true);
$WIKI_OPERATORS_LINE_HEAD['csv'] =
    array('symbol' => ',', 'group' => true);

function wiki_operator_table(&$context, $node, $block) {
    $table = dom_append_child_with_position($node, 'table', $block);
    $thead = dom_append_child($table, 'thead');
    $tbody = dom_append_child($table, 'tbody');
    $tfoot = dom_append_child($table, 'tfoot');

    $cells = array();

    $placeholders = array();
    
    $last_positions = array('');
    $last_fgcolors = array('');
    $last_bgcolors = array('');
    $last_formats = array('');
    $last_widths = array('');
    $last_heights = array('');
    foreach($block->children as &$tree) {
	$fields = wiki_tokens_split($tree->target, new WikiSymbolToken('|', 0, 0));
	$format_field = array_pop($fields);
	$custom = wiki_tokens_to_string($format_field);
	$len = strlen($custom);
	$positions = array();
	$bgcolors = array();
	$fgcolors = array();
	$formats = array();
	$widths = array();
	$heights = array();

	$target_element = &$tbody;
	$element = 'td';

	preg_match_all('_(?:[hfclrn#=%/,\.]|(?:w|e)(?:([\d.]+)(%|em|ex|px|pt)?)?)_', $custom, $all_matches, PREG_SET_ORDER);
	foreach($all_matches as $maches) {
	    switch($maches[0][0]) {
	    case 'h':
		$target_element = &$thead;
		$element = 'th';
		break;
	    case 'f':
		$target_element = &$tfoot;
		$element = 'th';
		break;
	    case 'c': $positions[] = 'center'; break;
	    case 'l': $positions[] = 'left'; break;
	    case 'r': $positions[] = 'right'; break;
	    case 'n': $positions[] = ''; break;
	    case '#': $bgcolors[] = 'background_emphasis'; break;
	    case '=': $bgcolors[] = ''; break;
	    case '%': $fgcolors[] = 'foreground_emphasis'; break;
	    case '/': $fgcolors[] = ''; break;
	    case ',': $formats[] = 'comma'; break;
	    case '.': $formats[] = ''; break;
	    case 'w': $widths[] = substr($maches[0], 1) . (isset($maches[1]) && !isset($maches[2]) ? 'px' : ''); break;
	    case 'e': $heights[] = substr($maches[0], 1) . (isset($maches[1]) && !isset($maches[2]) ? 'px' : ''); break;
	    }
	}

	$tr = dom_append_child_with_position($target_element, 'tr', $tree);

	if(empty($positions))
	    $positions = $last_positions;
	$positions_num = count($positions);
	if(empty($fgcolors))
	    $fgcolors = $last_fgcolors;
	$fgcolors_num = count($fgcolors);
	if(empty($bgcolors))
	    $bgcolors = $last_bgcolors;
	$bgcolors_num = count($bgcolors);
	if(empty($formats))
	    $formats = $last_formats;
	$formats_num = count($formats);
	if(empty($widths))
	    $widths = $last_widths;
	$widths_num = count($widths);
	if(empty($heights))
	    $heights = $last_heights;
	$heights_num = count($heights);

	$i = 0;
	$row = array();
	$y = count($cells);
	$last_offset = $tree->position;
	if(empty($fields))
	    dom_remove_empty($tr);
	foreach($fields as $field) {
	    $position = $positions[min($i, $positions_num - 1)];
	    $fgcolor = $fgcolors[min($i, $fgcolors_num - 1)];
	    $bgcolor = $bgcolors[min($i, $bgcolors_num - 1)];
	    $format = $formats[min($i, $formats_num - 1)];
	    $width = $widths[min($i, $widths_num - 1)];
	    $height = $heights[min($i, $heights_num - 1)];
	    $trim_tokens = wiki_tokens_trim_space($field);

	    $classes = array();
	    if(!empty($position))
		$classes[] = $position;
	    if(!empty($fgcolor))
		$classes[] = $fgcolor;
	    if(!empty($bgcolor))
		$classes[] = $bgcolor;
	    $class = implode(' ', $classes);

	    $styles = array();
	    if($width != '')
		$styles[] = 'width: ' . $width . ';';
	    if($height != '')
		$styles[] = 'height: ' . $height . ';';
	    $style = implode('', $styles);

	    if(!empty($field)) {
		$last_field = end($field);
		$last_offset = $last_field->position + $last_field->length;
	    } else {
		$last_offset++;
	    }
	    if(!empty($field[0]) && $field[0] instanceof WikiSymbolToken && $field[0]->t == '~') {
		$td = dom_create('th', $class);
		dom_set_position($td, $field);
		array_shift($field);
	    } else  {
		$td = dom_create('td', $class);
		if(empty($field)) {
		    $dummy_token = new WikiToken('', $last_offset, 0);
		    dom_set_position($td, $dummy_token);
		} else
		    dom_set_position($td, $field);
	    }
	    dom_append_text($td, '');
	    if($style !== '')
		dom_set_attribute($td, 'style', $style);

	    $trim_tokens_string = wiki_tokens_to_string($trim_tokens);
	    $cell = array(
		'state' => 'source',
		'dom' => $td,
		'tr' => $tr,
		'target' => $target_element,
		'x' => $i, 'y' => $y,
		'format' => $format,
		);
	    if($element === 'th' && count($trim_tokens) >= 1 &&
	       preg_match('|^@.*/.*@$|', $trim_tokens_string) &&
	       count($placeholder = template_get_placeholders($trim_tokens_string)) == 1) {
		$placeholder = $placeholder[0];
		dom_append_text($td, $placeholder['term']);
		if(!isset($placeholders[$i]))
		   $placeholders[$i] = $placeholder;
	    } else if(count($trim_tokens) == 1 && $trim_tokens[0] instanceof WikiSymbolToken && 
	       in_array($trim_tokens[0]->t, array('>', '<', '^', '_'))) {
		$cell['span'] = $trim_tokens[0]->t;
	    } else {
		$cell['tokens'] = $trim_tokens;
		$cell['trim_tokens_string'] = $trim_tokens_string;
		$cell['position'] = $position;
		dom_set_attribute($td, 'title', int_to_alpha($i).($y+1));
	    }
	    $row[] = $cell;

	    $i++;
	}
	$cells[] = $row;

	$last_positions = $positions;
	$last_fgcolors = $fgcolors;
	$last_bgcolors = $bgcolors;
	$last_formats = $formats;
	$last_widths = $widths;
	$last_heights = $heights;
    }
    
    foreach($cells as &$row) {
	foreach($row as &$cell) {
	    $context['page']['calc']['cells'] = &$cells;
	    wiki_operator_table_cell($context, $cell);
	}
    }
    foreach($cells as &$row) {
	$i = 0;
	foreach($row as &$cell) {
	    if(isset($cell['dom'])) {
		if(isset($placeholders[$i]) && $cell['target'] === $tbody && isset($cell['trim_tokens_string'])) {
		    dom_set_attribute($cell['dom'], 'data-option-value', $cell['trim_tokens_string']);
		    dom_set_attribute($cell['dom'], 'data-options', $placeholders[$i]['options_text']);
		}
	    }

	    wiki_operator_table_span($cells, $cell);
	    $i++;
	}
    }
    foreach($cells as &$row) {
	foreach($row as &$cell) {
	    if(isset($cell['dom']))
		dom_append_dom($cell['tr'], $cell['dom']);
	}
    }
    dom_remove_empty($thead);
    dom_remove_empty($tbody);
    dom_remove_empty($tfoot);
    dom_remove_empty($table);
}

function wiki_operator_table_cell(&$context, &$cell) {
    if($cell['state'] == 'source') {
	$cell['state'] = 'calc';

	if(isset($cell['span'])) {
	    $cell['value'] = 'IGNORE';
	} else {
	    $value = null;
	    $new_context = $context;
	    $new_context['cell'] = &$cell;
	    $new_context['calc_result'] = &$value;
	    if(!empty($cell['format'])) {
		$new_context['formats'][$cell['format']] = true;
	    }
	    if(!empty($cell['tokens'])) {
		wiki_convert_from_tokens($new_context, $cell['dom'], $cell['tokens']);
		if(!is_null($value)) {
		    $cell['value'] = $value;
		} else {
		    $text = trim(dom_text($cell['dom']));
		    if(preg_match('/^([\d\.,&\s]+).*$/', $text, $matches)) { 
			$cell['value'] = str_replace(',', '', $matches[1]);
			if(strpos($cell['value'], '&') !== false) {
			    $cell['value'] = explode('&', $cell['value']);
			    $cell['value'] = array_map('trim', $cell['value']);
			}
		    } else {
			$cell['value'] = 'IGNORE';
		    }
		}
	    } else {
		$cell['value'] = 'IGNORE';
	    }
	}

	$cell['state'] = 'done';
    } else if($cell['state'] == 'calc') {
	$cell['value'] = false;
	$cell['state'] = 'done';
    }
    return $cell['value'];
}

function wiki_operator_table_span(&$cells, &$cell) {
    if(!isset($cell['span']) && isset($cell['dom'])) {
	$x = $cell['x'];
	$y = $cell['y'];
	$target = $cell['target'];
	$u = $d = $l = $r = 1;
	while(isset($cells[$y - $u][$x]) && isset($cells[$y - $u][$x]['span']) &&
	      $cells[$y - $u][$x]['span'] == '_' && $cells[$y - $u][$x]['target'] === $target) $u++;
	while(isset($cells[$y + $d][$x]) && isset($cells[$y + $d][$x]['span']) &&
	      $cells[$y + $d][$x]['span'] == '^' && $cells[$y + $d][$x]['target'] === $target) $d++;
	while(isset($cells[$y][$x - $l]) && isset($cells[$y][$x - $l]['span']) &&
	      $cells[$y][$x - $l]['span'] == '>') $l++;
	while(isset($cells[$y][$x + $r]) && isset($cells[$y][$x + $r]['span']) &&
	      $cells[$y][$x + $r]['span'] == '<') $r++;
	$u--; $d--; $l--; $r--;
	$sx = $x - $l;
	$sy = $y - $u;
	$x_span = $l + $r + 1;
	$y_span = $u + $d + 1;
	if($u + $d + $l + $r > 0) {
	    if($sx != $x || $sy != $y) {
		$cells[$sy][$sx]['dom'] = $cells[$y][$x]['dom'];
		unset($cells[$y][$x]['dom']);
	    }
	    if($x_span > 1)
		dom_set_attribute($cells[$sy][$sx]['dom'], 'colspan', $x_span);
	    if($y_span > 1)
		dom_set_attribute($cells[$sy][$sx]['dom'], 'rowspan', $y_span);
	    for($j = $sy; $j < $sy + $y_span; $j++) {
		for($i = $sx; $i < $sx + $x_span; $i++) {
		    if($i != $sx || $j != $sy)
			unset($cells[$j][$i]['dom']);
		}
	    }

	}
    }
}

function wiki_operator_csv(&$context, $node, $block) {
    $table = dom_append_child_with_position($node, 'table', $block);
    $tbody = dom_append_child($table, 'tbody');

    foreach($block->children as &$tree) {
	$fields = str_getcsv(wiki_tokens_to_string($tree->target));
	//$tr = dom_append_child_with_position($tbody, 'tr', $tree);
	$tr = dom_append_child($tbody, 'tr');
	//$pos = wiki_get_position_data($tree->target);
	foreach($fields as $field) {
	    $td = dom_append_child($tr, 'td');
	    //$pos['position'] = $p;
	    //$pos['length'] = strlen($field);
	    //dom_set_position($td, $pos);
	    dom_append_text($td, $field);
	    //$pos['position'] += $pos['length'] + 1;
	}
    }
}
?>
