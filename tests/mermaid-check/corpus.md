---
title: 図の互換の確かめ
---

Mermaid の図の種類ごとの見本。tests/mermaid.sh (BROWSER=1) が作る。
resource/mermaid-check/compare.html が、このページと「prototype.js を読まない写し」を描き比べる。

## flowchart

```mermaid
flowchart TD
    A[調査] --> B[設計メモ]
    B --> C{利用者の承認}
    C -- 承認 --> D[ブランチを切る]
    C -- 差し戻し --> B
    subgraph 作る
      D --> E[テスト先行] --> F[実装]
    end
    F --> G((検証))
```

## sequence

```mermaid
sequenceDiagram
    autonumber
    participant A as 依頼する側
    participant B as 作る側
    A->>B: 依頼書を読んで
    loop 毎日
        B->>B: 調べる
    end
    alt 承認
        Note over B: 利用者が設計を承認
        B-->>A: 新しい版を出した
    else 差し戻し
        B-->>A: 質問
    end
```

## class

```mermaid
classDiagram
    class ページ {
        +String 名前
        +読む()
        +書く(本文)
    }
    ページ <|-- Markdownページ
    ページ "1" --> "*" 添付
```

## state

```mermaid
stateDiagram-v2
    [*] --> 下書き
    下書き --> 公開 : 承認
    公開 --> 下書き : 差し戻し
    公開 --> [*]
```

## er

```mermaid
erDiagram
    PAGE ||--o{ ATTACHMENT : has
    PAGE {
        string name
        int size
    }
```

## gantt

```mermaid
gantt
    title 予定
    dateFormat YYYY-MM-DD
    section 設計
    調査 :a1, 2026-10-01, 3d
    設計メモ :after a1, 2d
    section 実装
    実装 :2026-10-08, 5d
```

## pie

```mermaid
pie title 図の種類
    "流れ図" : 40
    "シーケンス図" : 35
    "その他" : 25
```

## journey

```mermaid
journey
    title 編集の流れ
    section 書く
      開く: 5: 利用者
      保存: 3: 利用者
```

## gitgraph

```mermaid
gitGraph
    commit
    branch feat
    commit
    checkout main
    merge feat
```

## mindmap

```mermaid
mindmap
  root((NextForm))
    Markdown
      図
      表
    Wiki記法
```

## timeline

```mermaid
timeline
    title 版の歴史
    2026-08 : v0.1
    2026-09 : v0.8
    2026-10 : v0.17
```

## quadrant

```mermaid
quadrantChart
    title 優先度
    x-axis 低い --> 高い
    y-axis 小さい --> 大きい
    A: [0.3, 0.6]
    B: [0.45, 0.23]
```

## sankey

```mermaid
sankey-beta

Input,Convert,10
Convert,Output,7
Convert,Waste,3
```

## xychart

```mermaid
xychart-beta
    title "読み込み"
    x-axis [a, b, c]
    y-axis "ms" 0 --> 100
    bar [10, 50, 80]
```

## block

```mermaid
block-beta
    columns 3
    a b c
```

## packet

```mermaid
packet-beta
    0-15: "Source Port"
    16-31: "Destination Port"
```

## kanban

```mermaid
kanban
    todo[やること]
        t1[依頼書を読む]
    done[済み]
        t2[調査]
```

## architecture

```mermaid
architecture-beta
    group api(cloud)[API]
    service db(database)[Database] in api
    service server(server)[Server] in api
    db:L -- R:server
```

## radar

```mermaid
radar-beta
    axis A, B, C
    curve c1{1, 2, 3}
```

## treemap

```mermaid
treemap-beta
"全体"
    "部分A": 10
    "部分B": 20
```

## requirement

```mermaid
requirementDiagram
    requirement r1 {
    id: 1
    text: "図を描く"
    risk: low
    verifymethod: test
    }
```

## c4

```mermaid
C4Context
    title 構成
    Person(u, "利用者")
    System(s, "NextForm")
    Rel(u, s, "使う")
```

## math

```mermaid
flowchart LR
    A["$$x^2 + y^2 = r^2$$"] --> B["$$\frac{a}{b}$$"]
```
