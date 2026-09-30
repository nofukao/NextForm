#!/bin/bash
# 添付した HTML を動かす機能 (FILE_ALLOW_HTML) のテスト
#
#   ./tests/file-html.sh
#
# 環境変数:
#   NF_SITE              複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   FILE_HTML_TEST_SITE  検証用に作るサイト (既定: /var/www/html/nf-file-html-test)
#   FILE_HTML_TEST_URL   その URL           (既定: http://localhost/nf-file-html-test)
#   WIKI_ADMIN           管理者ユーザー名   (既定: admin)
#   KEEP=1               終了後に検証サイトを消さない
#
# 添付した HTML を wiki と同じオリジンの text/html として返すと、中の
# スクリプトは開いた人の資格情報で wiki にリクエストを送れる (ダイジェスト認証は
# ブラウザが自動で付ける)。管理者が開けば、ページも設定も書き換えられる。
# そこで、動かすときは Content-Security-Policy: sandbox で wiki とは別の
# オリジンに隔離し、しかも既定では動かさない。ここで固定するのは次の 3 つ。
#
#   1. 既定 (設定なし) では、これまでどおりダウンロードになること
#   2. 有効にすると text/html で返り、**必ず sandbox が付く**こと
#      (付け忘れた瞬間に XSS の入口になる。ここが肝)
#   3. ファイルのページと Markdown の埋め込みに「開く」リンクが出ること
#
# 隔離した文書から wiki への POST は Origin: null になる。それを弾くことは
# tests/csrf.sh で見ている。
#
# 設定を書き換えるので、必ず複製したサイトに対して実行する。
# 複製元には触らない。sudo が要る。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
FILE_HTML_TEST_SITE="${FILE_HTML_TEST_SITE:-/var/www/html/nf-file-html-test}"
FILE_HTML_TEST_URL="${FILE_HTML_TEST_URL:-http://localhost/nf-file-html-test}"
WIKI_ADMIN="${WIKI_ADMIN:-admin}"

fail=0
total=0
WORK="$(mktemp -d)"

cleanup() {
    rm -rf "$WORK"
    if [[ "${KEEP:-0}" != "1" ]]; then
        sudo rm -rf "$FILE_HTML_TEST_SITE" 2>/dev/null
    else
        echo
        echo "KEEP=1 のため検証サイトを残しました: $FILE_HTML_TEST_SITE"
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
        "${FILE_HTML_TEST_SITE}/file-html-helper.php" \
        "${FILE_HTML_TEST_SITE}/index.php" "$WIKI_ADMIN" "$@" 2>/dev/null
}

value_of() {
    printf '%s\n' "$1" | sed -n "s/^$2=//p" | head -1
}

contains() {
    if [[ "$1" == *"$2"* ]]; then echo yes; else echo no; fi
}

# 添付の実体を取り、応答ヘッダーを $WORK/headers、本文を $WORK/body に置く
fetch_raw() {
    curl -sk -D "$WORK/headers" -o "$WORK/body" "${FILE_HTML_TEST_URL}/?${P_FILE}&action=raw"
}

# $1 ヘッダー名。値だけを返す (無ければ空)
header_of() {
    grep -i "^$1:" "$WORK/headers" | head -1 | cut -d: -f2- | tr -d '\r' | sed 's/^ *//'
}

# Content-Type の型の部分だけ (PHP が付ける ;charset=UTF-8 などを落とす)
media_type() {
    header_of Content-Type | cut -d';' -f1 | tr -d ' '
}

main_html() {
    curl -sk "${FILE_HTML_TEST_URL}/?$1"
}

if [[ ! -d "$NF_SITE" ]]; then
    echo "複製元がありません: $NF_SITE" >&2
    echo "tests/env.local の NF_SITE を設定してください。" >&2
    exit 1
fi

echo "複製元 = $NF_SITE"
echo "検証先 = $FILE_HTML_TEST_SITE"
echo "URL    = $FILE_HTML_TEST_URL"
echo

sudo rm -rf "$FILE_HTML_TEST_SITE"
sudo cp -a "$NF_SITE" "$FILE_HTML_TEST_SITE"
SITE_OWNER=$(sudo stat -c '%U' "${FILE_HTML_TEST_SITE}/index.php")

# 複製元に配置済みのコードではなく、リポジトリの作業ツリーを検証する
sudo rsync -a --delete "${REPO_ROOT}/NextForm/app/"      "${FILE_HTML_TEST_SITE}/app/"
sudo rsync -a --delete "${REPO_ROOT}/NextForm/resource/" "${FILE_HTML_TEST_SITE}/resource/"
sudo cp "${REPO_ROOT}/tests/file-html-helper.php" "${FILE_HTML_TEST_SITE}/"
sudo chown -R "$SITE_OWNER" "${FILE_HTML_TEST_SITE}/app" "${FILE_HTML_TEST_SITE}/resource" \
                            "${FILE_HTML_TEST_SITE}/file-html-helper.php"

if [[ "$(curl -sk -o /dev/null -w '%{http_code}' "${FILE_HTML_TEST_URL}/")" != "200" ]]; then
    echo "検証サイトが $FILE_HTML_TEST_URL で見えません。" >&2
    echo "tests/env.local の FILE_HTML_TEST_URL を設定してください。" >&2
    exit 1
fi

P_PARENT="FileHtmlTest"
P_FILE="FileHtmlTest/game.html"
FIXTURE="${FILE_HTML_TEST_SITE}/game-fixture.html"
BODY='<!doctype html><meta charset="utf-8"><title>検証</title><script>document.title = "動いた";</script><p>添付の HTML</p>'
printf '%s' "$BODY" | sudo tee "$FIXTURE" > /dev/null
sudo chown "$SITE_OWNER" "$FIXTURE"

out="$(helper write-file "$P_FILE" "$FIXTURE" text/html)"
if [[ "$(value_of "$out" written)" != "1" ]]; then
    echo "添付のページを作れませんでした。" >&2
    exit 1
fi
helper write "$P_PARENT" '![遊ぶ](game.html)' > /dev/null

echo "1. 既定ではダウンロードになること"
# 複製元の設定を引き継ぐので、まず項目を取り除いて既定値が効く状態にする
# (開発検証サイトで有効にしてあると、ここが「有効」から始まってしまう)。
out="$(helper unset-setting FILE_ALLOW_HTML)"
check_eq "設定を既定に戻せる" "1" "$(value_of "$out" saved)"
fetch_raw
check_eq "Content-Type は octet-stream"   "application/octet-stream" "$(media_type)"
check_eq "添付として渡す (attachment)"    "yes" "$(contains "$(header_of Content-Disposition)" attachment)"
check_eq "中身は変わらない"               "$BODY" "$(cat "$WORK/body")"
check_eq "ファイルのページに「開く」は出ない" "no" \
         "$(contains "$(main_html "$P_FILE")" 'class="open_html"')"
echo

echo "2. 有効にすると sandbox 付きの text/html で返ること"
out="$(helper set-setting FILE_ALLOW_HTML true)"
check_eq "設定を保存できる" "1" "$(value_of "$out" saved)"
fetch_raw
check_eq "Content-Type は text/html"      "text/html" "$(media_type)"
check_eq "ページとして開く (inline)"      "yes" "$(contains "$(header_of Content-Disposition)" inline)"
# ここが肝。allow-same-origin を足すと隔離の意味が無くなる。
# フォームの送信・ポップアップ・alert/prompt・最上位の移動も許さない。
check_eq "sandbox で隔離する (スクリプトだけ許す)" "sandbox allow-scripts" \
         "$(header_of Content-Security-Policy)"
check_eq "型の推測をさせない (nosniff)"   "nosniff" "$(header_of X-Content-Type-Options)"
check_eq "中身は変わらない"               "$BODY" "$(cat "$WORK/body")"
echo

echo "3. 「開く」リンクが出ること"
F_HTML="$(main_html "$P_FILE")"
check_eq "ファイルのページに「開く」が出る" "yes" "$(contains "$F_HTML" 'class="open_html"')"
check_eq "  行き先は実体 (action=raw)" "yes" \
         "$(contains "$F_HTML" "href=\"?${P_FILE}&amp;action=raw\"")"
check_eq "Markdown の ![..](game.html) にも「開く」が出る" "yes" \
         "$(contains "$(main_html "$P_PARENT")" 'class="open_html"')"
check_eq "  枠 (iframe) では動かさない" "no" \
         "$(contains "$(main_html "$P_PARENT")" '<iframe')"
echo

echo "4. 有効にしても他の種類は変わらないこと"
TEXT_FIXTURE="${FILE_HTML_TEST_SITE}/text-fixture.txt"
printf '%s' 'ただの文字' | sudo tee "$TEXT_FIXTURE" > /dev/null
sudo chown "$SITE_OWNER" "$TEXT_FIXTURE"
helper write-file "FileHtmlTest/note.txt" "$TEXT_FIXTURE" text/plain > /dev/null
curl -sk -D "$WORK/headers" -o "$WORK/body" "${FILE_HTML_TEST_URL}/?FileHtmlTest/note.txt&action=raw"
check_eq "text/plain は text/plain のまま"  "text/plain" "$(media_type)"
check_eq "  sandbox は付かない"             "" "$(header_of Content-Security-Policy)"
echo

if [[ $fail -eq 0 ]]; then
    echo "全 ${total} 件 通過"
    exit 0
else
    echo "${fail} / ${total} 件 失敗"
    exit 1
fi
