# コマンドラインでページを読み書きする (`app/tool/page`)

> **このドキュメントの読者**
> Web の画面を使わずに、サーバーの上で NextForm のページを読んだり書いたり
> したい方。AI (Claude Code など) に作業の記録をページへ残させたい方も、
> まずここを読んでください。AI に渡す手順は
> [plugins/nextform/skills/nextform-pages/SKILL.md](../plugins/nextform/skills/nextform-pages/SKILL.md) にあります。

`app/tool/page` は **NextForm 0.17.0 から**配布物に入っています。サイトの中の
`app/tool/page` がその道具です。

---

## 1. 動かし方

```bash
php <サイト>/app/tool/page <サイト>/index.php <操作> [引数..]
```

- **サイトの所有者 (`index.php` の所有者) として動かします。** Web サーバーの
  利用者 (apache など) がサイトの所有者なら `sudo -u apache php …` のようにします。
  それ以外の利用者 (root を含む) で動かすと `not_site_owner` で断ります。
  root で書くと、Web から書き換えられないファイルが `storage/` にできるためです。
- コマンドラインからしか動きません。Web からは `.htaccess` が拒否し、道具の側でも
  コマンドライン以外では何もせずに終わります。
- 出力は **JSON** です (`read --body` を除く)。断ったときも JSON で、`error` に理由を表す語、
  `message` に説明が入ります。引数の誤りのときだけ、使い方を標準エラーに出します。

操作は 4 つです。

| 操作 | 内容 |
|---|---|
| `users` | 利用者の名前と権限の一覧 |
| `list [<ページ名>]` | ページの一覧 (名前・種別・題名・最後の更新者・更新日時) |
| `read <ページ名>` | ページの meta と本文 |
| `write <ページ名> --user <名前>` | 標準入力の本文をページに書く |

`list` と `read` は `--user <名前>` を付けると、その利用者の権限で読みます。
付けなければ、ログインしていない人の権限で読みます。`write` には `--user` が必ず要ります。

---

## 2. 操作

### users — 利用者の一覧

```bash
sudo -u apache php /var/www/html/wiki/app/tool/page /var/www/html/wiki/index.php users
```

```json
[
    {
        "name": "$default$",
        "permission": "read",
        "guest": true
    },
    {
        "name": "editor",
        "permission": "write",
        "guest": false
    }
]
```

- `$default$` は**ログインしていない人**の権限です (`guest` が `true`)。
  この名前では書けません。
- `permission` は管理ツールの「ユーザー権限」で付けたものです。ページごとに
  分けている場合は `{"Private/*": "read", "*": "write"}` のような形になります。
- **パスワードのダイジェストは出しません。** パスワードがあるかどうかも出しません。

### list — ページの一覧

```bash
sudo -u apache php …/app/tool/page …/index.php list 作業記録 --user editor
```

```json
[
    {
        "name": "作業記録",
        "type": "markdown",
        "title": "作業記録の目次",
        "last_modify_user": "editor",
        "mtime": "2026-10-28T10:15:00+09:00"
    },
    {
        "name": "作業記録/2026-10",
        "type": "markdown",
        "title": null,
        "last_modify_user": "editor",
        "mtime": "2026-10-28T10:20:00+09:00"
    }
]
```

- ページ名を渡すと、そのページと配下のページだけを出します。省くとサイト全体です。
- 読む権限の無いページは出ません。
- 題名の無いページは `title` が `null` です (画面ではページ名で出ます)。

### read — ページを読む

```bash
sudo -u apache php …/app/tool/page …/index.php read 作業記録/2026-10 --user editor
```

```json
{
    "name": "作業記録/2026-10",
    "type": "markdown",
    "title": "10 月の作業",
    "last_modify_user": "editor",
    "mtime": "2026-10-28T10:20:00+09:00",
    "meta": {
        "type": "markdown",
        "title": "10 月の作業",
        "tag": "記録",
        "btime": "1761613200",
        "ticket": "5f0c…",
        "last_modify_user": "editor",
        "create_user": "editor"
    },
    "body": "# 10 月の作業\n\n…"
}
```

- `--body` を付けると、本文だけを**そのまま**出します。手元の原稿と比べるときに使います。

  ```bash
  … read 作業記録/2026-10 --user editor --body > 読んだ.md
  diff 原稿.md 読んだ.md
  ```

- ページが無ければ、終了コード 3 と `"error": "not_found"` で終わります。
  **ページがあるかどうかは、Web の応答コードでは確かめられません**
  (無いページの URL も 200 を返し、「作る」画面を出します)。この道具で確かめてください。
- 添付ファイル (種別 file) のように本文が UTF-8 でないページは、`body` が `null` に
  なります。中身は `--body` で取れます。

### write — ページを書く

本文は標準入力から渡します。

```bash
sudo -u apache php …/app/tool/page …/index.php write 作業記録/2026-10 \
    --user editor --dry-run < 原稿.md
```

```json
{
    "name": "作業記録/2026-10",
    "action": "create",
    "type": "markdown",
    "user": "editor",
    "permission": "write",
    "written": false,
    "title": "10 月の作業",
    "tags": ["記録"],
    "same_body": null
}
```

| オプション | 内容 |
|---|---|
| `--user <名前>` | 書き手。**必須**。`users` に出る名前で、そのページを読み書きできること |
| `--type markdown\|wiki\|text` | 新しいページの種別。省くと `markdown` |
| `--overwrite` | 既にあるページを書き換えるときに付ける |
| `--dry-run` | 何も書かずに、書いたときの結果を出す |

結果の項目:

| 項目 | 内容 |
|---|---|
| `action` | `create` (新しく作る) か `overwrite` (既にあるページを書き換える) |
| `written` | 書いたかどうか。`--dry-run` では必ず `false` |
| `title` / `tags` | 本文から取り出される題名とタグ (meta に入るもの) |
| `same_body` | 既にあるページの本文と同じかどうか。新しいページは `null` |

決まり:

- **画面から保存したときと同じ扱いをします。** 本文の CR と制御文字を落とし
  (改行は LF になります)、種別ごとの整え (Markdown のフロントマターや先頭の `#`、
  Wiki 記法の `&title` から題名とタグを取る。Wiki 記法の表の桁揃え) を通してから
  保存します。同じ本文を画面から保存したときと、meta も本文も同じになります。
- **既にあるページは、`--overwrite` を付けたときだけ書き換えます。** 付けなければ
  `"error": "exists"` で断ります。`--dry-run` でも同じ判定をするので、書く前に
  「新しく作るのか、書き換えるのか」を確かめられます。
- **上書きの前の版は、必ず履歴に残します。** 画面からの保存は、同じ人が 15 分以内に
  続けて保存すると前の版を残しませんが、この道具は毎回残します。履歴の
  「この版に戻す」で戻せます。
- 既にあるページの種別は変えません。違う `--type` を渡すと `type_mismatch` で断ります。
  添付ファイル (種別 file) には書けません。
- 空の本文は断ります (画面では空で保存するとページが消えますが、この道具では消しません)。
- ロック中のページは、ロックを外せる人 (管理者か、ロックの権限を持つロックした本人)
  だけが書けます。画面と同じ判定です。
- 組み込みのマニュアル (`NextFormManual/…`) は書けません。

---

## 3. ページ名

- 階層は `/` で区切ります (`作業記録/2026-10`)。
- 先頭や末尾の `/`、`//`、`./`、`../`、制御文字を含む名前は `invalid_name` で断ります。
  画面ではこれらを整えて別の名前として扱うので、黙って直さずに断っています。
- 長い名前 (保存のディレクトリが区切られる長さ) でも、日本語でも、そのまま使えます。
- コマンドラインでは、空白や記号を含む名前を引用符で囲んでください。

---

## 4. 終了コードと理由の語

| 終了コード | 意味 | `error` |
|---|---|---|
| 0 | 成功 (`--dry-run` なら「書ける」) | — |
| 1 | 断った。何も変えていない | `unknown_user` `guest_user` `no_permission` `exists` `locked` `type_mismatch` `not_text` `manual` `interrupted` |
| 2 | 使い方の誤り | `invalid_name` `invalid_type` `empty_body` `invalid_encoding` `not_site_owner` `invalid_site` (引数の誤りは JSON なし) |
| 3 | ページが無い (`read`) | `not_found` |
| 4 | 書こうとしたが保存できなかった | `write_failed` |

| `error` | 内容 |
|---|---|
| `unknown_user` | `--user` の名前が利用者の一覧に無い。`users` で確かめてください |
| `guest_user` | `--user '$default$'` (ログインしていない人) では書けない |
| `no_permission` | その利用者には、そのページを読む (書く) 権限が無い |
| `exists` | 既にあるページ。書き換えるなら `--overwrite` |
| `locked` | ロック中で、その利用者はロックを外せない |
| `type_mismatch` | 既にあるページの種別と `--type` が違う |
| `not_text` | 添付ファイル (種別 file) のページには書けない |
| `manual` | 組み込みのマニュアルは書けない |
| `interrupted` | 途中で終わった。本文が参照するページ (`&mtime([[ページ]])` など) を、その利用者が読めないときに起きる。ふつうは書く前に止まるが、念のため `read` で確かめてください |
| `invalid_name` | ページ名に使えない形 (§3) |
| `invalid_type` | `--type` が `markdown` `wiki` `text` のどれでもない |
| `empty_body` | 本文が空 |
| `invalid_encoding` | 本文が UTF-8 でない |
| `not_site_owner` | サイトの所有者として動かしていない |
| `invalid_site` | `index.php` が無い |
| `not_found` | ページが無い |
| `write_failed` | ディスクがいっぱい、ページ名が長すぎる (保存先のパスが長すぎる) など。`message` に理由が出ます |

---

## 5. SSH の先のサイトで使う

道具はサイトのあるサーバーの上で動かします。手元から SSH で動かすときは、
本文を標準入力でそのまま送れます。

```bash
ssh -o BatchMode=yes wiki-server \
    "sudo -n -u apache php /var/www/html/wiki/app/tool/page /var/www/html/wiki/index.php \
     write '作業記録/2026-10' --user editor --dry-run" < 原稿.md
```

- ページ名は、SSH の先のシェルがもう一度読むので、**内側の引用符**で囲みます。
- `-o BatchMode=yes` と `sudo -n` は、パスワードを求められたときに待たずに失敗させる
  ためのものです。

---

## 6. 書き方の注意 (Markdown)

- **題名**: フロントマターの `title:` か、本文の先頭の `#` 見出し (本文に `#` が
  1 つだけのとき) が題名になります。

  ```markdown
  ---
  title: 10 月の作業
  tags: [記録, サーバー]
  ---
  ```

- **ページへのリンク**: ページをフォルダーと見なした URL の規則です。
  `[表示](子)` と `[表示](./子)` はそのページの子、`[表示](../兄弟)` は兄弟、
  `[表示](/作業記録/2026-10)` はトップの階層から指します。
- 添付ファイルはこの道具では書けません。画面の「添付」から付けてください。
