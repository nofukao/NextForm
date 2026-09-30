#!/bin/bash
# エクスポート機能 (静的書き出し) のテスト
#
#   ./tests/export.sh
#
# 環境変数:
#   NF_SITE             複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   EXPORT_TEST_SITE    検証用に作るサイト (既定: /var/www/html/nf-export-test)
#   WIKI_ADMIN          管理者ユーザー名   (既定: admin)
#   KEEP=1              終了後に検証サイトを消さない (書き出したファイルを見たいとき)
#
# 書き出しは、設定表に並べたページを HTML などのファイルにし、ページ同士の
# リンクを書き出したファイル同士のリンクに張り替える。上流のときから種別 wiki
# しか変換しておらず、種別 Markdown のページは**原文をそのまま HTML の枠に
# 流し込んで**いた (記号が残り、生の HTML もエスケープされずに出る)。
#
# ここで固定するのは次の 3 つ。
#
#   1. 種別 wiki の書き出しがこれまでどおりであること (リンクの張り替え・
#      添付の書き出し・編集用の属性を落とすこと)
#   2. 種別 Markdown のページも HTML に変換して書き出すこと。wiki と同じく
#      リンクを張り替え、ページ内のリンクは見出しに届くこと
#   3. 設定表を Markdown の表で書いても読めること
#
# 入力は tests/export-input/ にある。ボタン (?option=export) は通さず、
# 書き出しの本体 export_pages() を直に呼ぶ (tests/export-helper.php)。
#
# ページを作って書き出すので、必ず複製したサイトに対して実行する。
# 複製元には触らない。sudo が要る。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
EXPORT_TEST_SITE="${EXPORT_TEST_SITE:-/var/www/html/nf-export-test}"
WIKI_ADMIN="${WIKI_ADMIN:-admin}"

fail=0
total=0

cleanup() {
    if [[ "${KEEP:-0}" != "1" ]]; then
        sudo rm -rf "$EXPORT_TEST_SITE" 2>/dev/null
    else
        echo
        echo "KEEP=1 のため検証サイトを残しました: $EXPORT_TEST_SITE"
        echo "書き出したファイル: ${EXPORT_TEST_SITE}/export/"
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
        "${EXPORT_TEST_SITE}/export-helper.php" \
        "${EXPORT_TEST_SITE}/index.php" "$WIKI_ADMIN" "$@" 2>/dev/null
}

contains() {
    if [[ "$1" == *"$2"* ]]; then echo yes; else echo no; fi
}

# 書き出しの結果から、そのページの出力の「ファイル名<TAB>結果」を取る
output_of() {
    printf '%s\n' "$1" | awk -F'\t' -v p="$2" '$1 == "output" && $2 == p { print $3 "\t" $4; exit }'
}

# 書き出したファイルの中身。$1 書き出し先 $2 ファイル名
exported() {
    sudo cat "${EXPORT_ROOT}/$1/$2" 2>/dev/null
}

# 書き出したファイルの中の href / src のうち、行き先が無いものを並べる。
# 張り替え損ねた wiki の URL (?ページ名) も行き先が無いものとして数える。
# 外部の URL と、ページの中の位置 (#…) は数えない (後者は別に見る)。
broken_links() {
    sudo python3 - "${EXPORT_ROOT}/$1" "$2" <<'PYEOF'
import html, os, re, sys
root, name = sys.argv[1], sys.argv[2]
text = open(os.path.join(root, name), encoding='utf-8').read()
bad = []
for value in re.findall(r'\b(?:href|src)="([^"]*)"', text):
    value = html.unescape(value)
    if re.match(r'^[a-zA-Z][a-zA-Z0-9+.-]*:|^//|^#', value):
        continue
    path = value.split('#')[0]
    if value.startswith('?') or path == '' or \
       not os.path.isfile(os.path.normpath(os.path.join(root, os.path.dirname(name), path))):
        bad.append(value)
print(' '.join(bad))
PYEOF
}

# ページの中の位置 (#…) を指すリンクのうち、その id を持つ要素が無いものを並べる
broken_fragments() {
    sudo python3 - "${EXPORT_ROOT}/$1" "$2" <<'PYEOF'
import html, os, re, sys
root, name = sys.argv[1], sys.argv[2]
text = open(os.path.join(root, name), encoding='utf-8').read()
ids = set(re.findall(r'\bid="([^"]*)"', text))
bad = [v for v in re.findall(r'\bhref="#([^"]*)"', text) if html.unescape(v) not in ids]
print(' '.join(bad))
PYEOF
}

if [[ ! -d "$NF_SITE" ]]; then
    echo "複製元がありません: $NF_SITE" >&2
    echo "tests/env.local の NF_SITE を設定してください。" >&2
    exit 1
fi

echo "複製元 = $NF_SITE"
echo "検証先 = $EXPORT_TEST_SITE"
echo

sudo rm -rf "$EXPORT_TEST_SITE"
sudo cp -a "$NF_SITE" "$EXPORT_TEST_SITE"
SITE_OWNER=$(sudo stat -c '%U' "${EXPORT_TEST_SITE}/index.php")
# 書き出し先は複製の中に置く。複製元の index.php が EXPORT_DIR_PATH を
# 別の場所 (公開しているディレクトリなど) に向けていても、そこへは書かない。
# 複製した index.php からその定義を消し、複製の中を指す定義を足す。
EXPORT_ROOT="${EXPORT_TEST_SITE}/export"
sudo rm -rf "$EXPORT_ROOT"
sudo sed -i "/define('EXPORT_DIR_PATH'/d" "${EXPORT_TEST_SITE}/index.php"
sudo sed -i "0,/^<?php/s##<?php\ndefine('EXPORT_DIR_PATH', '${EXPORT_ROOT}');#" "${EXPORT_TEST_SITE}/index.php"
if ! sudo grep -q "define('EXPORT_DIR_PATH', '${EXPORT_ROOT}')" "${EXPORT_TEST_SITE}/index.php"; then
    echo "複製した index.php の書き出し先を向け直せませんでした。" >&2
    exit 1
fi

# 複製元に配置済みのコードではなく、リポジトリの作業ツリーを検証する
sudo rsync -a --delete "${REPO_ROOT}/NextForm/app/"      "${EXPORT_TEST_SITE}/app/"
sudo rsync -a --delete "${REPO_ROOT}/NextForm/resource/" "${EXPORT_TEST_SITE}/resource/"
sudo cp "${REPO_ROOT}/tests/export-helper.php" "${EXPORT_TEST_SITE}/"
sudo cp "${REPO_ROOT}/deploy/scripts/gen-pages.php" "${EXPORT_TEST_SITE}/"
sudo rm -rf "${EXPORT_TEST_SITE}/export-input"
sudo cp -r "${REPO_ROOT}/tests/export-input" "${EXPORT_TEST_SITE}/export-input"
sudo chown -R "$SITE_OWNER" "${EXPORT_TEST_SITE}/app" "${EXPORT_TEST_SITE}/resource" \
                            "${EXPORT_TEST_SITE}/export-helper.php" \
                            "${EXPORT_TEST_SITE}/gen-pages.php" \
                            "${EXPORT_TEST_SITE}/export-input"

# 入力のページを入れる (tests/setup-fixtures.sh と同じ道具)
if ! sudo -u "$SITE_OWNER" php "${EXPORT_TEST_SITE}/gen-pages.php" "${EXPORT_TEST_SITE}/index.php" \
        --dir "${EXPORT_TEST_SITE}/export-input" --user "$WIKI_ADMIN" > /dev/null; then
    echo "入力のページを作れませんでした。" >&2
    exit 1
fi

OUT="$(helper export ExportTest/Config wiki)"
echo "書き出しで出たエラー:"
printf '%s\n' "$OUT" | awk -F'\t' '$1 == "error" { print "  " $2; n++ } END { if(!n) print "  なし" }'
echo

echo "1. 種別 wiki のページ (これまでどおり)"
W="$(exported wiki wiki.html)"
check_eq "設定表のファイル名で書き出す" "wiki.html	success" "$(output_of "$OUT" ExportTest/Wiki)"
check_eq "題名 (&title) がひな形に入る"   "yes" "$(contains "$W" '<title>wiki の書き出し')"
check_eq "本文が HTML になっている"       "yes" "$(contains "$W" '<strong>強調</strong>')"
check_eq "Markdown のページへのリンクを張り替える" "yes" "$(contains "$W" 'href="markdown.html"')"
check_eq "添付の画像を埋め込む"           "yes" "$(contains "$W" '<img ')"
check_eq "リンクと画像がすべて書き出したファイルを指す" "" "$(broken_links wiki wiki.html)"
check_eq "編集用の属性 (data-twp) が残らない"  "no" "$(contains "$W" 'data-twp=')"
check_eq "  data-link-pagename も残らない"     "no" "$(contains "$W" 'data-link-pagename=')"
# 書き出しのボタン (&export) は静的なサイトでは押せないので消す。
# 同じ段落に 2 つ並べたとき、以前は 1 つめしか消えなかった
check_eq "書き出しのボタン (フォーム) が残らない" "no" "$(contains "$W" '<form')"
echo

echo "2. 種別 Markdown のページ"
M="$(exported wiki markdown.html)"
check_eq "設定表のファイル名で書き出す" "markdown.html	success" "$(output_of "$OUT" ExportTest/Markdown)"
check_eq "題名 (フロントマター) がひな形に入る" "yes" "$(contains "$M" '<title>Markdown の書き出し')"
# 画面の題名は後ろに空白を付けて組み立てる (sentence_append)。書き出しには要らない
check_eq "  題名の後ろに空白が残らない"   "yes" "$(contains "$M" '<title>Markdown の書き出し</title>')"
check_eq "本文が HTML に変換されている"   "yes" "$(contains "$M" '<strong>強調</strong>')"
check_eq "  原文の記号が残らない"         "no"  "$(contains "$M" '**強調**')"
check_eq "  フロントマターが本文に出ない" "no"  "$(contains "$M" 'title: Markdown')"
check_eq "表の中の [[ページ|表示名]] を張り替える" "yes" \
         "$(contains "$M" 'href="wiki.html"')"
check_eq "折りたたみ (:::details) が出る" "yes" "$(contains "$M" '<details')"
# 変換せずに流し込んでいたころは、ここがそのまま HTML として出ていた
check_eq "生の HTML はエスケープする"     "no"  "$(contains "$M" '<script>')"
check_eq "添付の画像を埋め込む"           "yes" "$(contains "$M" '<img ')"
check_eq "リンクと画像がすべて書き出したファイルを指す" "" "$(broken_links wiki markdown.html)"
check_eq "ページ内のリンクが見出しに届く" "" "$(broken_fragments wiki markdown.html)"
check_eq "  見出しを指すリンクがある"     "yes" "$(contains "$M" 'href="#')"
check_eq "編集用の属性 (data-twp) が残らない"  "no" "$(contains "$M" 'data-twp=')"
check_eq "  data-link-pagename も残らない"     "no" "$(contains "$M" 'data-link-pagename=')"

# 設定表に無いページ (リンクで辿ったもの) は既定のファイル名で書き出す
CHILD="$(output_of "$OUT" ExportTest/Markdown/Child)"
CHILD_FILE="${CHILD%%	*}"
check_eq "リンクで辿った Markdown のページも書き出す" "success" "${CHILD##*	}"
check_eq "  既定のファイル名は .html で終わる" "yes" "$(contains "${CHILD_FILE: -5}" '.html')"
C="$(exported wiki "$CHILD_FILE")"
check_eq "  中身も HTML に変換されている"      "yes" "$(contains "$C" '<p><a ')"
# 子のページは title: を持たず、先頭の唯一の # が題名になる。ひな形の
# <h1>$title$</h1> と本文の # で見出しが二重にならないこと
check_eq "  題名 (先頭の #) がひな形に入る"   "yes" "$(contains "$C" '<title>子のページ</title>')"
check_eq "  本文に同じ見出しを二重に出さない" "1" "$(printf '%s' "$C" | grep -o '<h1' | wc -l)"
check_eq "  親のページへのリンクを張り替える"  "yes" "$(contains "$C" 'href="markdown.html"')"
echo

echo "3. 設定表を Markdown の表で書く"
OUT_MD="$(helper export ExportTest/ConfigMd md-config)"
check_eq "表のリンクのページを書き出す" "by-md-config.html	success" \
         "$(output_of "$OUT_MD" ExportTest/Markdown)"
check_eq "  中身は変換した HTML" "yes" "$(contains "$(exported md-config by-md-config.html)" '<strong>強調</strong>')"
echo

if [[ $fail -eq 0 ]]; then
    echo "全 ${total} 件 通過"
    exit 0
else
    echo "${fail} / ${total} 件 失敗"
    exit 1
fi
