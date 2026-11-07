<?php
/*
 * フロントマター (--- で囲んだ先頭のメタ情報) を読む最小のパーサ。
 *
 * 公式の FrontMatterExtension は symfony/yaml を要求するが、ここで要るのは
 * 「本文に出さない」ことと、あとで使えるよう素朴な key: value を拾うことだけ。
 * そのために 3,000 行のライブラリを同梱する理由がない。
 * 入れ子は解釈せず、書かれた文字列のまま返す。
 *
 * 並びだけは 2 通りに対応する。値を使う側 (markdown_parse_tags) で
 * 書式ごとの分岐を増やしたくないので、ブロック形式は配列にして返す。
 *
 *   tags: [alpha, beta]      → 'tags' => '[alpha, beta]'  (文字列のまま)
 *   tags:                    → 'tags' => array('alpha', 'beta')
 *     - alpha
 *     - beta
 *
 * **このファイルは vendor/autoload.php を読んだあとに require すること。**
 * 実装するインターフェイスが同梱ライブラリ側にあるため、先に読むと
 * 「インターフェイスが無い」で落ちる。
 */
class MarkdownFrontMatterParser
    implements League\CommonMark\Extension\FrontMatter\Data\FrontMatterDataParserInterface {
    public function parse(string $frontMatter) {
	$data = array();
	$key = null;
	foreach(explode("\n", $frontMatter) as $line) {
	    $line = rtrim($line, "\r");

	    /* 「  - 値」は 1 つ前のキーの並び */
	    if(!is_null($key) && preg_match('/^\s+-\s*(.*)$/', $line, $matches)) {
		if(!is_array($data[$key]))
		    $data[$key] = ($data[$key] === '') ? array() : array($data[$key]);
		$data[$key][] = self::value($matches[1]);
		continue;
	    }

	    if(preg_match('/^([A-Za-z0-9_-]+)\s*:\s*(.*)$/', $line, $matches)) {
		$key = $matches[1];
		$data[$key] = self::value($matches[2]);
		continue;
	    }

	    /* キーでも並びでもない行 (囲みの --- など) で並びは終わる */
	    $key = null;
	}
	return $data;
    }

    /*
     * 1 つの値。全体が同じ引用符の 1 組で囲まれていれば、その 1 組だけを外して中の逃がしを
     * 戻す (YAML と同じ。"…" は \" と \\、'…' は '')。値の端に引用符がある値や、# で始まる値
     * (よそのツールではコメントになる) を、囲んで書けるように。
     * 囲まれていなければ、今までどおり両端の空白と引用符を落とす。
     */
    private static function value($value) {
	$value = trim($value, " \t");
	$length = strlen($value);
	if($length >= 2 && $value[0] === '"' && $value[$length - 1] === '"')
	    return preg_replace('/\\\\(["\\\\])/', '$1', substr($value, 1, -1));
	if($length >= 2 && $value[0] === "'" && $value[$length - 1] === "'")
	    return str_replace("''", "'", substr($value, 1, -1));
	return trim($value, " \t\"'");
    }
}
?>
