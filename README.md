# NextForm

**ToraToraWiki 1.3.10 を PHP 8 で動くようにした fork です。**
データベースを使わず、展開してブラウザで開くだけで動く wiki エンジンです。

---

## NextForm とは

[ToraToraWiki](http://toratora.wiki/) は、データベースを必要としない
PHP 製の wiki エンジンです。よくできたソフトウェアですが、
**2015 年の 1.3.10 (rev.1285) を最後に更新が止まりました。**

その後 PHP は 8 系に進み、ToraToraWiki は**起動すらできなくなりました**。
`get_magic_quotes_gpc()` が PHP 8 で削除されたためで、インストーラにも到達できません。

NextForm はこの状態を解消するための fork です。
**まず「今の PHP でちゃんと動く」ところまで戻すこと**を第一の目的としています。

### 受け継いでいるもの

- **データベース不要。** ページも添付ファイルもファイルとして保存します
- **展開するだけで動く。** composer も外部ライブラリも要りません
- **PHP だけで完結。** 追加の拡張モジュールは不要です

この「持ち込みの少なさ」は上流の美点なので、今後も崩さない方針です。

---

## 現在の状態 — v0.2.0

v0.1 のゴールは **「PHP 8.3 で警告ゼロで完全に動くこと」だけ**です。
バグ修正や機能追加は意図的に含めていません。土台を固めてから先に進みます。

### v0.1 でやったこと

PHP 8.3 で全画面を巡回すると **4,980 件**の警告・非推奨が出る状態から始めました。

| 分類 | 件数 | 対応 |
|---|---:|---|
| 参照渡しコールバック (`usort` に `function f(&$a, &$b)`) | 4,554 | 8 関数から `&` を外した |
| null への配列アクセス | 378 | ページツール 11 関数を修正 |
| null 引数の非推奨 (`explode` `unpack` `urlencode`) | 184 | `app/util.inc` の 3 行に集中していた |
| `strftime()` の非推奨 | 37 | `date()` ベースの互換ラッパー `nf_date()` に集約 |
| PHP 8 で削除済みの関数 | 2 | `get_magic_quotes_gpc()` / `each()` |
| その他 (非数値の演算など) | 4 | |

**結果は 0 件です。** 対象は 33 ファイル、すべて `app/` の中に収まっています。

作業中は一貫して**ゴールデンマスターテスト**で HTML 出力を突き合わせ、
**振る舞いが変わっていないこと**を確認しながら進めました。
ページの保存形式は上流から一切変えていません。

### あわせて直した表示の不具合

実運用のテストで見つかったものです。

- インストーラがサイドバーを無効化する設定を書き込み、サイドバーが本文の下に回り込む
- 編集画面で、行頭にカーソルがあると Chrome で見えなくなる
- `&pre` の長い行が折り返されず、横スクロールが必要になる

---

## 動作環境

| | |
|---|---|
| PHP | **8.3** で動作確認 (PHP-FPM) |
| web サーバー | Apache 2.4 で動作確認。`.htaccess` を使います |
| データベース | **不要** |
| PHP 拡張 | 標準構成のみ。intl / bcmath などは不要です |
| その他 | 設置ディレクトリに web サーバーからの書き込み権限が必要です |

---

## インストール

配布物を展開して web サーバーに置き、ブラウザで開くだけです。

```bash
tar xzf NextForm.tar.gz
sudo mv NextForm /var/www/html/mywiki
sudo chown -R apache:apache /var/www/html/mywiki
```

ブラウザで `https://example.com/mywiki/` を開くとインストーラが起動します。
言語・管理者・テーマを設定すると、`index.php` が書き換えられて設置完了です。

詳しい手順と設置後の注意は **[docs/installation.md](docs/installation.md)** にあります。

---

## 既存の ToraToraWiki からのアップグレード

**すでに ToraToraWiki 1.3.10 を運用している場合、そのまま NextForm にできます。**
ページも添付ファイルも設定もそのままです。保存形式を変えていないためです。

同梱のスクリプトを使う方法と、手作業で行う方法の 2 通りを用意しています。
まず `--dry-run` で、何が起きるかだけ確認できます。

```bash
php NextForm/app/tool/upgrade /var/www/html/mywiki/index.php --dry-run
```

手順の詳細は **[docs/upgrade-guide.md](docs/upgrade-guide.md)** にあります
(配布物の中では `UPGRADE.md` という名前で同梱しています)。

書き換えるのは `app/` `resource/` `license.txt` と、再生成される `theme/` だけです。
`index.php` `install-info.dat` `storage/` `.htaccess` には触れません。
利用者が `app/plugin/` に置いたプラグインや独自テーマも、削除せずそのまま残します。

---

## 配布物の構成

```
NextForm/
├── index.php          エントリポイント (インストール時に書き換わる)
├── .htaccess
├── license.txt
├── UPGRADE.md         アップグレード手順
├── app/               ロジック全部
│   ├── handler/       ページ種別 (wiki / text / file)
│   ├── option/        画面 (42 種)
│   ├── theme/         テーマの**ソース**
│   ├── plugin/        利用者が追加するプラグインの置き場
│   └── tool/          CLI ツール (upgrade / update_wiki など)
└── resource/          JavaScript や画像
```

設置後、次の 3 つが生成されます。**配布物には含まれません。**

| | |
|---|---|
| `storage/` | ページ・添付ファイル・検索インデックス |
| `theme/` | 配信される静的な CSS / JavaScript / 画像 |
| `install-info.dat` | サイトの設定 |

> `app/theme/` は**テーマのソース**で、実際に配信される `theme/` はそこから
> 生成されます。`app/theme/` を編集しただけではブラウザに反映されません。
> 管理画面の「テーマ設定」を適用するか、`app/tool/update_wiki` を実行してください。

---

## これから

| 版 | 内容 |
|---|---|
| **v0.3** | 検索の不具合修正と性能改善。ページ数が増えると一覧表示が遅くなる問題 |
| v0.4 | セキュリティ監査 (XSS / CSRF / パストラバーサル / 認証方式の見直し) |
| v0.5 | 表示まわりの設定化と、`basic` 以外のテーマ |
| v0.6 | Markdown 記法への対応 |
| v0.7 | 検索機能の高度化 |

版ごとの変更点は [CHANGELOG.md](CHANGELOG.md) にあります。

---

## 開発について

| ドキュメント | 内容 |
|---|---|
| [CONTRIBUTING.md](CONTRIBUTING.md) | 開発の進め方の要約。まずここから |
| [docs/development/project-overview.md](docs/development/project-overview.md) | 目的・上流の実構造・確定した設計判断・ロードマップ |
| [docs/development/workflow.md](docs/development/workflow.md) | 開発フローとテスト戦略 |
| [docs/development/setup.md](docs/development/setup.md) | 開発環境の作り方 |

テストは 4 本立てです。

```bash
./tests/golden.sh      # ゴールデンマスター。HTML 出力が変わっていないこと
./tests/smoke.sh       # 全 42 画面の巡回と PHP 警告の集計
./tests/css-rules.sh   # 生成された静的 CSS のルール検査
./tests/upgrade.sh     # 既存サイトの複製に対してアップグレードを実走
```

---

## 上流との関係

NextForm は**独立した fork** です。上流は 11 年間更新されていないため、
変更を送り返すことはしていません。

素の上流は Git のタグ **`upstream/1.3.10`** にあります。fork が何を変えたかは
これで確認できます。

```bash
git diff upstream/1.3.10 v0.2.0 -- NextForm/app/
```

ToraToraWiki の作者である terus 氏に感謝します。
データベースを使わずここまで作り込まれた wiki エンジンは貴重で、
NextForm はその土台の上に成り立っています。

---

## ライセンス

**GPLv3** です。上流の ToraToraWiki から継承しています。
詳細と、同梱している第三者ソフトウェア (Prototype.js / google-code-prettify /
JSColor / md5.js) のライセンスについては [LICENSE](LICENSE) を参照してください。
