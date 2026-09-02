# 設計メモ: Markdown の折りたたみ `:::details`

> **このドキュメントの位置づけ**
> v0.9.0 で入れる Markdown の折りたたみ記法の設計。2026-09-02 に承認済み。
> 実装が終わったら、決まったことは
> [project-overview.md](project-overview.md) §7 に 1 行で移す。
> このファイルは「なぜその形にしたか」を残すための作業用メモ。

---

## 1. 何を作るか

種別 Markdown のページで、本文の一部を折りたたむ記法を足す。

```
:::details 開いていないときに出す文字
中身はふつうの Markdown。
見出しでも表でも書ける。
:::
```

出す HTML は素の `<details><summary>`。**JavaScript は使わない**。

## 2. なぜこの形か

### 折りたたみに Markdown の標準はない

CommonMark にも GFM にも無い。世の中の実装は 3 派ある。

| 方式 | 使っている所 | 素の Markdown では |
|---|---|---|
| `<details><summary>` の生 HTML | GitHub、多くの静的サイト | HTML がそのまま出るか、エスケープされる |
| `> [!note]-` (引用の拡張) | Obsidian のコールアウト | ただの引用として読める |
| `:::details` (囲みブロック) | VitePress、Docusaurus、MyST、markdown-it-container | `:::` の行が本文に見えるが、中身は普通に読める |

### 採らなかった案

| 案 | 却下の理由 |
|---|---|
| Notion 式の `>` | Notion のエディタでは `>` がトグル、`"` が引用。**CommonMark とは逆**で、`>` は引用。これをトグルに割り当てると、Obsidian・GitHub その他から貼った引用がすべてトグルに化ける。「他のツールから貼ってもおおむね同じ見え方」(CHANGELOG v0.6.0) を Notion 1 つのために捨てる取引になる |
| Obsidian 式の `> [!note]-` | 引用の意味に手を入れるので「引用のつもりが折り畳まれる」事故が残る。コールアウト全体 (note / warning / tip …) を実装しないと中途半端になり、範囲が大きい |
| 生 HTML を開ける (`MARKDOWN_ALLOW_HTML` を既定 true に) | 書ける人全員が任意のスクリプトを埋められる。既定を安全側に倒す方針 ([markdown.inc](../../NextForm/app/handler/markdown.inc) 冒頭) を崩す。GFM 同梱の `DisallowedRawHtmlExtension` が落とすのは `<script>` など 9 タグだけで、`<img onerror=>` は素通りする |
| wiki の `*(optional)` を移植 | **できない。** wiki の折りたたみが成立するのは節が範囲を持つ (`section.section` の入れ子) から。Markdown の見出しはフラットで範囲を持たない (v0.8 で確定済み) |

### `:::` を選ぶ利点

- CommonMark と衝突しない。既存ページの意味を一切変えない
- 生 HTML を開けずに `<details>` を出せる
- JS が要らない。wiki の `.optional` が prototype.js に依存しているのと対照的に、
  ブラウザ標準の要素だけで動き、静的エクスポートでも生きる
- 置き場所が既にある。`[[ページ名]]` と画像の差し替えを入れた拡張の登録口に
  ブロックパーサと描画器を 1 組足すだけ
- 他のツールに貼っても `:::` の行が余分に見えるだけで、中身は消えない

## 3. 記法の仕様

| 項目 | 決定 | 理由 |
|---|---|---|
| 開始 | 行頭 (字下げ 3 桁未満) から `:` を **3 つ以上**、続けて `details`、任意の開閉フラグ、空白 + ラベル | コードフェンスと同じ流儀 |
| 終了 | `:` を**開始と同じ数以上**並べただけの行 | 同上。入れ子を数で表せる |
| ラベル省略 | `詳細` / `Details` (`l()` 経由) | 空の `<summary>` は押せる場所が消える |
| 初期状態 | 既定は**閉じている** | wiki の `*(optional)` と揃える |
| 開いて出す | `:::details+` と `:::details open` の**両方**を受ける | `+` は wiki の折りたたみボタンの見た目、`open` は HTML の属性名。どちらを覚えていても書ける |
| 入れ子 | **外側のコロンを増やす** (`::::` の中に `:::`) | コードフェンスと同じ規則。実装上も必要 (§5) |
| 閉じ忘れ | 文書の終わりで暗黙に閉じる | コードフェンスと同じ |
| 段落の直後 | 空行なしでも始まる | コードフェンスと同じ |
| `details` 以外 | 反応しない (`:::note` はただの段落) | 将来コールアウトに広げる余地を残す |

### 開閉フラグの決め方

開始行を `/^(:{3,})details(\+)?[ \t]*(.*)$/` で見る。

1. `details` の直後に `+` があれば開いた状態。残りがラベル
2. `+` が無く、残りの最初の語が `open` (後ろが空白か行末) なら開いた状態。
   その語を落とした残りがラベル
3. どちらでもなければ閉じた状態。残りがラベル

`open` で始まるラベルを書きたいときは `:::details+ open な話` と書く。
**マニュアルにこの逃げ道を明記する。**

### 例

```
:::details 閉じて出る
中身
:::

:::details+ 開いて出る
中身
:::

:::details open 開いて出る (別の書き方)
中身
:::

::::details 外側
:::details 内側
中身
:::
::::
```

## 4. 出す HTML

```html
<details><summary>ラベル</summary><p>中身</p></details>
<details open="open"><summary>ラベル</summary><p>中身</p></details>
```

class は付けない。CSS は `section.markdown details` で当てる。

`markdown_append_html()` の DOMDocument 読み直しを通っても
`<details><summary>` が壊れないことは実測で確認済み (2026-09-02)。

## 5. 実装

新規ファイル 1 つ。`[[ページ名]]` (`wikilink.inc`) と画像 (`image.inc`) と
同じ置き場所・同じ流儀にする。

| 場所 | 中身 |
|---|---|
| `NextForm/app/handler/markdown/details.inc` (新規) | `MarkdownDetails` (AbstractBlock) / `MarkdownDetailsStartParser` / `MarkdownDetailsParser` / `MarkdownDetailsRenderer`。**約 120 行** |
| `NextForm/app/handler/markdown.inc` | `require_once` 1 行、`addBlockStartParser()` と `addRenderer()` で 2 行 |
| `NextForm/app/language.inc` | `'Details' => '詳細'` 1 行 |

ライブラリを知っている場所を増やさない、という今の作りは守る。

### 落とし穴: コードブロックの中の `:::` で閉じてしまう

````
:::details 例
```
:::          ← ここで閉じてはいけない
```
:::
````

league/commonmark は毎行、**外側のブロックから順に** `tryContinue()` を呼ぶ
(`Parser/MarkdownParser.php` の `parseBlockContinuation()`)。素直に書くと
`details` が内側のコードブロックより先に `:::` を見て、両方まとめて閉じる。

対策は `tryContinue()` の第 2 引数 (= いま一番内側で動いているパーサ) を見ること。

- 内側が `FencedCode` / `HtmlBlock` なら閉じフェンスを探さない
- 字下げ 4 桁以上 (`$cursor->isIndented()`) なら探さない → `IndentedCode` はこれで済む

markdown-it の container 系プラグインはこの穴を持ったままなので、
**ここは実装で勝てる所**。テストに入れる。

## 6. CSS

`NextForm/app/theme/common/style/.markdown.css` に `details` / `summary` の
見た目を足す。テーマ共通なので 4 テーマすべてに効く。
**静的 CSS の再生成が必要**で、`theme-diff.sh` は差分が出る (規則を足すので当然)。

印刷で閉じたまま中身が消える件は、CSS だけで確実に開かせる手がブラウザ依存。
**実測してから決める** (最悪は「印刷では折りたたみは開かない」と仕様に書く)。

## 7. 周辺への波及

| | 見込み | 確かめ方 |
|---|---|---|
| サイト内検索 | 中身も索引に載る (`markdown_texts()` は DOM から文字を集めるため) | `markdown.sh` に検査を足す |
| 目次 (`?option=summary`) | 中の見出しも出る。目次から飛ぶとブラウザが `details` を自動で開く**はず** | 要実測 |
| ブラウザのページ内検索 | 閉じた中身は**見つからない** | 仕様として受け入れ、マニュアルに書く |
| 静的エクスポート | そのまま動く (JS 非依存のため) | `smoke.sh` |
| v0.9 の部分編集 | ブロックの一種が増えるだけ。設計に影響なし | — |

## 8. テスト

| 対象 | 内容 |
|---|---|
| `tests/golden/input/GoldenMaster/MarkdownDetails.md` (新規) + `golden.sh` の TARGETS | 出力を固定する。**既存の `Markdown.md` は触らない** (expected の採り直しを避ける) |
| `tests/markdown.sh` | 入れ子 / 閉じ忘れ / **コードブロックの中の `:::`** / ラベル省略 / `+` と `open` / `:::note` は素通し / 検索に載るか |
| `tests/css-rules.sh` | `details` の規則を 1 つ名指しで |

新規追加なので既存の振る舞いは変わらない。開発フロー ④ の「変更前の固定」は
golden の再採取ではなく、**新しいフィクスチャの採取**という形になる。

## 9. マニュアル

| ページ | 変更 |
|---|---|
| `ja/Markdown/Extra` `en/Markdown/Extra` | 「*折りたたみ」節を新設 (「自動リンク」の後、「生のHTML」の前)。`+` / `open` と、`open` で始まるラベルの逃げ道も書く。**冒頭の「GitHubやObsidianで通じる書き方は，おおむねそのまま通じます」に例外の断りを足す** — これは NextForm 独自で、他のツールに貼ると `:::` の行がそのまま見える |
| `ja/Markdown/CheatSheet` `en` | 拡張要素の表に 1 行 |
| `ja/Comparison` `en` | 「見出しの折りたたみ｜`*(optional)`｜なし｜Markdown記法に折りたたみはありません」を書き換え。**Wiki は節の範囲を畳む / Markdown は囲んだ範囲を畳む**、と違いを明記 |
| `ja/Markdown` `en` (入口) | 「拡張要素(表，チェックボックス，脚注など)」に折りたたみを足す |
| `Markdown/Basic` の見出しの節 | 「Markdown記法の見出しは折りたたみを持たず」の文はそのまま残す (見出しの折りたたみは依然できない)。誤読されないよう `:::details` への参照を 1 行足す |

### ついでに直す誤記

`ja/Comparison` `en/Comparison` の「Markdown記法では書けないもの」の表に
**「折りたたみ｜`&more`」** という行があるが、`wiki_more.inc` を読むと
`&more` は折りたたみではなく**抜粋の区切り** (一覧に出す要約をどこまでにするか)。
今回この表を触るので合わせて直す。

## 10. やらないこと

- `>` や Obsidian コールアウトの採用
- `:::note` `:::warning` など他の種類
- wiki 記法側への追加
- 見出しに範囲を持たせる (= v0.9 の部分編集の話。別作業)
- `MARKDOWN_ALLOW_HTML` の既定変更

## 11. 版数と順番

**v0.9.0 に入れる。v0.8.0 を先にリリースしてから着手する。**

v0.8.0 は確認項目 (`upgrade-check.md`) を書き終えてアップグレードテストに
入る段階にある。ここで機能を 1 つ足すと、書いた項目を確かめ直すことになる。

ブランチは `feat/markdown-details`。
