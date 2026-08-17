#!/bin/bash
# CSRF 対策とダイジェスト認証の nonce 検証のテスト
#
#   ./tests/csrf.sh
#
# 環境変数:
#   NF_SITE          複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   CSRF_TEST_SITE   検証用に作るサイト (既定: /var/www/html/nf-csrf-test)
#   CSRF_TEST_URL    その URL           (既定: http://localhost/nf-csrf-test)
#   WIKI_ADMIN       管理者ユーザー名   (既定: admin)
#   KEEP=1           終了後に検証サイトを消さない
#
# 上流には CSRF 対策が 1 つも無かった。Referer / Origin / SameSite の検査は
# コード全体に存在せず、さらに args_get() が $_GET + $_POST なので
# **GET だけでページを作ったり消したりできた**。HTTP ダイジェスト認証は
# ブラウザが資格情報を自動送信するため、ログイン中の利用者が罠サイトを
# 開くだけで成立する。<img src="…?ページ名&option=delete&action=write">
# を貼るだけでページが消える状態だった。
#
# ここで固定するのは次の 4 つ:
#
#   1. 読み取りは GET のまま通ること (退行防止。ここが壊れると wiki が使えない)
#   2. GET では状態が変わらないこと
#   3. POST は同一オリジンからのものだけ通ること
#   4. ダイジェスト認証の nonce が使い回せないこと
#
# 資格情報をテストに置かずに済ませるため、複製したサイトの
# 「ログインしていない利用者」に write 権限を与えてから検査する。
# CSRF の検査は認証の前段に入るので、認証の有無で経路は変わらない。
#
# 権限と索引を書き換えるので、必ず複製したサイトに対して実行する。
# 複製元には触らない。root で実行する必要がある。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
CSRF_TEST_SITE="${CSRF_TEST_SITE:-/var/www/html/nf-csrf-test}"
CSRF_TEST_URL="${CSRF_TEST_URL:-http://localhost/nf-csrf-test}"
WIKI_ADMIN="${WIKI_ADMIN:-admin}"

fail=0
total=0

cleanup() {
    if [[ "${KEEP:-0}" != "1" ]]; then
        sudo rm -rf "$CSRF_TEST_SITE" 2>/dev/null
    else
        echo
        echo "KEEP=1 のため検証サイトを残しました: $CSRF_TEST_SITE"
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
        "${CSRF_TEST_SITE}/csrf-helper.php" \
        "${CSRF_TEST_SITE}/index.php" "$WIKI_ADMIN" "$@" 2>/dev/null
}

# ヘルパの終了コードだけが欲しいとき
helper_rc() {
    helper "$@" > /dev/null 2>&1
    echo $?
}

value_of() {
    printf '%s\n' "$1" | sed -n "s/^$2=//p" | head -1
}

# HTTP の応答コードだけを取る。$1 以降は curl の引数
code() {
    curl -sk -o /dev/null -w '%{http_code}' "$@"
}

# 編集フォームから ticket を取る。
# page_write() は既存ページに ticket の一致を求める (編集の衝突検出)。
# ブラウザと同じようにフォームから取らないと、CSRF 対策とは無関係に
# 「衝突」で弾かれてしまい、検査にならない。
ticket_of() {
    curl -sk "${CSRF_TEST_URL}/?$1&action=edit" \
        | grep -o '<input[^>]*name="ticket"[^>]*>' \
        | sed -E 's/.*value="([^"]*)".*/\1/' | head -1
}

if [[ ! -d "$NF_SITE" ]]; then
    echo "複製元がありません: $NF_SITE" >&2
    echo "tests/env.local の NF_SITE を設定してください。" >&2
    exit 1
fi

echo "複製元 = $NF_SITE"
echo "検証先 = $CSRF_TEST_SITE"
echo "URL    = $CSRF_TEST_URL"
echo

sudo rm -rf "$CSRF_TEST_SITE"
sudo cp -a "$NF_SITE" "$CSRF_TEST_SITE"
SITE_OWNER=$(sudo stat -c '%U' "${CSRF_TEST_SITE}/index.php")

# 複製元に配置済みのコードではなく、リポジトリの作業ツリーを検証する
# (search-index.sh と同じ理由)。
sudo rsync -a --delete "${REPO_ROOT}/NextForm/app/"      "${CSRF_TEST_SITE}/app/"
sudo rsync -a --delete "${REPO_ROOT}/NextForm/resource/" "${CSRF_TEST_SITE}/resource/"
sudo cp "${REPO_ROOT}/tests/csrf-helper.php" "${CSRF_TEST_SITE}/"
sudo chown -R "$SITE_OWNER" "${CSRF_TEST_SITE}/app" "${CSRF_TEST_SITE}/resource" \
                            "${CSRF_TEST_SITE}/csrf-helper.php"

out=$(helper guest-write)
if [[ "$(value_of "$out" saved)" != "1" ]]; then
    echo "ログインしていない利用者に write 権限を与えられませんでした。" >&2
    exit 1
fi

# 検証先が HTTP で見えているかを先に確かめる。ここで落ちると
# 以降の「拒否されました」が全部その所為になり、判定にならない。
if [[ "$(code "${CSRF_TEST_URL}/")" != "200" ]]; then
    echo "検証サイトが $CSRF_TEST_URL で見えません。" >&2
    echo "tests/env.local の CSRF_TEST_URL を設定してください。" >&2
    exit 1
fi

ORIGIN=$(printf '%s' "$CSRF_TEST_URL" | sed -E 's#^(https?://[^/]+).*#\1#')
EVIL="http://evil.example.net"

helper make-page "CsrfTest/Target" "元の本文です。" > /dev/null
echo

echo "1. 読み取りは GET のまま通ること"
check_eq "トップページ"                 "200" "$(code "${CSRF_TEST_URL}/")"
check_eq "ページ表示"                   "200" "$(code "${CSRF_TEST_URL}/?CsrfTest/Target")"
check_eq "編集フォームの表示 (action=edit)" "200" "$(code "${CSRF_TEST_URL}/?CsrfTest/Target&action=edit")"
check_eq "全ページ一覧"                 "200" "$(code "${CSRF_TEST_URL}/?option=allpage")"
check_eq "検索"                         "200" "$(code "${CSRF_TEST_URL}/?option=search&query=%E6%9C%AC%E6%96%87")"
check_eq "履歴"                         "200" "$(code "${CSRF_TEST_URL}/?CsrfTest/Target&option=history")"
check_eq "削除の確認フォーム"           "200" "$(code "${CSRF_TEST_URL}/?CsrfTest/Target&option=delete")"
echo

echo "2. GET では状態が変わらないこと"
code "${CSRF_TEST_URL}/?CsrfTest/ByGet&action=write&contents=GET%20で作った" > /dev/null
check_eq "GET でページを作れない" "0" \
         "$(value_of "$(helper page-exists 'CsrfTest/ByGet')" exists)"

code "${CSRF_TEST_URL}/?CsrfTest/Target&option=delete&action=write" > /dev/null
check_eq "GET でページを消せない" "1" \
         "$(value_of "$(helper page-exists 'CsrfTest/Target')" exists)"

code "${CSRF_TEST_URL}/?CsrfTest/Target&option=lock&action=write" > /dev/null
check_eq "GET でページをロックできない" "0" \
         "$(value_of "$(helper is-locked 'CsrfTest/Target')" locked)"

check_eq "GET の状態変更は 403 を返す" "403" \
         "$(code "${CSRF_TEST_URL}/?CsrfTest/Target&option=delete&action=write")"
echo

echo "3. POST は同一オリジンからのものだけ通ること"
check_eq "Origin が無い POST は 403" "403" \
         "$(code -X POST -d "option=delete" -d "action=write" "${CSRF_TEST_URL}/?CsrfTest/Target")"
check_eq "  ページは消えていない" "1" \
         "$(value_of "$(helper page-exists 'CsrfTest/Target')" exists)"

check_eq "他サイトの Origin が付いた POST は 403" "403" \
         "$(code -X POST -H "Origin: ${EVIL}" -d "option=delete" -d "action=write" \
                 "${CSRF_TEST_URL}/?CsrfTest/Target")"
check_eq "  ページは消えていない" "1" \
         "$(value_of "$(helper page-exists 'CsrfTest/Target')" exists)"

check_eq "他サイトの Referer だけの POST は 403" "403" \
         "$(code -X POST -H "Referer: ${EVIL}/a.html" -d "option=delete" -d "action=write" \
                 "${CSRF_TEST_URL}/?CsrfTest/Target")"
check_eq "  ページは消えていない" "1" \
         "$(value_of "$(helper page-exists 'CsrfTest/Target')" exists)"

# 正しいオリジンからの POST は通る。ここが通らないと wiki が編集できない。
curl -sk -o /dev/null -X POST -H "Origin: ${ORIGIN}" \
     -d "action=write" -d "ticket=$(ticket_of 'CsrfTest/Target')" \
     --data-urlencode "contents=Origin つきで書き換えました" \
     "${CSRF_TEST_URL}/?CsrfTest/Target"
check_eq "正しい Origin の POST は通る" "Origin つきで書き換えました" \
         "$(value_of "$(helper page-body 'CsrfTest/Target')" body)"

# Origin を送らないブラウザ向けに Referer も見る。
curl -sk -o /dev/null -X POST -H "Referer: ${CSRF_TEST_URL}/?CsrfTest/Target" \
     -d "action=write" -d "ticket=$(ticket_of 'CsrfTest/Target')" \
     --data-urlencode "contents=Referer つきで書き換えました" \
     "${CSRF_TEST_URL}/?CsrfTest/Target"
check_eq "正しい Referer だけの POST も通る" "Referer つきで書き換えました" \
         "$(value_of "$(helper page-body 'CsrfTest/Target')" body)"

curl -sk -o /dev/null -X POST -H "Origin: ${ORIGIN}" \
     -d "option=delete" -d "action=write" "${CSRF_TEST_URL}/?CsrfTest/Target"
check_eq "正しい Origin なら削除も通る" "0" \
         "$(value_of "$(helper page-exists 'CsrfTest/Target')" exists)"
helper cleanup > /dev/null
echo

echo "4. ダイジェスト認証の nonce が使い回せないこと"
nonce=$(value_of "$(helper nonce-issue)" nonce)
check_eq "nonce が発行される" "yes" \
         "$([[ -n "$nonce" ]] && echo yes || echo no)"
check_eq "nonce が推測しにくい長さである" "yes" \
         "$([[ ${#nonce} -ge 32 ]] && echo yes || echo no)"

# 発行のたびに別のものでなければならない。
# 時刻だけから作っていたときは、同じ秒に 401 を受けた 2 つのクライアントが
# 同じ nonce を共有し、片方が使った nc をもう片方が「戻っている」と
# 判定されて弾かれた。ブラウザは stale を受けて再挑戦するので、
# 同じ秒に人が 2 人来ただけで永久に認証できなくなる。
nonce_a=$(value_of "$(helper nonce-issue)" nonce)
nonce_b=$(value_of "$(helper nonce-issue)" nonce)
check_eq "続けて発行した nonce が重ならない" "yes" \
         "$([[ "$nonce_a" != "$nonce_b" ]] && echo yes || echo no)"
check_eq "一方を使っても他方の nc に響かない" "ok" \
         "$(helper nonce-check "$nonce_a" 00000001 > /dev/null; \
            value_of "$(helper nonce-check "$nonce_b" 00000001)" result)"

check_eq "発行した nonce は通る"           "ok"      "$(value_of "$(helper nonce-check "$nonce" 00000001)" result)"
check_eq "同じ nc の再送は通らない"        "stale"   "$(value_of "$(helper nonce-check "$nonce" 00000001)" result)"
check_eq "戻った nc も通らない"            "stale"   "$(value_of "$(helper nonce-check "$nonce" 00000000)" result)"
check_eq "進んだ nc は通る"                "ok"      "$(value_of "$(helper nonce-check "$nonce" 00000002)" result)"
check_eq "発行していない nonce は通らない" "stale"   \
         "$(value_of "$(helper nonce-check "ffffffffffffffffffffffffffffffff" 00000001)" result)"

# 応答が正しくても nonce が通らなければ認証は成立しない。
nonce2=$(value_of "$(helper nonce-issue)" nonce)
check_eq "正しい応答 + 発行済み nonce は認証できる" "csrftestuser" \
         "$(value_of "$(helper digest-auth "$nonce2" 00000001)" user)"
check_eq "同じヘッダの再送は認証できない" "1" "$(helper_rc digest-auth "$nonce2" 00000001)"
check_eq "発行していない nonce では認証できない" "1" \
         "$(helper_rc digest-auth "ffffffffffffffffffffffffffffffff" 00000001)"
echo

echo "5. ダイジェスト認証の realm"
# 上流は 'User login' 固定で、全 NextForm サイトで同じ事前計算表が通用した。
# サイトごとに違えばその表は作り直しになる。
#
# ただし realm を変えると保存済みのダイジェストは全部無効になる
# (md5(利用者名:realm:パスワード) で作られており、認証時に平文は届かない
#  ので作り直せない)。**既存サイトで変えると全利用者が締め出される。**
# そのため決めるのはインストーラだけ。ここではその境目を固定する。
check_eq "既定は上流と同じ 'User login'" "User login" \
         "$(value_of "$(helper realm)" realm)"

realm_new=$(value_of "$(helper realm-init)" realm)
check_eq "インストーラが決めるとサイト固有になる" "yes" \
         "$([[ "$realm_new" != "User login" && -n "$realm_new" ]] && echo yes || echo no)"
check_eq "  決めたあとは同じ値が返る" "$realm_new" "$(value_of "$(helper realm)" realm)"
check_eq "  2 回目の初期化では変わらない" "$realm_new" \
         "$(value_of "$(helper realm-init)" realm)"

# realm が変わっても認証そのものは同じように働く
nonce3=$(value_of "$(helper nonce-issue)" nonce)
check_eq "新しい realm でも認証できる" "csrftestuser" \
         "$(value_of "$(helper digest-auth "$nonce3" 00000001)" user)"
echo

echo "6. 拒否したときに PHP の警告を出さないこと"
before=$(sudo wc -l "${PHP_ERROR_LOG:-/var/log/php-fpm/www-error.log}" 2>/dev/null | awk "{print \$1}")
code -X POST -H "Origin: ${EVIL}" -d "option=delete" -d "action=write" \
     "${CSRF_TEST_URL}/?CsrfTest/Nothing" > /dev/null
code "${CSRF_TEST_URL}/?CsrfTest/Nothing&action=write&contents=x" > /dev/null
after=$(sudo wc -l "${PHP_ERROR_LOG:-/var/log/php-fpm/www-error.log}" 2>/dev/null | awk "{print \$1}")
check_eq "拒否してもエラーログが増えない" "$before" "$after"
echo

if [[ $fail -eq 0 ]]; then
    printf '%d/%d 件すべて通りました。\n' "$total" "$total"
else
    printf '%d/%d 件が失敗しました。\n' "$fail" "$total"
fi
exit $((fail == 0 ? 0 : 1))
