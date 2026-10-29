---
title: 図の安全の確かめ
---

誰でも書ける wiki で、図の中に書かれても困らないことを確かめる見本。
tests/mermaid.sh (BROWSER=1) が作り、resource/mermaid-check/compare.html が中を調べる。
どれを開いても、アラートが出ず、フォーム・入力欄・外部の画像・ページを覆う箱が出ないこと。

## ラベルのスクリプト

```mermaid
flowchart LR
    A["<script>window.__nfProbe = 'script'</script>スクリプト"] --> B["<img src=x onerror=&quot;window.__nfProbe='onerror'&quot;>画像の onerror"]
```

## ラベルのフォームと入力欄

```mermaid
flowchart LR
    A["<form action='https://example.invalid/'><input name=password type=password><button>送る</button></form>フォーム"]
```

## ラベルの style と外部の画像

```mermaid
flowchart LR
    A["<span style='position:fixed;top:0;left:0;width:100vw;height:100vh;background:red'>覆う箱</span>"] --> B["<img src='https://example.invalid/probe.png'>外部の画像"]
```

## javascript: のリンク

```mermaid
flowchart LR
    A[押す] --> B
    click A href "javascript:window.__nfProbe='href'" "ツールチップ"
```

## 関数を呼ぶクリック

```mermaid
flowchart LR
    A[押す]
    click A call alert("call")
```

## 図の中の設定で securityLevel を緩める

```mermaid
%%{init: {"securityLevel": "loose"}}%%
flowchart LR
    A["<img src=x onerror=&quot;window.__nfProbe='loose'&quot;>loose"]
```

## フロントマターの設定で securityLevel を緩める

```mermaid
---
config:
  securityLevel: loose
---
flowchart LR
    A["<img src=x onerror=&quot;window.__nfProbe='frontmatter'&quot;>frontmatter"]
```

## themeCSS で外部を読む・ページを覆う

```mermaid
%%{init: {"themeCSS": "svg { position: fixed; top: 0; left: 0; background: url(https://example.invalid/theme.png); }"}}%%
flowchart LR
    A[themeCSS]
```

## fontFamily で外部を読む

```mermaid
%%{init: {"fontFamily": "x; background: url(https://example.invalid/font.png)"}}%%
flowchart LR
    A[fontFamily]
```

## シーケンス図のラベル

```mermaid
sequenceDiagram
    A->>B: <img src=x onerror="window.__nfProbe='sequence'">シーケンス
    Note over A: <form><input type=password></form>ノート
```
