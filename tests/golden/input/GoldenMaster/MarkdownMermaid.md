---
title: ゴールデンマスター (Mermaid の図)
---

図はブラウザで描く。サーバーが出すのはコードブロックのまま。

## シーケンス図

```mermaid
sequenceDiagram
    participant A as 依頼する側
    participant B as 作る側
    A->>B: 依頼書を読んで
    Note over B: 利用者が設計を承認
    B-->>A: 新しい版を出した
```

## 流れ図と数式

```mermaid
flowchart LR
    A["$$x^2$$"] --> B{分かれ道}
    B -- はい --> C[<script>alert(1)</script>]
```

## 言語名の大文字

```Mermaid
pie title 割合
    "A" : 1
    "B" : 2
```

## ふつうのコード

```php
echo '<b>mermaid</b>';
```

:::example
```mermaid
flowchart TD
    X --> Y
```
:::
