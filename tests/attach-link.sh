#!/bin/bash
# 添付ファイルへのリンクの振る舞いのテスト
#
#   ./tests/attach-link.sh
#
# 環境変数:
#   NF_SITE                複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   ATTACH_LINK_TEST_SITE  検証用に作るサイト (既定: /var/www/html/nf-attach-link-test)
#   ATTACH_LINK_TEST_URL   その URL           (既定: http://localhost/nf-attach-link-test)
#   WIKI_ADMIN             管理者ユーザー名   (既定: admin)
#   PHP_ERROR_LOG          PHP のエラーログ   (既定: /var/log/php-fpm/www-error.log)
#   KEEP=1                 終了後に検証サイトを消さない
#
# Markdown の書き方で、添付の種類によらず次のように振る舞うことを固定する。
#
#   ![説明](ファイル)  画像は埋め込む (説明は代替テキスト)。それ以外は
#                      「説明」のリンクで、押すとそのファイルを開く。
#                      PDF などは実体 (action=raw)、md は wiki の画面に描画した
#                      もの (action=view)。説明が空ならファイル名を出す
#   [説明](ファイル)   どの種類でもファイルのページへのリンク (今のまま)
#
# md のファイルのページには「開く」(描画した画面) と「ダウンロード」(実体) を出す。
# 実体 (action=raw) はクローンやダウンロードが使うので、md でも変えない。
#
# ページと添付を作るので、必ず複製したサイトに対して実行する。
# 複製元には触らない。sudo が要る。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
SITE="${ATTACH_LINK_TEST_SITE:-/var/www/html/nf-attach-link-test}"
URL="${ATTACH_LINK_TEST_URL:-http://localhost/nf-attach-link-test}"
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

# $1 HTML  $2 リンクの文字。そのリンクの行き先を、符号化を解いて返す
# (?AttachLinkTest/資料.pdf&action=raw のように読める形)
link_href() {
    printf '%s' "$1" | grep -o "<a [^>]*>$2</a>" | head -1 \
        | sed -n 's/.*href="\([^"]*\)".*/\1/p' \
        | php -r 'echo rawurldecode(html_entity_decode(stream_get_contents(STDIN)));'
}

# $1 HTML。本文 (section.markdown) だけを返す。サイドのページなどは含めない
markdown_section() {
    printf '%s' "$1" | php -r '
        $d = new DOMDocument();
        @$d->loadHTML("<?xml encoding=\"utf-8\"?>" . stream_get_contents(STDIN));
        foreach((new DOMXPath($d))->query("//section[@class=\"markdown\"]") as $s)
            echo $d->saveHTML($s);'
}

# $1 HTML  $2 リンクの文字。その文字で実体 (action=raw) を指すリンクの数
raw_link_count() {
    printf '%s' "$1" | grep -o "<a [^>]*action=raw[^>]*>$2</a>" | wc -l | tr -d ' '
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

# 添付の実体を取り、応答ヘッダーを $WORK/headers、本文を $WORK/body に置く
fetch() {
    curl -sk -D "$WORK/headers" -o "$WORK/body" -w '%{http_code}' "${URL}/?$1"
}

header_of() {
    grep -i "^$1:" "$WORK/headers" | head -1 | cut -d: -f2- | tr -d '\r' | sed 's/^ *//'
}

media_type() {
    header_of Content-Type | cut -d';' -f1 | tr -d ' '
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
    echo "tests/env.local の ATTACH_LINK_TEST_URL を設定してください。" >&2
    exit 1
fi

log_before=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')
log_before="${log_before:-0}"

# --- 準備: 添付と、それを指すページを作る ------------------------------------
P="AttachLinkTest"

cp "${REPO_ROOT}/tests/golden/input/GoldenMaster/Markdown/portforward01.png" "$WORK/図.png"
printf '%%PDF-1.4\n%% 検証用\n' > "$WORK/資料.pdf"
printf 'PK\003\004 検証用' > "$WORK/表.xlsx"
cat > "$WORK/読み物.md" <<'EOF'
# 読み物の題

本文の **太字**。

## 節

![図の説明](図.png)

[資料へ](./資料.pdf)
EOF
printf '## 種類が text/markdown の md\n' > "$WORK/メモ.md"
# UTF-8 でない md (Shift_JIS の「日本語」)。描画できないのでファイルのページに戻す
printf '# \x93\xfa\x96\x7b\x8c\xea\n' > "$WORK/sjis.md"

put_file "$P/図.png"    "$WORK/図.png"    image/png
put_file "$P/資料.pdf"  "$WORK/資料.pdf"  application/pdf
put_file "$P/表.xlsx"   "$WORK/表.xlsx"   application/vnd.openxmlformats-officedocument.spreadsheetml.sheet
# ブラウザは .md を「種類不明」で送ってくることがある (開発検証サイトで確認)
put_file "$P/読み物.md" "$WORK/読み物.md" application/octet-stream
put_file "$P/メモ.md"   "$WORK/メモ.md"   text/markdown
put_file "$P/sjis.md"   "$WORK/sjis.md"   application/octet-stream

helper write "$P" "$(cat <<'EOF'
- ![図の説明](図.png)
- ![資料の説明](資料.pdf)
- ![](資料.pdf)
- [資料へのリンク](資料.pdf)
- ![読み物の説明](読み物.md)
- [読み物へのリンク](読み物.md)
- ![表の説明](表.xlsx)
EOF
)" > /dev/null
helper write-wiki "$P/Wiki" "$(cat <<'EOF'
&include([[../資料.pdf]]);
&include([[../資料.pdf]],資料を読む);
&include([[../読み物.md]]);
EOF
)" > /dev/null

HTML="$(page_html "$P")"

echo "1. ![説明](ファイル) — 画像は埋め込み、それ以外は「説明」のリンクで開く"
check_eq "画像は埋め込む" "yes" \
         "$(contains "$HTML" "data-link-pagename=\"$P/図.png\" width=")"
check_eq "  説明は画像の代替テキストになる" "yes" "$(contains "$HTML" 'alt="図の説明"')"
check_eq "PDF は「資料の説明」で、押すと実体を開く" "?$P/資料.pdf&action=raw" \
         "$(link_href "$HTML" '資料の説明')"
check_eq "  説明が空ならファイル名を出す" "?$P/資料.pdf&action=raw" \
         "$(link_href "$HTML" '資料.pdf')"
check_eq "  ページ名の全体は出さない" "0" "$(raw_link_count "$HTML" "$P/資料.pdf")"
check_eq "md は「読み物の説明」で、押すと描画した画面を開く" "?$P/読み物.md&action=view" \
         "$(link_href "$HTML" '読み物の説明')"
check_eq "その他 (xlsx) は「表の説明」で、押すと実体 (ダウンロード)" "?$P/表.xlsx&action=raw" \
         "$(link_href "$HTML" '表の説明')"
echo

echo "2. [説明](ファイル) — どの種類でもファイルのページへ"
check_eq "PDF" "?$P/資料.pdf"  "$(link_href "$HTML" '資料へのリンク')"
check_eq "md"  "?$P/読み物.md" "$(link_href "$HTML" '読み物へのリンク')"
echo

echo "3. ファイルのページ"
F_MD="$(page_html "$P/読み物.md")"
check_eq "md に「開く」(描画した画面) が出る" "?$P/読み物.md&action=view" "$(link_href "$F_MD" '開く')"
check_eq "  「ダウンロード」(実体) も出る"      "?$P/読み物.md&action=raw"  "$(link_href "$F_MD" 'ダウンロード')"
F_PDF="$(page_html "$P/資料.pdf")"
check_eq "PDF は「ダウンロード」だけ (今のまま)" "?$P/資料.pdf&action=raw" "$(link_href "$F_PDF" 'ダウンロード')"
check_eq "  「開く」は出ない" "" "$(link_href "$F_PDF" '開く')"
check_eq "画像に「開く」は出ない (今のまま)" "no" \
         "$(contains "$(page_html "$P/図.png")" 'action=view')"
F_MEMO="$(page_html "$P/メモ.md")"
check_eq "種類が text/markdown の md にも「開く」が出る" "?$P/メモ.md&action=view" \
         "$(link_href "$F_MEMO" '開く')"
echo

echo "4. md を描画した画面 (action=view)"
code="$(fetch "$P/読み物.md&action=view")"
VIEW="$(cat "$WORK/body")"
check_eq "GET で開ける (応答は 200)" "200" "$code"
check_eq "Markdown として描画する" "yes" \
         "$(contains "$VIEW" '<p>本文の <strong>太字</strong>。</p>')"
check_eq "  先頭の # は題名になる" "yes" "$(contains "$VIEW" '<title>読み物の題')"
check_eq "  部分編集の位置は付けない (編集できない)" "no" \
         "$(contains "$(markdown_section "$VIEW")" 'data-twp')"
check_eq "md の中の ![図の説明](図.png) は、同じページの添付を指す" "yes" \
         "$(contains "$VIEW" "data-link-pagename=\"$P/図.png\" width=")"
check_eq "md の中の [資料へ](./資料.pdf) も、同じページの添付を指す" "?$P/資料.pdf" \
         "$(link_href "$VIEW" '資料へ')"
code="$(fetch "$P/資料.pdf&action=view")"
check_eq "md でないファイルはファイルのページを出す" "?$P/資料.pdf&action=raw" \
         "$(link_href "$(cat "$WORK/body")" 'ダウンロード')"
code="$(fetch "$P/sjis.md&action=view")"
check_eq "UTF-8 でない md は、描画せずにファイルのページを出す" "?$P/sjis.md&action=raw" \
         "$(link_href "$(cat "$WORK/body")" 'ダウンロード')"
check_eq "  そのことを知らせる" "yes" "$(contains "$(cat "$WORK/body")" 'UTF-8 ではない')"
echo

echo "5. 実体 (action=raw) は md でも変えない (クローンとダウンロードが使う)"
fetch "$P/読み物.md&action=raw" > /dev/null
check_eq "Content-Type は octet-stream"  "application/octet-stream" "$(media_type)"
check_eq "添付として渡す (attachment)"   "yes" "$(contains "$(header_of Content-Disposition)" attachment)"
check_eq "中身は変わらない"              "$(cat "$WORK/読み物.md")" "$(cat "$WORK/body")"
echo

echo "6. Wiki 記法の &include も同じ表示になる"
WIKI="$(page_html "$P/Wiki")"
check_eq "PDF はファイル名のリンク" "?$P/資料.pdf&action=raw" "$(link_href "$WIKI" '資料.pdf')"
check_eq "  表示名を書けばそれを出す (今のまま)" "?$P/資料.pdf&action=raw" \
         "$(link_href "$WIKI" '資料を読む')"
check_eq "md は描画した画面を開く" "?$P/読み物.md&action=view" "$(link_href "$WIKI" '読み物.md')"
echo

echo "7. PHP の警告が出ていないこと"
log_new=$(sudo tail -n +"$((log_before + 1))" "$PHP_ERROR_LOG" 2>/dev/null | grep -F "$SITE" || true)
check_eq "この検証サイトの警告が増えていない" "" "$log_new"
echo

if [[ $fail -eq 0 ]]; then
    printf '%d/%d 件すべて通りました。\n' "$total" "$total"
else
    printf '%d/%d 件が失敗しました。\n' "$fail" "$total"
fi
exit $((fail == 0 ? 0 : 1))
