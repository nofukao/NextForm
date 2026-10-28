#!/bin/bash
# ページを読み書きする道具 (app/tool/page) のテスト
#
#   ./tests/page-tool.sh
#
# 環境変数:
#   NF_SITE              複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   PAGE_TOOL_TEST_SITE  検証用に作るサイト (既定: /var/www/html/nf-page-tool-test)
#   PAGE_TOOL_TEST_URL   その URL           (既定: http://localhost/nf-page-tool-test)
#   WIKI_ADMIN           管理者ユーザー名   (既定: admin)
#   KEEP=1               終了後に検証サイトを消さない
#
# AI やコマンドラインから、サーバーの上でページを読み書きする道具。
# ここで固定するのは次のとおり:
#
#   1. write で作ったページは、題名が meta に入り、一覧と検索に題名で出ること。
#      meta と本文は、Web で同じ本文を保存したときと同じになること
#      (更新者・作成者・日時・ticket を除く)
#   2. 既にあるページは --overwrite が無ければ書かないこと。上書きの前の版は、
#      同じ人が続けて書いても履歴に残ること
#   3. 実在しない名前・guest・権限の無い名前・ロック中のページには書かないこと。
#      どの場合も HTML を出さないこと
#   4. users にダイジェストが出ないこと
#   5. Web からは呼べないこと (.htaccess と、道具の側の CLI の検査の両方)
#   6. 日本語・複数行の本文と、ディレクトリが区切られる長いページ名でも、
#      read で書いたとおりに戻ること
#   7. --dry-run は何も書かずに、新規か上書きか・書き手・題名を出すこと
#   8. 使い方の誤りと、無いページの終了コード
#   9. サイトの所有者以外 (root) で動かすと断り、root の持ち物を作らないこと
#
# 資格情報をテストに置かずに済ませるため、Web からの保存は、複製したサイトの
# 「ログインしていない利用者」に write 権限を与えて行う。
#
# 権限を書き換えるので、必ず複製したサイトに対して実行する。
# 複製元には触らない。sudo が要る。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
SITE="${PAGE_TOOL_TEST_SITE:-/var/www/html/nf-page-tool-test}"
URL="${PAGE_TOOL_TEST_URL:-http://localhost/nf-page-tool-test}"
WIKI_ADMIN="${WIKI_ADMIN:-admin}"
PHP_ERROR_LOG="${PHP_ERROR_LOG:-/var/log/php-fpm/www-error.log}"

fail=0
total=0
WORK="$(mktemp -d)"
chmod 755 "$WORK"

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
    sudo -u "$SITE_OWNER" php "${SITE}/page-tool-helper.php" \
         "${SITE}/index.php" "$WIKI_ADMIN" "$@" 2>/dev/null
}

value_of() {
    printf '%s\n' "$1" | sed -n "s/^$2=//p" | head -1
}

# 道具を走らせる。標準出力を $WORK/out、標準エラーを $WORK/err に置く。
# 終了コードは $rc に入れる (呼び出し側で $? を取り損ねないように)
tool() {
    sudo -u "$SITE_OWNER" php "${SITE}/app/tool/page" "${SITE}/index.php" "$@" \
         < /dev/null > "$WORK/out" 2> "$WORK/err"
    rc=$?
}

# 本文をファイルから標準入力で渡す。$1 ファイル  $2.. 道具の引数
tool_in() {
    local file="$1"
    shift
    sudo -u "$SITE_OWNER" php "${SITE}/app/tool/page" "${SITE}/index.php" "$@" \
         < "$file" > "$WORK/out" 2> "$WORK/err"
    rc=$?
}

# 出力の JSON から値を取る
j() {
    jq -r "$1" "$WORK/out" 2>/dev/null
}

# 出力が JSON として読めるか
is_json() {
    jq -e . "$WORK/out" > /dev/null 2>&1 && echo yes || echo no
}

# 出力に HTML が混ざっているか (認証の画面など)
has_html() {
    grep -qiE '<html|<!doctype|<body|<form' "$WORK/out" "$WORK/err" && echo yes || echo no
}

uri() {
    jq -rn --arg s "$1" '$s|@uri'
}

# 画面に文字があるか。$1 URL  $2 探す文字
# **curl を grep -q にパイプでつながない。** grep が見つけた時点で終わると、
# まだ書いている curl が SIGPIPE で落ち、pipefail がそれを「無い」に変える
# (大きい画面ほど起きる。deploy/scripts/make-dist.sh の list_has() と同じ罠)。
page_has() {
    curl -sk -o "$WORK/page" "$1"
    grep -qF "$2" "$WORK/page" && echo yes || echo no
}

# Web の画面から保存したときと同じ要求を送る。$1 ページ名  $2 種別  $3 ファイル
web_write() {
    curl -sk -o /dev/null -w '%{http_code}' -X POST -H "Origin: ${ORIGIN}" \
         -d "action=write" -d "ticket=" -d "newpage_type=$2" \
         --data-urlencode "contents@$3" "${URL}/?$(uri "$1")"
}

code() {
    curl -sk -o /dev/null -w '%{http_code}' "$@"
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
sudo cp "${REPO_ROOT}/tests/page-tool-helper.php" "${SITE}/"
sudo chown -R "$SITE_OWNER" "${SITE}/app" "${SITE}/resource" "${SITE}/page-tool-helper.php"

# 置いたコードが PHP-FPM に載るのを待つ (tests/markdown.sh と同じ理由)
OPCACHE_FREQ=$(php -r 'echo (int)ini_get("opcache.revalidate_freq");' 2>/dev/null)
sleep $(( ${OPCACHE_FREQ:-2} + 1 ))

if [[ "$(value_of "$(helper guest write)" saved)" != "1" ]]; then
    echo "ログインしていない利用者に write 権限を与えられませんでした。" >&2
    exit 1
fi
if [[ "$(code "${URL}/")" != "200" ]]; then
    echo "検証サイトが $URL で見えません。" >&2
    echo "tests/env.local の PAGE_TOOL_TEST_URL を設定してください。" >&2
    exit 1
fi
ORIGIN=$(printf '%s' "$URL" | sed -E 's#^(https?://[^/]+).*#\1#')
log_before=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')
log_before="${log_before:-0}"

# 検証用の利用者。パスワードは毎回作る使い捨て (ダイジェストが出ないことを見るため)
helper set-user nfwriter write "$(head -c 12 /dev/urandom | base64)" > /dev/null
helper set-user nfreader read > /dev/null
helper set-user nfpartial '{"PageToolSecret":"keepout","*":"write"}' > /dev/null

# 本文
cat > "$WORK/md.txt" <<'EOF'
---
title: 道具で書いた題名
tags: [道具, 試験]
---
# 見出し

本文です。検索語はぺーじつーるけんさく。

| 列 | 長い列の見出し |
|---|---|
| あ | い |
EOF
cat > "$WORK/md-heading.txt" <<'EOF'
# 見出しから取る題名

フロントマターの無い本文。
EOF
cat > "$WORK/wiki.txt" <<'EOF'
&title ウィキの題名
*見出し
|あ|いいいい|
|ううううう|え|
EOF
printf '%s\n' '&pages([[PageToolTest]]);' > "$WORK/list.txt"
printf '%s\n' '書き直した本文。' > "$WORK/other.txt"
printf '%s\n' '本文。' > "$WORK/plain.txt"
printf '' > "$WORK/empty.txt"
printf '\xff\xfe\x00壊れた' > "$WORK/broken.txt"
printf '一行目\r\n二行目\r\n' > "$WORK/crlf.txt"
chmod 644 "$WORK"/*.txt


echo "1. write で作ったページは、題名が meta に入り、一覧と検索に題名で出ること"
tool_in "$WORK/md.txt" write PageToolTest/Md --user "$WIKI_ADMIN"
check_eq "終了コード 0" "0" "$rc"
check_eq "  出力は JSON で、新規に作った" "yes|create|true" "$(is_json)|$(j .action)|$(j .written)"
check_eq "  meta の題名" '"道具で書いた題名"' \
         "$(value_of "$(helper meta PageToolTest/Md)" meta | jq -c .title)"
tool read PageToolTest/Md --user "$WIKI_ADMIN"
check_eq "  read の題名と種別" "道具で書いた題名|markdown" "$(j .title)|$(j .type)"
tool list PageToolTest --user "$WIKI_ADMIN"
check_eq "  list に題名で出る" "道具で書いた題名" \
         "$(j '.[] | select(.name == "PageToolTest/Md") | .title')"
tool_in "$WORK/list.txt" write PageToolList --type wiki --user "$WIKI_ADMIN"
check_eq "  子ページの一覧 (&pages) に題名で出る" "yes" \
         "$(page_has "${URL}/?PageToolList" '道具で書いた題名')"
check_eq "  検索の結果に題名で出る" "yes" \
         "$(page_has "${URL}/?option=search&query=$(uri 'ぺーじつーるけんさく')" \
                     '道具で書いた題名 (PageToolTest/Md)')"
tool_in "$WORK/md-heading.txt" write PageToolTest/Heading --user "$WIKI_ADMIN"
check_eq "  先頭の # も題名になる" '"見出しから取る題名"' \
         "$(value_of "$(helper meta PageToolTest/Heading)" meta | jq -c .title)"

# 同じ本文を Web の画面からも保存して比べる
for kind in md:markdown md-heading:markdown wiki:wiki; do
    file="${kind%%:*}"
    type="${kind#*:}"
    tool_in "$WORK/${file}.txt" write "PageToolTest/Tool-${file}" --type "$type" --user "$WIKI_ADMIN"
    check_eq "${file} (${type}) を道具で書く" "0" "$rc"
    check_eq "  Web から保存する (302)" "302" "$(web_write "PageToolTest/Web-${file}" "$type" "$WORK/${file}.txt")"
    tool_meta=$(value_of "$(helper meta "PageToolTest/Tool-${file}")" meta)
    web_meta=$(value_of "$(helper meta "PageToolTest/Web-${file}")" meta)
    check_eq "  meta が Web と同じ" "$web_meta" "$tool_meta"
    check_eq "  本文が Web と同じ" \
             "$(value_of "$(helper body-sha "PageToolTest/Web-${file}")" sha)" \
             "$(value_of "$(helper body-sha "PageToolTest/Tool-${file}")" sha)"
done
check_eq "  Wiki 記法の題名も入る (比べた meta が空でない)" '"ウィキの題名"' \
         "$(value_of "$(helper meta PageToolTest/Tool-wiki)" meta | jq -c .title)"
tool_in "$WORK/crlf.txt" write PageToolTest/Tool-crlf --user "$WIKI_ADMIN"
web_write PageToolTest/Web-crlf markdown "$WORK/crlf.txt" > /dev/null
check_eq "  CRLF の本文も Web と同じ (LF になる)" \
         "$(value_of "$(helper body-sha PageToolTest/Web-crlf)" sha)" \
         "$(value_of "$(helper body-sha PageToolTest/Tool-crlf)" sha)"
check_eq "    Web の側でも CR が落ちている (比べ方の前提)" \
         "$(printf '一行目\n二行目\n' | sha256sum | cut -d' ' -f1)" \
         "$(value_of "$(helper body-sha PageToolTest/Web-crlf)" sha)"
echo


echo "2. 既にあるページは --overwrite が無ければ書かないこと"
before=$(value_of "$(helper body-sha PageToolTest/Md)" sha)
backups=$(value_of "$(helper backups PageToolTest/Md)" backups)
tool_in "$WORK/other.txt" write PageToolTest/Md --user "$WIKI_ADMIN"
check_eq "--overwrite なし: 終了コード 1、理由は exists" "1|exists" "$rc|$(j .error)"
check_eq "  本文は変わらない" "$before" "$(value_of "$(helper body-sha PageToolTest/Md)" sha)"
tool_in "$WORK/other.txt" write PageToolTest/Md --user "$WIKI_ADMIN" --overwrite
check_eq "--overwrite あり: 終了コード 0、上書きした" "0|overwrite|true" "$rc|$(j .action)|$(j .written)"
check_eq "  本文が変わる" "$(sha256sum < "$WORK/other.txt" | cut -d' ' -f1)" \
         "$(value_of "$(helper body-sha PageToolTest/Md)" sha)"
check_eq "  題名は本文から取り直す (無くなる)" "null" \
         "$(value_of "$(helper meta PageToolTest/Md)" meta | jq -c .title)"
check_eq "  前の版が履歴に残る (同じ人が続けて書いても)" "$((backups + 1))" \
         "$(value_of "$(helper backups PageToolTest/Md)" backups)"
tool_in "$WORK/md.txt" write PageToolTest/Md --user "$WIKI_ADMIN" --overwrite
check_eq "  もう一度上書きしても版が増える" "$((backups + 2))" \
         "$(value_of "$(helper backups PageToolTest/Md)" backups)"
echo


echo "3. 書き手の名前と権限が足りなければ書かないこと。HTML を出さないこと"
# $1 説明  $2 期待する理由  $3.. 道具の引数
refuse() {
    local label="$1" reason="$2"
    shift 2
    tool_in "$WORK/plain.txt" "$@"
    check_eq "$label: 終了コード 1、理由は $reason" "1|$reason" "$rc|$(j .error)"
    check_eq "  HTML を出さない" "no" "$(has_html)"
}
refuse "実在しない名前"   unknown_user  write PageToolTest/Nobody --user no-such-user
check_eq "  ページはできていない" "0" "$(value_of "$(helper exists PageToolTest/Nobody)" exists)"
refuse "guest の名前"     guest_user    write PageToolTest/Nobody --user '$default$'
refuse "read の権限だけ"  no_permission write PageToolTest/Nobody --user nfreader
check_eq "  ページはできていない" "0" "$(value_of "$(helper exists PageToolTest/Nobody)" exists)"
tool_in "$WORK/plain.txt" write PageToolTest/Nobody
check_eq "--user なし: 使い方の誤り (2)" "2" "$rc"
check_eq "  ページはできていない" "0" "$(value_of "$(helper exists PageToolTest/Nobody)" exists)"
tool_in "$WORK/plain.txt" write PageToolTest/Locked --user "$WIKI_ADMIN"
check_eq "ロックの準備" "1" "$(value_of "$(helper lock PageToolTest/Locked)" locked)"
before=$(value_of "$(helper body-sha PageToolTest/Locked)" sha)
refuse "ロック中のページ (ロックを外せない人)" locked write PageToolTest/Locked --user nfwriter --overwrite
check_eq "  本文は変わらない" "$before" "$(value_of "$(helper body-sha PageToolTest/Locked)" sha)"
tool_in "$WORK/plain.txt" write PageToolTest/ByWriter --user nfwriter
check_eq "write の権限があれば書ける" "0|nfwriter" "$rc|$(j .user)"
refuse "組み込みマニュアル" manual write NextFormManual/ja --user "$WIKI_ADMIN" --overwrite
# 本文が参照するページ (&mtime の引数) を読めないと、変換の途中で auth_check() が
# 認証の画面を出して終わる。画面の代わりに JSON を出し、何も書かないこと
tool_in "$WORK/plain.txt" write PageToolSecret --user "$WIKI_ADMIN"
printf '%s\n' '&mtime([[PageToolSecret]]);' > "$WORK/mtime.txt"
chmod 644 "$WORK/mtime.txt"
tool_in "$WORK/mtime.txt" write PageToolTest/Mtime --type wiki --user nfpartial
check_eq "参照するページを読めない: 終了コード 1、理由は interrupted" "1|interrupted" "$rc|$(j .error)"
check_eq "  HTML を出さない" "no" "$(has_html)"
check_eq "  ページはできていない" "0" "$(value_of "$(helper exists PageToolTest/Mtime)" exists)"
tool list --user no-such-user
check_eq "list でも実在しない名前は断る" "1|unknown_user" "$rc|$(j .error)"
helper guest keepout > /dev/null
tool read PageToolTest/ByWriter
check_eq "--user なしで読めないページ: 理由は no_permission" "1|no_permission|no" "$rc|$(j .error)|$(has_html)"
helper guest write > /dev/null
echo


echo "4. users にダイジェストが出ないこと"
tool users
check_eq "終了コード 0、JSON" "0|yes" "$rc|$(is_json)"
check_eq "  管理者と、足した利用者が出る" "admin|write" \
         "$(j ".[] | select(.name == \"$WIKI_ADMIN\") | .permission")|$(j '.[] | select(.name == "nfwriter") | .permission')"
check_eq "  guest は guest と分かる" "true" "$(j '.[] | select(.name == "$default$") | .guest')"
cp "$WORK/out" "$WORK/users.json"
chmod 644 "$WORK/users.json"
check_eq "  どの利用者のダイジェストも出ない" "0" "$(value_of "$(helper digest-leaked "$WORK/users.json")" leaked)"
check_eq "  digest という項目も無い" "no" "$(grep -qi digest "$WORK/out" && echo yes || echo no)"
echo


echo "5. Web からは呼べないこと"
check_eq ".htaccess が app/tool/page を拒否する (403)" "403" "$(code "${URL}/app/tool/page")"
# 道具の側の検査を見るため、道具を読み込む .php を置いて Web から叩く。
# .htaccess が通すのは index.php という名前だけなので、ディレクトリを切ってそこに置く。
# 検査が無ければ、users の一覧がそのまま出る
sudo mkdir -p "${SITE}/page-tool-probe"
sudo tee "${SITE}/page-tool-probe/index.php" > /dev/null <<EOF
<?php
\$_SERVER['argv'] = array('page', dirname(__DIR__) . '/index.php', 'users');
\$argv = \$_SERVER['argv'];
include(dirname(__DIR__) . '/app/tool/page');
echo 'probe-end';
EOF
sudo chown -R "$SITE_OWNER" "${SITE}/page-tool-probe"
probe=$(curl -sk "${URL}/page-tool-probe/index.php")
check_eq "道具を Web から読み込むと、何も出さずに終わる" "" "$probe"
sudo rm -rf "${SITE}/page-tool-probe"
echo


echo "6. 日本語・複数行の本文と長いページ名でも、read で書いたとおりに戻ること"
LONG="PageToolTest/$(printf 'ながいなまえ%.0s' $(seq 1 10))"
cat > "$WORK/ja.txt" <<'EOF'
# 日本語の題名

一行目。全角スペース　と タブ	を含む。
二行目: 記号 <>&"' と絵文字 🐯。

  行頭の空白も残る。
末尾に改行が無い行
EOF
printf '%s' "$(cat "$WORK/ja.txt")" > "$WORK/ja-noeol.txt"
chmod 644 "$WORK/ja-noeol.txt"
tool_in "$WORK/ja.txt" write "$LONG" --user "$WIKI_ADMIN"
check_eq "長いページ名で書ける" "0" "$rc"
check_eq "  ディレクトリが区切られている (検査の前提)" "1" "$(value_of "$(helper chunked "$LONG")" chunked)"
tool read "$LONG" --user "$WIKI_ADMIN" --body
check_eq "  read --body が書いたとおり" "$(sha256sum < "$WORK/ja.txt" | cut -d' ' -f1)" \
         "$(sha256sum < "$WORK/out" | cut -d' ' -f1)"
tool read "$LONG" --user "$WIKI_ADMIN"
check_eq "  read の JSON の本文も書いたとおり" "$(sha256sum < "$WORK/ja.txt" | cut -d' ' -f1)" \
         "$(jq -j .body "$WORK/out" | sha256sum | cut -d' ' -f1)"
check_eq "  JSON の名前も同じ" "$LONG" "$(j .name)"
tool_in "$WORK/ja-noeol.txt" write PageToolTest/NoEol --user "$WIKI_ADMIN"
tool read PageToolTest/NoEol --user "$WIKI_ADMIN" --body
check_eq "末尾に改行が無い本文も書いたとおり" "$(sha256sum < "$WORK/ja-noeol.txt" | cut -d' ' -f1)" \
         "$(sha256sum < "$WORK/out" | cut -d' ' -f1)"
echo


echo "7. --dry-run は何も書かずに、新規か上書きか・書き手・題名を出すこと"
tool_in "$WORK/md.txt" write PageToolTest/Dry --user nfwriter --dry-run
check_eq "新規: 終了コード 0、create、書いていない" "0|create|false" "$rc|$(j .action)|$(j .written)"
check_eq "  書き手と権限、題名、タグ" "nfwriter|write|道具で書いた題名|道具,試験" \
         "$(j .user)|$(j .permission)|$(j .title)|$(j '.tags | join(",")')"
check_eq "  ページはできていない" "0" "$(value_of "$(helper exists PageToolTest/Dry)" exists)"
before=$(value_of "$(helper body-sha PageToolTest/Md)" sha)
backups=$(value_of "$(helper backups PageToolTest/Md)" backups)
tool_in "$WORK/md.txt" write PageToolTest/Md --user "$WIKI_ADMIN" --dry-run
check_eq "既にある (--overwrite なし): 終了コード 1、overwrite、理由は exists" "1|overwrite|exists" \
         "$rc|$(j .action)|$(j .error)"
tool_in "$WORK/md.txt" write PageToolTest/Md --user "$WIKI_ADMIN" --overwrite --dry-run
check_eq "既にある (--overwrite あり): 終了コード 0、本文は今と同じ" "0|overwrite|true" \
         "$rc|$(j .action)|$(j .same_body)"
check_eq "  本文も履歴も変わらない" "$before|$backups" \
         "$(value_of "$(helper body-sha PageToolTest/Md)" sha)|$(value_of "$(helper backups PageToolTest/Md)" backups)"
tool_in "$WORK/plain.txt" write PageToolTest/Dry --user nfreader --dry-run
check_eq "権限が無い: 終了コード 1、理由は no_permission" "1|no_permission" "$rc|$(j .error)"
echo


echo "8. 使い方の誤りと、無いページ"
tool read PageToolTest/NoSuchPage --user "$WIKI_ADMIN"
check_eq "無いページの read: 終了コード 3、理由は not_found" "3|not_found" "$rc|$(j .error)"
for bad in '../Escape' '/PageToolTest/Abs' 'PageToolTest//Double' 'PageToolTest/./Dot' 'PageToolTest/'; do
    tool_in "$WORK/plain.txt" write "$bad" --user "$WIKI_ADMIN"
    check_eq "ページ名 '$bad': 終了コード 2、理由は invalid_name" "2|invalid_name" "$rc|$(j .error)"
done
tool_in "$WORK/empty.txt" write PageToolTest/Empty --user "$WIKI_ADMIN"
check_eq "空の本文: 終了コード 2、理由は empty_body" "2|empty_body" "$rc|$(j .error)"
tool_in "$WORK/empty.txt" write PageToolTest/Md --user "$WIKI_ADMIN" --overwrite
check_eq "  既にあるページを空で上書きしても消さない" "2|1" \
         "$rc|$(value_of "$(helper exists PageToolTest/Md)" exists)"
tool_in "$WORK/broken.txt" write PageToolTest/Broken --user "$WIKI_ADMIN"
check_eq "UTF-8 でない本文: 終了コード 2、理由は invalid_encoding" "2|invalid_encoding" "$rc|$(j .error)"
tool_in "$WORK/plain.txt" write PageToolTest/BadType --type file --user "$WIKI_ADMIN"
check_eq "種別 file: 終了コード 2、理由は invalid_type" "2|invalid_type" "$rc|$(j .error)"
before=$(value_of "$(helper meta PageToolTest/Md)" meta)
tool_in "$WORK/wiki.txt" write PageToolTest/Md --type wiki --user "$WIKI_ADMIN" --overwrite
check_eq "既にあるページの種別と違う: 終了コード 1、理由は type_mismatch" "1|type_mismatch" "$rc|$(j .error)"
check_eq "  種別は変わらない" "$before" "$(value_of "$(helper meta PageToolTest/Md)" meta)"
tool_in "$WORK/plain.txt" write PageToolTest/Md --user "$WIKI_ADMIN" --overwrite
check_eq "  --type を省けば今の種別のまま書く" "0|markdown" "$rc|$(j .type)"
tool_in "$WORK/plain.txt" write PageToolTest/Default --user "$WIKI_ADMIN"
check_eq "新しいページの既定の種別は markdown" "0|markdown" "$rc|$(j .type)"
tool frobnicate
check_eq "知らない操作: 終了コード 2" "2" "$rc"
tool read
check_eq "ページ名が無い: 終了コード 2" "2" "$rc"
tool_in "$WORK/plain.txt" write PageToolTest/Opt --user "$WIKI_ADMIN" --no-such-option
check_eq "知らないオプション: 終了コード 2" "2" "$rc"
echo


echo "9. サイトの所有者以外 (root) で動かすと断ること"
sudo php "${SITE}/app/tool/page" "${SITE}/index.php" write PageToolTest/ByRoot \
     --user "$WIKI_ADMIN" < "$WORK/plain.txt" > "$WORK/out" 2> "$WORK/err"
rc=$?
check_eq "終了コード 2、理由は not_site_owner" "2|not_site_owner" "$rc|$(j .error)"
check_eq "  ページはできていない" "0" "$(value_of "$(helper exists PageToolTest/ByRoot)" exists)"
check_eq "  storage に root の持ち物ができていない" "" \
         "$(sudo find "${SITE}/storage" -user root 2>/dev/null | head -3)"
echo


echo "10. PHP の警告が出ていないこと"
tool_in "$WORK/md.txt" write PageToolTest/Quiet --user "$WIKI_ADMIN"
check_eq "道具の標準エラーが空" "" "$(cat "$WORK/err")"
log_new=$(sudo tail -n +"$((log_before + 1))" "$PHP_ERROR_LOG" 2>/dev/null \
              | grep -F "$SITE" || true)
check_eq "検証サイトの警告が増えていない" "" "$log_new"
echo

if [[ $fail -eq 0 ]]; then
    printf '%d/%d 件すべて通りました。\n' "$total" "$total"
else
    printf '%d/%d 件が失敗しました。\n' "$fail" "$total"
fi
exit $((fail == 0 ? 0 : 1))
