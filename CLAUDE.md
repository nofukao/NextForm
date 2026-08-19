# NextForm — Claude Code 向けの規約

## セッション開始時にやること

`docs/development/project-overview.md` を読む。プロジェクトの目的・上流の実構造・
確定した設計判断がすべてそこにある。作業内容によっては
`docs/development/workflow.md`(開発フロー詳細)と
`docs/development/setup.md`(環境)も読む。

## プロジェクトの前提

- ToraToraWiki 1.3.10 (rev.1285) の fork。GPLv3 を継承。上流は 2015 年で更新停止。
- PHP の手続き型コード、約 21,800 行 / 212 ファイル。DB は使わずファイルに保存。
- **v0.1〜v0.2.0 のゴールは「PHP 8.3 で警告ゼロで完全に動く」ことだけだった**。
  バグ修正・機能追加は v0.3 以降。スコープ外の改善を勝手に始めない。

## ディレクトリ

| パス | 内容 |
|---|---|
| `NextForm/` | wiki ソース本体(= 配布単位)。`index.php` `.htaccess` `license.txt` `app/` `resource/` |
| `NextForm/app/` | ロジック全部。`handler/` `option/` `theme/`(テーマの**ソース**)`tool/` `test/` `vendor/`(同梱ライブラリ。手で編集しない) |
| `NextForm/storage/` `NextForm/theme/` | **生成物。追跡しない。編集もしない** |
| `docs/` | **利用者向け** (`installation.md` `upgrade-guide.md`)。`upgrade-guide.md` はリリース時に配布物へ `UPGRADE.md` として同梱される |
| `docs/development/` | **開発者向け** (`project-overview.md` `workflow.md` `setup.md`) |
| `tests/` | ゴールデンマスター + スモークテスト |
| `deploy/scripts/` | デプロイ、テストデータ生成 |
| `tmp/` | スクラッチ(追跡しない) |

> 上流は PukiWiki レイアウトでは**ない**。`plugin/` `skin/` `wiki/` `attach/`
> `backup/` `counter/` `diff/` は存在しない。ページは
> `storage/page/<bin2hex(ページ名)>/head` に保存される。

## 開発フロー

**① 調査 → ② 設計メモ → 人間の承認 → ③ ブランチ → ④ テスト先行 → ⑤ 実装 → ⑥ 検証 → ⑦ レビュー → ⑧ マージ**

- **②の承認前に実装を始めない。** 調査を頼まれたら調査で止まる。
- **④を飛ばさない。** 既存の振る舞いを変える変更は、先に「変更前の出力」を固定する。
- 作業中に別の問題を見つけたら、**直さずに報告する**。直すかは人間が決める。
- 設計上の決定をしたら `docs/development/project-overview.md` §7 に追記する。

## Git

- `main` に直接コミットしない。作業単位でブランチを切る。
- プレフィックス: `feat/` `fix/` `test/` `docs/` `chore/` `refactor/`
- 1 つの意図 = 1 コミット。機械的な一括置換と手作業の判断は分けてコミットする。
- コミットメッセージ 1 行目は命令形の要約(72 文字以内)。

## テスト

```bash
./tests/setup-fixtures.sh   # 入力ページを投入 (冪等。壊したときの復旧にも使う)
./tests/golden.sh           # ゴールデンマスター。--update で expected/ を更新
./tests/smoke.sh            # 全 option 巡回 + ページの経路 + PHP 警告 (警告が出たら失敗)
./tests/css-rules.sh        # 生成された静的 CSS のルール検査 (全テーマ + 配信中)
./tests/theme-diff.sh       # テーマの生成物を git の ref と比べる (既定は main)
./tests/theme-switch.sh     # テーマを切り替えると生成物ができるか (複製サイトを作る。要 sudo)
./tests/upgrade.sh          # tora2 の複製に対してアップグレードを実走 (要 sudo)
./tests/search-index.sh     # 検索インデックスの整合 (複製サイトを作る。要 sudo)
./tests/search-cache.sh     # 検索のテキストキャッシュ (複製サイトを作る。要 sudo)
./tests/csrf.sh             # CSRF 対策 (複製サイトを作る。要 sudo)
./tests/dispatch.sh         # 不正な action で 500 にならないか (複製サイトを作る。要 sudo)
```

- ゴールデンマスター(`tests/golden/`)+ HTTP スモーク(`tests/smoke.sh`)が主軸。
- フィクスチャは **`GoldenMaster/` 配下に隔離**してある。それ以外のページは
  手動テストで自由に作ってよい。内容依存の一覧 (`?option=allpage` など) を
  ゴールデンマスターの対象にしないこと。
- 大量生成は `deploy/scripts/gen-pages.php`。**スクリプトを apache 所有の場所に
  複製してから** `sudo -u apache` で実行する(`docs/development/setup.md` §6)。
- **テーマのソース (`app/theme/`) を変えたら静的 CSS の再生成が必要。**
  再生成しないとブラウザに反映されない。ゴールデンマスターは HTML しか見ないので
  CSS の崩れは `css-rules.sh` で守る。テーマ共通の部分は
  `app/theme/common/` にあり、全テーマが読む。振る舞いを変えないはずの整理は
  `theme-diff.sh` で「生成物が 1 バイトも変わらない」ことを確かめる。
- **`date()` は `strftime()` の素直な置き換えにならない**。実測で `%j` は
  `z` と 1 ずれ、`%W` は `W`(ISO-8601)と定義が違う。詳細は
  `docs/development/project-overview.md` の v0.1 の節。
- PHPUnit は **`require-dev` のみ**。`vendor/` は配布物に含めない。
- **ランタイム依存を増やさない。** 「tar.gz を展開するだけで動く」配布形式を守る。
  **例外は同梱 (`NextForm/app/vendor/`) のみ**。条件は 3 つ:
  ① 利用者に `composer install` を要求しない ② 取り込みが再現可能
  (`deploy/scripts/update-vendor.sh` + `composer.lock`) ③ 上流の脆弱性に追随し、
  NextForm のリリースに乗せて配る。`composer.json` / `composer.lock` /
  リポジトリ直下の `vendor/` は**開発用で、配布物には入らない**。
  同梱物のライセンスは `LICENSE` に列挙する (GPLv3 と両立するものだけ)。
- **`search-index.sh` は複製したサイトを作ってから走る。** 索引をわざと壊す検査を
  含むので、複製元 (`NF_SITE`) には触らない。リポジトリの作業ツリーを rsync して
  から検証するので、`deploy.sh` を先に走らせる必要はない。
- **アップグレード手順書に書いたコマンドは `tests/upgrade.sh` から実走する。**
  `docs/upgrade-guide.md`(正本)と `NextForm/app/tool/upgrade` は 1 対 1。
  片方だけ変えない。手順書を変えたらテストも合わせる。

## コーディング

- ライセンス: GPLv3(上流から継承)。
- **既存コードのスタイル(タブインデント、命名、構造)に合わせる。**
  PSR-12 への一括整形はしない。差分が読めなくなる。
- 日付処理は `date()` ベースの互換ラッパー **1 つに集約**する。
  `strftime()` は 33 箇所 / 11 ファイルにあり、`TIME_FORMAT` / `DATE_FORMAT` は
  管理画面から変更できる strftime 形式の文字列。ラッパーは
  `%-m`(桁詰め、`app/language.inc` の日本語定義で使用)にも対応が必要。
- `setlocale()` は上流で一度も呼ばれていない。ロケール依存の月名は使われていない。

## 環境

- **環境固有の値はリポジトリに書かない。** テストの向け先・ホスト名・管理者名は
  `tests/env.local`(`.gitignore` 済み)に置く。雛形は `tests/env.local.example`。
- PHP 8.3 + **PHP-FPM**(mod_php ではない)。`.htaccess` の `php_value` は効かない。
- PHP のエラーは **`/var/log/php-fpm/www-error.log`** に出る(Apache のログではない)。
  読むには sudo が必要: `sudo cat /var/log/php-fpm/www-error.log | tail -50`
  `display_errors` は `Off` のまま(HTML に警告が混ざるとゴールデンマスターが壊れる)。
- 日常開発: `php -S 0.0.0.0:8080 -t NextForm/`
- 統合確認: `deploy/scripts/deploy.sh` で Apache 配下へ配置する。
- **上流の参照インスタンスは基準。参照専用。変更しない。**(`docs/development/setup.md` §4)

## 公開について

**リポジトリは public。** コミットに**資格情報と環境固有の値を入れない**
(パスワード、MD5 認証ダイジェスト、内部のホスト名や IP)。コードにも
ドキュメントにもコミットメッセージにも書かない。一度入れると履歴から消せない。

**バイナリ (docx / pdf / 画像) に埋まっていないか注意する。** `git grep` では
見つからない。公開前に、テスト記録の docx に管理者パスワードが入っているのを
見落としかけた。リリース手順は `docs/development/workflow.md` §6。
