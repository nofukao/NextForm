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
#   8. 部分編集の範囲 (data-twp / data-twl) — ブロックごとの切り出しが原文と
#      1 バイト単位で一致すること。**ここがずれると保存で本文が壊れる**。
#      見出しの範囲が深さどおりに入れ子になること (ダブルクリックで広げる順)
#   9. 範囲を差し替えても他が変わらないこと
#  10. 続けて保存できること (「保存して編集続行」がサーバ側で頼っている前提)
#  11. 記法の例 (:::example) — ソースと表示の対、原文をそのまま控えていること、
#      囲みの記号がソース側に混ざらないこと、例の全体が部分編集の 1 単位になること
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

# helper positions の出力から 1 件取り出す。
#   $1 出力  $2 タグ名  $3 何番目 (既定 1)  $4 列 (3=位置 4=長さ 5=切り出し。既定 5)
pos_field() {
    printf '%s\n' "$1" \
        | awk -F'\t' -v tag="$2" -v n="${3:-1}" -v f="${4:-5}" \
              '$1 == "pos" && $2 == tag { c++; if(c == n) { print $f; exit } }'
}

# helper positions の出力で、ある要素を編集している欄をダブルクリックし続けた
# ときに開く要素のタグを順に並べる。nextform.js の markdownEditWiderElement()
# と同じ規則 (いまの範囲を含み、それより長いものの中でいちばん短いもの。
# 同じ長さなら文書順で先のもの) を繰り返す。最後のページ全体は数えない。
#   $1 出力  $2 タグ名  $3 何番目 (既定 1)
widen_chain() {
    printf '%s\n' "$1" | python3 -c 'import sys
tag, n = sys.argv[1], int(sys.argv[2])
items = []
for line in sys.stdin:
    f = line.rstrip("\n").split("\t")
    if len(f) >= 4 and f[0] == "pos":
        items.append((f[1], int(f[2]), int(f[3])))
current = [x for x in items if x[0] == tag][n - 1]
chain = []
while True:
    wider = None
    for x in items:
        if x[1] <= current[1] and current[1] + current[2] <= x[1] + x[2] \
           and x[2] > current[2] and (wider is None or x[2] < wider[2]):
            wider = x
    if wider is None:
        break
    chain.append(wider[0])
    current = wider
print(" ".join(chain))' "$2" "${3:-1}"
}

# ?option=partial が返す原文を、改行を \n に直して取り出す。
# ヘルパと同じ形にして比べるため。status は見ない (書き込み権限が無くても
# source は返る。ここで確かめたいのは範囲であって権限ではない)。
partial_source() {
    curl -sk "${MARKDOWN_TEST_URL}/?$1&option=partial&ticket=$2&position=$3&length=$4" \
        | python3 -c 'import json,sys
d = json.load(sys.stdin)
sys.stdout.write(d.get("source", "").replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n"))'
}

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
P_POS="MarkdownTest/Positions"
P_CRLF="MarkdownTest/Crlf"
P_EXAMPLE="MarkdownTest/Example"
P_TABLE="MarkdownTest/Table"
P_WIDEN="MarkdownTest/Widen"

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
# 属性の並び順に寄りかからない。部分編集の範囲 (data-twp) が details にも
# 付くので、'<details>' のような閉じ括弧まで込みの照合はここでは使えない。
check_eq "details が出る" "yes" "$(contains "$D_HTML" '<details')"
check_eq "  summary にラベルが入る" "yes" "$(contains "$D_HTML" '<summary>手順</summary>')"
check_eq "  既定は閉じている" "no" "$(contains "$D_HTML" 'open="open"')"
check_eq "  中身は Markdown として変換される" "yes" "$(contains "$D_HTML" '<strong>中身</strong>')"

helper write "$P_DETAILS" "$(printf -- ':::details\n中身\n:::\n')" > /dev/null
check_eq "ラベルを省くと既定の語が入る" "yes" \
         "$(contains "$(main_html "$P_DETAILS")" '<summary>詳細</summary>')"

helper write "$P_DETAILS" "$(printf -- ':::details+ 開いて出る\n中身\n:::\n')" > /dev/null
check_eq "+ で開いた状態になる" "yes" \
         "$(contains "$(main_html "$P_DETAILS")" 'open="open"')"

helper write "$P_DETAILS" "$(printf -- ':::details open 開いて出る\n中身\n:::\n')" > /dev/null
D_HTML="$(main_html "$P_DETAILS")"
check_eq "open でも開いた状態になる" "yes" "$(contains "$D_HTML" 'open="open"')"
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
         "$(contains "$(printf '%s' "$D_HTML" | tr -d '\n')" '<summary>外</summary><details')"

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
D_HTML="$(printf '%s' "$(main_html "$P_DETAILS")" | tr -d '\n')"
check_eq "閉じ忘れても文書の終わりで閉じる" "yes" \
         "$(contains "$D_HTML" '<summary>閉じ忘れ</summary><p')"
check_eq "  中身が中に入ったまま閉じる" "yes" "$(contains "$D_HTML" '中身</p></details>')"

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

echo "8. 部分編集の範囲 (data-twp / data-twl)"
# **この節が一番きつい検査**。範囲が 1 バイトずれると、?option=replace が
# 隣のブロックを巻き込んで保存し、ページが壊れる。だからブロックの種類ごとに
# 「その範囲を切り出したら原文のこれになる」を全部書き出して突き合わせる。
#
# フロントマターを付けてあるのは、行番号の起点がずれていないかを見るため
# (見出しは 5 行目 = 23 バイト目から始まる)。
#
# 本文はファイル経由で入れる。$( ) は末尾の改行を落とすので、引数で渡すと
# 「改行で終わる本文」を作れず、最後のブロックの範囲だけ 1 バイト短くなる。
POS_BODY='---\ntitle: 位置の検査\n---\n\n# 見出し 1\n\n段落その 1 です。\n**強調**もある。\n\n- 項目 A\n- 項目 B\n\n| a | b |\n|---|---|\n| 1 | 2 |\n\n```\nコード\n```\n\n> 引用文\n\n## 見出し 2\n\n節の中身。\n\n# 見出し 3\n\n終わり。\n'
POS_FIXTURE="${MARKDOWN_TEST_SITE}/pos-fixture.txt"
printf -- "$POS_BODY" | sudo tee "$POS_FIXTURE" > /dev/null
sudo chown "$SITE_OWNER" "$POS_FIXTURE"
helper write-file "$P_POS" "$POS_FIXTURE" > /dev/null
POS="$(helper positions "$P_POS")"

check_eq "段落"           '段落その 1 です。\n**強調**もある。\n' "$(pos_field "$POS" p)"
check_eq "箇条書き全体"   '- 項目 A\n- 項目 B\n'                   "$(pos_field "$POS" ul)"
check_eq "  1 つめの項目" '- 項目 A\n'                             "$(pos_field "$POS" li 1)"
check_eq "  2 つめの項目" '- 項目 B\n'                             "$(pos_field "$POS" li 2)"
check_eq "表"             '| a | b |\n|---|---|\n| 1 | 2 |\n'      "$(pos_field "$POS" table)"
check_eq "コードブロック" '```\nコード\n```\n'                     "$(pos_field "$POS" pre)"
check_eq "引用"           '> 引用文\n'                             "$(pos_field "$POS" blockquote)"

# 見出しは「その節の終わりまで」。押した所で粒度が決まるので、見出しを押すと
# 節まるごと、段落を押すとその段落になる。次の同じ深さ以上の見出しの手前で切る。
# 深い見出し (##) は節の中なので、そこでは切らない。切るのは同じ深さ以上。
check_eq "見出し 1 (次の # の手前まで)" \
         '# 見出し 1\n\n段落その 1 です。\n**強調**もある。\n\n- 項目 A\n- 項目 B\n\n| a | b |\n|---|---|\n| 1 | 2 |\n\n```\nコード\n```\n\n> 引用文\n\n## 見出し 2\n\n節の中身。\n' \
         "$(pos_field "$POS" h1 1)"
check_eq "見出し 2 (次の # の手前まで)" '## 見出し 2\n\n節の中身。\n' "$(pos_field "$POS" h2)"
check_eq "見出し 3 (文書の終わりまで)"   '# 見出し 3\n\n終わり。\n'    "$(pos_field "$POS" h1 2)"
# 節と節のあいだの空行は範囲に入れない。入れると節を保存するたびに 1 つ消える
# (二重クリックの編集は末尾の改行を 1 つだけ戻すため)
check_eq "  節の後ろの空行は入らない" "no" \
         "$(contains "$(pos_field "$POS" h2)" '節の中身。\n\n')"

# 表の行とセルには範囲を持たせない (ライブラリが行番号を持たないため)。
# 持たせないと決めたものが、うっかり付いていないことを見る。
check_eq "表の行には付かない" "" "$(pos_field "$POS" tr)"
check_eq "見出しの中の強調には付かない" "" "$(pos_field "$POS" strong)"

# ?option=partial は substr するだけだが、**画面が出した属性の値**を
# そのまま渡して同じものが返ることを、HTTP の経路でも 1 度は見ておく。
POS_TICKET="$(value_of "$(helper meta "$P_POS" ticket)" value)"
check_eq "?option=partial が同じ原文を返す" '段落その 1 です。\n**強調**もある。\n' \
         "$(partial_source "$P_POS" "$POS_TICKET" "$(pos_field "$POS" p 1 3)" "$(pos_field "$POS" p 1 4)")"

# 画面の HTML にも属性が出ていること (ヘルパだけで通って画面で出ない、を防ぐ)
check_eq "画面の HTML に属性が出る" "yes" "$(contains "$(main_html "$P_POS")" 'data-twp=')"

# 折りたたみは独自のブロックなので、描画器が属性を落としていないかを別に見る
DETAILS_FIXTURE="${MARKDOWN_TEST_SITE}/details-fixture.txt"
printf -- ':::details ラベル\n中の段落\n:::\n' | sudo tee "$DETAILS_FIXTURE" > /dev/null
sudo chown "$SITE_OWNER" "$DETAILS_FIXTURE"
helper write-file "$P_DETAILS" "$DETAILS_FIXTURE" > /dev/null
D_POS="$(helper positions "$P_DETAILS")"
check_eq "折りたたみ全体" ':::details ラベル\n中の段落\n:::\n' "$(pos_field "$D_POS" details)"
# 閉じの ::: は囲みのもの。中の段落の範囲に入っていると、その段落を
# 編集して保存したときに ::: ごと消えて折りたたみが壊れる。
check_eq "  中の段落"     '中の段落\n'                          "$(pos_field "$D_POS" p)"

# CRLF のページでも合うこと。行→バイトの表を strlen + 1 で作るので \r ごと数える。
# 本文はファイル経由で渡す。$( ) は末尾の改行を落とすので、引数で渡すと
# 「末尾が改行で終わる CRLF のページ」を作れない。
CRLF_FIXTURE="${MARKDOWN_TEST_SITE}/crlf-fixture.txt"
printf -- '# 題\r\n\r\n本文です。\r\n' | sudo tee "$CRLF_FIXTURE" > /dev/null
sudo chown "$SITE_OWNER" "$CRLF_FIXTURE"
helper write-file "$P_CRLF" "$CRLF_FIXTURE" > /dev/null
CRLF_POS="$(helper positions "$P_CRLF")"
check_eq "CRLF でも段落が合う" '本文です。\r\n' "$(pos_field "$CRLF_POS" p)"

# 表の中の [[ページ|表示名]] は、表の解析器を包み、行の中の | を逃がしてから
# 渡している (handler/markdown/table.inc)。ライブラリに渡す行を書き換えるので、
# 範囲が原文のままであること、前後の段落を巻き込まないことを見る。
#
# 段落と表のあいだには空行を置く。空行なしで段落のすぐ次の行から表を書くと、
# **同梱ライブラリ自身が**表の開始行を段落の開始行にし、段落の終わりを 0 行目に
# する (包む前から同じ)。それはこの検査の対象ではない。
TABLE_FIXTURE="${MARKDOWN_TEST_SITE}/table-fixture.txt"
printf -- '前の段落\n\n| [[P|見出し]] | b |\n|---|---|\n| [[Q|表示名]] | 2 |\n\n後の段落\n' \
    | sudo tee "$TABLE_FIXTURE" > /dev/null
sudo chown "$SITE_OWNER" "$TABLE_FIXTURE"
helper write-file "$P_TABLE" "$TABLE_FIXTURE" > /dev/null
T_POS="$(helper positions "$P_TABLE")"
check_eq "表の中の [[ページ|表示名]]" '| [[P|見出し]] | b |\n|---|---|\n| [[Q|表示名]] | 2 |\n' \
         "$(pos_field "$T_POS" table)"
check_eq "  前の段落は表に入らない" '前の段落\n' "$(pos_field "$T_POS" p 1)"
check_eq "  後の段落"               '後の段落\n' "$(pos_field "$T_POS" p 2)"

# 編集中の欄をもう一度ダブルクリックすると、範囲が外側へ一段ずつ広がる。
# Markdown の見出しは節を包む要素を持たないので、次にどれを開くかは
# nextform.js が**範囲の入れ子**で選ぶ。ここではその入れ子が見出しの深さどおりに
# なっていることを固定する (JavaScript そのものはブラウザで確かめる)。
# 項目が 1 つだけの箇条書きは、項目と同じ範囲なので飛ばす。
WIDEN_FIXTURE="${MARKDOWN_TEST_SITE}/widen-fixture.txt"
printf -- '# 章\n\n前書き。\n\n## 節\n\n- 項目 A\n- 項目 B\n\n### 小節\n\n段落です。\n\n## 次の節\n\n- ひとつだけ\n' \
    | sudo tee "$WIDEN_FIXTURE" > /dev/null
sudo chown "$SITE_OWNER" "$WIDEN_FIXTURE"
helper write-file "$P_WIDEN" "$WIDEN_FIXTURE" > /dev/null
W_POS="$(helper positions "$P_WIDEN")"
check_eq "広げる順: ### の下の段落" "h3 h2 h1" "$(widen_chain "$W_POS" p 2)"
check_eq "広げる順: 箇条書きの項目" "ul h2 h1" "$(widen_chain "$W_POS" li 1)"
check_eq "広げる順: # の直下の段落" "h1"       "$(widen_chain "$W_POS" p 1)"
check_eq "広げる順: 1 項目だけの箇条書き (同じ範囲は飛ばす)" "h2 h1" \
         "$(widen_chain "$W_POS" li 3)"

echo
echo "9. 範囲を差し替えても他が変わらないこと"
# ここが本番。JavaScript は末尾の改行を外して見せ、送るときに戻すので、
# テストも同じように末尾の改行を付けて送る。
helper write-file "$P_POS" "$POS_FIXTURE" > /dev/null
POS="$(helper positions "$P_POS")"
R="$(helper replace "$P_POS" "$(pos_field "$POS" p 1 3)" "$(pos_field "$POS" p 1 4)" $'差し替えた段落。\n')"
check_eq "保存できる" "1" "$(value_of "$R" written)"
check_eq "  段落だけが入れ替わる" \
         '---\ntitle: 位置の検査\n---\n\n# 見出し 1\n\n差し替えた段落。\n\n- 項目 A\n- 項目 B\n\n| a | b |\n|---|---|\n| 1 | 2 |\n\n```\nコード\n```\n\n> 引用文\n\n## 見出し 2\n\n節の中身。\n\n# 見出し 3\n\n終わり。\n' \
         "$(value_of "$R" contents)"

# 見出しを押したときは節ごと入れ替わる
helper write-file "$P_POS" "$POS_FIXTURE" > /dev/null
POS="$(helper positions "$P_POS")"
R="$(helper replace "$P_POS" "$(pos_field "$POS" h2 1 3)" "$(pos_field "$POS" h2 1 4)" $'## 別の節\n\n別の中身。\n')"
check_eq "節ごと入れ替わる" \
         '---\ntitle: 位置の検査\n---\n\n# 見出し 1\n\n段落その 1 です。\n**強調**もある。\n\n- 項目 A\n- 項目 B\n\n| a | b |\n|---|---|\n| 1 | 2 |\n\n```\nコード\n```\n\n> 引用文\n\n## 別の節\n\n別の中身。\n\n# 見出し 3\n\n終わり。\n' \
         "$(value_of "$R" contents)"

# 箇条書きの 1 項目だけを差し替えても、隣の項目を巻き込まないこと
helper write-file "$P_POS" "$POS_FIXTURE" > /dev/null
POS="$(helper positions "$P_POS")"
R="$(helper replace "$P_POS" "$(pos_field "$POS" li 1 3)" "$(pos_field "$POS" li 1 4)" $'- 項目 A を直した\n')"
check_eq "項目だけ入れ替わる" "yes" \
         "$(contains "$(value_of "$R" contents)" '- 項目 A を直した\n- 項目 B\n')"
echo


echo "10. 続けて保存できること (保存して編集続行)"
# 二重クリックの編集画面の「保存して編集続行」は、保存 → ページを読み直し →
# 同じ箇所を開き直す、という作りにしてある。サーバ側で起きるのは
# 「同じ位置への差し替えが 2 回続く」ことなので、そこを固定する。
#
# **読み直しが要る理由もここで固定する。** 差し替えるとその箇所の長さが変わるので、
# 古い長さのまま 2 回目を送ると隣のブロックを巻き込む。画面ごと取り直せば、
# 範囲もチケットも新しいものになる。
helper write-file "$P_POS" "$POS_FIXTURE" > /dev/null
POS="$(helper positions "$P_POS")"
P_AT="$(pos_field "$POS" p 1 3)"
P_LEN="$(pos_field "$POS" p 1 4)"
R="$(helper replace "$P_POS" "$P_AT" "$P_LEN" $'1 回目。\n')"
check_eq "1 回目が保存できる" "1" "$(value_of "$R" written)"

# 読み直すと、その段落の範囲は**書いた文字ちょうど**になっている。
# 開き直した編集画面がここを使う。
POS="$(helper positions "$P_POS")"
check_eq "読み直すと範囲が新しい本文に合う" '1 回目。\n' "$(pos_field "$POS" p)"
check_eq "  始まりは動いていない" "$P_AT" "$(pos_field "$POS" p 1 3)"
check_eq "  長さは変わっている" "no" "$(contains "$(pos_field "$POS" p 1 4)" "$P_LEN")"

R="$(helper replace "$P_POS" "$(pos_field "$POS" p 1 3)" "$(pos_field "$POS" p 1 4)" $'2 回目。\n')"
check_eq "続けて 2 回目が保存できる" "1" "$(value_of "$R" written)"
check_eq "  2 回目だけが残り、他は変わらない" \
         '---\ntitle: 位置の検査\n---\n\n# 見出し 1\n\n2 回目。\n\n- 項目 A\n- 項目 B\n\n| a | b |\n|---|---|\n| 1 | 2 |\n\n```\nコード\n```\n\n> 引用文\n\n## 見出し 2\n\n節の中身。\n\n# 見出し 3\n\n終わり。\n' \
         "$(value_of "$R" contents)"
echo

echo "11. 記法の例 (:::example)"
# wiki 記法の &wikiexample{} に当たるもの。ソースと表示を並べて出す。
# 肝は「CommonMark が囲みの原文を残さない」こと — tryContinue() で 1 行ずつ
# 控えているので、控え損ねると「ソース」側が空になる。
#
# ソース側と表示側は、同じ原文から作られた別物なので分けて見る。
example_source() { printf '%s' "$1" | sed -n '/<dt>ソース<\/dt>/,/<dt>表示<\/dt>/p'; }
example_display() { printf '%s' "$1" | sed -n '/<dt>表示<\/dt>/,$p'; }

helper write "$P_EXAMPLE" "$(printf -- ':::example\n**強調**\n\n- 項目\n:::\n')" > /dev/null
E_HTML="$(main_html "$P_EXAMPLE")"
E_SRC="$(example_source "$E_HTML")"
E_DISP="$(example_display "$E_HTML")"
check_eq "dl.example が出る" "yes" "$(contains "$E_HTML" 'class="example"')"
check_eq "  ソースの見出しが出る" "yes" "$(contains "$E_HTML" '<dt>ソース</dt>')"
check_eq "  表示の見出しが出る"   "yes" "$(contains "$E_HTML" '<dt>表示</dt>')"
check_eq "  ソース側に原文が出る" "yes" "$(contains "$E_SRC" '**強調**')"
check_eq "  ソース側で記法が効いていない" "no"  "$(contains "$E_SRC" '<strong>')"
check_eq "  ソース側に閉じの ::: が入らない" "no" "$(contains "$E_SRC" ':::')"
check_eq "  表示側は変換されている" "yes" "$(contains "$E_DISP" '<strong>強調</strong>')"
check_eq "  表示側に箇条書きが出る" "yes" "$(contains "$E_DISP" '<li>項目</li>')"

# ここが肝。ライブラリは外側のブロックから順に tryContinue() を呼ぶので、
# 素直に書くとコードブロックの中の ::: で閉じる (:::details と同じ穴)。
helper write "$P_EXAMPLE" "$(printf -- ':::example\n```\nコード\n:::\n```\n:::\n')" > /dev/null
E_HTML="$(main_html "$P_EXAMPLE")"
check_eq "コードブロックの中の ::: では閉じない" "1" \
         "$(printf '%s' "$E_HTML" | grep -o 'class="example"' | wc -l)"
check_eq "  ::: が表示側でコードとして残る" "yes" \
         "$(contains "$(example_display "$E_HTML")" '<code>')"

helper write "$P_EXAMPLE" "$(printf -- '::::example\n:::example\n中身\n:::\n::::\n')" > /dev/null
check_eq "外側のコロンを増やすと入れ子になる" "2" \
         "$(printf '%s' "$(main_html "$P_EXAMPLE")" | grep -o 'class="example"' | wc -l)"

# 囲みの記号は原文に混ざってはいけない。getRemainder() を使う理由がこれ。
helper write "$P_EXAMPLE" "$(printf -- '> :::example\n> 引用の中。\n> :::\n')" > /dev/null
E_HTML="$(main_html "$P_EXAMPLE")"
check_eq "引用の中でも囲みになる" "yes" "$(contains "$E_HTML" 'class="example"')"
check_eq "  引用の > がソースに混ざらない" "yes" "$(contains "$E_HTML" '<pre>引用の中。</pre>')"

helper write "$P_EXAMPLE" "$(printf -- '- :::example\n  箇条書きの中。\n  :::\n')" > /dev/null
E_HTML="$(main_html "$P_EXAMPLE")"
check_eq "箇条書きの中でも囲みになる" "yes" "$(contains "$E_HTML" 'class="example"')"
check_eq "  字下げがソースに混ざらない" "yes" "$(contains "$E_HTML" '<pre>箇条書きの中。</pre>')"

helper write "$P_EXAMPLE" "$(printf -- ':::exampleX ラベル\n中身\n:::\n')" > /dev/null
check_eq "区切りが無ければ反応しない" "no" \
         "$(contains "$(main_html "$P_EXAMPLE")" 'class="example"')"

helper write "$P_EXAMPLE" "$(printf -- ':::sample\n中身\n:::\n')" > /dev/null
check_eq ":::sample は囲みにならない" "no" \
         "$(contains "$(main_html "$P_EXAMPLE")" 'class="example"')"

helper write "$P_EXAMPLE" "$(printf -- ':::example\n閉じ忘れ\n')" > /dev/null
E_HTML="$(main_html "$P_EXAMPLE")"
check_eq "閉じ忘れても文書の終わりで閉じる" "yes" "$(contains "$E_HTML" 'class="example"')"
check_eq "  中身は入ったまま" "yes" "$(contains "$E_HTML" '<pre>閉じ忘れ</pre>')"

helper write "$P_EXAMPLE" "$(printf -- ':::example\n探したい言葉\n:::\n')" > /dev/null
check_eq "例の中身も検索の文字に入る" "yes" \
         "$(contains "$(helper texts "$P_EXAMPLE")" '探したい言葉')"

# 部分編集は例の全体で 1 単位。中のブロックには範囲を振らない。
# 振ると、同じ原文がソースと表示の 2 箇所に出ているのに押せるのは片方だけ、
# という妙な形になる。
# 例のあとに段落を置く。$( ) は末尾の改行を落とすので、これが無いと
# 「閉じの ::: の次の改行まで範囲に入る」ことを確かめられない。
helper write "$P_EXAMPLE" "$(printf -- ':::example\n中の段落。\n:::\n\n外の段落。\n')" > /dev/null
E_POS="$(helper positions "$P_EXAMPLE")"
check_eq "例の全体が 1 単位になる" ':::example\n中の段落。\n:::\n' "$(pos_field "$E_POS" dl)"
check_eq "  範囲を持つ段落は例の外の 1 つだけ" "1" \
         "$(printf '%s\n' "$E_POS" | awk -F'\t' '$1 == "pos" && $2 == "p"' | wc -l)"
check_eq "  それは例の外の段落" '外の段落。' "$(pos_field "$E_POS" p)"
echo

helper cleanup > /dev/null

if [[ $fail -eq 0 ]]; then
    echo "全 ${total} 件 通過"
    exit 0
fi
echo "${fail} / ${total} 件 失敗"
exit 1
