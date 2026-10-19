# ファイルへの直接のリンク

## 文中

PDF は [[./a.pdf|資料]] です。
zip は [[./a.zip|まとめて]]、テキストは [[./a.txt|メモ]]、md は [[./a.md|読みもの]]。
画像は [[./a.png|写真]]、音声は [[./a.mp3|録音]]、動画は [[./a.mp4|動画]]。
まだ無いものは [[./none.pdf|無いもの]]、ふつうのページは [[WikiMdTest/Clean|原文]]。
説明なしは [[./a.pdf]] と [[./a.png]]。
説明の記号は [資料\|right \[版1\]](./a.pdf) と [[./a.pdf|重要な資料]]。
raw を書いた link でも [[./a.pdf|開く]]。

## 単独の段落

[[./a.pdf|単独の資料]]

[[./a.png|単独の写真]]

## 縮小画像

同じ画像 [&include(\[\[./a.png\]\],40x30);](./a.png)

別の画像 [&include(\[\[./thumb.png\]\]);](./a.png)

ページへ [&include(\[\[./thumb.png\]\],max 10x10);](./a.png)

ふつうのページへ [&include(\[\[./thumb.png\]\]);](/WikiMdTest/Clean)

画像でないもの [&include(\[\[./a.pdf\]\]);](./a.pdf)
