---
title: ゴールデンマスター (フロントマター由来の題名)
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

## 画像と添付

子ページの画像を埋め込む: ![添付の図](portforward01.png)

外部の画像はそのまま: ![外部](https://example.com/nosuch.png)

まだ無い添付はリンクになる: ![未作成の図](nosuchattachment.png)

## 脚注

脚注つきの文[^1]。

[^1]: これが脚注の中身。

## 生 HTML

<script>alert(1)</script>

<b>太字のつもりの HTML</b>

## 水平線

---

## 同じ文字の見出し

見出しの文字が重なったとき、id は先に出たほうにだけ振られる (wiki 記法と同じ)。

#### 深さが飛んだ見出し

h2 の次が h4 でも、目次では 1 段だけ深くなる。

## 同じ文字の見出し

2 つめ。目次には出るが、id は振られない。
