<?php
/*
 * フロントマター (--- で囲んだ先頭のメタ情報) を読む最小のパーサ。
 *
 * 公式の FrontMatterExtension は symfony/yaml を要求するが、ここで要るのは
 * 「本文に出さない」ことと、あとで使えるよう素朴な key: value を拾うことだけ。
 * そのために 3,000 行のライブラリを同梱する理由がない。
 * 入れ子や配列は解釈せず、書かれた文字列のまま返す。
 *
 * **このファイルは vendor/autoload.php を読んだあとに require すること。**
 * 実装するインターフェイスが同梱ライブラリ側にあるため、先に読むと
 * 「インターフェイスが無い」で落ちる。
 */
class MarkdownFrontMatterParser
    implements League\CommonMark\Extension\FrontMatter\Data\FrontMatterDataParserInterface {
    public function parse(string $frontMatter) {
	$data = array();
	foreach(explode("\n", $frontMatter) as $line) {
	    if(preg_match('/^([A-Za-z0-9_-]+)\s*:\s*(.*)$/', $line, $matches))
		$data[$matches[1]] = trim($matches[2], " \t\"'");
	}
	return $data;
    }
}
?>
