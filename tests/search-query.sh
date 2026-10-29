#!/bin/bash
# 検索の書き方のテスト (v0.19。docs/development/design-search-query.md)
#
#   ./tests/search-query.sh            検査する
#   ./tests/search-query.sh --update   1. の期待値 (今の書き方の結果と印) を作り直す
#
# 環境変数:
#   NF_SITE                 複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   SEARCH_QUERY_TEST_SITE  検証用に作るサイト (既定: /var/www/html/nf-search-query-test)
#   SEARCH_QUERY_TEST_URL   その URL           (既定: http://localhost/nf-search-query-test)
#   WIKI_ADMIN              管理者ユーザー名   (既定: admin)
#   PHP_ERROR_LOG           PHP のエラーログ   (既定: /var/log/php-fpm/www-error.log)
#   KEEP=1                  終了後に検証サイトを消さない
#
# 検索の画面 (?option=search) と、結果を開いたページを HTTP で見る。見るのは
# このテストが置く SearchQueryTest/ のページだけ (複製元のページは数に入れない)。
#
#   1. 今の書き方 (空白区切り・"..."・全角の空白・大文字小文字・1 文字・-rf だけ・
#      端の OR・知らない xxx:・/api/) の結果・スコア・スニペットの印と、開いたページの印が
#      変わらないこと。期待値は tests/search-query/legacy.expected (変える前の版で --update)
#   2. PHP の警告が出ない
#
# ページと権限を書き換えるので、必ず複製したサイトに対して実行する。
# 複製元には触らない。sudo が要る。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
SITE="${SEARCH_QUERY_TEST_SITE:-/var/www/html/nf-search-query-test}"
URL="${SEARCH_QUERY_TEST_URL:-http://localhost/nf-search-query-test}"
WIKI_ADMIN="${WIKI_ADMIN:-admin}"
PHP_ERROR_LOG="${PHP_ERROR_LOG:-/var/log/php-fpm/www-error.log}"
EXPECTED="${REPO_ROOT}/tests/search-query/legacy.expected"
UPDATE=0
[[ "${1:-}" == "--update" ]] && UPDATE=1

TEST_USER="searchtestuser"
TEST_PASS="$(head -c 18 /dev/urandom | base64 | tr -d '/+=')"

fail=0
total=0
WORK="$(mktemp -d)"

cleanup() {
    rm -rf "$WORK"
    if [[ "${KEEP:-0}" != "1" ]]; then
        sudo rm -rf "$SITE" 2>/dev/null
    else
        echo
        echo "KEEP=1 のため検証サイトを残しました: $SITE"
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
        "${SITE}/search-query-helper.php" "${SITE}/index.php" "$WIKI_ADMIN" "$@" 2>/dev/null
}

value_of() {
    printf '%s\n' "$1" | sed -n "s/^$2=//p" | head -1
}

urlq() {
    python3 -I -c 'import sys, urllib.parse; print(urllib.parse.quote(sys.argv[1], safe=""))' "$1"
}

# $1 URL の ? より後ろ  $2 "auth" ならログインして取る
fetch() {
    if [[ "${2:-}" == "auth" ]]; then
        # login=1 のクッキーが無いとサイトが 401 を返さず、curl が資格情報を送らない
        curl -sk --digest -u "${TEST_USER}:${TEST_PASS}" -b login=1 "${URL}/?$1"
    else
        curl -sk "${URL}/?$1"
    fi
}

# $1 問い合わせ  $2 そのほかの引数 (&manual=with など)  $3 "auth"
search_html() {
    fetch "option=search&query=$(urlq "$1")${2:-}" "${3:-}"
}

# 標準入力の検索の画面から、SearchQueryTest/ の結果のページ名を順に「,」でつなぐ
result_pages() {
    python3 -I -c '
import re, sys
html = sys.stdin.read()
m = re.search(r"<ul class=\"search_result\">(.*?)</ul>", html, re.S)
names = re.findall(r"<li><a [^>]*data-link-pagename=\"([^\"]*)\"", m.group(1)) if m else []
print(",".join(n.split("/", 1)[1] for n in names if n.startswith("SearchQueryTest/")))'
}

# 標準入力の検索の画面の結果の、マニュアルのページの数
manual_count() {
    python3 -I -c '
import re, sys
html = sys.stdin.read()
m = re.search(r"<ul class=\"search_result\">(.*?)</ul>", html, re.S)
names = re.findall(r"<li><a [^>]*data-link-pagename=\"([^\"]*)\"", m.group(1)) if m else []
print(len([n for n in names if n.startswith("NextFormManual")]))'
}

# 標準入力の検索の画面の結果の、マニュアルでないページの数
other_count() {
    python3 -I -c '
import re, sys
html = sys.stdin.read()
m = re.search(r"<ul class=\"search_result\">(.*?)</ul>", html, re.S)
names = re.findall(r"<li><a [^>]*data-link-pagename=\"([^\"]*)\"", m.group(1)) if m else []
print(len([n for n in names if not n.startswith("NextFormManual")]))'
}

# 標準入力の検索の画面から、SearchQueryTest/ の結果を 1 件 1 行にする
# (ページ名 | リンクの文字 | スコア | スニペットの HTML)。更新日時は毎回変わるので除く
result_dump() {
    python3 -I -c '
import re, sys
html = sys.stdin.read()
m = re.search(r"<ul class=\"search_result\">(.*?)</ul>", html, re.S)
for li in re.findall(r"<li>(.*?)</li>", m.group(1), re.S) if m else []:
    name = re.search(r"data-link-pagename=\"([^\"]*)\"", li).group(1)
    if not name.startswith("SearchQueryTest/"):
        continue
    text = re.search(r"<a [^>]*>(.*?)</a>", li, re.S).group(1)
    score = re.search(r"<span class=\"score\">(.*?)</span>", li, re.S)
    pre = re.search(r"<pre>(.*?)</pre>", li, re.S)
    print(" | ".join([name, text, score.group(1) if score else "", (pre.group(1) if pre else "").replace("\n", "\\n")]))'
}

# 標準入力のページから、印 (mark) を順に「class:文字」で 1 行ずつ
marks() {
    python3 -I -c '
import re, sys
for cls, text in re.findall(r"<mark class=\"([^\"]*)\">(.*?)</mark>", sys.stdin.read(), re.S):
    print("%s:%s" % (cls, text))'
}

# 標準入力のページの「検索キーワード:」の段落
notice_of() {
    grep -o '<p class="search_query">.*</p>' | head -1 | sed 's#</p>.*#</p>#'
}

contains() {
    if [[ "$1" == *"$2"* ]]; then echo yes; else echo no; fi
}

if [[ ! -d "$NF_SITE" ]]; then
    echo "複製元がありません: $NF_SITE" >&2
    exit 1
fi

echo "複製元 = $NF_SITE"
echo "検証先 = $SITE"
echo "URL    = $URL"
echo

sudo rm -rf "$SITE"
sudo cp -a "$NF_SITE" "$SITE"
SITE_OWNER=$(sudo stat -c '%U' "${SITE}/index.php")

# 複製元に配置済みのコードではなく、リポジトリの作業ツリーを検証する
sudo rsync -a --delete "${REPO_ROOT}/NextForm/app/"      "${SITE}/app/"
sudo rsync -a --delete "${REPO_ROOT}/NextForm/resource/" "${SITE}/resource/"
sudo cp "${REPO_ROOT}/tests/search-query-helper.php" "${SITE}/"
sudo chown -R "$SITE_OWNER" "${SITE}/app" "${SITE}/resource" "${SITE}/search-query-helper.php"
sudo cp -p "${SITE}/index.php" "$WORK/index.php.orig"

# opcache は revalidate_freq の間ファイルを stat し直さない (tests/markdown.sh と同じ理由)
OPCACHE_FREQ=$(php -r 'echo (int)ini_get("opcache.revalidate_freq");' 2>/dev/null)
wait_opcache() { sleep $(( ${OPCACHE_FREQ:-2} + 1 )); }

# $1 index.php の <?php の次の行に足す PHP の文 (空なら元に戻す)
index_php_with() {
    sudo cp -p "$WORK/index.php.orig" "${SITE}/index.php"
    if [[ -n "${1:-}" ]]; then
        sudo sed -i "0,/<?php/s##<?php\n$1#" "${SITE}/index.php"
    fi
    sudo touch "${SITE}/index.php"
    wait_opcache
}

[[ "$(value_of "$(helper guest read)" saved)" == "1" ]] || { echo "ゲストの権限を変えられませんでした" >&2; exit 1; }
[[ "$(value_of "$(helper set-password "$TEST_USER" "$TEST_PASS")" saved)" == "1" ]] || { echo "利用者を作れませんでした" >&2; exit 1; }
echo "索引を再構築しています.. $(helper rebuild)"
wait_opcache

if [[ "$(curl -sk -o /dev/null -w '%{http_code}' "${URL}/")" != "200" ]]; then
    echo "検証サイトが $URL で見えません。tests/env.local の SEARCH_QUERY_TEST_URL を設定してください。" >&2
    exit 1
fi

LOG_START=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | cut -d' ' -f1)

# 検索の対象。複製元のページに当たらないよう、このテストでしか使わない語 (sq…・鵺) を使う
helper write "SearchQueryTest/Alpha" markdown "---
title: アルファの手順
tags: [手順, 共有]
---

sqapple と sqbanana を使う。設定 方法を書く。

バージョン v1.2 を入れる。鵺" > /dev/null
helper write "SearchQueryTest/Beta" wiki "&title ベータの下書き
sqapple だけの段落。

下書きのメモ。コマンドは -rf を付ける。" > /dev/null
helper write "SearchQueryTest/Gamma" markdown "sqbanana と sqcherry の話。

道は /api/v1/ を使う。OR という語と title:x という文字。https://example.com/sq を見る。" > /dev/null
helper write "SearchQueryTest/Delta" wiki "SQAPPLE を大文字で書く。

全角　空白　の行。" > /dev/null
helper write "SearchQueryTest/Old" markdown "sqold 古いページ。sqapple も少し。" > /dev/null
helper write "SearchQueryTest/Tagged" markdown "---
tags: [手順書]
---

sqtag のページ。" > /dev/null
helper write "SearchQueryTest/Long" markdown "$(python3 -I -c 'print("あいうえお" * 400)')" > /dev/null
helper set-mtime "SearchQueryTest/Old" 2020-01-15 > /dev/null

echo "1. 今の書き方の結果と印が変わらない"
# 今の版では「マニュアルは除く」が ignore_system_page=on。新しい版はこれを読まず、
# 既定でマニュアルを含めない。どちらでもマニュアルを除いた結果になる
LEGACY_QUERIES=(
    "sqapple"
    "sqapple sqbanana"
    "\"設定 方法\""
    "sqapple　sqbanana"
    "SQAPPLE"
    "鵺"
    "-rf"
    "https://example.com/sq"
    "sqcherry OR"
    "/api/"
    "下書き"
    "手順"
)
{
    for q in "${LEGACY_QUERIES[@]}"; do
        echo "query: $q"
        search_html "$q" "&ignore_system_page=on" | result_dump
    done
    for target in "SearchQueryTest/Alpha|sqapple 設定" "SearchQueryTest/Beta|下書き" "SearchQueryTest/Delta|sqapple"; do
        page="${target%%|*}"
        q="${target#*|}"
        echo "page: $page query: $q"
        html=$(fetch "${page}&query=$(urlq "$q")")
        printf '%s' "$html" | notice_of
        printf '%s' "$html" | marks
    done
} > "$WORK/legacy.actual"
if [[ "$UPDATE" == "1" ]]; then
    mkdir -p "$(dirname "$EXPECTED")"
    cp "$WORK/legacy.actual" "$EXPECTED"
    echo "  期待値を作り直しました: $EXPECTED ($(wc -l < "$EXPECTED") 行)"
else
    if diff -u "$EXPECTED" "$WORK/legacy.actual" > "$WORK/legacy.diff"; then
        check_eq "今の書き方 ${#LEGACY_QUERIES[@]} 件の結果と、開いたページ 3 件の印" "同じ" "同じ"
    else
        check_eq "今の書き方 ${#LEGACY_QUERIES[@]} 件の結果と、開いたページ 3 件の印" "同じ" "違う"
        sed 's/^/        /' "$WORK/legacy.diff" | head -40
    fi
fi
echo

if [[ "$UPDATE" == "1" ]]; then
    exit 0
fi

echo "2. PHP の警告"
if [[ -n "$LOG_START" ]]; then
    warnings=$(sudo tail -n +"$(( LOG_START + 1 ))" "$PHP_ERROR_LOG" | grep -cF "$SITE")
    check_eq "この検証サイトの警告" "0" "$warnings"
    [[ "$warnings" != "0" ]] && sudo tail -n +"$(( LOG_START + 1 ))" "$PHP_ERROR_LOG" | grep -F "$SITE" | head -5 | sed 's/^/        /'
fi
echo

if [[ $fail -eq 0 ]]; then
    echo "全 ${total} 件 ok"
else
    echo "${fail} / ${total} 件 失敗"
    exit 1
fi
