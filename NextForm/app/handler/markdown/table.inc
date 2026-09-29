<?php
/*
 * 表の中の [[ページ名|表示名]] を 1 つのセルに収める。
 *
 * GFM の表は、インライン記法を読む**前に**行を | でセルに分ける
 * (同梱ライブラリの TableParser::split())。[[...]] を読むのはそのあとなので、
 * 中の | が区切りとして使われ、[[ページ名 と 表示名]] の 2 つのセルに割れる。
 * 見出し行にあるとセルの数が区切り行より多くなり、表そのものにならない。
 *
 * GFM では、セルの中に | を書くには \| と逃がす決まりがあり、split() は
 * \| を | に戻してからセルの中身として渡す。そこで、**ライブラリに渡す行の
 * [[...]] の中の | だけを \| にしてから**渡す。解析も描画もライブラリのまま。
 *
 *   MarkdownTableStartParser  ライブラリの TableStartParser を包む。見出し行は
 *                             段落の最後の行として読まれるので、段落の中身を
 *                             見せる口 (getParagraphContent) で置き換える
 *   MarkdownTableParser       ライブラリの TableParser を包む。本体の行は
 *                             addLine() で受け取るので、そこで置き換える
 *
 * vendor には手を入れない。ライブラリの TableStartParser / TableParser は
 * final なので、継承ではなく包んで委譲する。
 *
 * 部分編集の範囲は行番号から出す (markdown_set_block_positions()) ので、
 * 行の中身を書き換えても変わらない。
 *
 * **このファイルは vendor/autoload.php と wikilink.inc を読んだあとに require すること。**
 */

/*
 * 行の中の [[...]] の中の、まだ逃がしていない | を \| にする。
 * すでに \| と書いてあるものはそのまま (二重に逃がすと \ が残る)。
 */
function markdown_table_escape_wikilinks($line) {
    return preg_replace_callback('/' . MarkdownWikiLinkParser::PATTERN . '/', function($matches) {
	return preg_replace('/(?<!\\\\)\|/', '\\\\|', $matches[0]);
    }, $line);
}

class MarkdownTableStartParser implements League\CommonMark\Parser\Block\BlockStartParserInterface {
    private $inner;

    public function __construct() {
	$this->inner = new League\CommonMark\Extension\Table\TableStartParser();
    }

    public function tryStart(League\CommonMark\Parser\Cursor $cursor,
			     League\CommonMark\Parser\MarkdownParserStateInterface $parserState): ?League\CommonMark\Parser\Block\BlockStart {
	$start = $this->inner->tryStart($cursor, new MarkdownTableParserState($parserState));
	if($start === null || $start->isAborting())
	    return $start;

	$parsers = array();
	foreach($start->getBlockParsers() as $parser) {
	    if($parser instanceof League\CommonMark\Extension\Table\TableParser)
		$parser = new MarkdownTableParser($parser);
	    $parsers[] = $parser;
	}
	/* 位置は包む前と同じ (ライブラリが行末まで進めた $cursor) */
	$result = League\CommonMark\Parser\Block\BlockStart::of(...$parsers)->at($cursor);
	if($start->isReplaceActiveBlockParser())
	    $result->replaceActiveBlockParser();
	return $result;
    }
}

/*
 * 解析の状態を見せる口の包み。段落の最後の行 (= 表の見出し行) だけを置き換える。
 * それより前の行はライブラリが段落として作り直すので、原文のまま渡す。
 */
class MarkdownTableParserState implements League\CommonMark\Parser\MarkdownParserStateInterface {
    private $state;

    public function __construct(League\CommonMark\Parser\MarkdownParserStateInterface $state) {
	$this->state = $state;
    }

    public function getActiveBlockParser(): League\CommonMark\Parser\Block\BlockContinueParserInterface {
	return $this->state->getActiveBlockParser();
    }

    public function getLastMatchedBlockParser(): League\CommonMark\Parser\Block\BlockContinueParserInterface {
	return $this->state->getLastMatchedBlockParser();
    }

    public function getParagraphContent(): ?string {
	$paragraph = $this->state->getParagraphContent();
	if($paragraph === null)
	    return null;
	$position = strrpos($paragraph, "\n");
	if($position === false)
	    return markdown_table_escape_wikilinks($paragraph);
	return substr($paragraph, 0, $position + 1)
	    . markdown_table_escape_wikilinks(substr($paragraph, $position + 1));
    }
}

class MarkdownTableParser extends League\CommonMark\Parser\Block\AbstractBlockContinueParser
    implements League\CommonMark\Parser\Block\BlockContinueParserWithInlinesInterface {
    private $inner;

    public function __construct(League\CommonMark\Extension\Table\TableParser $inner) {
	$this->inner = $inner;
    }

    public function getBlock(): League\CommonMark\Node\Block\AbstractBlock {
	return $this->inner->getBlock();
    }

    public function canHaveLazyContinuationLines(): bool {
	return $this->inner->canHaveLazyContinuationLines();
    }

    public function tryContinue(League\CommonMark\Parser\Cursor $cursor,
				League\CommonMark\Parser\Block\BlockContinueParserInterface $activeBlockParser): ?League\CommonMark\Parser\Block\BlockContinue {
	return $this->inner->tryContinue($cursor, $activeBlockParser);
    }

    public function addLine(string $line): void {
	$this->inner->addLine(markdown_table_escape_wikilinks($line));
    }

    public function closeBlock(): void {
	$this->inner->closeBlock();
    }

    public function parseInlines(League\CommonMark\Parser\InlineParserEngineInterface $inlineParser): void {
	$this->inner->parseInlines($inlineParser);
    }
}
?>
