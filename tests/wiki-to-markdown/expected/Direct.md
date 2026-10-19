# ファイルへの直接のリンク

## 文中

PDF は ![資料](./a.pdf) です。
zip は ![まとめて](./a.zip)、テキストは ![メモ](./a.txt)、md は ![読みもの](./a.md)。
画像は [写真](./a.png)、音声は [録音](./a.mp3)、動画は [動画](./a.mp4)。
まだ無いものは [無いもの](./none.pdf)、ふつうのページは [原文](/WikiMdTest/Clean)。
説明なしは ![](./a.pdf) と [a.png](./a.png)。
説明の記号は ![資料｜right \[版1\]](./a.pdf) と ![重要な資料](./a.pdf)。
raw を書いた link でも ![開く](./a.pdf)。

## 単独の段落

![単独の資料](./a.pdf)

[単独の写真](./a.png)

## 縮小画像

同じ画像 ![|40x30](./a.png)

別の画像 [![|!link](./thumb.png)](./a.png)

ページへ [![|max 10x10 !link](./thumb.png)](./a.png)

ふつうのページへ [![|!link](./thumb.png)](/WikiMdTest/Clean)

外部のサイトへ [![|!link](./thumb.png)](https://example.com/)

画像でないもの `&direct([[./a.pdf]]){&include([[./a.pdf]]);}`
