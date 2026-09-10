# 設計メモ: Markdown の記法例 `:::example`

> **このドキュメントの位置づけ**
> 種別 Markdown で「ソースと表示を並べて見せる」記法の設計。
> 2026-09-11 に相談・承認。要点は [project-overview.md](project-overview.md) §7 に
> 1 行で移す。このファイルは「なぜその形にしたか」と却下した案の記録。
>
> 先行する [design-markdown-details.md](design-markdown-details.md) と同じ
> `:::` の囲みブロックなので、実装の作法はそちらを引き継ぐ。

---

## 1. 何を作るか

wiki 記法の `&wikiexample{}` に当たるものを、種別 Markdown にも用意する。

```
:::example
- 項目
- 項目
:::
```

出すのは「ソース」と「表示」を並べた `<dl>`。記法の説明を書くときに、
**同じ原文を 2 度書かなくて済む**ようにするのが目的。

## 2. 上流にあるもの

`&wikiexample{}` は上流 (ToraToraWiki) からある。実装は 13 行しかない。

```php
/* NextForm/app/handler/wiki/function/wiki_wikiexample.inc */
$dl = ...; $dt = 'Source'; $dd = <pre>トークンを文字列に戻したもの</pre>;
$dt = 'Display'; $dd = トークンを変換したもの;
```

**同じトークン列を「文字列に戻す」と「変換する」の 2 回使う**だけ。
語句 (`Source` / `Display`) は `app/language.inc` に既にある。

### 上流のこれは文書化されていない

組み込みマニュアルの本文で **42 回使われている**が、早見表にも
ファンクション一覧にも項目が無い。マニュアルを書くための道具として
使われているだけで、利用者には案内されていない。
**今回、wiki 側の文書化も一緒に行う** (§7)。

## 3. なぜこの形か

### Markdown に標準はない

CommonMark にも GFM にも無く、同梱の league/commonmark にも拡張が無い。
世の中のやり方は 2 つ。

| 方式 | 使っている所 | 問題 |
|---|---|---|
| 手で二重に書く | 多くの静的サイト | 原文が 2 箇所にあり、直すとき必ず片方が腐る |
| MDX のコンポーネント | Docusaurus, Nextra | React が要る。「展開するだけで動く」方針に乗らない |

### 採らなかった案

| 案 | 却下の理由 |
|---|---|
| コードフェンスの info string (```` ```markdown-example ````) | 他のツールでコードブロックとして素直に劣化する利点はあるが、info string は**言語名の場所**で、意味を乗っ取ることになる。`:::details` で `:::` を選んだ判断と食い違う |
| 生 HTML で書く | `MARKDOWN_ALLOW_HTML` の既定 (`escape`) を崩す。折りたたみのときと同じ理由で却下 |
| 描画のときに変換器をもう一度呼ぶ | **壊れる。** `MarkdownConverter` は `MarkdownParser` を 1 つ持ち回しており、描画の途中で `convert()` を呼ぶと解析中の状態を踏む。原文は解析のときに控えておけばよく (§5)、2 回目の変換は要らない |

### `:::example` を選ぶ利点

- `:::details` で使った囲みブロックの仕組みにそのまま乗る。
  コードブロックの中の `:::` で閉じない対策も、入れ子の規則も再利用できる
- CommonMark と衝突しない。既存ページの意味を変えない。
  **手元の 2 サイト計 2,710 ページを調べて、`:::example` を書いているページは 0 件**
- 他のツールに貼っても `:::` の行が余分に見えるだけで、中身は消えない

### 名前のぶつかり

MkDocs の注意書き (admonition) に `example` という種別がある。
将来 `:::note` / `:::warning` を足すときに名前がぶつかる。
**承知のうえで `example` を採る** — この記法が出すのは注意書きではなく
「ソースと表示の対」で、利用者にとっては `wikiexample` と同じものだから。
注意書きを足すときは、ぶつかる 1 語を避ければよい。

## 4. 記法の仕様

| 項目 | 決定 | 理由 |
|---|---|---|
| 開始 | 行頭 (字下げ 3 桁未満) から `:` を **3 つ以上**、続けて `example`、後ろは空白か行末 | `:::details` と同じ流儀 |
| 終了 | `:` を**開始と同じ数以上**並べただけの行 | 同上 |
| 入れ子 | **外側のコロンを増やす** | 同上 |
| 閉じ忘れ | 文書の終わりで暗黙に閉じる | 同上 |
| `:::exampleX` | 反応しない | ラベルの前に区切りを必須にする作法を `details` から引き継ぐ |
| **ラベル** | **受け付けない**。`:::example 説明` の「説明」は無視せず、**そもそも囲みにしない** | `&wikiexample{}` にラベルが無い。片方だけに足すと非対称になる。将来足す余地は残る (いまラベルを黙って捨てると、後で意味を変えることになる) |
| 中身 | ふつうの Markdown として解析する | 「表示」側がこれで作れる |

## 5. 出す HTML と、原文をどう取るか

```html
<dl class="example">
<dt>ソース</dt><dd><pre>- 項目
- 項目</pre></dd>
<dt>表示</dt><dd><ul><li>項目</li><li>項目</li></ul></dd>
</dl>
```

wiki 側と同じ `dl` / `dt` / `dd` / `pre` の形にする。
**class は Markdown 側だけ付ける** — wiki 側の `dl` は class を持たず、
いま CSS で狙えない。そちらに足すと既存 HTML が変わりゴールデンマスターが
動くので、**別件にする** (§9 に残す)。

### 肝: CommonMark は原文を残さない

囲みの中は AST に変換されてしまい、ブロックは原文の文字列を持たない。
`&wikiexample` がトークン列を 2 度使えるのとは事情が違う。

対策は **`tryContinue()` で 1 行ずつ控えること**。ライブラリは囲みの中の
各行について、まず外側の `tryContinue()` を呼ぶ。そこで
`$cursor->getRemainder()` を溜めれば原文が手に入り、子は今までどおり
解析されて AST になる。**解析は 1 回のまま**、描画のときに
「控えた原文」と「変換済みの子」の両方が使える。

`getRemainder()` を使うのは、外側の囲み記号が既に落ちているため。
引用 (`> `) や箇条書きの中に書いた例でも、`> ` や字下げが
ソース側に混ざらない。**試作で実測済み** (2026-09-11)。

## 6. 実装

| 場所 | 中身 |
|---|---|
| `NextForm/app/handler/markdown/example.inc` (新規) | `MarkdownExample` / `MarkdownExampleStartParser` / `MarkdownExampleParser` / `MarkdownExampleRenderer`。**約 120 行** |
| `NextForm/app/handler/markdown.inc` | `require_once` 1 行、`addBlockStartParser()` と `addRenderer()` で 2 行 |
| `NextForm/app/handler/markdown.inc` の `markdown_set_block_positions_children()` | 例の中へは降りない (下記) |

`app/language.inc` は触らない。`Source` / `Display` が既にある。

### 部分編集は「例の全体で 1 単位」

中のブロックに `data-twp` を振らない。振ると、同じ原文が「ソース」と
「表示」の 2 箇所に出ているのに押せるのは片方だけ、という妙な形になる。
`markdown_set_block_positions_children()` で、例そのものには範囲を付け、
**その中へは降りない**。脚注を飛ばしているのと同じ場所に 1 行足す。

### コードブロックの中の `:::` で閉じない

`:::details` と同じ対策をそのまま使う。`tryContinue()` の第 2 引数
(いま一番内側で動いているパーサ) が `FencedCode` / `HtmlBlock` なら
閉じフェンスを探さない。**試作で実測済み**。

## 7. マニュアル

| ページ | 変更 |
|---|---|
| `ja/Markdown/Extra` `en/Markdown/Extra` | 「記法の例」節を新設。**`:::details` の節の並び**に置く |
| `ja/Markdown/CheatSheet` `en` | 拡張要素の表に 1 行 |
| `ja/Wiki/Function` `en` | **`&wikiexample` を新規に文書化**。42 回使われているのに項目が無い |
| `ja/Wiki/CheatSheet/Function` `en` | 早見表に 1 行 |
| `ja/Comparison` `en` | 対応表に「記法の例」の行。**両方の記法にある**ので「そのまま写せるもの」側 |

## 8. テスト

| 対象 | 内容 |
|---|---|
| `tests/golden/input/GoldenMaster/MarkdownExample.md` (新規) + `golden.sh` の TARGETS | 出力を固定する。既存のフィクスチャは触らない |
| `tests/markdown.sh` | 入れ子 / 閉じ忘れ / **コードブロックの中の `:::`** / `:::exampleX` は素通し / 引用と箇条書きの中 / 検索の文字に入るか / 部分編集の範囲が例の全体になっているか |
| `tests/css-rules.sh` | `dl.example` の規則を 1 つ名指しで |
| `tests/manual.sh` | 既存の全ページ巡回でリンク切れを見る |

新規追加なので既存の振る舞いは変わらない。④「変更前の固定」は、
**新しいフィクスチャを実装前に採る**という形で行う
(`:::example` がただの文字として出る状態を先に固定し、実装後の差分を読む)。

## 9. 積み残し (今回やらない)

| 件 | 理由 |
|---|---|
| wiki 側の `dl` に class を付ける | 既存 HTML が変わり、ゴールデンマスターの採り直しが要る。混ぜない |
| 検索の文字が二重に載る | 中身が「ソース」と「表示」の 2 箇所に出るので、索引に 2 回入る。**wiki 側も同じ**なので、いま片方だけ変えると挙動がずれる |
| `:::example` にラベルを足す | §4 のとおり、wiki 側と非対称になる。要望が出てから |
