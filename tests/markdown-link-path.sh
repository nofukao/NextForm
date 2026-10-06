#!/bin/bash
# Markdown のリンク・画像の行き先の規則と、その変更を見つける道具のテスト
#
#   ./tests/markdown-link-path.sh
#
# 環境変数:
#   NF_SITE              複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   LINK_PATH_TEST_SITE  検証用に作るサイト (既定: /var/www/html/nf-link-path-test)
#   LINK_PATH_TEST_URL   その URL           (既定: http://localhost/nf-link-path-test)
#   WIKI_ADMIN           管理者ユーザー名   (既定: admin)
#   PHP_ERROR_LOG        PHP のエラーログ   (既定: /var/log/php-fpm/www-error.log)
#   KEEP=1               終了後に検証サイトを消さない
#
# Markdown の [説明](行き先) と ![説明](行き先) は、Web のリンクと同じく
# 書いた文字だけで行き先が決まる。ページがあるかどうかで変わらない。
#
#   名前        そのページの子 (= 隣に置いた添付)。./名前 と同じ
#   ./名前      そのページの子
#   ../名前     兄弟
#   /名前       トップ階層。リンクでも画像でも同じ
#
# 添付した .md の中では、.md を添付したページが「そのページ」になる。
# Markdown の中の [[...]] は Wiki 記法と同じ規則のまま (名前 = トップ階層)。
#
# 0.12.0 までは、名前をまず子として探し、無ければトップ階層として読んでいた。
# リンクの /… はページにせず、サイトの根からの URL として残していた。
# app/tool/markdown_link_check は、その違いで行き先が変わったリンクのうち、
# 直す必要のあるものを出す。
#
# ページと添付を作るので、必ず複製したサイトに対して実行する。
# 複製元には触らない。sudo が要る。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
SITE="${LINK_PATH_TEST_SITE:-/var/www/html/nf-link-path-test}"
URL="${LINK_PATH_TEST_URL:-http://localhost/nf-link-path-test}"
WIKI_ADMIN="${WIKI_ADMIN:-admin}"
PHP_ERROR_LOG="${PHP_ERROR_LOG:-/var/log/php-fpm/www-error.log}"

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
        "${SITE}/file-html-helper.php" "${SITE}/index.php" "$WIKI_ADMIN" "$@" 2>/dev/null
}

value_of() {
    printf '%s\n' "$1" | sed -n "s/^$2=//p" | head -1
}

contains() {
    if [[ "$1" == *"$2"* ]]; then echo yes; else echo no; fi
}

page_html() {
    curl -sk "${URL}/?$1"
}

# $1 HTML  $2 リンクの文字。その文字のリンクが指すページ名 (data-link-pagename)。
# ページを指さないリンク (URL のまま) なら「URL 行き先」を返す
link_target() {
    local a
    a="$(printf '%s' "$1" | grep -o "<a [^>]*>$2</a>" | head -1)"
    if [[ "$a" == *data-link-pagename=* ]]; then
        printf '%s' "$a" | sed -n 's/.*data-link-pagename="\([^"]*\)".*/\1/p'
    elif [[ -n "$a" ]]; then
        printf 'URL %s' "$(printf '%s' "$a" | sed -n 's/.*href="\([^"]*\)".*/\1/p')"
    fi
}

# $1 HTML  $2 代替テキスト。その画像が埋め込んでいるページ名
image_target() {
    printf '%s' "$1" | grep -o "<img [^>]*alt=\"$2\"[^>]*>" | head -1 \
        | sed -n 's/.*data-link-pagename="\([^"]*\)".*/\1/p'
}

# 種別 file のページを作る。$1 ページ名  $2 中身のファイル  $3 Content-type
put_file() {
    local fixture="${SITE}/fixture-$RANDOM"
    sudo cp "$2" "$fixture"
    sudo chown "$SITE_OWNER" "$fixture"
    local out
    out="$(helper write-file "$1" "$fixture" "$3")"
    sudo rm -f "$fixture"
    if [[ "$(value_of "$out" written)" != "1" ]]; then
        echo "添付のページを作れませんでした: $1" >&2
        exit 1
    fi
}

put_markdown() {
    if [[ "$(value_of "$(helper write "$1" "$2")" written)" != "1" ]]; then
        echo "ページを作れませんでした: $1" >&2
        exit 1
    fi
}

check_tool() {
    sudo -u "$SITE_OWNER" php "${SITE}/app/tool/markdown_link_check" \
        "${SITE}/index.php" --user "$WIKI_ADMIN" "$@" 2>&1
}

# storage/page の中身の指紋 (道具が書き換えないことを見る)
page_fingerprint() {
    sudo find "${SITE}/storage/page" -type f -printf '%P %s %T@\n' | sort | md5sum
}

if [[ ! -d "$NF_SITE" ]]; then
    echo "複製元がありません: $NF_SITE" >&2
    echo "tests/env.local の NF_SITE を設定してください。" >&2
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
sudo cp "${REPO_ROOT}/tests/file-html-helper.php" "${SITE}/"
sudo chown -R "$SITE_OWNER" "${SITE}/app" "${SITE}/resource" "${SITE}/file-html-helper.php"

if [[ "$(curl -sk -o /dev/null -w '%{http_code}' "${URL}/")" != "200" ]]; then
    echo "検証サイトが $URL で見えません。" >&2
    echo "tests/env.local の LINK_PATH_TEST_URL を設定してください。" >&2
    exit 1
fi

log_before=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')
log_before="${log_before:-0}"

# --- 準備 ----------------------------------------------------------------------
#
#   LinkPathTest            Markdown のページ (ここに書いたリンクを見る)
#   LinkPathTest/子.png      子の添付 (画像)
#   LinkPathTest/子.pdf      子の添付
#   LinkPathTest/両方.pdf    子の添付。トップにも同じ名前がある
#   LinkPathTest/読み物.md   子の添付 (md)。中のリンクも見る
#   LinkPathTop             トップのページ
#   両方.pdf                トップの添付
P="LinkPathTest"
T="LinkPathTop"

cp "${REPO_ROOT}/tests/golden/input/GoldenMaster/Markdown/portforward01.png" "$WORK/子.png"
printf '%%PDF-1.4\n%% 検証用\n' > "$WORK/子.pdf"
cat > "$WORK/読み物.md" <<EOF
- [md の名前]($T)
- [md の /名前](/$T)
- [md の子](子.pdf)
EOF

put_file "$P/子.png"    "$WORK/子.png"    image/png
put_file "$P/子.pdf"    "$WORK/子.pdf"    application/pdf
put_file "$P/両方.pdf"  "$WORK/子.pdf"    application/pdf
put_file "両方.pdf"     "$WORK/子.pdf"    application/pdf
put_file "$P/読み物.md" "$WORK/読み物.md" application/octet-stream
put_markdown "$T" "トップのページ"

put_markdown "$P" "$(cat <<EOF
- ![子の画像](子.png)
- [子の添付](子.pdf)
- [./ の子](./子.pdf)
- [名前だけ]($T)
- [/ の名前](/$T)
- [../ の名前](../$T)
- [まだ無い名前](まだ無いページ)
- [/ のまだ無い名前](/まだ無いトップ)
- [サイトの根](/)
- [../ だけ](../)
- ![名前だけの両方](両方.pdf)
- ![/ の両方](/両方.pdf)
- [/ の両方へのリンク](/両方.pdf)
- [[$T|wiki のリンク]]
- [? の URL](?$T)
- [外部](https://example.com/)
EOF
)"

HTML="$(page_html "$P")"

echo "1. 何も付けない名前は、そのページの子"
check_eq "画像 (子が有る)"               "$P/子.png"       "$(image_target "$HTML" '子の画像')"
check_eq "リンク (子が有る)"             "$P/子.pdf"       "$(link_target "$HTML" '子の添付')"
check_eq "トップに同じ名前があっても子"  "$P/$T"           "$(link_target "$HTML" '名前だけ')"
check_eq "まだ無いページも子を指す (押すと子として作る)" \
                                          "$P/まだ無いページ" "$(link_target "$HTML" 'まだ無い名前')"
check_eq "両方にあるときは子"            "$P/両方.pdf"     "$(link_target "$HTML" '名前だけの両方')"
echo

echo "2. ./ と ../ はこれまでどおり"
check_eq "./名前 は子"                   "$P/子.pdf"       "$(link_target "$HTML" './ の子')"
check_eq "../名前 は兄弟"                "$T"              "$(link_target "$HTML" '../ の名前')"
echo

echo "3. /名前 はトップ階層。リンクでも画像でも同じ"
check_eq "リンクの /名前"                "$T"              "$(link_target "$HTML" '/ の名前')"
check_eq "リンクの /まだ無い名前"        "まだ無いトップ"  "$(link_target "$HTML" '/ のまだ無い名前')"
check_eq "画像の /名前 (子にも同じ名前がある)" "両方.pdf"  "$(link_target "$HTML" '/ の両方')"
check_eq "リンクの /名前 (子にも同じ名前がある)" "両方.pdf" "$(link_target "$HTML" '/ の両方へのリンク')"
# トップページの名前はサイトの設定 (DEFAULT_PAGENAME) なので、../ と比べる
top_page="$(link_target "$HTML" '../ だけ')"
check_eq "/ だけは wiki のトップページ (../ と同じ)" "$top_page" "$(link_target "$HTML" 'サイトの根')"
check_eq "  URL のままにしない" "no" "$(contains "$top_page" 'URL ')"
echo

echo "4. 変わらないもの"
check_eq "Markdown の中の [[名前]] はトップ階層 (Wiki 記法と同じ)" \
                                          "$T"              "$(link_target "$HTML" 'wiki のリンク')"
check_eq "? で始まるものは URL のまま"   "URL ?$T"         "$(link_target "$HTML" '? の URL')"
check_eq "外部は URL のまま"             "URL https://example.com/" "$(link_target "$HTML" '外部')"
echo

echo "5. 添付した .md の中では、.md を添付したページが起点"
VIEW="$(page_html "$P/読み物.md&action=view")"
check_eq "名前は、添付したページの子"    "$P/$T"           "$(link_target "$VIEW" 'md の名前')"
check_eq "/名前 はトップ階層"            "$T"              "$(link_target "$VIEW" 'md の /名前')"
check_eq "隣の添付"                      "$P/子.pdf"       "$(link_target "$VIEW" 'md の子')"
echo

echo "6. markdown_link_check — 行き先が変わって直す必要のあるリンクを出す"
before="$(page_fingerprint)"
OUT="$(check_tool --list 0)"
rc=$?
after="$(page_fingerprint)"
check_eq "見つかれば終了コード 1" "1" "$rc"
check_eq "ページ・添付は書き換えない" "$before" "$after"
# 出すもの:
#   a. これまで実在するページを指していて、いまは無いページを指す
#   b. これまでと別の実在するページを指す
#   c. これまでサイトの根からの URL で、いまは無いページを指す
check_eq "a. 名前だけ → 子に無い (これまでトップに有った)" "yes" \
         "$(contains "$OUT" "	$P: [](${T}) → $P/$T (これまで $T)")"
check_eq "a. 添付した .md の中も見る" "yes" \
         "$(contains "$OUT" "	$P/読み物.md: [](${T}) → $P/$T (これまで $T)")"
check_eq "b. 画像の /名前 → トップ (これまで子)" "yes" \
         "$(contains "$OUT" "	$P: ![](/両方.pdf) → 両方.pdf (これまで $P/両方.pdf)")"
check_eq "c. リンクの /まだ無い名前 (これまで URL)" "yes" \
         "$(contains "$OUT" "	$P: [](/まだ無いトップ) → まだ無いトップ (これまで URL /まだ無いトップ)")"
# 出さないもの
check_eq "リンクの /名前 で、いまの行き先が有るものは出さない" "no" \
         "$(contains "$OUT" "[](/$T)")"
check_eq "これまでも無く、いまも無いものは出さない" "no" \
         "$(contains "$OUT" "まだ無いページ")"
check_eq "行き先が変わらないもの (子・./・../・[[ ]]) は出さない" "no" \
         "$(contains "$OUT" "子.pdf")"
check_eq "直し方を書く" "yes" "$(contains "$OUT" "先頭に / を付ける")"
echo

echo "7. 直したあとは何も出さない"
# 出たリンクを直す: トップを指したいものは / を付け、子を指したいものはそのまま
put_markdown "$P" "$(cat <<EOF
- [名前だけ](/$T)
- [/ の両方へのリンク](/両方.pdf)
- [/ のまだ無い名前](まだ無いトップ)
EOF
)"
printf -- '- [md の名前](/%s)\n' "$T" > "$WORK/読み物.md"
put_file "$P/読み物.md" "$WORK/読み物.md" application/octet-stream
OUT="$(check_tool)"
rc=$?
check_eq "終了コード 0" "0" "$rc"
check_eq "問題なしと出す" "yes" "$(contains "$OUT" "=> 問題なし")"
check_eq "--quiet なら何も出さない" "" "$(check_tool --quiet)"
echo

echo "8. PHP の警告が出ていないこと"
log_new=$(sudo tail -n +"$((log_before + 1))" "$PHP_ERROR_LOG" 2>/dev/null | grep -F "$SITE" || true)
check_eq "この検証サイトの警告が増えていない" "" "$log_new"
echo

if [[ $fail -eq 0 ]]; then
    printf '%d/%d 件すべて通りました。\n' "$total" "$total"
else
    printf '%d/%d 件が失敗しました。\n' "$fail" "$total"
fi
exit $((fail == 0 ? 0 : 1))
