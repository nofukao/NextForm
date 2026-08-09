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

上流から引き継いだ既知の問題として、web サーバーの設定によっては
`https://example.com/mywiki/storage/` のディレクトリ一覧が外から見えることが
あります。個別のファイルは `.htaccess` で 403 になりますが、一覧そのものは
`Options Indexes` が有効だと返ります。気になる場合は web サーバー側で
`Options -Indexes` を設定してください。

> この件は v0.4 のセキュリティ監査で、配布する `.htaccess` に対処を入れる予定です。

---

## 6. うまくいかないとき

| 症状 | 対処 |
|---|---|
| 画面が真っ白、500 エラー | `display_errors` が `Off` だと画面には何も出ません。サーバーのエラーログを見てください |
| 「書き込めません」と出る | 設置ディレクトリの所有者を web サーバーのユーザーに合わせてください |
| インストーラが出ずファイル一覧が見える | `.htaccess` が効いていません。Apache の `AllowOverride` を確認してください |
| 一度インストールしたのをやり直したい | `storage/` `theme/` `install-info.dat` を削除し、`index.php` を配布物のものに戻すと初期状態になります |
| 見た目が崩れている | ブラウザのキャッシュを消して再読み込みしてください (Ctrl+F5) |
