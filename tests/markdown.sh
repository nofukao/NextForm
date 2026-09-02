#!/bin/bash
# 種別 markdown のフロントマターのテスト
#
#   ./tests/markdown.sh
#
# 環境変数:
#   NF_SITE              複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   MARKDOWN_TEST_SITE   検証用に作るサイト (既定: /var/www/html/nf-markdown-test)
#   MARKDOWN_TEST_URL    その URL           (既定: http://localhost/nf-markdown-test)
#   WIKI_ADMIN           管理者ユーザー名   (既定: admin)
#   KEEP=1               終了後に検証サイトを消さない
#
# フロントマターは「そのページの題名とタグを本文の中で決める」ための場所で、
# **書いてあれば設定し、書いていなければ消す**。ファイルだけ見ればページの
# 状態が分かるようにするため (外のエディタと往復させる前提の記法なので)。
# ここで固定するのは次の 5 つ。
#
#   1. 閉じの --- の後ろに改行が無くてもフロントマターとして読むこと
#      (読めないと水平線 + 見出しとして本文に出てしまう)
#   2. tags: がシステムのタグになること。タグ一覧の数え上げも合うこと
#   3. title: / tags: を消すと、meta の題名とタグも消えること
#   4. メタ情報画面 / タグ画面で付けた値は、本文の保存で上書きされること
#   5. 本文が空でも <section class="markdown"> が閉じること
#      (閉じないと footer が article.main の直下から外れ、枠が出なくなる)
#   6. 種別 wiki の &title{} も同じに揃うこと (消せば題名も消える)
#   7. 折りたたみ (:::details) — 開閉の書き方、入れ子、コードブロックの中の :::、
#      折りたたんだ中身が検索と目次に載ること
#
# ページを作って消すので、必ず複製したサイトに対して実行する。
# 複製元には触らない。sudo が要る。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
MARKDOWN_TEST_SITE="${MARKDOWN_TEST_SITE:-/var/www/html/nf-markdown-test}"
MARKDOWN_TEST_URL="${MARKDOWN_TEST_URL:-http://localhost/nf-markdown-test}"
WIKI_ADMIN="${WIKI_ADMIN:-admin}"

fail=0
total=0

cleanup() {
    if [[ "${KEEP:-0}" != "1" ]]; then
        sudo rm -rf "$MARKDOWN_TEST_SITE" 2>/dev/null
    else
        echo
        echo "KEEP=1 のため検証サイトを残しました: $MARKDOWN_TEST_SITE"
    fi
}
trap cleanup EXIT

# $1 説明  $2 期待値  $3 実測値
check_eq() {
    total=$((total + 1))
    if [[ "$2" == "$3" ]]; then
        printf '  ok    %s\n' "$1"
    else
        printf '  FAIL  %s\n        期待: %s\n        実測: %s\n' "$1" "$2" "$3"
        fail=$((fail + 1))
    fi
}

helper() {
    sudo -u "$SITE_OWNER" php -d memory_limit=512M \
        "${MARKDOWN_TEST_SITE}/markdown-helper.php" \
        "${MARKDOWN_TEST_SITE}/index.php" "$WIKI_ADMIN" "$@" 2>/dev/null
}

value_of() {
    printf '%s\n' "$1" | sed -n "s/^$2=//p" | head -1
}

# 文字列が含まれるか (yes/no)。
#
# `curl | grep -q ...` と書いてはいけない。grep -q は見つけた時点で終わるので
# curl が SIGPIPE で落ち、set -o pipefail のせいでパイプライン全体が失敗と
# 扱われる。先に変数へ受けてから調べる (manual.sh と同じ)。
contains() {
    case "$1" in
        *"$2"*) echo yes ;;
        *)      echo no  ;;
    esac
}

# ページ本文だけを取り出す。footer がどこに付いたかを見たいので
# <article class="main"> の中をそのまま返す。
main_html() {
    curl -sk "${MARKDOWN_TEST_URL}/?$1" \
        | tr '\n' '\001' \
        | sed -e 's/.*<article class="main">//' -e 's;</article>.*;;' \
        | tr '\001' '\n'
}

if [[ ! -d "$NF_SITE" ]]; then
    echo "複製元がありません: $NF_SITE" >&2
    echo "tests/env.local の NF_SITE を設定してください。" >&2
    exit 1
fi

echo "複製元 = $NF_SITE"
echo "検証先 = $MARKDOWN_TEST_SITE"
echo "URL    = $MARKDOWN_TEST_URL"
echo

sudo rm -rf "$MARKDOWN_TEST_SITE"
sudo cp -a "$NF_SITE" "$MARKDOWN_TEST_SITE"
SITE_OWNER=$(sudo stat -c '%U' "${MARKDOWN_TEST_SITE}/index.php")

# 複製元に配置済みのコードではなく、リポジトリの作業ツリーを検証する
sudo rsync -a --delete "${REPO_ROOT}/NextForm/app/"      "${MARKDOWN_TEST_SITE}/app/"
sudo rsync -a --delete "${REPO_ROOT}/NextForm/resource/" "${MARKDOWN_TEST_SITE}/resource/"
sudo cp "${REPO_ROOT}/tests/markdown-helper.php" "${MARKDOWN_TEST_SITE}/"
sudo chown -R "$SITE_OWNER" "${MARKDOWN_TEST_SITE}/app" "${MARKDOWN_TEST_SITE}/resource" \
                            "${MARKDOWN_TEST_SITE}/markdown-helper.php"

# 置いたコードが PHP-FPM に載るのを待つ。
#
# opcache は validate_timestamps が有効でも、revalidate_freq の間は
# ファイルを stat し直さない。rsync の直後に curl すると **古いバイトコードの
# ままの画面**が返る。ヘルパは CLI なので新しいコードで動き、「保存した値は
# 正しいのに画面だけ直っていない」という分かりにくい落ち方をする。
OPCACHE_FREQ=$(php -r 'echo (int)ini_get("opcache.revalidate_freq");' 2>/dev/null)
sleep $(( ${OPCACHE_FREQ:-2} + 1 ))

if [[ "$(curl -sk -o /dev/null -w '%{http_code}' "${MARKDOWN_TEST_URL}/")" != "200" ]]; then
    echo "検証サイトが $MARKDOWN_TEST_URL で見えません。" >&2
    echo "tests/env.local の MARKDOWN_TEST_URL を設定してください。" >&2
    exit 1
fi

helper cleanup > /dev/null

P_TAIL="MarkdownTest/Tail"
P_TAGS="MarkdownTest/Tags"
P_BLOCK="MarkdownTest/Block"
P_DROP="MarkdownTest/Drop"
P_HAND="MarkdownTest/Hand"
P_EMPTY="MarkdownTest/Empty"
P_WIKI="MarkdownTest/Wiki"
P_DETAILS="MarkdownTest/Details"

echo "1. 閉じの --- の後ろに改行が無くても読むこと"
# printf の書式に改行を入れない。ここが本題で、末尾は --- で終わる。
check_eq "保存できる" "1" \
         "$(value_of "$(helper write "$P_TAIL" "$(printf -- '---\ntitle: 会議メモ\ntags: [議事録]\n---')")" written)"
check_eq "title: が題名になる" "会議メモ" "$(value_of "$(helper meta "$P_TAIL" title)" value)"
check_eq "tags: がタグになる"  "議事録"   "$(value_of "$(helper tags "$P_TAIL")" tags)"
TAIL_HTML="$(main_html "$P_TAIL")"
check_eq "  本文にフロントマターが出ない" "no"  "$(contains "$TAIL_HTML" 'title: 会議メモ')"
check_eq "  水平線として読まれていない"   "no"  "$(contains "$TAIL_HTML" '<hr')"
# 編集画面は本文を変換しないので、題名は meta['title'] からしか出せない。
# 保存のときに meta へ写せていないと、編集画面だけ題名が消える。
check_eq "  編集画面にも題名が出る" "yes" \
         "$(contains "$(main_html "${P_TAIL}&action=edit")" '会議メモ')"
echo

echo "2. tags: がシステムのタグになること"
check_eq "[a, b] 形式" "alpha beta" \
         "$(value_of "$(helper write "$P_TAGS" "$(printf -- '---\ntags: [alpha, beta]\n---\n\n本文\n')" > /dev/null; helper tags "$P_TAGS")" tags)"
check_eq "ブロック形式" "gamma delta" \
         "$(value_of "$(helper write "$P_BLOCK" "$(printf -- '---\ntags:\n  - gamma\n  - delta\n---\n\n本文\n')" > /dev/null; helper tags "$P_BLOCK")" tags)"
check_eq "タグ一覧の数え上げ (alpha)" "1" "$(value_of "$(helper alltag alpha)" count)"
check_eq "タグ一覧の数え上げ (gamma)" "1" "$(value_of "$(helper alltag gamma)" count)"
echo

echo "3. title: / tags: を消すと meta からも消えること"
helper write "$P_DROP" "$(printf -- '---\ntitle: 消える題名\ntags: [epsilon]\n---\n\n本文\n')" > /dev/null
check_eq "まず題名が付く" "消える題名" "$(value_of "$(helper meta "$P_DROP" title)" value)"
check_eq "まずタグが付く" "epsilon"    "$(value_of "$(helper tags "$P_DROP")" tags)"
helper write "$P_DROP" "$(printf -- '本文だけ\n')" > /dev/null
check_eq "title: を消すと題名も消える" "0" "$(value_of "$(helper meta "$P_DROP" title)" isset)"
check_eq "tags: を消すとタグも消える" ""  "$(value_of "$(helper tags "$P_DROP")" tags)"
check_eq "タグ一覧からも減る" "0" "$(value_of "$(helper alltag epsilon)" count)"
# 消した題名が検索インデックスに残らないこと。meta['title'] は索引の対象で、
# page_write() が古い ngram を数える前に消すと、消した題名で引けてしまう。
check_eq "消した題名が索引に残らない" "0" "$(value_of "$(helper index-stale "$P_DROP")" stale)"
echo

echo "4. 画面で付けた値より本文が強いこと"
helper write "$P_HAND" "$(printf -- '本文だけ\n')" > /dev/null
helper set-meta-title "$P_HAND" "画面で付けた題名" > /dev/null
helper set-tags "$P_HAND" zeta > /dev/null
check_eq "画面から題名を付けられる" "画面で付けた題名" "$(value_of "$(helper meta "$P_HAND" title)" value)"
check_eq "画面からタグを付けられる" "zeta" "$(value_of "$(helper tags "$P_HAND")" tags)"
helper write "$P_HAND" "$(printf -- '---\ntitle: 本文の題名\ntags: [eta]\n---\n\n本文\n')" > /dev/null
check_eq "本文の保存で題名が置き換わる" "本文の題名" "$(value_of "$(helper meta "$P_HAND" title)" value)"
check_eq "本文の保存でタグが置き換わる" "eta" "$(value_of "$(helper tags "$P_HAND")" tags)"
helper write "$P_HAND" "$(printf -- '本文だけ\n')" > /dev/null
check_eq "宣言を消すと画面の値も残らない" "0" "$(value_of "$(helper meta "$P_HAND" title)" isset)"
echo

echo "5. 本文が空でも section が閉じること"
# 本文を空白 1 つにしてある。$( ) は末尾の改行を落とすので、これが無いと
# 「閉じの --- の後ろに改行がある」場合を作れず、1. と同じ入力になってしまう。
check_eq "保存できる" "1" \
         "$(value_of "$(helper write "$P_EMPTY" "$(printf -- '---\ntitle: 空のページ\n---\n \n')")" written)"
EMPTY_HTML="$(main_html "$P_EMPTY")"
check_eq "section.markdown が閉じている" "yes" \
         "$(contains "$EMPTY_HTML" '<section class="markdown"></section>')"
check_eq "自己終了形になっていない" "no" \
         "$(contains "$EMPTY_HTML" '<section class="markdown"/>')"
# 閉じていれば footer は article.main の直下に戻る。
# 直下かどうかは「footer より前で section.page が閉じているか」で見る。
check_eq "footer の前で section.page が閉じている" "yes" \
         "$(contains "$(printf '%s' "$EMPTY_HTML" | tr -d '\n' | sed 's/<footer>.*//')" '</section></section>')"
echo

echo "6. 種別 wiki の &title{} も同じに揃うこと"
check_eq "保存できる" "1" \
         "$(value_of "$(helper write-wiki "$P_WIKI" "$(printf -- '&title{wiki の題名};\n*見出し\n本文\n')")" written)"
check_eq "&title{} が題名になる" "wiki の題名" "$(value_of "$(helper meta "$P_WIKI" title)" value)"
helper write-wiki "$P_WIKI" "$(printf -- '*見出し\n本文\n')" > /dev/null
check_eq "&title{} を消すと題名も消える" "0" "$(value_of "$(helper meta "$P_WIKI" title)" isset)"
check_eq "消した題名が索引に残らない" "0" "$(value_of "$(helper index-stale "$P_WIKI")" stale)"

# メタ情報の画面で付けた題名も、本文を保存すると置き換わる (markdown と同じ)
helper set-meta-title "$P_WIKI" "画面で付けた題名" > /dev/null
check_eq "画面から題名を付けられる" "画面で付けた題名" "$(value_of "$(helper meta "$P_WIKI" title)" value)"
helper write-wiki "$P_WIKI" "$(printf -- '*見出し\n別の本文\n')" > /dev/null
check_eq "本文を保存すると画面の値も残らない" "0" "$(value_of "$(helper meta "$P_WIKI" title)" isset)"
echo

echo "7. 折りたたみ (:::details)"
helper write "$P_DETAILS" "$(printf -- ':::details 手順\n**中身**\n:::\n')" > /dev/null
D_HTML="$(main_html "$P_DETAILS")"
check_eq "details が出る" "yes" "$(contains "$D_HTML" '<details>')"
check_eq "  summary にラベルが入る" "yes" "$(contains "$D_HTML" '<summary>手順</summary>')"
check_eq "  既定は閉じている" "no" "$(contains "$D_HTML" '<details open')"
check_eq "  中身は Markdown として変換される" "yes" "$(contains "$D_HTML" '<strong>中身</strong>')"

helper write "$P_DETAILS" "$(printf -- ':::details\n中身\n:::\n')" > /dev/null
check_eq "ラベルを省くと既定の語が入る" "yes" \
         "$(contains "$(main_html "$P_DETAILS")" '<summary>詳細</summary>')"

helper write "$P_DETAILS" "$(printf -- ':::details+ 開いて出る\n中身\n:::\n')" > /dev/null
check_eq "+ で開いた状態になる" "yes" \
         "$(contains "$(main_html "$P_DETAILS")" '<details open="open">')"

helper write "$P_DETAILS" "$(printf -- ':::details open 開いて出る\n中身\n:::\n')" > /dev/null
D_HTML="$(main_html "$P_DETAILS")"
check_eq "open でも開いた状態になる" "yes" "$(contains "$D_HTML" '<details open="open">')"
check_eq "  open はラベルに残らない" "yes" "$(contains "$D_HTML" '<summary>開いて出る</summary>')"

# open で始まるラベルを書きたいときの逃げ道。マニュアルにも書いてある
helper write "$P_DETAILS" "$(printf -- ':::details+ open な話\n中身\n:::\n')" > /dev/null
check_eq "+ を使えば open で始まるラベルも書ける" "yes" \
         "$(contains "$(main_html "$P_DETAILS")" '<summary>open な話</summary>')"

helper write "$P_DETAILS" "$(printf -- ':::detailsX ラベル\n中身\n:::\n')" > /dev/null
check_eq "ラベルの前に区切りが無ければ反応しない" "no" \
         "$(contains "$(main_html "$P_DETAILS")" '<details')"

helper write "$P_DETAILS" "$(printf -- '::::details 外\n:::details 内\n中身\n:::\n::::\n')" > /dev/null
D_HTML="$(main_html "$P_DETAILS")"
check_eq "外側のコロンを増やすと入れ子になる" "2" \
         "$(printf '%s' "$D_HTML" | grep -o '<details' | wc -l)"
check_eq "  内側が外側の中に入る" "yes" \
         "$(contains "$(printf '%s' "$D_HTML" | tr -d '\n')" '<summary>外</summary><details><summary>内</summary>')"

# ここが肝。ライブラリは外側のブロックから順に tryContinue() を呼ぶので、
# 素直に書くとコードブロックの中の ::: で両方まとめて閉じる。
helper write "$P_DETAILS" "$(printf -- ':::details 例\n```\n:::\n```\n:::\n')" > /dev/null
D_HTML="$(main_html "$P_DETAILS")"
check_eq "コードブロックの中の ::: では閉じない" "1" \
         "$(printf '%s' "$D_HTML" | grep -o '<details' | wc -l)"
check_eq "  ::: はコードとして残る" "yes" "$(contains "$D_HTML" '<code>:::')"

helper write "$P_DETAILS" "$(printf -- ':::details 例\n    :::\n:::\n')" > /dev/null
check_eq "字下げした ::: では閉じない" "1" \
         "$(printf '%s' "$(main_html "$P_DETAILS")" | grep -o '<details' | wc -l)"

helper write "$P_DETAILS" "$(printf -- ':::details 閉じ忘れ\n中身\n')" > /dev/null
check_eq "閉じ忘れても文書の終わりで閉じる" "yes" \
         "$(contains "$(printf '%s' "$(main_html "$P_DETAILS")" | tr -d '\n')" '<summary>閉じ忘れ</summary><p>中身</p></details>')"

helper write "$P_DETAILS" "$(printf -- ':::note 注意\n中身\n:::\n')" > /dev/null
D_HTML="$(main_html "$P_DETAILS")"
check_eq ":::note は折りたたみにならない" "no" "$(contains "$D_HTML" '<details')"
check_eq "  そのまま文字として出る" "yes" "$(contains "$D_HTML" ':::note 注意')"

helper write "$P_DETAILS" "$(printf -- ':::details ラベル\n隠れた言葉\n:::\n')" > /dev/null
check_eq "折りたたんだ中身も検索の文字に入る" "yes" \
         "$(contains "$(helper texts "$P_DETAILS")" '隠れた言葉')"

helper write "$P_DETAILS" "$(printf -- ':::details ラベル\n# 中の見出し\n:::\n')" > /dev/null
check_eq "中の見出しは目次に出る" "yes" \
         "$(contains "$(main_html "${P_DETAILS}&option=summary")" '中の見出し')"
echo

helper cleanup > /dev/null

if [[ $fail -eq 0 ]]; then
    echo "全 ${total} 件 通過"
    exit 0
fi
echo "${fail} / ${total} 件 失敗"
exit 1
