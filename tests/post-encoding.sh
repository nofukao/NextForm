#!/bin/bash
# 包んだ POST (WAF 対策) のテスト
#
#   ./tests/post-encoding.sh
#
# 環境変数:
#   NF_SITE                  複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   POST_ENCODING_TEST_SITE  検証用に作るサイト (既定: /var/www/html/nf-post-encoding-test)
#   POST_ENCODING_TEST_URL   その URL           (既定: http://localhost/nf-post-encoding-test)
#   WIKI_ADMIN               管理者ユーザー名   (既定: admin)
#   KEEP=1                   終了後に検証サイトを消さない
#
# 共用サーバーの WAF (ConoHa WING の SiteGuard Lite など) は、POST の本文に
# `'--` や `<script>` があると SQL インジェクションや XSS とみなし、PHP に
# 届く前に 403 で遮断する。wiki の本文にはコード例としてそういう文字列が
# 普通に入るので、保存できなくなる。全体の編集画面は編集前の本文も
# source_contents として送るので、一度そういう文字列が入ったページは
# 消して保存し直すことすらできない。
#
# そこでブラウザは POST の値を Base64 に包んで送り (nextform.js の formdata)、
# サーバーは args_get() で戻す。ここで固定するのは次のとおり:
#
#   1. 包まない POST (JavaScript が無効なときなど) がこれまでどおり通ること
#   2. 包んだ POST の保存結果が、包まない場合と 1 バイトも変わらないこと
#   3. 全体の編集画面と同じ項目 (ticket・source_contents・押したボタン) を
#      包んでも上書きできること
#   4. 包んでも CSRF の検査は効くこと
#   5. 印が GET にあるだけでは戻さないこと
#   6. 戻せない値があれば 400 で断り、ページを変えないこと
#      (空として扱うと「本文が空 = ページの削除」になる)
#   7. 添付 (multipart) で、MAX_FILE_SIZE だけ包まずに送れば受け付けること
#
# WAF そのものはこの環境に無い。ここで見るのは「包んだものを正しく戻せるか」で、
# ブラウザが包んで送ることはブラウザの開発者ツールで確かめる。
#
# 権限を書き換えるので、必ず複製したサイトに対して実行する。
# 複製元には触らない。root で実行する必要がある。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
POST_ENCODING_TEST_SITE="${POST_ENCODING_TEST_SITE:-/var/www/html/nf-post-encoding-test}"
POST_ENCODING_TEST_URL="${POST_ENCODING_TEST_URL:-http://localhost/nf-post-encoding-test}"
WIKI_ADMIN="${WIKI_ADMIN:-admin}"
PHP_ERROR_LOG="${PHP_ERROR_LOG:-/var/log/php-fpm/www-error.log}"

SITE="$POST_ENCODING_TEST_SITE"
URL="$POST_ENCODING_TEST_URL"

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
        "${SITE}/post-encoding-helper.php" \
        "${SITE}/index.php" "$WIKI_ADMIN" "$@" 2>/dev/null
}

value_of() {
    printf '%s\n' "$1" | sed -n "s/^$2=//p" | head -1
}

# 保存されている本文 (base64)
body_of() {
    value_of "$(helper body "$1")" body
}

# 保存されている本文に、その文字列が入っているか (yes/no)
body_has() {
    if body_of "$1" | base64 -d 2>/dev/null | grep -qF -- "$2"; then
        echo yes
    else
        echo no
    fi
}

exists_of() {
    value_of "$(helper exists "$1")" exists
}

# ブラウザと同じ包み方 (UTF-8 のバイト列をそのまま Base64)
b64() {
    printf '%s' "$1" | base64 -w0
}

# 編集フォームから ticket を取る。page_write() は既存ページに ticket の
# 一致を求めるので、ブラウザと同じようにフォームから取る。
ticket_of() {
    curl -sk "${URL}/?$1&action=edit" \
        | grep -o '<input[^>]*name="ticket"[^>]*>' \
        | sed -E 's/.*value="([^"]*)".*/\1/' | head -1
}

# POST して応答コードを返す。$1 ページ名  $2 以降は curl の引数
post() {
    local page="$1"
    shift
    curl -sk -o /dev/null -w '%{http_code}' -X POST "$@" "${URL}/?${page}"
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
sudo cp "${REPO_ROOT}/tests/post-encoding-helper.php" "${SITE}/"
sudo chown -R "$SITE_OWNER" "${SITE}/app" "${SITE}/resource" \
                            "${SITE}/post-encoding-helper.php"

# 置いたコードが PHP-FPM に載るのを待つ (tests/markdown.sh と同じ理由。
# opcache は revalidate_freq の間ファイルを stat し直さない)。
OPCACHE_FREQ=$(php -r 'echo (int)ini_get("opcache.revalidate_freq");' 2>/dev/null)
sleep $(( ${OPCACHE_FREQ:-2} + 1 ))

if [[ "$(value_of "$(helper guest-write)" saved)" != "1" ]]; then
    echo "ログインしていない利用者に write 権限を与えられませんでした。" >&2
    exit 1
fi

if [[ "$(curl -sk -o /dev/null -w '%{http_code}' "${URL}/")" != "200" ]]; then
    echo "検証サイトが $URL で見えません。" >&2
    echo "tests/env.local の POST_ENCODING_TEST_URL を設定してください。" >&2
    exit 1
fi

helper cleanup > /dev/null

ORIGIN=$(printf '%s' "$URL" | sed -E 's#^(https?://[^/]+).*#\1#')
log_before=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')
log_before="${log_before:-0}"

P_RAW="PostEncodingTest/Raw"
P_ENC="PostEncodingTest/Encoded"
P_CSRF="PostEncodingTest/NoOrigin"
P_GET="PostEncodingTest/GetMarker"

# WAF が弾く文字列を並べた本文。ブラウザが送るのと同じく改行は CRLF にし、
# 制御文字も 1 つ混ぜる (どちらも args_get() の normalize_string() が落とす。
# 包んだときも同じ落ち方をすることを見る)。
printf '# 見出し\r\n'"'"'--aa'"'"'\r\n<script>alert(1)</script>\r\nSELECT * FROM t WHERE a = '"'"'1'"'"' OR '"'"'1'"'"'='"'"'1'"'"'; -- 注釈\r\n制御\001文字\r\n' \
    > "$WORK/text"
TEXT_B64=$(base64 -w0 < "$WORK/text")

echo "1. 包まない POST がこれまでどおり通ること"
post "$P_RAW" -H "Origin: ${ORIGIN}" -d "action=write" \
     --data-urlencode "contents@${WORK}/text" > /dev/null
check_eq "保存できる" "1" "$(exists_of "$P_RAW")"
check_eq "  本文に '--aa' が入っている" "yes" "$(body_has "$P_RAW" "'--aa'")"
echo

echo "2. 包んだ POST の保存結果が、包まない場合と同じになること"
code=$(post "$P_ENC" -H "Origin: ${ORIGIN}" \
            --data-urlencode "action=$(b64 write)" \
            --data-urlencode "contents=${TEXT_B64}" \
            -d "post_encoding=base64")
check_eq "応答は保存後の移動 (302)" "302" "$code"
check_eq "保存できる" "1" "$(exists_of "$P_ENC")"
check_eq "  本文が包まない場合と 1 バイトも変わらない" "$(body_of "$P_RAW")" "$(body_of "$P_ENC")"
check_eq "  本文に '--aa' が入っている (包んだまま保存していない)" "yes" "$(body_has "$P_ENC" "'--aa'")"
check_eq "  本文に <script> が入っている" "yes" "$(body_has "$P_ENC" "<script>")"
check_eq "  CR は落ちている" "no" "$(body_has "$P_ENC" $'\r')"
check_eq "  制御文字は落ちている" "no" "$(body_has "$P_ENC" $'\001')"
echo

echo "3. 全体の編集画面と同じ項目を包んでも上書きできること"
old_text=$(body_of "$P_ENC" | base64 -d)
new_text="書き換えた本文
'--bb'
<script>alert(2)</script>"
code=$(post "$P_ENC" -H "Origin: ${ORIGIN}" \
            --data-urlencode "action=$(b64 write)" \
            --data-urlencode "page=$(b64 "$P_ENC")" \
            --data-urlencode "pagename=$(b64 "$P_ENC")" \
            --data-urlencode "ticket=$(b64 "$(ticket_of "$P_ENC")")" \
            --data-urlencode "source_contents=$(b64 "$old_text")" \
            --data-urlencode "contents=$(b64 "$new_text")" \
            --data-urlencode "save_and_quit=$(b64 '保存して終了(S)')" \
            -d "post_encoding=base64")
check_eq "応答は保存後の移動 (302)" "302" "$code"
check_eq "  本文が書き換わっている" "$(b64 "$new_text")" "$(body_of "$P_ENC")"
echo

echo "4. 包んでも CSRF の検査は効くこと"
code=$(post "$P_CSRF" --data-urlencode "action=$(b64 write)" \
            --data-urlencode "contents=$(b64 'Origin の無い要求')" \
            -d "post_encoding=base64")
check_eq "Origin の無い POST は 403" "403" "$code"
check_eq "  ページはできていない" "0" "$(exists_of "$P_CSRF")"
echo

echo "5. 印が GET にあるだけでは戻さないこと"
code=$(curl -sk -o /dev/null -w '%{http_code}' -X POST -H "Origin: ${ORIGIN}" \
            -d "action=write" --data-urlencode "contents=${TEXT_B64}" \
            "${URL}/?${P_GET}&post_encoding=base64")
check_eq "保存できる" "1" "$(exists_of "$P_GET")"
check_eq "  本文は送った Base64 の文字列のまま" "$(b64 "$TEXT_B64")" "$(body_of "$P_GET")"
echo

echo "6. 戻せない値があれば 400 で断り、ページを変えないこと"
before=$(body_of "$P_ENC")
code=$(post "$P_ENC" -H "Origin: ${ORIGIN}" \
            --data-urlencode "action=$(b64 write)" \
            --data-urlencode "ticket=$(b64 "$(ticket_of "$P_ENC")")" \
            --data-urlencode "contents=@@ Base64 ではない @@" \
            -d "post_encoding=base64")
check_eq "応答は 400" "400" "$code"
check_eq "  ページは消えていない" "1" "$(exists_of "$P_ENC")"
check_eq "  本文は変わっていない" "$before" "$(body_of "$P_ENC")"
echo

echo "7. 添付 (multipart) は MAX_FILE_SIZE だけ包まずに受け付けること"
printf "添付の中身 '--cc'\n" > "$WORK/sample.txt"
code=$(post "$P_RAW" -H "Origin: ${ORIGIN}" \
            -F "MAX_FILE_SIZE=2097152" \
            -F "option=$(b64 attach)" \
            -F "action=$(b64 write)" \
            -F "page=$(b64 "$P_RAW")" \
            -F "pagename=$(b64 "$P_RAW")" \
            -F "files[]=@${WORK}/sample.txt" \
            -F "save_upload=$(b64 'アップロード(S)')" \
            -F "post_encoding=base64")
check_eq "応答は 200" "200" "$code"
check_eq "  添付ファイルのページができている" "1" "$(exists_of "${P_RAW}/sample.txt")"
echo

echo "8. PHP の警告が出ていないこと"
log_new=$(sudo tail -n +"$((log_before + 1))" "$PHP_ERROR_LOG" 2>/dev/null | grep -F "$SITE" || true)
check_eq "この検証サイトの警告が増えていない" "" "$log_new"
echo

helper cleanup > /dev/null

if [[ $fail -eq 0 ]]; then
    printf '%d/%d 件すべて通りました。\n' "$total" "$total"
else
    printf '%d/%d 件が失敗しました。\n' "$fail" "$total"
fi
exit $((fail == 0 ? 0 : 1))
