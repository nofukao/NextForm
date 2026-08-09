# 開発環境の作り方

NextForm を開発するための環境を用意する手順です。
NextForm を**使う**だけなら [../installation.md](../installation.md) を参照してください。

---

## 1. 必要なもの

| | |
|---|---|
| PHP | **8.3**。標準構成でよく、追加の拡張は不要 |
| web サーバー | Apache 2.4。`.htaccess` を使うので `AllowOverride` が有効なこと |
| その他 | `git`、`curl`、`python3`(テストの正規化に使う)、`rsync` |
| composer | **開発用のみ**。PHPUnit を入れる場合だけ |

データベースは要りません。NextForm 本体には**ランタイム依存が一切ありません**。
`vendor/` は配布物に含めない方針なので、composer で入れるものはすべて
`require-dev` に置きます。

---

## 2. PHP の設定 — 警告を見えるようにする

**これが開発環境で一番重要な設定です。** NextForm の v0.1 は
「PHP 8.3 で警告ゼロ」を目標にしており、警告が見えない環境では検証になりません。

`php.ini` を直接編集せず、専用の設定ファイルを足します。パッケージ更新で
消えないためです。

```ini
; /etc/php.d/99-nextform-dev.ini
error_reporting = E_ALL
display_errors = Off
log_errors = On
date.timezone = Asia/Tokyo
```

**`display_errors` は `Off` のままにしてください。** HTML に警告が混ざると、
ゴールデンマスターテストが本来の差分と区別できなくなります。
警告はログで確認します。

反映と確認:

```bash
sudo systemctl restart php-fpm
php -i | grep -E 'error_reporting|display_errors|date.timezone'
```

### 警告がどこに出るか

**PHP-FPM を使っている場合、Apache のエラーログには出ません。**

```bash
sudo tail -50 /var/log/php-fpm/www-error.log
```

mod_php の場合は Apache のエラーログに出ます。
どちらかは `php -i | grep 'Server API'` で分かります。
テストスクリプトには `PHP_ERROR_LOG` で場所を教えます (§5)。

---

## 3. リポジトリと実行環境

```bash
git clone git@github.com:nofukao/NextForm.git
cd NextForm
```

### 3.1 日常の開発 — PHP 内蔵サーバー

```bash
php -S 0.0.0.0:8080 -t NextForm/
```

手軽ですが、`.htaccess` が効かない、PHP-FPM 特有の挙動が出ないといった
違いがあります。仕上げの確認は Apache で行ってください。

### 3.2 統合確認 — Apache への配置

```bash
./deploy/scripts/deploy.sh          # 既定の配置先は /var/www/html/nextform
DEST=/var/www/html/mywiki ./deploy/scripts/deploy.sh --init   # 初回
```

`deploy.sh` は rsync です。要点が 2 つあります。

- `storage/` `theme/` `install-info.dat` は**除外**します (インスタンス固有のデータ)
- 2 回目以降は `index.php` も**除外**します。インストール時に認証設定が
  書き込まれているためです。初回だけ `--init` を付けます

除外パターンはすべて `/` で始めて**先頭に固定**してあります。`theme/` のように
書くと `app/theme/` にも一致してしまい、テーマのソースが配置されずに
インストーラが失敗します (実際に起きました)。

配置後は web サーバーのユーザーに所有者を合わせます。

```bash
sudo chown -R apache:apache /var/www/html/nextform
```

### 3.3 テーマを変更したときは

`app/theme/` は**テーマのソース**で、実際に配信される `theme/` は
`theme_convert()` が生成します。**ソースを変えただけではブラウザに反映されません。**

```bash
sudo -u apache php NextForm/app/tool/update_wiki /var/www/html/nextform/index.php
```

管理画面の `?option=admin_setup` で「テーマ」を無変更のまま「適用」しても
同じことが起きます。

---

## 4. 上流と比べられるようにする

判断に迷ったときに**上流の振る舞いを確認できる**と早く済みます。
素の ToraToraWiki 1.3.10 を別のパスに立てておくことを勧めます。

```bash
git archive --format=tar --prefix=tora/ upstream/1.3.10:NextForm | tar x -C /tmp
sudo mv /tmp/tora /var/www/html/tora
sudo chown -R apache:apache /var/www/html/tora
```

> ⚠️ **素の上流は PHP 8 では起動しません。** `app/util.inc` の
> `get_magic_quotes_gpc()` が PHP 8 で削除済みのため Fatal error になり、
> インストーラにも到達できません。この 1 行と
> `app/option/search.inc` の `each()` を消すと動きます。
> PHP 8 で削除済みの関数はこの 2 箇所だけです。

この参照インスタンスは**変更しないでください**。比較の基準として意味があります。

差分だけを見たいなら Git で足ります。

```bash
git diff upstream/1.3.10 HEAD -- NextForm/app/
```

---

## 5. テストの向け先を設定する

テストスクリプトは対象インスタンスの URL を必要とします。
開発環境ごとに違うので、Git に入れない設定ファイルで渡します。

```bash
cp tests/env.local.example tests/env.local
$EDITOR tests/env.local
```

```bash
: "${BASE_URL:=https://wiki.example.com/nextform}"
: "${TEST_URL:=https://wiki.example.com/nf-upgrade-test}"
: "${PHP_ERROR_LOG:=/var/log/php-fpm/www-error.log}"
```

`tests/env.local` は `.gitignore` してあります。**環境固有の値をリポジトリに
書かないでください。** 1 回だけ別の向け先にしたいときはコマンドラインで上書きできます。

```bash
BASE_URL=https://other.example.com/wiki ./tests/css-rules.sh
```

### フィクスチャの投入

ゴールデンマスターテストは、決まった内容のページが対象インスタンスに
入っていることを前提にします。

```bash
./tests/setup-fixtures.sh
```

冪等なので、壊したときの復旧にも使えます。フィクスチャは `GoldenMaster/`
配下に隔離してあるので、それ以外のページは手動テストで自由に作って構いません。

> `deploy/scripts/gen-pages.php` は wiki 本体の API を経由してページを書きます。
> `app/tool/common` の `eval_index_php()` が `index.php` の所有者と
> `getmyuid()` の一致を要求するため、**スクリプトを index.php と同じ所有者の
> 場所に複製してから** `sudo -u apache` で実行する必要があります。
> `setup-fixtures.sh` はそこまで面倒を見ています。

### 実行

```bash
./tests/golden.sh      # HTML 出力が変わっていないこと
./tests/smoke.sh       # 全 42 画面 + PHP 警告の集計 (sudo が要る)
./tests/css-rules.sh   # 生成された静的 CSS のルール検査
./tests/upgrade.sh     # 既存サイトの複製に対してアップグレードを実走 (sudo が要る)
```

各テストの狙いは [workflow.md](workflow.md) §4 にあります。

---

## 6. 性能計測用のテストデータ

`storage_page_find()` の性能はページ数に強く依存します。計測するには
まとまった数のページが要ります。

```bash
sudo cp deploy/scripts/gen-pages.php /var/www/html/nextform/
sudo chown apache:apache /var/www/html/nextform/gen-pages.php
sudo -u apache php /var/www/html/nextform/gen-pages.php \
     /var/www/html/nextform/index.php --count 500 --prefix Perf
sudo rm /var/www/html/nextform/gen-pages.php
```

生成したページは `?option=allpage` から削除できますが、数が多いと手間なので
使い捨てのインスタンスで行うほうが楽です。

---

## 7. うまくいかないとき

| 症状 | 対処 |
|---|---|
| 500 エラー、画面が真っ白 | `display_errors` が `Off` なので画面には出ません。§2 のログを見てください |
| `.htaccess` が効かない | Apache の `AllowOverride` を確認してください |
| `php_value` を書いても効かない | PHP-FPM では `.htaccess` の `php_value` は無視されます。§2 のように `/etc/php.d/` に置いてください |
| 見た目だけ古いまま | §3.3 のテーマ再生成が済んでいないか、ブラウザのキャッシュです |
| インストーラが「テーマを適用できません」 | `deploy.sh` の除外パターンが `app/theme/` まで巻き込んでいないか確認してください (§3.2) |
| `bad user. file owner id:…` | CLI ツールは `index.php` と所有者を揃える必要があります (§5) |
