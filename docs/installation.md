# インストール

NextForm を新しく設置する手順です。

**すでに ToraToraWiki を運用している場合は、ここではなく
[upgrade-guide.md](upgrade-guide.md) を参照してください。** データを引き継いだまま
NextForm にできます。

---

## 1. 必要なもの

| | |
|---|---|
| PHP | **8.3** で動作確認しています |
| web サーバー | Apache 2.4 で動作確認。`.htaccess` を使うので `AllowOverride` が有効なこと |
| データベース | **不要** |
| PHP 拡張 | 標準構成のみ。intl / bcmath などは要りません |
| ディスク | 本体は約 6 MB。あとはページと添付ファイル次第です |

設置ディレクトリに、**web サーバーからの書き込み権限**が必要です。
ページも設定もファイルとして保存するためです。

---

## 2. 入手して配置する

**[Releases](https://github.com/nofukao/NextForm/releases/latest) から
`NextForm.tar.gz` をダウンロードします。**

> ⚠️ GitHub が自動で付ける「Source code (zip / tar.gz)」ではありません。
> あちらはリポジトリ全体で、開発用のファイルを含み、`UPGRADE.md` が入らず、
> 展開先も `NextForm-<版数>/` になります。設置に使うのは、リリースに
> **添付されている `NextForm.tar.gz`** のほうです。

```bash
tar xzf NextForm.tar.gz
sudo mv NextForm /var/www/html/mywiki
sudo chown -R apache:apache /var/www/html/mywiki
```

所有者名は環境によって違います (`apache` / `www-data` など)。
`ps aux | grep -E 'httpd|apache2|php-fpm'` で確認できます。

展開した中身は次のとおりです。

```
mywiki/
├── index.php          エントリポイント
├── .htaccess          index.php 以外へのアクセスを塞ぐ
├── license.txt        GPLv3
├── UPGRADE.md         既存サイトからのアップグレード手順
├── app/               ロジック
└── resource/          JavaScript や画像
```

---

## 3. インストーラを実行する

ブラウザで設置先を開きます。

```
https://example.com/mywiki/
```

インストーラが起動するので、順に設定します。

1. **言語** — 日本語 / English
2. **サイト名**
3. **管理者** — ユーザー名とパスワード。あとから管理画面で変更できます
4. **テーマ** — v0.2.0 時点では `basic` のみです。配色は後から変えられます

完了すると `index.php` に認証設定が書き込まれ、wiki が使える状態になります。
**以後 `index.php` はこのサイト固有のファイルになります。** アップグレードの際も
上書きしません。

### 生成されるもの

インストール時に次の 3 つができます。**配布物には含まれていません。**

| | |
|---|---|
| `storage/` | ページ、添付ファイル、検索インデックス、バックアップ |
| `theme/` | 実際に配信される静的な CSS / JavaScript / 画像 |
| `install-info.dat` | サイトの設定 |

バックアップを取るときは、`storage/` と `install-info.dat` と `index.php` の
3 つが本体です。`theme/` は再生成できます。

---

## 4. 動作を確認する

- トップページが表示されること
- 編集して保存できること
- 添付ファイルをアップロードできること
- `?option=admin_info` の「バージョン」に `NextForm 0.2.0` と出ること

サーバーのエラーログに PHP の警告が出ていないことも確認してください。

```bash
# PHP-FPM の場合
sudo tail -50 /var/log/php-fpm/www-error.log
```

**NextForm は PHP 8.3 で警告が出ないことを確認しています。** 警告が出る場合は
設置環境の側に原因があるかもしれません。

---

## 5. 設置後に知っておくこと

### テーマを変えたとき

`app/theme/` は**テーマのソース**で、配信される `theme/` はそこから生成されます。
`app/theme/` のファイルを直接編集しても、そのままではブラウザに反映されません。
管理画面の「テーマ設定」を適用すると再生成されます。

### プラグインを足すとき

`app/plugin/` に置きます。アップグレードのときも、NextForm が配っていない
ファイルは削除しません。

### `storage/` の公開について

配布する `.htaccess` の先頭に `Options -Indexes` が入っています (v0.3.1 以降)。
これが無いと `https://example.com/mywiki/storage/` のディレクトリ一覧が
外から見えます。個別のファイルは以前から 403 ですが、**一覧が出ると
どんなページが存在するかは分かってしまいます**。

設置したら一度確認してください。

```bash
curl -o /dev/null -w '%{http_code}\n' https://example.com/mywiki/storage/
# 403 になれば止まっています
```

> **500 になる場合**、サーバーの `AllowOverride` が `Options` を許していません。
> `.htaccess` からその行を消して、`httpd.conf` 側の該当 `<Directory>` に
> `Options -Indexes` を書いてください。

### コードを Web サーバーに書かせない

インストーラは `index.php` を自分で書き換えて完了します。そのため多くの設置で、
**プログラムのファイルが Web サーバーの利用者 (`apache` など) の持ち物**に
なっています。この状態だと、ファイルを 1 つ書ける不具合が見つかっただけで
任意のコードを実行されます。とくに `app/plugin/*.inc` は起動のたびに
無条件で読み込まれるため、そこへ置かれると毎回走ります。

いまどうなっているかは **`?option=admin_info` の「設置」**で分かります。

```
設置
  index.php:    Web サーバーから書き込めます
  app/:         Web サーバーから書き込めます
  app/plugin/:  Web サーバーから書き込めます
```

気になる場合は、インストール完了後に**コードの所有者を Web サーバー以外に
変えます**。`storage/` と `theme/` は Web サーバーが書けなければならないので、
そこは残します。

```bash
cd /var/www/html/mywiki

# コードは別の利用者のものにし、Web サーバーには読ませるだけにする
sudo chown -R root:root index.php .htaccess license.txt app resource
sudo chmod -R go-w      index.php .htaccess license.txt app resource

# データと生成物は Web サーバーが書けるままにする
sudo chown -R apache:apache storage theme install-info.dat
```

> **この設定にすると、アップグレードは `sudo` で実行することになります。**
> `app/tool/upgrade` は `app/` `resource/` `theme/` を置き換えるためです。
> 手順は `docs/upgrade-guide.md` を参照してください。
>
> `index.php` にはサイト独自の `define()` を書きます (`?option=admin_user` が
> 出力する行など)。読み取り専用にすると、その編集にも `sudo` が要ります。

---

## 6. うまくいかないとき

| 症状 | 対処 |
|---|---|
| 画面が真っ白、500 エラー | `display_errors` が `Off` だと画面には何も出ません。サーバーのエラーログを見てください |
| 「書き込めません」と出る | 設置ディレクトリの所有者を web サーバーのユーザーに合わせてください |
| インストーラが出ずファイル一覧が見える | `.htaccess` が効いていません。Apache の `AllowOverride` を確認してください |
| 一度インストールしたのをやり直したい | `storage/` `theme/` `install-info.dat` を削除し、`index.php` を配布物のものに戻すと初期状態になります |
| 見た目が崩れている | ブラウザのキャッシュを消して再読み込みしてください (Ctrl+F5) |
