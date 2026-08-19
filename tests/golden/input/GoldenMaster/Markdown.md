---
title: フロントマターは本文に出ない
tags: [markdown, golden]
---

# Markdown のゴールデンマスター

CommonMark の基本と、採用した拡張がすべて出るようにしてある。
**強調**と*斜体*と`インラインコード`、それに~~打ち消し~~。

## 箇条書き

- ひとつめ
- ふたつめ
  - 入れ子
- [ ] 未了のタスク
- [x] 済んだタスク

1. 番号つき
2. ふたつめ

## 引用とコード

> 引用文。
> 2 行目。

```php
function example($argument) { return "test"; }
```

    インデントによるコードブロック

## 表

| 記法 | 出るもの |
|---|---|
| `**強調**` | 太字 |
| `~~打ち消し~~` | 取り消し線 |

## リンク

[通常のリンク](https://example.com/)、自動リンク https://example.com/auto 、
それから [[GoldenMaster/Top]] と [[GoldenMaster/Syntax|表示名つき]]、
相対指定の [[../Markdown]]、存在しない [[GoldenMaster/NoSuchPage]]、
見出しを指す [[GoldenMaster/Syntax#その他の記法]]。
コードの中の `[[GoldenMaster/Top]]` はリンクにならない。

長い URL が折り返されること: https://example.com/very/long/path/without/any/space/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa

## 脚注

脚注つきの文[^1]。

[^1]: これが脚注の中身。

## 生 HTML

<script>alert(1)</script>

<b>太字のつもりの HTML</b>

## 水平線

---
