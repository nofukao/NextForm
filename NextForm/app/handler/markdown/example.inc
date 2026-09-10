<?php
/*
 * Markdown の記法例 :::example。
 *
 *   :::example
 *   - 項目
 *   - 項目
 *   :::
 *
 * ソースと表示を並べた <dl> を出す。記法の説明を書くときに、同じ原文を
 * 2 度書かなくて済むようにするためのもの。wiki 記法の &wikiexample{}
 * (handler/wiki/function/wiki_wikiexample.inc) に当たる。
 *
 * **CommonMark は囲みの原文を残さない。** 中身は解析されて AST になり、
 * ブロックは文字列を持たない。&wikiexample がトークン列を「文字列に戻す」と
 * 「変換する」の 2 回使えるのとは事情が違う。そこで tryContinue() で 1 行ずつ
 * 控えておき、描画のときに「控えた原文」と「変換済みの子」の両方を使う。
 * **解析は 1 回のまま**で、描画の途中で変換器をもう一度呼ぶ必要は無い
 * (呼ぶと MarkdownConverter が持ち回している MarkdownParser の状態を踏む)。
 *
 * 経緯と却下した案は docs/development/design-markdown-example.md にある。
 *
 * **このファイルは vendor/autoload.php を読んだあとに require すること。**
 */

class MarkdownExample extends League\CommonMark\Node\Block\AbstractBlock {
    private $source = '';

    /*
     * 囲みの中の 1 行を控える。渡すのは Cursor::getRemainder() の結果で、
     * 外側の囲み記号 (引用の "> "、箇条書きの字下げ) は既に落ちている。
     * getLine() を使うとそれらがソース側に混ざる。
     */
    public function addSourceLine($line) {
	$this->source .= $line . "\n";
    }

    public function getSource() {
	return $this->source;
    }
}

class MarkdownExampleStartParser implements League\CommonMark\Parser\Block\BlockStartParserInterface {
    public function tryStart(League\CommonMark\Parser\Cursor $cursor,
			     League\CommonMark\Parser\MarkdownParserStateInterface $parserState)
	: ?League\CommonMark\Parser\Block\BlockStart {
	if($cursor->isIndented() || $cursor->getNextNonSpaceCharacter() !== ':')
	    return League\CommonMark\Parser\Block\BlockStart::none();

	/*
	 * 名前の後ろには空白か行末が要る (:::exampleX は囲みにしない)。
	 * :::details と同じ作法。区切りを必須にしておかないと、将来
	 * :::examplefoo のような名前を足したときに古い書き方の意味が変わる。
	 */
	$match = $cursor->matchInPlace('/\G[ \t]*:{3,}example(?=[ \t]|$)/');
	if($match === null)
	    return League\CommonMark\Parser\Block\BlockStart::none();

	$fence = ltrim($match, " \t");
	$fence_length = strlen($fence) - strlen('example');

	/*
	 * ラベルは受け付けない。&wikiexample{} に無いものを Markdown 側だけに
	 * 足すと非対称になる。いま黙って捨てておけば、後から足すときに
	 * 古い書き方の意味を変えずに済む。
	 */
	$cursor->advanceToEnd();

	return League\CommonMark\Parser\Block\BlockStart::of(
	    new MarkdownExampleParser($fence_length))->at($cursor);
    }
}

class MarkdownExampleParser extends League\CommonMark\Parser\Block\AbstractBlockContinueParser {
    private $block;
    private $fence_length;

    public function __construct(int $fence_length) {
	$this->fence_length = $fence_length;
	$this->block = new MarkdownExample();
    }

    public function getBlock(): MarkdownExample {
	return $this->block;
    }

    public function isContainer(): bool {
	return true;
    }

    public function canContain(League\CommonMark\Node\Block\AbstractBlock $childBlock): bool {
	return true;
    }

    public function tryContinue(League\CommonMark\Parser\Cursor $cursor,
				League\CommonMark\Parser\Block\BlockContinueParserInterface $activeBlockParser)
	: ?League\CommonMark\Parser\Block\BlockContinue {
	/*
	 * **コードブロックの中の ::: では閉じない。** :::details と同じ穴で、
	 * 対策も同じ。ライブラリは毎行、外側のブロックから順に tryContinue() を
	 * 呼ぶので、素直に書くとこちらが内側のコードブロックより先に ::: を見て
	 * 両方まとめて閉じる。いま一番内側で動いているパーサを見て、literal を
	 * 食うブロックが開いている間は閉じフェンスを探さない。
	 * 字下げ 4 桁以上のコード (IndentedCode) は isIndented() で弾ける。
	 */
	$active = $activeBlockParser->getBlock();
	$in_literal =
	    ($active instanceof League\CommonMark\Extension\CommonMark\Node\Block\FencedCode ||
	     $active instanceof League\CommonMark\Extension\CommonMark\Node\Block\HtmlBlock);

	if(!$in_literal && !$cursor->isIndented() && $cursor->getNextNonSpaceCharacter() === ':') {
	    /* 閉じは開始と同じ数以上。入れ子は外側のコロンを増やして表す */
	    $match = League\CommonMark\Util\RegexHelper::matchFirst(
		'/^:{3,}(?=[ \t]*$)/', $cursor->getLine(), $cursor->getNextNonSpacePosition());
	    if($match !== null && strlen($match[0]) >= $this->fence_length)
		return League\CommonMark\Parser\Block\BlockContinue::finished();
	}

	/* 子に渡す前に控える。ここを飛ばすとソース側が空になる */
	$this->block->addSourceLine($cursor->getRemainder());
	return League\CommonMark\Parser\Block\BlockContinue::at($cursor);
    }
}

class MarkdownExampleRenderer implements League\CommonMark\Renderer\NodeRendererInterface {
    public function render(League\CommonMark\Node\Node $node,
			   League\CommonMark\Renderer\ChildNodeRendererInterface $childRenderer) {
	if(!($node instanceof MarkdownExample))
	    return '';

	/*
	 * 部分編集の範囲 (data-twp / data-twl) は
	 * markdown_set_block_positions() が data に入れる。標準の描画器は
	 * これを読んで属性にするので、独自の描画器も同じように読む。
	 * 読まないと、例だけ二重クリックで編集できない。
	 */
	$attributes = $node->data->get('attributes', array());
	if(!is_array($attributes))
	    $attributes = array();
	/* wiki 側の dl は class を持たない。こちらは CSS で狙えるようにする */
	$attributes['class'] = 'example';

	$separator = $childRenderer->getInnerSeparator();

	/* 末尾の改行は落とす。<pre> の中で余分な空行になるため */
	$source = rtrim($node->getSource(), "\n");
	$pre = new League\CommonMark\Util\HtmlElement(
	    'pre', array(), League\CommonMark\Util\Xml::escape($source));

	$contents = $separator
	    . new League\CommonMark\Util\HtmlElement('dt', array(), League\CommonMark\Util\Xml::escape(l('Source')))
	    . $separator
	    . new League\CommonMark\Util\HtmlElement('dd', array(), $pre)
	    . $separator
	    . new League\CommonMark\Util\HtmlElement('dt', array(), League\CommonMark\Util\Xml::escape(l('Display')))
	    . $separator
	    . new League\CommonMark\Util\HtmlElement(
		'dd', array(), $separator . $childRenderer->renderNodes($node->children()) . $separator)
	    . $separator;

	return new League\CommonMark\Util\HtmlElement('dl', $attributes, $contents);
    }
}
?>
