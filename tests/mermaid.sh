#!/bin/bash
# Markdown のページの Mermaid の図のテスト
#
#   ./tests/mermaid.sh               サーバー側を検査して、検証サイトを消す
#   BROWSER=1 ./tests/mermaid.sh     加えてブラウザで見るページを作り、検証サイトを残す
#
# 環境変数:
#   NF_SITE             複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   MERMAID_TEST_SITE   検証用に作るサイト (既定: /var/www/html/nf-mermaid-test)
#   MERMAID_TEST_URL    その URL           (既定: http://localhost/nf-mermaid-test)
#   WIKI_ADMIN          管理者ユーザー名   (既定: admin)
#   PHP_ERROR_LOG       PHP のエラーログ   (既定: /var/log/php-fpm/www-error.log)
#   KEEP=1              終了後に検証サイトを消さない
#
# 図を描くのはブラウザ (resource/script/diagram.js) で、サーバーは本文の
# コードブロックをそのまま出し、図のあるページにだけ <head> に 2 行足す。
# ここで固定するのはサーバー側の次のこと。
#
#   1. 図のあるページの <head> に diagram.js と <meta name="nextform-mermaid"> が
#      1 つずつ入る。図の無いページには入らず、<head> はその 2 行の分しか違わない
#   2. 図の中の文字はエスケープしたまま (<script> がそのまま出ない)
#   3. プレビュー・取り込み (![](ページ))・添付した md・:::example の中・言語名の
#      大文字 (```Mermaid) でも入る。種別 wiki の &pre(code,mermaid) には入らない
#   4. サイト設定 MARKDOWN_DIAGRAM を false にすると入らない
#   5. <meta> の URL で Mermaid が JavaScript として取れ、ファイルが変わったときだけ
#      URL が変わる (更新時刻の印が付く)。sha512 が deploy/scripts/mermaid.lock と合う
#   6. PHP の警告が出ない
#
# 描画そのもの (prototype.js と一緒に描けるか、安全、誤りの出し方、部分編集) は
# ブラウザが要るので、BROWSER=1 のときに見るページを作って URL を出す。
#
# ページを作って設定を変えるので、必ず複製したサイトに対して実行する。
# 複製元には触らない。sudo が要る。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
SITE="${MERMAID_TEST_SITE:-/var/www/html/nf-mermaid-test}"
URL="${MERMAID_TEST_URL:-http://localhost/nf-mermaid-test}"
WIKI_ADMIN="${WIKI_ADMIN:-admin}"
PHP_ERROR_LOG="${PHP_ERROR_LOG:-/var/log/php-fpm/www-error.log}"
[[ "${BROWSER:-0}" == "1" ]] && KEEP=1

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

mermaid_helper() {
    sudo -u "$SITE_OWNER" php -d memory_limit=512M \
        "${SITE}/mermaid-helper.php" "${SITE}/index.php" "$WIKI_ADMIN" "$@" 2>/dev/null
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

# $1 HTML。<head> の中身だけを返す
head_of() {
    printf '%s' "$1" | tr '\n' '\r' | sed -n 's/.*<head>\(.*\)<\/head>.*/\1/p' | tr '\r' '\n'
}

# $1 <head>。<title> を除いたもの (題名にはページ名が入り、ページごとに違う)
without_title() {
    printf '%s' "$1" | sed 's#<title>.*</title>##'
}

# $1 <head>。図のために足すもの (diagram.js と <meta>) の数を「script,meta」で返す
diagram_tags() {
    local scripts metas
    scripts=$(printf '%s' "$1" | grep -o '<script[^>]*src="resource/script/diagram\.js[^"]*"' | wc -l)
    metas=$(printf '%s' "$1" | grep -o '<meta name="nextform-mermaid"[^>]*>' | wc -l)
    echo "${scripts// /},${metas// /}"
}

# $1 <head>。図のために足すものを抜いた <head> (図の無いページと比べる)
without_diagram_tags() {
    printf '%s' "$1" \
        | sed -e 's#<script[^>]*src="resource/script/diagram\.js[^"]*"[^>]*></script>##' \
              -e 's#<meta name="nextform-mermaid"[^>]*>##'
}

# $1 <head>。<meta name="nextform-mermaid"> の属性 $2 の値
meta_attr() {
    printf '%s' "$1" | grep -o '<meta name="nextform-mermaid"[^>]*>' | head -1 \
        | sed -n "s/.* $2=\"\([^\"]*\)\".*/\1/p" \
        | php -r 'echo html_entity_decode(stream_get_contents(STDIN), ENT_QUOTES);'
}

# 種別 file のページを作る。$1 ページ名  $2 中身のファイル  $3 Content-type
put_file() {
    local fixture="${SITE}/fixture-$total-$RANDOM"
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

write_page() {
    local out
    out="$(helper "$1" "$2" "$3")"
    if [[ "$(value_of "$out" written)" != "1" ]]; then
        echo "ページを作れませんでした: $2" >&2
        exit 1
    fi
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
sudo cp "${REPO_ROOT}/tests/file-html-helper.php" "${REPO_ROOT}/tests/mermaid-helper.php" "${SITE}/"
sudo chown -R "$SITE_OWNER" "${SITE}/app" "${SITE}/resource" \
     "${SITE}/file-html-helper.php" "${SITE}/mermaid-helper.php"

if [[ "$(curl -sk -o /dev/null -w '%{http_code}' "${URL}/")" != "200" ]]; then
    echo "検証サイトが $URL で見えません。" >&2
    echo "tests/env.local の MERMAID_TEST_URL を設定してください。" >&2
    exit 1
fi

log_before=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')
log_before="${log_before:-0}"

# --- 準備 --------------------------------------------------------------------
P="MermaidTest"

DIAGRAM='```mermaid
sequenceDiagram
    participant A as 依頼する側
    A->>A: <script>alert(1)</script>
```'
PLAIN='```text
sequenceDiagram
    participant A as 依頼する側
    A->>A: <script>alert(1)</script>
```'

# 図のあるページと、同じ題名で図だけ抜いたページ (<head> を比べる)
write_page write "$P/図あり" "$(printf -- '---\ntitle: 図の検査\n---\n\n%s\n' "$DIAGRAM")"
write_page write "$P/図なし" "$(printf -- '---\ntitle: 図の検査\n---\n\n%s\n' "$PLAIN")"
write_page write "$P/大文字" "$(printf '```Mermaid\npie title 割合\n    "A" : 1\n```\n')"
write_page write "$P/取り込み" "$(printf '本文\n\n![](/%s/図あり)\n' "$P")"
write_page write "$P/例" "$(printf ':::example\n```mermaid\nflowchart TD\n    X --> Y\n```\n:::\n')"
write_page write-wiki "$P/Wiki" "$(printf '&pre(code,mermaid){{{\nflowchart TD\n    X --> Y\n}}}\n')"
printf '# 添付の md\n\n%s\n' "$DIAGRAM" > "$WORK/資料.md"
put_file "$P/資料.md" "$WORK/資料.md" application/octet-stream

HTML_WITH="$(page_html "$P/図あり")"
HTML_WITHOUT="$(page_html "$P/図なし")"
HEAD_WITH="$(head_of "$HTML_WITH")"
HEAD_WITHOUT="$(head_of "$HTML_WITHOUT")"

echo "1. 図のあるページにだけ、<head> に diagram.js と <meta> を足す"
check_eq "図のあるページ: diagram.js と <meta> が 1 つずつ" "1,1" "$(diagram_tags "$HEAD_WITH")"
check_eq "図の無いページ: どちらも無い" "0,0" "$(diagram_tags "$HEAD_WITHOUT")"
check_eq "  <head> の違いはその 2 つだけ (<title> のページ名を除く)" \
         "$(without_title "$HEAD_WITHOUT")" "$(without_title "$(without_diagram_tags "$HEAD_WITH")")"
check_eq "  diagram.js は prototype.js と nextform.js より後" "yes" \
         "$(printf '%s' "$HEAD_WITH" | tr '\n' ' ' \
            | grep -q 'prototype\.js.*nextform\.js.*diagram\.js' && echo yes || echo no)"

echo
echo "2. 図の中の文字はエスケープしたまま"
check_eq "<script> がそのまま出ない" "no" "$(contains "$HTML_WITH" '<script>alert(1)')"
check_eq "  文字として出る" "yes" "$(contains "$HTML_WITH" '&lt;script&gt;alert(1)&lt;/script&gt;')"
check_eq "  コードブロックに部分編集の範囲が付いたまま" "yes" \
         "$(printf '%s' "$HTML_WITH" | grep -q '<pre data-twp="[0-9]*" data-twl="[0-9]*"><code class="language-mermaid">' \
            && echo yes || echo no)"

echo
echo "3. プレビュー・取り込み・添付した md・:::example・大文字でも足す。Wiki 記法には足さない"
check_eq "プレビュー (図あり)" "1,1" \
         "$(diagram_tags "$(mermaid_helper preview "$P/新しい" "$DIAGRAM")")"
check_eq "プレビュー (図なし)" "0,0" \
         "$(diagram_tags "$(mermaid_helper preview "$P/新しい" "$PLAIN")")"
check_eq "図のあるページを取り込んだページ" "1,1" \
         "$(diagram_tags "$(head_of "$(page_html "$P/取り込み")")")"
check_eq "添付した md (action=view)" "1,1" \
         "$(diagram_tags "$(head_of "$(page_html "$P/資料.md&action=view")")")"
check_eq ":::example の中の図" "1,1" "$(diagram_tags "$(head_of "$(page_html "$P/例")")")"
check_eq '```Mermaid (大文字)' "1,1" "$(diagram_tags "$(head_of "$(page_html "$P/大文字")")")"
check_eq "種別 wiki の &pre(code,mermaid) には足さない" "0,0" \
         "$(diagram_tags "$(head_of "$(page_html "$P/Wiki")")")"

echo
echo "4. サイト設定 MARKDOWN_DIAGRAM"
check_eq "設定を false にできる" "1" "$(value_of "$(helper set-setting MARKDOWN_DIAGRAM false)" saved)"
check_eq "  false なら足さない" "0,0" "$(diagram_tags "$(head_of "$(page_html "$P/図あり")")")"
check_eq "  プレビューでも足さない" "0,0" \
         "$(diagram_tags "$(mermaid_helper preview "$P/新しい" "$DIAGRAM")")"
check_eq "設定を外せる" "1" "$(value_of "$(helper unset-setting MARKDOWN_DIAGRAM)" saved)"
check_eq "  既定 (描く) に戻る" "1,1" "$(diagram_tags "$(head_of "$(page_html "$P/図あり")")")"

echo
echo "5. Mermaid の URL と中身"
SRC="$(meta_attr "$HEAD_WITH" content)"
check_eq "<meta> の URL は同梱した Mermaid を指し、更新時刻の印が付く" "yes" \
         "$( [[ "$SRC" =~ ^resource/script/mermaid/mermaid\.min\.js\?t=[0-9]+$ ]] && echo yes || echo no)"
check_eq "  文言 (描けないとき) が付く" "yes" \
         "$( [[ -n "$(meta_attr "$HEAD_WITH" data-error)" ]] && echo yes || echo no)"
for path in "$SRC" "resource/script/diagram.js"; do
    type=$(curl -sk -o /dev/null -w '%{http_code} %{content_type}' "${URL}/${path}")
    check_eq "${path%%\?*} が JavaScript として取れる" "200 js" \
             "$(printf '%s' "$type" | sed -E 's#^([0-9]+) .*(javascript).*#\1 js#')"
done
LOCK_JS=$(sed -n 's/^js=//p' "${REPO_ROOT}/deploy/scripts/mermaid.lock")
check_eq "mermaid.min.js の sha512 が mermaid.lock と合う" "$LOCK_JS" \
         "$(sudo sha512sum "${SITE}/resource/script/mermaid/mermaid.min.js" | cut -d' ' -f1)"
sudo touch -d '2026-01-01 00:00:00' "${SITE}/resource/script/mermaid/mermaid.min.js"
check_eq "ファイルが変われば URL も変わる" "yes" \
         "$( [[ "$(meta_attr "$(head_of "$(page_html "$P/図あり")")" content)" != "$SRC" ]] && echo yes || echo no)"

echo
echo "6. PHP の警告"
log_after=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')
log_after="${log_after:-0}"
new_lines=$((log_after - log_before))
if (( new_lines < 0 )); then new_lines=0; fi
check_eq "PHP の警告が増えていない" "0" \
         "$(sudo tail -n "$new_lines" "$PHP_ERROR_LOG" 2>/dev/null | grep -c 'PHP ' || true)"

if [[ "${BROWSER:-0}" == "1" ]]; then
    echo
    echo "7. ブラウザで見るページ"
    . "${REPO_ROOT}/tests/mermaid-browser.sh"
fi

echo
if (( fail == 0 )); then
    echo "全 $total 件 ok"
    exit 0
fi
echo "$total 件中 $fail 件 FAIL"
exit 1
