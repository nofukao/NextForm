<?php
/*
 * Wiki 記法の本文を Markdown にする (option/tomarkdown.inc と app/tool/wiki_to_markdown)。
 * 設計は docs/development/design-wiki-to-markdown.md の 3e。
 *
 * 表示 (wiki_convert.inc) と同じく wiki_lex() から木を作り、その木を辿って Markdown を
 * 書き出す。正規表現で原文を書き換えないのは、表示と同じ読み方をするため
 * (app/tool/wiki_syntax_survey と同じ考え方)。
 *
 * Markdown に無いものの扱い (利用者の判断 2026-10-18):
 *   装飾 (&color・&size など)   中の文字だけ残す
 *   動く機能 (&calendar など)   原文をコードの枠で残す (文中は `...`)
 * 落ちたもの・原文の枠で残したものは $result['lost'] / $result['kept'] に数える。
 */

/*
 * 記法の分類。wiki_syntax_survey もこの表を読む (2 つの表が食い違わないように)。
 *   a  そのまま写せる
 *   b  写せるが落ちるものがある
 *   c  Markdown に無い (原文をコードの枠で残す。装飾は中の文字だけ残す)
 * 別名は展開先の名前で引く。
 */
$WIKI_MARKDOWN_CLASSES = array(
    'root'          => array('a', '文書そのもの'),
    'paragraph'     => array('a', '段落'),
    'section'       => array('a', '節 -> 見出し (折りたたみは :::details)'),
    'list'          => array('a', '箇条書き'),
    'blockquote'    => array('a', '引用'),
    'hr'            => array('a', '区切り線'),
    'pre'           => array('a', '整形済みテキスト -> コードブロック'),
    'code'          => array('a', 'コード'),
    'link'          => array('a', 'リンク'),
    'bracelink'     => array('a', 'ページへのリンク'),
    'em'            => array('a', '強調'),
    'strong'        => array('a', '重要'),
    'del'           => array('a', '打ち消し'),
    'br'            => array('a', '改行'),
    'title'         => array('a', 'ページの題名 -> 先頭の #'),
    'entity'        => array('a', '文字'),
    'entity_number' => array('a', '文字 (番号指定)'),
    'include'       => array('a', '取り込み -> ![](..)'),
    'index'         => array('a', '目次 -> [TOC]'),

    'table'         => array('b', '表 (セル結合・幅・色・計算は落ちる)'),
    'csv'           => array('b', 'CSV 形式の表 (表に直す)'),
    'deflist'       => array('b', '関連リスト (箇条書きに直す)'),
    'plain'         => array('b', 'そのまま表示 (エスケープで代用)'),
    'paa'           => array('b', 'アスキーアート (コードブロックに直す)'),
    'through'       => array('b', '素通し'),
    'pages'         => array('b', 'ページの一覧 (ツリーの形だけ [PAGES] に直す)'),
    'align'         => array('b', '配置の指定 (中の文字だけ残す)'),
    'bgcolor'       => array('b', '背景色 (中の文字だけ残す)'),
    'color'         => array('b', '文字色 (中の文字だけ残す)'),
    'size'          => array('b', '文字の大きさ (中の文字だけ残す)'),
    'more'          => array('b', '抜粋の区切り (落とす)'),

    'alias'         => array('c', '記法の別名の定義'),
    'btime'         => array('c', '作成日時の埋め込み'),
    'calc'          => array('c', '数値計算'),
    'calclist'      => array('c', '数値計算 (行頭)'),
    'calendar'      => array('c', 'カレンダー'),
    'counter'       => array('c', 'カウンタ'),
    'datepages'     => array('c', '日付のページの一覧'),
    'export'        => array('c', '書き出し'),
    'feed'          => array('c', '新着の一覧'),
    'insert'        => array('c', '書き込みフォーム'),
    'inserttemplate'=> array('c', 'テンプレートからの書き込み'),
    'listadd'       => array('c', '箇条書きへの追記フォーム'),
    'math'          => array('c', '数式'),
    'meta'          => array('c', 'メタ情報の埋め込み'),
    'mtime'         => array('c', '更新日時の埋め込み'),
    'popularpages'  => array('c', 'よく見られているページ'),
    'redirect'      => array('c', '転送'),
    'search'        => array('c', '検索結果の埋め込み'),
    'select'        => array('c', '選択肢'),
    'taro'          => array('c', '飾り'),
    'time'          => array('c', '日時の埋め込み'),
    'toro'          => array('c', '飾り'),
    'toratora'      => array('c', '飾り'),
    'wikiexample'   => array('c', '記法の実演'),
    'writepage'     => array('c', 'ページの書き込み'),
    );

/* 装飾。中の文字だけ残す */
$WIKI_MARKDOWN_DECORATIONS = array('color' => true, 'bgcolor' => true, 'size' => true, 'align' => true);

/* 独自の絵文字の実体参照 (wiki_entity.inc の $WIKI_ENTITY_IMAGES) */
$WIKI_MARKDOWN_ENTITY_EMOJIS = array(
    'heart' => '♥', 'smile' => '🙂', 'bigsmile' => '😃', 'huh' => '😕',
    'oh' => '😮', 'wink' => '😉', 'sad' => '🙁', 'worried' => '😟',
    );

/*
 * 変換する。返すのは
 *   markdown  本文 (フロントマター・題名の # を含む。末尾の一文は含まない)
 *   lost      落ちたもの   (説明 => 数)
 *   kept      原文の枠で残したもの (記法 => 数)
 */
function wiki_markdown_convert(&$page, $source) {
    /* &include の指定の読み方は Markdown の表示と同じ (handler/markdown/image.inc) */
    markdown_converter();
    $state = array('source' => $source, 'title' => null, 'lost' => array(), 'kept' => array(),
		   'depth' => 0);
    $context = wiki_default_context($page);
    $body = array();
    if(trim($source) !== '') {
	$tree = wiki_markdown_parse($context, $source, true);
	$body = wiki_markdown_blocks($state, $context, $tree);
    }
    if(preg_match('/^\*+\(end\)/m', $source))
	wiki_markdown_lost($state, l('Ranges of sections given by *(end)'));
    /* コメントは表示に出ないが、本文からも消える (履歴にだけ残る)。:// の // は除く */
    if(preg_match('/(^|[^:\/])\/\/|\/\*/m', $source))
	wiki_markdown_lost($state, l('Comments (// and /* */)'));

    /* 題名: &title が無ければ、メタ情報の画面で付けた題名 (見えている題名を保つ) */
    $title = $state['title'];
    if(is_null($title) && !empty($page['meta']['title']))
	$title = $page['meta']['title'];

    $head = array();
    $tags = tag_get_page_tags($page);
    if(!empty($tags))
	$head[] = "---\n" . wiki_markdown_front_matter_tags($tags) . "\n---";
    if(!is_null($title) && trim($title) !== '')
	$head[] = '# ' . wiki_markdown_heading_text(wiki_markdown_escape(trim($title)));
    $blocks = array_merge($head, $body);
    $markdown = empty($blocks) ? '' : implode("\n\n", $blocks) . "\n";
    /* フロントマターは本文の先頭に --- が要る。題名の前の空行を詰める */
    return array('markdown' => $markdown, 'lost' => $state['lost'], 'kept' => $state['kept']);
}

/*
 * フロントマターの tags:。ふつうは [a, b] と 1 行で書く。読む側 (markdown_parse_tags())
 * は [..] の中をカンマで分けるだけなので、カンマや [ ] を含むタグがあれば、1 行に 1 つの
 * 並びの形 (  - a) で書く。
 */
function wiki_markdown_front_matter_tags($tags) {
    foreach($tags as $tag)
	if(preg_match('/[,\[\]]/u', $tag))
	    return "tags:\n" . implode("\n", array_map(function($tag) { return '  - ' . $tag; }, $tags));
    return 'tags: [' . implode(', ', $tags) . ']';
}

/* 原文 (または対象のトークン) を木にする。表示と同じく行頭のトークンを足してから */
function wiki_markdown_parse(&$context, $source, $is_root = false) {
    $tokens = &wiki_lex($context, $source);
    return wiki_markdown_parse_tokens($context, $tokens, $is_root);
}

/*
 * $is_inline は文中 (表のセル・箇条書きの項目・ファンクションの対象など)。表示
 * (wiki_convert_from_tokens()) と同じく、文中の中身が空白で始まるときは行頭の
 * トークンを足さない (`- 項目` や `| セル |` の空白を整形済みとして読まない)。
 * $append_line_head が false なら足さない (行頭の ~ の中身。wiki_operator_through())。
 */
function wiki_markdown_parse_tokens(&$context, $tokens, $is_root = false, $is_inline = false,
				    $append_line_head = true) {
    if(empty($tokens))
	return array();
    if($append_line_head && !($is_inline && ctype_space($tokens[0]->t)))
	array_unshift($tokens, new WikiLineHeadToken('', $tokens[0]->position, 0));
    $context['tokens'] = $tokens;
    $subtrees = &wiki_parse_tokens_to_subtrees($context, $tokens);
    $blocks = &wiki_subtrees_to_blocks($subtrees);
    if($is_root)
	return wiki_blocks_to_tree($blocks);
    return $blocks;
}

function wiki_markdown_lost(&$state, $label) {
    $state['lost'][$label] = isset($state['lost'][$label]) ? $state['lost'][$label] + 1 : 1;
}

function wiki_markdown_kept(&$state, $label) {
    $state['kept'][$label] = isset($state['kept'][$label]) ? $state['kept'][$label] + 1 : 1;
}

/* ---- ブロック ------------------------------------------------------- */

/* 木をブロック (Markdown の段落などの文字列) の並びにする */
function wiki_markdown_blocks(&$state, &$context, $tree) {
    if(is_array($tree)) {
	$blocks = array();
	$paragraph = array();
	foreach($tree as $child) {
	    /* 段落の外に出た文字やファンクションは、まとめて段落として扱う */
	    if($child instanceof WikiTextTree ||
	       ($child instanceof WikiApplyTree && !wiki_markdown_is_block_apply($child))) {
		$paragraph[] = $child;
		continue;
	    }
	    if(!empty($paragraph)) {
		$blocks = array_merge($blocks, wiki_markdown_paragraph($state, $context, $paragraph));
		$paragraph = array();
	    }
	    $blocks = array_merge($blocks, wiki_markdown_blocks($state, $context, $child));
	}
	if(!empty($paragraph))
	    $blocks = array_merge($blocks, wiki_markdown_paragraph($state, $context, $paragraph));
	return $blocks;
    }
    if($tree instanceof WikiApplyTree) {
	switch($tree->id) {
	case 'root':
	    return wiki_markdown_blocks($state, $context, $tree->children);
	case 'section':
	    return wiki_markdown_section($state, $context, $tree);
	case 'hr':
	    /* --- は本文の先頭でフロントマターの囲みとして読まれるので使わない */
	    return array('***');
	}
	return wiki_markdown_paragraph($state, $context, array($tree));
    }
    if($tree instanceof WikiDivisionTree)
	return wiki_markdown_blocks($state, $context, $tree->children);
    if($tree instanceof WikiBlockTree) {
	switch((string)$tree->group) {
	case '':
	    return wiki_markdown_paragraph($state, $context, $tree->children);
	case 'list':
	    return array(wiki_markdown_list($state, $context, $tree));
	case 'deflist':
	    return array(wiki_markdown_deflist($state, $context, $tree));
	case 'blockquote':
	    return array(wiki_markdown_blockquote($state, $context, $tree));
	case 'pre':
	    return array(wiki_markdown_pre($state, $tree));
	case 'table':
	    return array(wiki_markdown_table($state, $context, $tree));
	case 'csv':
	    return array(wiki_markdown_csv($state, $tree));
	case 'through':
	    return wiki_markdown_paragraph($state, $context, wiki_markdown_children_targets($tree));
	}
	/* 行頭の記法で、Markdown に無いもの (行頭の = の計算など) は原文の枠で */
	wiki_markdown_kept($state, (string)$tree->group);
	return array(wiki_markdown_fence(wiki_markdown_raw_block($state, $tree), 'wiki'));
    }
    if($tree instanceof WikiTextTree)
	return wiki_markdown_paragraph($state, $context, array($tree));
    return array();
}

/* 段落の中に置かず、ブロックとして扱う木 (節・区切り線) */
function wiki_markdown_is_block_apply($tree) {
    return in_array($tree->id, array('root', 'section', 'hr'), true);
}

/* 素通し (行頭の ~) の行は、記号を落とした中身を段落の文字にする */
function wiki_markdown_children_targets($block) {
    $items = array();
    foreach($block->children as $child) {
	if($child instanceof WikiApplyTree)
	    $items[] = new WikiTextTree($child->target);
	else
	    $items[] = $child;
    }
    return $items;
}

/* 節 → 見出し。深さ N は # を N+1 個 (6 個で止める)。*(optional) は :::details */
function wiki_markdown_section(&$state, &$context, $tree) {
    $depth = strlen((string)$tree->symbol);
    $level = min(6, $depth + 1);
    if($depth + 1 > 6)
	wiki_markdown_lost($state, l('Heading levels deeper than 6 (made level 6)'));
    $params = wiki_get_parameters_string($tree);
    $is_optional = in_array('optional', $params, true);
    $children = wiki_markdown_blocks($state, $context, $tree->children);

    if(!$is_optional) {
	$text = trim(str_replace("\n", ' ', wiki_markdown_inline_tokens($state, $context,
									  wiki_markdown_strip_line_tokens($tree->target))));
	$blocks = array();
	if($text !== '')
	    $blocks[] = str_repeat('#', $level) . ' ' . wiki_markdown_heading_text($text);
	return array_merge($blocks, $children);
    }
    /* 折りたたみの見出しは記法を読まない素の文字。表示と同じ文字にする (記号は落とす) */
    $text = wiki_markdown_rendered_text($context, wiki_markdown_strip_line_tokens($tree->target));

    /* 折りたたみ。入れ子は外側のコロンを長くする */
    $inner = implode("\n\n", $children);
    $colons = 3;
    if(preg_match_all('/^(:{3,})/m', $inner, $matches))
	$colons = max(array_map('strlen', $matches[1])) + 1;
    $fence = str_repeat(':', $colons);
    $label = $text === '' ? '' : ' ' . str_repeat('#', $level) . ' ' . $text;
    return array($fence . 'details' . $label . ($inner === '' ? '' : "\n" . $inner) . "\n" . $fence);
}

/* トークンの並びの文字 (wiki_tokens_to_string() は参照を取るので、値で渡せるように包む) */
function wiki_markdown_text($tokens) {
    return wiki_tokens_to_string($tokens);
}

/* 同じく、エスケープ (&&) を戻す前の原文の文字 */
function wiki_markdown_raw_text($tokens) {
    return wiki_tokens_to_string_without_escape($tokens);
}

/*
 * 見出しなどの対象の、先頭と末尾の行頭・行末の印を落とす。途中の改行 (-{…} のような
 * 複数行の中身) は残す。
 */
function wiki_markdown_strip_line_tokens($tokens) {
    $tokens = array_values((array)$tokens);
    while(!empty($tokens) && wiki_markdown_is_line_token($tokens[0]))
	array_shift($tokens);
    while(!empty($tokens) && wiki_markdown_is_line_token(end($tokens)))
	array_pop($tokens);
    return $tokens;
}

function wiki_markdown_is_line_token($token) {
    return $token instanceof WikiLineHeadToken || $token instanceof WikiLineTailToken;
}

function wiki_markdown_has_newline($tokens) {
    foreach($tokens as $token)
	if($token instanceof WikiLineTailToken)
	    return true;
    return false;
}

/* 表示と同じ読み方で、対象の文字だけを取る (折りたたみの見出し・キャプション。記号は落ちる) */
function wiki_markdown_rendered_text(&$context, $tokens) {
    if(empty($tokens))
	return '';
    $dom = dom_create_document();
    $sub_context = wiki_modify_context($context, array('display' => 'inline'), 1);
    wiki_convert_from_tokens($sub_context, $dom, $tokens, array('display' => 'inline', 'append_line_head' => false), 0);
    return trim(preg_replace('/\s+/u', ' ', dom_text($dom)));
}

/*
 * 項目などの中身。途中に改行があれば (-{…} の複数行)、中身をブロックとして変換する
 * (入れ子の箇条書きや段落を保つ)。無ければ文中として。どちらも行ごとにエスケープ済み。
 */
function wiki_markdown_item_blocks(&$state, &$context, $tokens) {
    $tokens = wiki_markdown_strip_line_tokens($tokens);
    if(!wiki_markdown_has_newline($tokens)) {
	$text = trim(wiki_markdown_inline_tokens($state, $context, $tokens));
	return $text === '' ? array() : array(wiki_markdown_escape_lines($text));
    }
    return wiki_markdown_blocks($state, $context, wiki_markdown_parse_tokens($context, $tokens));
}

/* ブロックの並びを、1 行目は $first の後ろに、2 行目からは $indent 桁下げてつなぐ */
function wiki_markdown_indent_blocks($first, $blocks, $indent) {
    $lines = explode("\n", implode("\n\n", $blocks));
    $output = $first . array_shift($lines);
    foreach($lines as $line)
	$output .= "\n" . ($line === '' ? '' : str_repeat(' ', $indent) . $line);
    return $output;
}

/*
 * 見出しの文字。末尾の # は Markdown では閉じの記号として落ちるので、
 * 文字として残すときはエスケープする (Issue # など)。
 */
function wiki_markdown_heading_text($text) {
    return preg_replace('/(^|\s)(#+)(\s*)$/', '$1\\\\$2$3', $text);
}

/*
 * 段落。行ごとに見て、ブロックになるファンクション (&include・&index・&pages・
 * 原文の枠で残すもの) だけの行は段落から出す。行末の ~ は改行 (\ + 改行)。
 */
function wiki_markdown_paragraph(&$state, &$context, $children) {
    /* 文字の木はトークンにほどき、ファンクションはそのまま並べる */
    $items = array();
    foreach($children as $child) {
	if($child instanceof WikiTextTree) {
	    foreach($child->target as $token)
		$items[] = $token;
	} else if($child instanceof WikiBlockTree || $child instanceof WikiDivisionTree) {
	    foreach(wiki_markdown_flatten($child) as $item)
		$items[] = $item;
	} else {
	    $items[] = $child;
	}
    }

    $lines = array();
    $line = array();
    foreach($items as $item) {
	if($item instanceof WikiLineHeadToken)
	    continue;
	if($item instanceof WikiLineTailToken) {
	    $lines[] = $line;
	    $line = array();
	    continue;
	}
	$line[] = $item;
    }
    if(!empty($line))
	$lines[] = $line;

    $blocks = array();
    $paragraph = array();
    foreach($lines as $line) {
	$line = wiki_markdown_trim_items($line);
	if(count($line) == 1 && $line[0] instanceof WikiApplyTree) {
	    $block = wiki_markdown_function_block($state, $context, $line[0]);
	    if(!is_null($block)) {
		if(!empty($paragraph))
		    $blocks[] = wiki_markdown_join_lines($paragraph);
		$paragraph = array();
		if($block !== '')
		    $blocks[] = $block;
		continue;
	    }
	}
	if(empty($line))
	    continue;
	/* 行末の ~ は改行 */
	$is_break = false;
	$last = end($line);
	if($last instanceof WikiSymbolToken && $last->t === '~') {
	    array_pop($line);
	    $is_break = true;
	}
	$text = trim(wiki_markdown_inline_items($state, $context, $line));
	if($text === '' && !$is_break)
	    continue;
	$paragraph[] = array('text' => $text, 'break' => $is_break);
    }
    if(!empty($paragraph))
	$blocks[] = wiki_markdown_join_lines($paragraph);
    return $blocks;
}

function wiki_markdown_flatten($tree) {
    $items = array();
    if($tree instanceof WikiTextTree) {
	foreach($tree->target as $token)
	    $items[] = $token;
    } else if($tree instanceof WikiBlockTree || $tree instanceof WikiDivisionTree) {
	foreach($tree->children as $child)
	    $items = array_merge($items, wiki_markdown_flatten($child));
    } else {
	$items[] = $tree;
    }
    return $items;
}

/* 前後の空白のトークンを落とす */
function wiki_markdown_trim_items($items) {
    while(!empty($items) && wiki_markdown_is_space_item($items[0]))
	array_shift($items);
    while(!empty($items) && wiki_markdown_is_space_item(end($items)))
	array_pop($items);
    return $items;
}

function wiki_markdown_is_space_item($item) {
    return $item instanceof WikiToken && !($item instanceof WikiURIToken) && trim($item->t) === '';
}

/* 段落の行をつなぐ。Markdown の行頭で別の記法に読まれる文字はエスケープする */
function wiki_markdown_join_lines($lines) {
    $output = '';
    $n = count($lines);
    foreach($lines as $i => $line) {
	$output .= wiki_markdown_escape_lines($line['text']);
	if($i < $n - 1)
	    $output .= $line['break'] ? "\\\n" : "\n";
    }
    return $output;
}

/* 箇条書き。- は -、+ は 1.。入れ子は親の印の幅だけ字下げする */
function wiki_markdown_list(&$state, &$context, $block) {
    $lines = array();
    $indents = array();	/* 深さ => その深さの中身の字下げ */
    $numbers = array();	/* 深さ => 番号 */
    $types = array();	/* 深さ => 番号付きか */
    $previous = 0;
    foreach($block->children as $tree) {
	$symbol = (string)$tree->symbol;
	$depth = strlen($symbol);
	$is_ordered = substr($symbol, -1) === '+';
	if(in_array('optional', wiki_get_parameters_string($tree), true))
	    wiki_markdown_lost($state, l('Folding of list items (-(optional))'));
	/* 飛んだ深さは、直前の深さの 1 段下にする */
	if($depth > $previous + 1)
	    $depth = $previous + 1;
	$indent = $depth > 1 && isset($indents[$depth - 1]) ? $indents[$depth - 1] : 0;
	foreach(array_keys($numbers) as $d)
	    if($d > $depth)
		unset($numbers[$d], $indents[$d]);
	/* 記号が変わったら別の箇条書き (Markdown も同じ)。番号は振り直す */
	if(!isset($numbers[$depth]) || $previous < $depth ||
	   (isset($types[$depth]) && $types[$depth] !== $is_ordered))
	    $numbers[$depth] = 0;
	$types[$depth] = $is_ordered;
	$numbers[$depth]++;
	$marker = $is_ordered ? $numbers[$depth] . '. ' : '- ';
	$indents[$depth] = $indent + strlen($marker);

	$blocks = wiki_markdown_item_blocks($state, $context, $tree->target);
	$lines[] = wiki_markdown_indent_blocks(str_repeat(' ', $indent) . $marker,
					       empty($blocks) ? array('') : $blocks, $indents[$depth]);
	$previous = $depth;
    }
    return implode("\n", $lines);
}

/* 関連リスト → - **語**: 説明 */
function wiki_markdown_deflist(&$state, &$context, $block) {
    wiki_markdown_lost($state, l('Definition lists (made bullet lists)'));
    $lines = array();
    foreach($block->children as $tree) {
	list($term, $desc) = wiki_tokens_separate($tree->target, new WikiSymbolToken('|', 0, 0));
	$term = trim(str_replace("\n", ' ', wiki_markdown_inline_tokens($state, $context, wiki_markdown_strip_line_tokens($term))));
	$desc = wiki_markdown_item_blocks($state, $context, $desc);
	$head = '- ' . ($term === '' ? '' : '**' . $term . '**' . (empty($desc) ? '' : ': '));
	$lines[] = wiki_markdown_indent_blocks($head, empty($desc) ? array('') : $desc, 2);
    }
    return implode("\n", $lines);
}

/* 引用。wiki 記法は引用の中の記法を読まないので、中身は文字としてエスケープする */
function wiki_markdown_blockquote(&$state, &$context, $block) {
    /*
     * wiki の引用は行をそのまま見せる (white-space: pre) ので、行の区切りは
     * Markdown の改行 (行末の \) で保つ。>{…} の複数行も同じ。
     */
    $lines = array();
    foreach($block->children as $tree) {
	$prefix = str_repeat('> ', max(1, strlen((string)$tree->symbol)));
	$text = wiki_markdown_raw_text(wiki_markdown_strip_line_tokens($tree->target));
	foreach(explode("\n", $text) as $line)
	    $lines[] = array('prefix' => $prefix,
			     'text' => wiki_markdown_escape_line(wiki_markdown_escape(trim($line))));
    }
    $output = array();
    $n = count($lines);
    foreach($lines as $i => $line) {
	$has_next = $i < $n - 1 && $lines[$i + 1]['text'] !== '';
	$output[] = rtrim($line['prefix'] . $line['text']) . ($line['text'] !== '' && $has_next ? '\\' : '');
    }
    return implode("\n", $output);
}

/* 整形済み (行頭の空白)。1 行目の (code, 言語) は枠の言語にする */
function wiki_markdown_pre(&$state, $block) {
    $lang = '';
    $lines = array();
    foreach($block->children as $tree) {
	$params = wiki_get_parameters_string($tree);
	foreach($params as $param) {
	    if(in_array($param, array('linenum', 'wrap', 'nowrap', 'paa'), true) || preg_match('/^linenum=/', $param))
		wiki_markdown_lost($state, l('Line numbers and wrapping of preformatted text'));
	    else if($param !== 'code')
		$lang = $param;
	}
	$text = wiki_markdown_raw_text(wiki_markdown_strip_line_tokens($tree->target));
	if(!empty($params) && trim($text) === '')
	    continue;
	$lines[] = $text;
    }
    return wiki_markdown_fence(implode("\n", $lines), $lang);
}

/* コードの枠。中身より長い ` の並びで囲む */
function wiki_markdown_fence($text, $lang = '') {
    $length = 3;
    if(preg_match_all('/`{3,}/', $text, $matches))
	$length = max(array_map('strlen', $matches[0])) + 1;
    $fence = str_repeat('`', $length);
    return $fence . $lang . "\n" . $text . "\n" . $fence;
}

/* 表。h の行は見出しの行。無ければ空の見出しの行を作る (GFM の表は見出しの行が要る) */
function wiki_markdown_table(&$state, &$context, $block) {
    $header = null;
    $rows = array();
    $aligns = array();
    foreach($block->children as $tree) {
	$fields = wiki_tokens_split($tree->target, new WikiSymbolToken('|', 0, 0));
	$format = wiki_markdown_text(wiki_markdown_strip_line_tokens(array_pop($fields)));
	$cells = array();
	/*
	 * Markdown の表は 1 行目が必ず見出しの行 (見出しの行の無い表は書けない)。
	 * wiki で見出しの行 (行の末尾の h か、セルが全部見出しのセル ~) でなければ、
	 * 1 行目の見え方が変わるので落ちるものに数える。2 行目からはふつうの行
	 */
	$is_header = is_null($header);
	if($is_header && strpos($format, 'h') === false && !wiki_markdown_table_all_header_cells($fields))
	    wiki_markdown_lost($state, l('Tables without a header row (made the first row the header)'));
	foreach($fields as $field) {
	    $field = wiki_markdown_strip_line_tokens($field);
	    if(!empty($field) && $field[0] instanceof WikiSymbolToken && $field[0]->t === '~') {
		array_shift($field);
		if(!$is_header)
		    wiki_markdown_lost($state, l('Header cells in the middle of tables (made ordinary cells)'));
	    }
	    $raw = trim(wiki_tokens_to_string($field));
	    if(in_array($raw, array('>', '<', '^', '_'), true)) {
		wiki_markdown_lost($state, l('Cell merges, colors, widths and calculations of tables'));
		$cells[] = '';
		continue;
	    }
	    /* 改行は空白を整える前に見る (セルの末尾の &br; の改行が削られて \ だけ残らないように) */
	    $text = wiki_markdown_inline_tokens($state, $context, $field);
	    if(strpos($text, "\\\n") !== false)
		wiki_markdown_lost($state, l('Line breaks in table cells'));
	    /* | はエスケープ済み。URL などエスケープしていない所の | だけ直す */
	    $text = trim(str_replace(array("\\\n", "\n"), ' ', $text));
	    $cells[] = preg_replace('/(?<!\\\\)\|/', '\\\\|', $text);
	}
	if(preg_match('/[#=%\/,\.we]/', $format))
	    wiki_markdown_lost($state, l('Cell merges, colors, widths and calculations of tables'));
	if(strpos($format, 'f') !== false)
	    wiki_markdown_lost($state, l('Footer rows of tables (made ordinary rows)'));
	if(empty($aligns) && preg_match_all('/[clrn]/', $format, $matches))
	    $aligns = $matches[0];
	if($is_header)
	    $header = $cells;
	else
	    $rows[] = $cells;
    }
    return wiki_markdown_table_lines(is_null($header) ? array() : $header, $rows, $aligns);
}

/* 行のセルが全部、見出しのセル (先頭が ~) か */
function wiki_markdown_table_all_header_cells($fields) {
    if(empty($fields))
	return false;
    foreach($fields as $field) {
	$field = wiki_markdown_strip_line_tokens($field);
	if(empty($field) || !($field[0] instanceof WikiSymbolToken) || $field[0]->t !== '~')
	    return false;
    }
    return true;
}

/*
 * 表の行を並べる。wiki 記法の保存 (wiki_normalize()) と同じく原文の桁をそろえる
 * (幅は common_strwidth()。全角は 2)。区切り行は - を伸ばしてそろえる。
 * $aligns は列ごとの c l r n (足りない列は最後のものを使う)。
 */
function wiki_markdown_table_lines($header, $rows, $aligns) {
    $columns = count($header);
    foreach($rows as $row)
	$columns = max($columns, count($row));
    $widths = array_fill(0, max(1, $columns), 3);
    foreach(array_merge(array($header), $rows) as $cells)
	foreach($cells as $i => $cell)
	    $widths[$i] = max($widths[$i], common_strwidth($cell));

    $separators = array();
    foreach($widths as $i => $width) {
	$align = isset($aligns[$i]) ? $aligns[$i] : (empty($aligns) ? '' : end($aligns));
	$left = ($align === 'c' || $align === 'l') ? ':' : '';
	$right = ($align === 'c' || $align === 'r') ? ':' : '';
	$separators[] = $left . str_repeat('-', $width - strlen($left . $right)) . $right;
    }
    $lines = array(wiki_markdown_table_row($header, $widths),
		   '| ' . implode(' | ', $separators) . ' |');
    foreach($rows as $row)
	$lines[] = wiki_markdown_table_row($row, $widths);
    return implode("\n", $lines);
}

function wiki_markdown_table_row($cells, $widths) {
    $fields = array();
    foreach(array_pad($cells, count($widths), '') as $i => $cell)
	$fields[] = $cell . str_repeat(' ', $widths[$i] - common_strwidth($cell));
    return '| ' . implode(' | ', $fields) . ' |';
}

/*
 * CSV 形式の表 (行頭の ,)。wiki 記法では見出しの行が無いが、Markdown の表は 1 行目が
 * 必ず見出しの行になる (wiki_markdown_table() と同じく落ちるものに数える)
 */
function wiki_markdown_csv(&$state, $block) {
    $rows = array();
    foreach($block->children as $tree) {
	$text = wiki_markdown_raw_text(wiki_markdown_strip_line_tokens($tree->target));
	$cells = array();
	foreach(str_getcsv($text, ',', '"', '\\') as $cell)
	    $cells[] = str_replace('|', '\\|', wiki_markdown_escape(trim((string)$cell)));
	$rows[] = $cells;
    }
    wiki_markdown_lost($state, l('Tables without a header row (made the first row the header)'));
    $header = empty($rows) ? array() : array_shift($rows);
    return wiki_markdown_table_lines($header, $rows, array());
}

/* 木の原文 (コードの枠で残すもの)。位置が分からなければトークンから組み立てる */
function wiki_markdown_raw($state, $tree) {
    if(isset($tree->position) && $tree->position >= 0 && isset($tree->length) && $tree->length > 0)
	return rtrim(substr($state['source'], $tree->position, $tree->length), "\n");
    return '';
}

function wiki_markdown_raw_block($state, $block) {
    $lines = array();
    foreach($block->children as $tree) {
	$raw = wiki_markdown_raw($state, $tree);
	if($raw === '')
	    $raw = (string)$tree->symbol . wiki_markdown_raw_text(wiki_markdown_strip_line_tokens($tree->target));
	$lines[] = $raw;
    }
    return implode("\n", $lines);
}

/* ---- ファンクション ------------------------------------------------- */

/*
 * 別名を展開した木を返す (表示の wiki_alias_expand() と同じ置き換え)。
 * 別名でなければそのまま。展開した木の原文の位置は使えないので、原文は元の木から取る。
 */
function wiki_markdown_resolve_alias(&$context, $tree, $depth = 0) {
    global $wiki_operator_alias_expand_replace_args;
    if($depth > 4 || !($tree instanceof WikiApplyTree) || empty($context['aliases'][$tree->id]))
	return $tree;
    $alias = $context['aliases'][$tree->id];
    $use_equalities = array();
    if(preg_match_all('/<([A-Za-z_-]+)(=[^>]*)?>/', $alias, $matches))
	foreach($matches[1] as $name)
	    if($name !== 'PARAMS' && $name !== 'TARGET')
		$use_equalities[$name] = true;
    $values = array();
    $equalities = array();
    foreach(wiki_get_parameters_string($tree) as $param) {
	if(preg_match('/^([^=]+)\s*=\s*(.*)$/', $param, $matches) && !empty($use_equalities[$matches[1]]))
	    $equalities[$matches[1]] = $matches[2];
	else
	    $values[] = $param;
    }
    $wiki_operator_alias_expand_replace_args = array(
	'target' => wiki_tokens_to_string_without_escape($tree->target),
	'params' => $values, 'equalities' => $equalities);
    $body = preg_replace_callback('/<([A-Za-z_-]+(=[^>]+)?)>/', 'wiki_alias_expand_replace', $alias);

    $alias_context = $context;
    unset($alias_context['aliases'][$tree->id]);
    $blocks = wiki_markdown_parse($alias_context, $body);
    foreach(wiki_markdown_flatten_blocks($blocks) as $item)
	if($item instanceof WikiApplyTree)
	    return wiki_markdown_resolve_alias($alias_context, $item, $depth + 1);
    return $tree;
}

function wiki_markdown_flatten_blocks($blocks) {
    $items = array();
    foreach((array)$blocks as $block)
	$items = array_merge($items, wiki_markdown_flatten($block));
    return $items;
}

/*
 * 行にそれ 1 つだけのファンクションをブロックにする。ブロックにならなければ null
 * (文中と同じ扱い)。空文字はブロックを出さない (&title・&more など)。
 */
function wiki_markdown_function_block(&$state, &$context, $tree) {
    $resolved = wiki_markdown_resolve_alias($context, $tree);
    switch($resolved->id) {
    case 'title':
	wiki_markdown_title($state, $context, $resolved);
	return '';
    case 'more':
	wiki_markdown_lost($state, l('Excerpt marks (&more)'));
	return '';
    case 'include':
	$image = wiki_markdown_include($state, $context, $resolved);
	return is_null($image) ? wiki_markdown_kept_block($state, $tree, 'include') : $image;
    case 'index':
	if(!empty(wiki_get_parameters_string($resolved)))
	    return wiki_markdown_kept_block($state, $tree, 'index');
	return '[TOC]';
    case 'pages':
	$pages = wiki_markdown_pages($state, $context, $resolved, $tree);
	return is_null($pages) ? wiki_markdown_kept_block($state, $tree, $tree->id) : $pages;
    case 'pre': case 'code': case 'paa':
	return wiki_markdown_pre_function($state, $resolved);
    }
    if(wiki_markdown_is_kept($resolved->id))
	return wiki_markdown_kept_block($state, $tree, $tree->id);
    return null;
}

/* 原文をコードの枠で残すファンクションか (表に c とあるもの、表に無い記法) */
function wiki_markdown_is_kept($id) {
    global $WIKI_MARKDOWN_CLASSES, $WIKI_OPERATORS_FUNCTION;
    if(isset($WIKI_MARKDOWN_CLASSES[$id]))
	return $WIKI_MARKDOWN_CLASSES[$id][0] === 'c';
    return !empty($WIKI_OPERATORS_FUNCTION[$id]);
}

function wiki_markdown_kept_block(&$state, $tree, $id) {
    wiki_markdown_kept($state, '&' . $id);
    return wiki_markdown_fence(wiki_markdown_raw($state, $tree), 'wiki');
}

/* &title{題名}。本文の先頭の # にする (wiki_markdown_convert())。(show) は落ちる */
function wiki_markdown_title(&$state, &$context, $tree) {
    if(in_array('show', wiki_get_parameters_string($tree), true))
	wiki_markdown_lost($state, l('Showing the title in the text (&title(show))'));
    $state['title'] = trim(wiki_markdown_text(wiki_markdown_strip_line_tokens($tree->target)));
}

/*
 * &include(対象, 指定){キャプション} → ![|指定](パス "キャプション")。
 * 対象が無い・読めない形なら null (原文の枠で残す)。$extra_options は指定の後ろに足す語
 * (リンクの中に置く縮小画像の !link。wiki_markdown_link_image())。
 */
function wiki_markdown_include(&$state, &$context, $tree, $extra_options = array()) {
    $path = null;
    $options = array();
    global $WIKI_OPERATOR_INCLUDE_URI_HANDLERS;
    foreach((array)$tree->parameters as $tokens) {
	$param = trim(wiki_tokens_to_string($tokens));
	if(is_null($path)) {
	    if(count($tokens) == 1 && $tokens[0] instanceof WikiURIToken) {
		$path = $tokens[0]->t;
		/* 画像でない URL は、wiki でもリンクになる */
		$image = $WIKI_OPERATOR_INCLUDE_URI_HANDLERS['image']['regex'];
		if(!preg_match($image, $path))
		    return '<' . $path . '>';
	    } else if(preg_match('/^\[\[(.*)\]\]$/', $param, $matches))
		$path = wiki_markdown_page_path($matches[1]);
	    else
		return null;
	    continue;
	}
	$alt = '|' . $param;
	if($param !== '' && markdown_image_split_params($alt) !== array())
	    $options[] = $param;
	else if($param !== '')
	    wiki_markdown_lost($state, l('Some options of &include'));
    }
    if(is_null($path))
	return null;
    $options = array_merge($options, $extra_options);
    /* キャプションはタイトルの欄 (素の文字)。表示と同じ文字にし、\ と " をエスケープする */
    $caption = wiki_markdown_rendered_text($context, wiki_markdown_strip_line_tokens($tree->target));
    $destination = str_replace(array(' ', '(', ')', '<', '>'), array('%20', '%28', '%29', '%3C', '%3E'), $path);
    return '![' . (empty($options) ? '' : '|' . implode(' ', $options)) . '](' . $destination .
	($caption === '' ? '' : ' "' . str_replace(array('\\', '"'), array('\\\\', '\\"'), $caption) . '"') . ')';
}

/*
 * wiki のページの指定を Markdown の行き先にする。名前だけは wiki ではトップ階層、
 * Markdown では子なので / を付ける。./ と ../ はそのまま。空はこのページ。
 */
function wiki_markdown_page_path($path) {
    $path = trim($path);
    if($path === '')
	return './';
    if(strpos($path, './') === 0 || strpos($path, '../') === 0 || substr($path, 0, 1) === '/')
	return $path;
    return '/' . $path;
}

/*
 * &pages (と &treepages などの別名) → [PAGES …]。書けない指定や、ツリー・箇条書き
 * でない形なら null (原文の枠で残す)。fields は落ちる (ツリーはページ名で出す)。
 */
function wiki_markdown_pages(&$state, &$context, $tree, $original) {
    if(!wiki_tokens_is_whitespace($tree->target))
	return null;
    $args = array();
    $show = 'list';
    foreach(wiki_get_parameters_string($tree) as $param) {
	$param = trim($param);
	if($param === '')
	    continue;
	if(preg_match('/^\[\[(.*)\]\]$/', $param, $matches)) {
	    $path = trim($matches[1]);
	    array_unshift($args, $path === '' ? '/' : str_replace(' ', '%20', wiki_markdown_page_path($path)));
	} else if(preg_match('/^show=(.*)$/', $param, $matches))
	    $show = trim($matches[1]);
	else if(preg_match('/^sort=(mtime|btime|size)$/', $param) ||
		in_array($param, array('reverse', 'withsystem', 'children'), true))
	    $args[] = $param;
	else if(preg_match('/^(?:limit=)?([1-9][0-9]*)$/', $param, $matches))
	    $args[] = $matches[1];
	else if(preg_match('/^fields=/', $param) || $param === 'showhidden')
	    wiki_markdown_lost($state, l('Fields of page lists (shown by page name)'));
	else
	    return null;
    }
    if($show !== 'list' && $show !== 'tree')
	return null;
    if($show === 'list' && $original->id !== 'treepages')
	wiki_markdown_lost($state, l('Flat page lists (made trees)'));
    return '[PAGES' . (empty($args) ? '' : ' ' . implode(' ', $args)) . ']';
}

/* &pre{...} / &code(言語){...} / &paa{...} → コードの枠 */
function wiki_markdown_pre_function(&$state, $tree) {
    $lang = '';
    foreach(wiki_get_parameters_string($tree) as $param) {
	if(in_array($param, array('linenum', 'wrap', 'nowrap'), true) || preg_match('/^linenum=/', $param))
	    wiki_markdown_lost($state, l('Line numbers and wrapping of preformatted text'));
	else if($param !== 'code')
	    $lang = $param;
    }
    $text = wiki_tokens_to_string_without_escape($tree->target);
    $text = preg_replace('/^\n|\n$/', '', $text);
    return wiki_markdown_fence($text, $lang);
}

/* ---- 文中 ----------------------------------------------------------- */

/* トークンの並び (ファンクションの対象など) を文中の Markdown にする */
function wiki_markdown_inline_tokens(&$state, &$context, $tokens, $append_line_head = true) {
    if(empty($tokens))
	return '';
    $blocks = wiki_markdown_parse_tokens($context, $tokens, false, true, $append_line_head);
    $items = array();
    foreach(wiki_markdown_flatten_blocks($blocks) as $item)
	if(!($item instanceof WikiLineHeadToken))
	    $items[] = $item;
    while(!empty($items) && end($items) instanceof WikiLineTailToken)
	array_pop($items);
    return wiki_markdown_inline_items($state, $context, $items);
}

function wiki_markdown_inline_items(&$state, &$context, $items) {
    $output = '';
    $n = count($items);
    for($i = 0; $i < $n; $i++) {
	$item = $items[$i];
	if($item instanceof WikiLineHeadToken)
	    continue;
	if($item instanceof WikiLineTailToken) {
	    $output .= "\n";
	    continue;
	}
	if($item instanceof WikiSymbolToken && $item->t === '~' && $i + 1 < $n &&
	   $items[$i + 1] instanceof WikiLineTailToken) {
	    $output .= "\\";
	    continue;
	}
	if($item instanceof WikiURIToken) {
	    /* <URL> で書く。素のまま書くと、末尾の _ などがリンクから外れ、file: なども効かない */
	    $output .= '<' . $item->t . '>';
	    continue;
	}
	if($item instanceof WikiToken) {
	    $output .= wiki_markdown_escape($item->t);
	    continue;
	}
	if($item instanceof WikiTextTree) {
	    $output .= wiki_markdown_inline_items($state, $context, $item->target);
	    continue;
	}
	if($item instanceof WikiApplyTree)
	    $output .= wiki_markdown_inline_apply($state, $context, $item);
    }
    return $output;
}

function wiki_markdown_inline_apply(&$state, &$context, $tree) {
    switch($tree->id) {
    case 'em':
	return wiki_markdown_wrap($state, wiki_markdown_inline_tokens($state, $context, $tree->target), '*');
    case 'strong':
	return wiki_markdown_wrap($state, wiki_markdown_inline_tokens($state, $context, $tree->target), '**');
    case 'del':
	return wiki_markdown_wrap($state, wiki_markdown_inline_tokens($state, $context, $tree->target), '~~');
    case 'bracelink':
	return wiki_markdown_bracelink($state, $context, $tree);
    case 'entity': case 'entity_number':
	/* 文字実体参照は記号の側に名前が入っている (&amp; なら amp) */
	return wiki_markdown_function_inline($state, $context, $tree);
    case 'plain':
	/* ""そのまま"" は、中を記法として読まない文字 */
	return wiki_markdown_escape(wiki_tokens_to_string_without_escape($tree->target));
    case 'through':
	/* 文中の行頭の ~ (-~項目 など)。表示と同じく ~ だけを消し、中の記法は効かせる */
	return wiki_markdown_inline_tokens($state, $context, $tree->target, false);
    }
    if(!isset($tree->symbol) || $tree->symbol !== '&') {
	/* 表のセルの計算 (=1+2) など、Markdown に無い働きは原文を文中のコードで残す */
	if(wiki_markdown_is_kept($tree->id)) {
	    global $WIKI_MARKDOWN_CLASSES;
	    $label = isset($WIKI_MARKDOWN_CLASSES[$tree->id]) ? $WIKI_MARKDOWN_CLASSES[$tree->id][1] : $tree->id;
	    wiki_markdown_kept($state, (string)$tree->symbol . ' (' . $label . ')');
	    $raw = trim(wiki_markdown_raw($state, $tree));
	    return wiki_markdown_code_span($raw !== '' ? $raw :
					   (string)$tree->symbol . wiki_tokens_to_string_without_escape($tree->target));
	}
	/* ファンクションでない記法 (表示では原文のまま出るもの) は文字として */
	wiki_markdown_lost($state, l('Notation not in Markdown (shown as text)'));
	return wiki_markdown_escape(wiki_tokens_to_string_without_escape($tree->target));
    }
    return wiki_markdown_function_inline($state, $context, $tree);
}

/* 強調などで囲む。中身が空なら何も出さない */
function wiki_markdown_wrap(&$state, $inner, $mark) {
    if(trim($inner) === '')
	return $inner;
    /*
     * 中身の端が約物 (「」など) だと、CommonMark の規則で強調にならないことがある。
     * 書き換えはせず、落ちるものとして数える。
     */
    if(preg_match('/^\p{P}|\p{P}$/u', $inner))
	wiki_markdown_lost($state, l('Emphasis next to punctuation (may not be emphasized)'));
    return $mark . $inner . $mark;
}

/* 文中のファンクション */
function wiki_markdown_function_inline(&$state, &$context, $tree) {
    global $WIKI_MARKDOWN_DECORATIONS, $WIKI_MARKDOWN_ENTITY_EMOJIS;
    $resolved = wiki_markdown_resolve_alias($context, $tree);
    $id = $resolved->id;
    if(isset($WIKI_MARKDOWN_DECORATIONS[$id])) {
	wiki_markdown_lost($state, l('Colors, sizes and alignment of text (only the text remains)'));
	return wiki_markdown_inline_tokens($state, $context, $resolved->target);
    }
    switch($id) {
    case 'title':
	wiki_markdown_title($state, $context, $resolved);
	return '';
    case 'more':
	wiki_markdown_lost($state, l('Excerpt marks (&more)'));
	return '';
    case 'br':
	return "\\\n";
    case 'plain':
	return wiki_markdown_escape(wiki_tokens_to_string_without_escape($resolved->target));
    case 'entity':
	/* 名前は記号の側に入っている (&amp; なら amp)。Markdown も文字実体参照を読む */
	$name = (string)$resolved->symbol;
	return isset($WIKI_MARKDOWN_ENTITY_EMOJIS[$name]) ? $WIKI_MARKDOWN_ENTITY_EMOJIS[$name] : '&' . $name . ';';
    case 'entity_number':
	$number = ltrim((string)$resolved->symbol, '#');
	return '&#' . $number . ';';
    case 'link':
	return wiki_markdown_link_function($state, $context, $resolved, $tree);
    case 'include':
	$image = wiki_markdown_include($state, $context, $resolved);
	if(!is_null($image)) {
	    /* 文中の ![](ページ) は取り込まずにリンクになる (段落を割らない) */
	    if(preg_match('/^!\[[^\]]*\]\((?!https?:)[^)]*\)$/', $image) &&
	       !preg_match('/\.(jpe?g|png|gif|svg|webp|mp3|ogg|wav|mp4|webm)(\s|\)|$)/i', $image))
		wiki_markdown_lost($state, l('Pages pulled in in the middle of a sentence (made links)'));
	    return $image;
	}
	break;
    case 'pre': case 'code':
	return wiki_markdown_code_span(wiki_tokens_to_string_without_escape($resolved->target));
    }
    if(isset($WIKI_MARKDOWN_ENTITY_EMOJIS[$id]))
	return $WIKI_MARKDOWN_ENTITY_EMOJIS[$id];
    global $WIKI_OPERATORS_FUNCTION;
    if(empty($WIKI_OPERATORS_FUNCTION[$id]))
	return wiki_markdown_unknown_function($state, $context, $tree);
    /* 原文を文中のコードとして残す */
    wiki_markdown_kept($state, '&' . $tree->id);
    return wiki_markdown_code_span(wiki_markdown_raw($state, $tree));
}

/*
 * 登録されていないファンクション (&VoIP製品 や、名前に文が続いた &calcの方は… など)。
 * 表示は & から木の終わりまでを、先頭の & を文字にしてから読み直す (中の記法は効く。
 * wiki_convert_from_tree() の else)。同じく文字として写す。
 */
function wiki_markdown_unknown_function(&$state, &$context, $tree) {
    $raw = wiki_markdown_raw($state, $tree);
    if($raw === '')
	return wiki_markdown_escape('&' . $tree->id . wiki_tokens_to_string_without_escape($tree->target));
    $tokens = wiki_lex($context, $raw);
    /* 原文での位置に直す (中にコードの枠で残すファンクションがあれば、その原文を位置で取る) */
    foreach($tokens as $token)
	$token->position += $tree->position;
    $tokens[0] = new WikiTextToken($tokens[0]->t, $tokens[0]->position, $tokens[0]->length);
    return wiki_markdown_inline_tokens($state, $context, $tokens);
}

/* 文中のコード。中身の ` より長い ` で囲む */
function wiki_markdown_code_span($text) {
    $length = 1;
    if(preg_match_all('/`+/', $text, $matches))
	$length = max(array_map('strlen', $matches[0])) + 1;
    $fence = str_repeat('`', $length);
    $pad = (substr($text, 0, 1) === '`' || substr($text, -1) === '`') ? ' ' : '';
    return $fence . $pad . $text . $pad . $fence;
}

/*
 * &link(対象, 指定){表示名}。対象は URL か [[ページ]] の引数 (ほかは raw・open などの印や、
 * ページの画面の名前)。ページの画面 (edit など) へのリンクは Markdown に写さず原文で残す。
 * raw (ファイルそのもの。&direct はこれの別名) は wiki_markdown_direct_link()。
 */
function wiki_markdown_link_function(&$state, &$context, $tree, $original) {
    $url = null;
    $pagename = null;
    $is_raw = false;
    $actions = array();
    foreach((array)$tree->parameters as $tokens) {
	$param = trim(wiki_tokens_to_string($tokens));
	if(count($tokens) == 1 && $tokens[0] instanceof WikiURIToken)
	    $url = $tokens[0]->t;
	else if(preg_match('/^\[\[(.*)\]\]$/', $param, $matches))
	    $pagename = trim($matches[1]);
	else if($param === 'open')
	    wiki_markdown_lost($state, l('Opening in a new tab (>>)'));
	else if($param === 'raw')
	    $is_raw = true;
	else if($param !== '' && !preg_match('/^charset=/', $param))
	    $actions[] = $param;
    }
    if(!is_null($url)) {
	$destination = str_replace(array(' ', ')'), array('%20', '%29'), $url);
	/* 表示名が縮小画像だけのときだけ変える (外部の画像と文字を並べたものは今までどおり写せる) */
	$image = wiki_markdown_link_image($state, $context, $tree->target, $destination);
	if(is_string($image))
	    return $image;
	$name = trim(wiki_markdown_inline_tokens($state, $context, $tree->target));
	if($name === '')
	    return '<' . $url . '>';
	return '[' . $name . '](' . $destination . ')';
    }
    if(!is_null($pagename) && empty($actions)) {
	$image = wiki_markdown_link_image($state, $context, $tree->target,
					  wiki_markdown_link_destination($pagename), $pagename, $is_raw);
	if(is_string($image))
	    return $image;
	if($image !== null) {
	    $name = wiki_markdown_plain_name($tree->target);
	    if($is_raw)
		return wiki_markdown_direct_link($state, $context, $pagename, $name);
	    return wiki_markdown_page_link($pagename, $name);
	}
    }
    wiki_markdown_kept($state, '&' . $original->id);
    return wiki_markdown_code_span(wiki_markdown_raw($state, $original));
}

/*
 * wiki の [[行き先]] が指す、いまの人が読めるページ。無い・読めない・見出しを付けた
 * 行き先なら null。
 */
function wiki_markdown_readable_page(&$context, $locate) {
    if(strpos($locate, '#') !== false)
	return null;
    $pagename = page_path_to_pagename($locate, $context['page']['name'], false);
    if($pagename === '' || !page_is_exists($pagename) || !auth_has_permission($pagename, 'read'))
	return null;
    return page_read($pagename);
}

/* 種別 file のページの、埋め込みの関数の名前 (画像なら file_image_embed)。file でなければ '' */
function wiki_markdown_file_embed($page) {
    if(is_null($page) || page_get_type($page) !== 'file')
	return '';
    $content_type_handler = &file_get_content_type_handler($page);
    return isset($content_type_handler['embed']) ? $content_type_handler['embed'] : '';
}

/*
 * &direct([[行き先]]){説明} (&link の raw)。ファイルそのものを開く文字のリンク。
 *
 * Markdown でファイルそのものへの文字のリンクになるのは ![説明](行き先) だけで、そうなるのは
 * 埋め込みが文字のリンクになる種類 (PDF・テキスト・種類の分からないもの・添付 HTML・md。
 * embed_label の印) に限られる (handler/markdown/image.inc)。md は描画した画面への
 * リンクになる (ダウンロードではなくなる。利用者の判断 2026-10-19)。
 *
 * 画像・音声・動画は ![..](..) だと本文に画像や再生の枠が出るので、[説明](行き先) で
 * ファイルのページへの文字のリンクにする (見た目を変えない)。ふつうのページ・まだ無い
 * ページ・読めないページも同じ (ふつうのページを ![..](..) にすると取り込みになる)。
 * どちらも落ちるものとして数える。
 */
function wiki_markdown_direct_link(&$state, &$context, $locate, $name) {
    $locate = trim($locate);
    $name = trim($name);
    $page = wiki_markdown_readable_page($context, $locate);
    if(!is_null($page) && page_get_type($page) === 'file') {
	$content_type_handler = &file_get_content_type_handler($page);
	if(!empty($content_type_handler['embed_label'])) {
	    /* 説明の末尾の |… は大きさなどの指定として読まれるので、| は全角にする */
	    return '![' . wiki_markdown_escape(str_replace('|', '｜', $name)) . '](' .
		wiki_markdown_link_destination($locate) . ')';
	}
    }
    wiki_markdown_lost($state, l('Direct links to images, audio, video and pages (&direct) (made links to the page)'));
    /* 説明が無ければ、![](x.pdf) と同じくファイル名 (最後の部分) を出す */
    if($name === '')
	$name = page_get_simplename(page_path_to_pagename($locate, $context['page']['name']));
    return '[' . wiki_markdown_escape($name) . '](' . wiki_markdown_link_destination($locate) . ')';
}

/*
 * リンクの表示名が &include の画像 1 つだけのとき (縮小画像を押すと元の画像が開く書き方)、
 * その画像をリンクにした Markdown。表示名に &include が無ければ false (ふつうの表示名)。
 * 画像でない・ほかの文字と並んでいるなど、Markdown で書けなければ null (原文の枠で残す)。
 *
 *   &direct([[./a.png]]){&include([[./a.png]],100x100);}
 *       → ![|100x100](./a.png)
 *         同じ画像なら、添付の画像の埋め込みそのもの (押すと画像そのもの) と同じになる
 *   &link([[行き先]]){&include([[./小.png]]);}  (行き先は URL でもよい)
 *       → [![|!link](./小.png)](行き先)
 *         !link で画像の側のリンクを外す (リンクの中にリンクを入れない)。&direct で
 *         別の画像なら、行き先のファイルのページへのリンクになる (落ちるものに数える)
 *
 * $destination は Markdown の行き先。$locate は wiki の [[行き先]] (URL なら null)。
 */
function wiki_markdown_link_image(&$state, &$context, $tokens, $destination, $locate = null,
				  $is_raw = false) {
    $items = array();
    $has_include = false;
    $blocks = wiki_markdown_parse_tokens($context, wiki_markdown_strip_line_tokens($tokens), false, true);
    foreach(wiki_markdown_flatten_blocks($blocks) as $item) {
	if(wiki_markdown_is_line_token($item) ||
	   ($item instanceof WikiToken && trim($item->t) === '') ||
	   ($item instanceof WikiTextTree && wiki_tokens_is_whitespace($item->target)))
	    continue;
	if($item instanceof WikiApplyTree) {
	    $item = wiki_markdown_resolve_alias($context, $item);
	    if($item->id === 'include')
		$has_include = true;
	}
	$items[] = $item;
    }
    if(!$has_include)
	return false;
    if(count($items) !== 1)
	return null;
    $include = $items[0];

    /* 取り込む先が画像か。添付は種類で、URL は &include と同じ正規表現で見る */
    global $WIKI_OPERATOR_INCLUDE_URI_HANDLERS;
    $parameters = array_values((array)$include->parameters);
    $first = empty($parameters) ? array() : $parameters[0];
    $is_url = count($first) == 1 && $first[0] instanceof WikiURIToken;
    if($is_url) {
	if(!preg_match($WIKI_OPERATOR_INCLUDE_URI_HANDLERS['image']['regex'], $first[0]->t))
	    return null;
	$image_page = null;
    } else {
	if(!preg_match('/^\[\[(.*)\]\]$/', trim(wiki_tokens_to_string($first)), $matches))
	    return null;
	$image_page = wiki_markdown_readable_page($context, trim($matches[1]));
	if(wiki_markdown_file_embed($image_page) !== 'file_image_embed')
	    return null;
    }

    if($is_raw && !is_null($image_page) && !is_null($locate)) {
	$target = wiki_markdown_readable_page($context, trim($locate));
	if(!is_null($target) && $target['name'] === $image_page['name'])
	    return wiki_markdown_include($state, $context, $include);
    }
    $image = wiki_markdown_include($state, $context, $include, $is_url ? array() : array('!link'));
    if(is_null($image))
	return null;
    if($is_raw)
	wiki_markdown_lost($state, l('Direct links to images, audio, video and pages (&direct) (made links to the page)'));
    return '[' . $image . '](' . $destination . ')';
}

/* ページへのリンク。[[行き先|表示名]] は wiki と同じ行き先の規則なので、並べ替えるだけ */
function wiki_markdown_page_link($locate, $name) {
    $locate = trim($locate);
    $name = trim($name);
    if($name === '' || $name === $locate)
	return '[[' . $locate . ']]';
    /*
     * 表示名に [ ] < > があると [[..|..]] の形では読めない (中に [ ] を許さない) ので、
     * [表示名](行き先) で書く。行き先は Markdown のリンクの規則に直す。
     */
    if(preg_match('/[\[\]<>]/u', $name))
	return '[' . wiki_markdown_escape($name) . '](' . wiki_markdown_link_destination($locate) . ')';
    return '[[' . $locate . '|' . str_replace('|', '｜', $name) . ']]';
}

/* wiki のページの指定 (ページ#見出し) を、Markdown の [..](行き先) の行き先にする */
function wiki_markdown_link_destination($locate) {
    $section = '';
    $position = strpos($locate, '#');
    if($position !== false) {
	$section = substr($locate, $position);
	$locate = substr($locate, 0, $position);
    }
    $path = $locate === '' ? '' : wiki_markdown_page_path($locate);
    return str_replace(array(' ', '(', ')', '<', '>'), array('%20', '%28', '%29', '%3C', '%3E'), $path . $section);
}

/* 表示名の文字。[[..|..]] の表示名は装飾が効かないので、記号を落とした文字だけ */
function wiki_markdown_plain_name($tokens) {
    $text = wiki_markdown_raw_text(wiki_markdown_strip_line_tokens($tokens));
    return str_replace(array("'''", "''", '%%'), '', $text);
}

/*
 * [[...]]。区切り (| > :) の選び方は表示 (wiki_operator_bracelink()) と同じ。
 * URL なら [表示名](URL)、ページなら [[行き先|表示名]]。>> は落ちる。
 */
function wiki_markdown_bracelink(&$state, &$context, $tree) {
    $separators = array(new WikiSymbolToken('|', 0, 0), new WikiSymbolToken('>', 0, 0),
			new WikiSymbolToken(':', 0, 0));
    $name_tokens = $locate_tokens = null;
    $is_open = false;
    $max_score = 0;
    foreach($separators as $separator) {
	foreach(array('wiki_tokens_separate_from_right', 'wiki_tokens_separate') as $func) {
	    list($c_name_tokens, $c_locate_tokens) = $func($tree->target, $separator);
	    $c_is_open = false;
	    $score = empty($c_name_tokens) ? -1 : 1;
	    if(empty($c_locate_tokens))
		$score--;
	    else {
		$score++;
		if($separator->t == '>') {
		    if(wiki_is_symbol($c_locate_tokens[0], '>')) {
			$c_is_open = true;
			array_shift($c_locate_tokens);
		    } else if(!empty($c_name_tokens) &&
			      wiki_is_symbol($c_name_tokens[count($c_name_tokens) - 1], '>')) {
			$c_is_open = true;
			array_pop($c_name_tokens);
		    }
		}
		if(count($c_locate_tokens) === 1 && $c_locate_tokens[0] instanceof WikiURIToken)
		    $score++;
	    }
	    if($max_score < $score) {
		$max_score = $score;
		$name_tokens = $c_name_tokens;
		$locate_tokens = $c_locate_tokens;
		$is_open = $c_is_open;
	    }
	}
    }
    if($max_score <= 0) {
	$name_tokens = array();
	$locate_tokens = $tree->target;
    }
    if($is_open)
	wiki_markdown_lost($state, l('Opening in a new tab (>>)'));

    if(count($locate_tokens) == 1 && $locate_tokens[0] instanceof WikiURIToken) {
	$url = $locate_tokens[0]->t;
	$name = trim(wiki_markdown_inline_tokens($state, $context, $name_tokens));
	if($name === '')
	    return '<' . $url . '>';
	return '[' . $name . '](' . str_replace(array(' ', ')'), array('%20', '%29'), $url) . ')';
    }
    return wiki_markdown_page_link(wiki_tokens_to_string($locate_tokens),
				   empty($name_tokens) ? '' : wiki_markdown_plain_name($name_tokens));
}

/* ---- エスケープ ----------------------------------------------------- */

/*
 * 文の中で Markdown の記号として読まれる文字をエスケープする。NextForm が足した
 * 記法 ([[ ・![..|..]) も、[ をエスケープすれば効かない。
 */
function wiki_markdown_escape($text) {
    $text = preg_replace('/([\\\\`*_\[\]<>|~@])/u', '\\\\$1', $text);
    /* www. で始まる語は GFM の自動リンクになるので、. をエスケープする */
    $text = preg_replace('/\b(www)\./i', '$1\\\\.', $text);
    /* 文字実体参照に見えるもの (&amp; など) は & をエスケープする */
    return preg_replace('/&(?=#?[A-Za-z0-9]+;)/', '\\\\&', $text);
}

/*
 * 行頭で別の記法に読まれる文字をエスケープする (見出し・引用・箇条書き・番号・
 * 区切り線・NextForm の :::・[TOC]・[PAGES]・フロントマターの ---)。
 */
/* 改行を含む文字を、1 行ずつ wiki_markdown_escape_line() に通す */
function wiki_markdown_escape_lines($text) {
    return implode("\n", array_map('wiki_markdown_escape_line', explode("\n", $text)));
}

function wiki_markdown_escape_line($line) {
    $line = ltrim($line, " \t");
    if(preg_match('/^(#{1,6}(\s|$)|>|[-+*](\s|$)|={3,}\s*$|-{3,}\s*$|:{3,}|\[(TOC|PAGES))/i', $line))
	return '\\' . $line;
    return preg_replace('/^(\d+)([.)])(\s|$)/', '$1\\\\$2$3', $line);
}
?>
