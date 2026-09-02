<?php
/*
 * Markdown の折りたたみ :::details。
 *
 *   :::details 開いていないときに出す文字
 *   中身はふつうの Markdown。
 *   :::
 *
 * 出すのは素の <details><summary>。**JavaScript は使わない**。
 * wiki 記法の *(optional) は prototype.js が実行時に畳んでいるが、こちらは
 * ブラウザ標準の要素なので、静的エクスポートでも JS 無効でも折りたためる。
 *
 * **折りたたみに Markdown の標準は無い。** CommonMark にも GFM にも無く、
 * 世の中には <details> の生 HTML / Obsidian のコールアウト / ::: の囲みの
 * 3 派がある。::: を採ったのは、CommonMark と衝突せず (既存ページの意味を
 * 一切変えない)、生 HTML を開けずに済むため。Notion 式の > は採れない —
 * CommonMark では > が引用なので、他のツールから貼った引用が全部トグルに化ける。
 *
 * 経緯と却下した案は docs/development/design-markdown-details.md にある。
 *
 * **このファイルは vendor/autoload.php を読んだあとに require すること。**
 */

class MarkdownDetails extends League\CommonMark\Node\Block\AbstractBlock {
    private $label;
    private $is_open;

    public function __construct(string $label, bool $is_open) {
	parent::__construct();
	$this->label = $label;
	$this->is_open = $is_open;
    }

    public function getLabel(): string {
	return $this->label;
    }

    public function isOpen(): bool {
	return $this->is_open;
    }
}

class MarkdownDetailsStartParser implements League\CommonMark\Parser\Block\BlockStartParserInterface {
    public function tryStart(League\CommonMark\Parser\Cursor $cursor,
			     League\CommonMark\Parser\MarkdownParserStateInterface $parserState)
	: ?League\CommonMark\Parser\Block\BlockStart {
	if($cursor->isIndented() || $cursor->getNextNonSpaceCharacter() !== ':')
	    return League\CommonMark\Parser\Block\BlockStart::none();

	/*
	 * ラベルの前には空白が要る (:::detailsX は折りたたみにしない)。
	 * 区切りを必須にしておかないと、将来 :::detailsfoo のような名前を
	 * 足したときに古い書き方の意味が変わる。
	 */
	$match = $cursor->matchInPlace('/\G[ \t]*:{3,}details\+?(?=[ \t]|$)/');
	if($match === null)
	    return League\CommonMark\Parser\Block\BlockStart::none();

	$fence = ltrim($match, " \t");
	$is_open = (substr($fence, -1) === '+');
	$fence_length = strlen($fence) - strlen('details') - ($is_open ? 1 : 0);

	$label = trim($cursor->getRemainder());
	/*
	 * :::details+ と :::details open の両方を受ける。+ は wiki の
	 * 折りたたみボタンの見た目、open は HTML の属性名で、どちらを
	 * 覚えていても書ける。open で始まるラベルを書きたいときは
	 * :::details+ open な話 と書く (マニュアルに明記してある)。
	 */
	if(!$is_open && preg_match('/^open(?:[ \t]+(.*))?$/', $label, $matches)) {
	    $is_open = true;
	    $label = isset($matches[1]) ? trim($matches[1]) : '';
	}
	$cursor->advanceToEnd();

	return League\CommonMark\Parser\Block\BlockStart::of(
	    new MarkdownDetailsParser($fence_length, $label, $is_open))->at($cursor);
    }
}

class MarkdownDetailsParser extends League\CommonMark\Parser\Block\AbstractBlockContinueParser {
    private $block;
    private $fence_length;

    public function __construct(int $fence_length, string $label, bool $is_open) {
	$this->fence_length = $fence_length;
	$this->block = new MarkdownDetails($label, $is_open);
    }

    public function getBlock(): MarkdownDetails {
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
	 * **コードブロックの中の ::: では閉じない。**
	 *
	 * ライブラリは毎行、外側のブロックから順に tryContinue() を呼ぶ
	 * (Parser/MarkdownParser.php の parseBlockContinuation)。素直に書くと
	 * こちらが内側のコードブロックより先に ::: を見て、両方まとめて閉じる。
	 * いま一番内側で動いているパーサを見て、literal を食うブロックが
	 * 開いている間は閉じフェンスを探さない。
	 * 字下げ 4 桁以上のコード (IndentedCode) は isIndented() で弾ける。
	 */
	$active = $activeBlockParser->getBlock();
	if($active instanceof League\CommonMark\Extension\CommonMark\Node\Block\FencedCode ||
	   $active instanceof League\CommonMark\Extension\CommonMark\Node\Block\HtmlBlock)
	    return League\CommonMark\Parser\Block\BlockContinue::at($cursor);

	if(!$cursor->isIndented() && $cursor->getNextNonSpaceCharacter() === ':') {
	    /* 閉じは開始と同じ数以上。入れ子は外側のコロンを増やして表す */
	    $match = League\CommonMark\Util\RegexHelper::matchFirst(
		'/^:{3,}(?=[ \t]*$)/', $cursor->getLine(), $cursor->getNextNonSpacePosition());
	    if($match !== null && strlen($match[0]) >= $this->fence_length)
		return League\CommonMark\Parser\Block\BlockContinue::finished();
	}

	return League\CommonMark\Parser\Block\BlockContinue::at($cursor);
    }
}

class MarkdownDetailsRenderer implements League\CommonMark\Renderer\NodeRendererInterface {
    public function render(League\CommonMark\Node\Node $node,
			   League\CommonMark\Renderer\ChildNodeRendererInterface $childRenderer) {
	if(!($node instanceof MarkdownDetails))
	    return '';

	/* ラベルは素の文字として扱う。中で記法を効かせない (押す所なので短く保たせる) */
	$label = $node->getLabel();
	if($label === '')
	    $label = l('Details');
	$summary = new League\CommonMark\Util\HtmlElement(
	    'summary', array(), League\CommonMark\Util\Xml::escape($label));

	$attributes = array();
	if($node->isOpen())
	    $attributes['open'] = 'open';

	$separator = $childRenderer->getInnerSeparator();
	$filling = $childRenderer->renderNodes($node->children());
	$contents = ($filling === '')
	    ? $summary . $separator
	    : $summary . $separator . $filling . $separator;

	return new League\CommonMark\Util\HtmlElement('details', $attributes, $contents);
    }
}
?>
