# NextForm 開発フロー

> **このドキュメントの位置づけ**
> Claude Code と一緒に NextForm を開発するときの進め方を定めたもの。
> 機械可読な要約版が `CLAUDE.md` としてリポジトリルートにあり、
> Claude Code はセッション開始時にそれを自動で読み込む。
> **ルールを変えるときは、このファイルと `CLAUDE.md` の両方を更新すること。**

---

## 1. 方針 — Superpowers を使わない

当初は [Superpowers](https://github.com/obra/superpowers) プラグインの
brainstorming / writing-plans / subagent-driven-development を使う計画だったが、**採用しない**。

代わりに **Claude Code 標準機能 + `CLAUDE.md` によるルール化**で進める。

| 使うもの | 用途 |
|---|---|
| plan mode | 実装前に方針を提示させ、人間が承認する |
| TODO リスト | 複数ステップの作業の進捗管理 |
| 通常の git ブランチ | 作業の分離(worktree の自動生成はしない) |
| `docs/` 配下の設計メモ | 決定事項の記録 |
| `CLAUDE.md` | 上記の進め方を Claude に守らせる |

追加のインストールは不要で、挙動が読みやすい。

---

## 2. 基本サイクル

1 つの作業(バグ修正・機能追加・リファクタ)は次の順で進める。

```
  ① 調査        コードを読み、現状と影響範囲を報告する
                 → 人間が「そこで合っている」と確認
       ↓
  ② 設計メモ    やること・やらないこと・想定リスクを短く書き出す
                 → 人間が承認 (ここが唯一のゲート)
       ↓
  ③ ブランチ    作業単位のブランチを切る
       ↓
  ④ テスト先行  ゴールデンマスター or リグレッションテストを先に用意し、
                 「変更前の振る舞い」を固定する
       ↓
  ⑤ 実装        最小の差分で変更する
       ↓
  ⑥ 検証        テスト再実行 + error_log に警告が出ていないか確認
                 + ブラウザで実際に触る
       ↓
  ⑦ レビュー    人間が差分を確認
       ↓
  ⑧ マージ      main へマージ。リリース時は tag を打つ
```

### ルール

- **② の承認前に実装を始めない。** 調査だけを頼まれたときは調査で止まる。
- **④ を飛ばさない。** 21,800 行の手続き型コードに対して、
  「変更前の出力」を固定せずに触るのは危険。
  ただし新規追加ファイルなど、既存の振る舞いを変えないものは対象外。
- **設計メモは短くてよい。** 数行で足りるならそれでよい。
  重要な判断は `docs/project-overview.md` の §7(確定した設計判断)に追記する。
- **スコープを勝手に広げない。** 作業中に別の問題を見つけたら、
  直さずに報告する。直すかどうかは人間が決める。
- **`tora2` は変更しない。** 上流の振る舞いの基準として温存する。

---

## 3. ブランチとコミット

### ブランチ

```
main
 ├─ chore/import-upstream
 ├─ test/golden-master
 ├─ fix/php8-strftime
 ├─ docs/xxx
 └─ (v0.1 tag)
```

| プレフィックス | 用途 |
|---|---|
| `feat/` | 機能追加 |
| `fix/` | バグ修正 |
| `test/` | テストの追加・整備 |
| `docs/` | ドキュメント |
| `chore/` | ビルド・依存・雑作業 |
| `refactor/` | 振る舞いを変えない整理 |

- `main` に直接コミットしない。
- 作業が終わったら `main` にマージし、ブランチは削除してよい。
- リリース時は `git tag v0.1` のように tag を打つ。

### コミット

- **1 つの意図 = 1 コミット**。「strftime を置換」と「テストを追加」は分ける。
- 1 行目は命令形の要約(72 文字以内)。必要なら空行を挟んで理由を書く。
- 機械的な一括置換は、**手作業の判断が入った部分と分けてコミットする**
  (レビューしやすくするため)。

---

## 4. テスト戦略

### 4.1 ゴールデンマスターテスト

**変更前の出力を「正解」として固定し、変更後も同じ出力になることを確認する。**

```
tests/
├── golden/
│   ├── input/GoldenMaster/*.wiki  ← wiki 記法のサンプル (追跡する)
│   ├── expected/*.html            ← 変更前のパーサ出力 (追跡する)
│   └── actual/                    ← 実行時の出力 (.gitignore)
├── setup-fixtures.sh      ← 入力ページを対象インスタンスに投入 (冪等)
├── golden.sh              ← 取得 → 正規化 → 比較
├── smoke.sh               ← 全 option 巡回 + 警告集計
├── css-rules.sh           ← 生成された静的 CSS のルール検査
├── theme-diff.sh          ← テーマの生成物を git の ref と比べる
├── theme-lib.sh           ← テーマを生成する共通処理 (上の 2 つが source する)
└── theme-css.php          ← 設定を差し替えて 1 パターン生成する PHP
```

実行:

```bash
./tests/golden.sh            # expected/ と比較
./tests/golden.sh --update   # 現在の出力を expected/ として保存
```

手順:

1. `./tests/setup-fixtures.sh` で入力を対象インスタンスに投入する
2. **修正前**に `--update` で `expected/` を生成する(← これを最初にやる)
3. `expected/` をコミットする
4. コードを修正する
5. `./tests/golden.sh` で差分ゼロを確認する

意図した変更で差分が出た場合のみ `--update` し、**差分を目視で確認してから**
コミットする。

**現在の入力サンプル**(`tests/golden/input/GoldenMaster/`):

| ページ | 押さえている記法 |
|---|---|
| `GoldenMaster/Top` | 強調・斜体・打ち消し、内部/外部リンク、リスト(入れ子・番号付き)、表 |
| `GoldenMaster/Calendar` | `&calendar`(`strftime` 14 箇所の本丸)、`&mtime` / `&btime`、`&time` の書式指定子を 1 つずつ |
| `GoldenMaster/Syntax` | `&pre`(折り返し)、`&pre(code)` の言語指定あり/なし、`&pre(paa)`、`&calc`、`&index`、日本語の見出し |

入力ディレクトリの階層がそのままページ名の階層になる
(`input/GoldenMaster/Top.wiki` → ページ `GoldenMaster/Top`)。

> **フィクスチャは `GoldenMaster/` 配下に隔離してある。**
> それ以外のページは手動テストで自由に作成・編集してよい。
>
> 比較対象は **`<article class="main">` の中だけ**に絞ってある。ページ全体を
> 比較すると、サイド列 (`<article class="side">`) やサイトナビ
> (`<nav class="site_menu">`) が入ってしまい、Side ページを作る・マニュアル
> ページを生成するといったフィクスチャと無関係な操作で落ちる。そうなると
> 「パーサの出力が変わったのか、周辺のデータが変わっただけなのか」が区別
> できない。`article.main` の中にページツール・本文・meta がすべて入っている。
> 同じ理由で `?option=allpage` のような内容依存の一覧も対象にしないこと。
> 画面が壊れていないことは `smoke.sh` 側で見る。
>
> `setup-fixtures.sh` は投入前に既存のフィクスチャページを storage から消す。
> 上書きすると `page_write()` がバックアップを作り、「変更点」「履歴」の
> ツールリンクが disabled から有効に変わって、実行回数で出力が変わってしまうため。

> **日付の扱いに注意**: 出力にタイムスタンプが含まれると実行のたびに差分が出る。
> - `&time(...){固定日時}` のように**入力側で時刻を固定できるものは固定する**。
>   `Calendar.wiki` は 2015-05-20 09:08:07 に固定してあるので出力は決定的。
> - ページの `mtime` / `btime` のように固定できないものは `golden.sh` の
>   `normalize()` が潰す(`data-ticket`、`YYYY-MM-DD HH:MM:SS`、`compare[12]time=`)。
> - **`YYYY-MM-DD` 単独は正規化していない**。固定入力の日付と区別できなくなるため、
>   日付だけを出す `&mtime(%Y-%m-%d)` のような記法は入力サンプルに入れないこと。

### 4.2 HTTP スモークテスト

全画面を巡回して、**HTTP 200 が返り、かつ PHP の警告が出ないこと**を確認する。
`E_DEPRECATED` が見えるようになっている前提(`setup.md` §2)。

```bash
./tests/smoke.sh
```

`tests/smoke.sh` がやっていること:

1. `/var/log/php-fpm/www-error.log` の現在行数を記録する(ログは消さない)
2. `NextForm/app/option/*.inc` から option 名を拾い、全画面を GET する
3. 期待ステータス(未ログインで 401 が正しい 5 画面以外は 200)と突き合わせる
4. この巡回で新たに出た警告を発生箇所ごとに集計する

> **PHP のエラーは `/var/log/php-fpm/www-error.log` に出る。**
> Apache の `error_log` / `ssl_error_log` ではない(`setup.md` §2)。

新しい option を追加したとき、未ログインで 401 が正しい応答になるものは
`smoke.sh` の `EXPECT_401` に追記すること。

### 4.3 生成 CSS のルール検査

`app/theme/basic/style/*.css` は PHP テンプレートで、`theme_convert()` が
`theme/basic/style/main.css` を生成する。ゴールデンマスターは HTML しか見ないため
生成 CSS の崩れを検出できず、**サイドバーのレイアウトと編集画面のカーソルの
2 件を取り逃がしている**(どちらも利用者からの報告で発覚した)。

```bash
./tests/css-rules.sh              # 全テーマ (ローカル生成) + 配信中のサイト
./tests/css-rules.sh basic        # テーマを絞る
```

全体のスナップショットは取らない。フォントサイズや配色は管理画面から変更できる
ため、それだけで落ちてしまう。**過去に壊れた箇所を名指しで検査する**方式にして
ある。CSS 由来の不具合を直したら、ここに 1 行足すこと。

`theme_convert()` は**選ばれているテーマの分しか生成しない**ので、配信中の CSS を
見るだけでは他のテーマを検査できない。作業ツリーの複製に対して全テーマを生成し、
共通の項目 (`rules_common`) とテーマ固有の項目 (`rules_<テーマ名>`) を分けて見る。
**テーマを足したら `rules_<テーマ名>` を書くこと。**

生成物が変わっていないことを丸ごと確かめたいときは、別のスクリプトを使う。

```bash
./tests/theme-diff.sh             # main と現在の作業ツリーを比べる
./tests/theme-diff.sh v0.4.1      # 比べる相手を指定する
```

16 通りの設定 (7 色調・両レイアウト・左右サイド・3 種の文字サイズ・ヘッダー構成・
背景画像) で全テーマを生成して突き合わせる。共通部分の抽出やファイルの移動など、
**振る舞いを変えないはずの整理**を担保するのに使う。

> テーマのソース (`app/theme/`) を変更したら、**静的 CSS の再生成が必要**。
> 管理画面 `?option=admin_setup` の「テーマ」を無変更のまま「適用」するか、
> CLI で `theme_convert(THEME)` を呼ぶ。再生成しないとブラウザには反映されない。

### 4.4 アップグレードの検証

既存の ToraToraWiki サイトを NextForm へ更新する経路 — スクリプト
(`NextForm/app/tool/upgrade`) と手順書 (`docs/upgrade-guide.md`) の両方 — を、
実在の 2015 年代インスタンス **tora2 の複製**に対して実際に走らせる。

```bash
./tests/upgrade.sh          # KEEP=1 を付けると検証サイトを残す
```

複製元の tora2 には触らない。確認するのは、利用者のデータ (`index.php` /
`install-info.dat` / `storage/`) が変わらないこと、利用者が `app/` に置いた
プラグイン・独自テーマが残ること、静的 CSS が再生成されること、所有者が
元のままであること、`--dry-run` が本当に何も書き換えないこと。

> **手順書に書いたコマンドは、テストからそのまま実行する。**
> ドキュメントは放っておくと実物とずれる。実際これで 2 件見つかっている
> (所有者を戻す順序、`diff` の出力のロケール依存)。
> `docs/upgrade-guide.md` を変えたら `tests/upgrade.sh` も合わせること。

### 4.5 PHPUnit

composer で**開発用依存としてのみ**導入する(`require-dev`)。
`vendor/` は配布物に含めない。

```bash
composer require --dev phpunit/phpunit
./vendor/bin/phpunit tests/
```

既存の `NextForm/app/test/test`(自前ハーネス、テスト 3 件)は当面そのまま残す。
PHPUnit へ移すかどうかは v0.3 以降で判断する。

### 4.6 上流との突き合わせ

判断に迷ったら**上流の参照インスタンスで振る舞いを確認する**
(立て方は [setup.md](setup.md) §4)。
tora2 と nextform に同じ操作をして出力を比較できる。

---

## 5. デプロイ

```bash
./deploy/scripts/deploy.sh
```

中身は `setup.md` §3.2 の rsync。要点:

- `storage/` `theme/` `install-info.dat` は**除外する**(インスタンス固有データ)
- 2 回目以降は `index.php` も**除外する**(インストール後のカスタム設定が入るため)
- 配置後に `sudo chown -R apache:apache`
- `app/theme/` の CSS を変えたときは、管理画面 `?option=admin_setup` の
  「テーマ」を無変更で「適用」して静的 CSS を再生成する

---

## 6. リリース

1. 該当バージョンの全項目が終わっていることを確認する
2. `NextForm/app/version.inc` の `NEXTFORM_VERSION` を上げる
3. `./tests/golden.sh` `./tests/smoke.sh` `./tests/css-rules.sh` `./tests/upgrade.sh`
   `./tests/search-index.sh` が通ることを確認する
4. 確認用インスタンスで全機能を手動で巡回する
   (GET だけでは踏めない POST 経路 — 編集・添付・ロック・管理画面)
5. `docs/project-overview.md` のロードマップを更新する
6. `main` に tag を打つ: `git tag v0.1 && git push origin v0.1`
7. 配布物を作る: `./deploy/scripts/make-dist.sh v0.1`
8. **配布物を展開してインストールが通ることを確認する**
   リポジトリで動いていても配布物で動くとは限らない。実際 v0.1 で、
   git が空ディレクトリを追跡しないために `app/plugin/` が
   配布物から抜けているのを、この手順で見つけている
9. **配布物からのアップグレードが通ることを確認する**
   `tests/upgrade.sh` は作業ツリーを使うので、tar.gz を展開して
   そこの `app/tool/upgrade` を既存サイトの複製に対して実行し、
   `?option=admin_info` の版数が上がることまで見る

### v0.1 完成後の公開前チェック

リポジトリを public にする前に、必ず点検する:

- [ ] 資格情報がコミットに含まれていないか(パスワード、MD5 認証ダイジェスト)
- [ ] 内部のホスト名・IP アドレスが残っていないか
- [ ] バイナリ (docx / pdf / 画像) に上記が埋まっていないか。**`git grep` では
      見つからない**。実際、テスト記録の docx に管理者パスワードが入っていた
- [ ] `git log -p` と `git log --all --diff-filter=A --name-only` で過去のコミットも確認
      (public 化後は履歴も公開される)
- [ ] `LICENSE` が GPLv3 として置かれ、`README.md` に fork 経緯が明記されているか

> 内部ホスト名はドキュメント(`docs/`)には手順として必要なので残してよい。
> 問題になるのは**認証情報**と、コード中に埋め込まれた環境固有の値。
