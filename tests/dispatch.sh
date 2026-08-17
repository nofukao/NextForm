#!/bin/bash
# ディスパッチの安全性テスト
#
#   ./tests/dispatch.sh
#
# 環境変数:
#   NF_SITE              複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   DISPATCH_TEST_SITE   検証用に作るサイト (既定: /var/www/html/nf-dispatch-test)
#   DISPATCH_TEST_URL    その URL           (既定: http://localhost/nf-dispatch-test)
#   WIKI_ADMIN           管理者ユーザー名   (既定: admin)
#   KEEP=1               終了後に検証サイトを消さない
#
# handle() と option() は、登録表のキーを全部「URL から呼べる action」として
# 扱っていた。登録表には次の 2 種類が混ざっているため、これは正しくない。
#
#   1. 画面を出す action        … show / edit / write など
#   2. 内部から使う部品          … texts / normalize
#
# 2 を URL から呼ぶと引数の数と型が合わず、上流から次の 2 つが壊れていた。
#
#   ?ページ名&action=normalize      → wiki_normalize(&$page, $text) に DOM を渡して 500
#   ?option=listedit&action=insert  → listedit_insert() が存在せず 500
#   ?ページ名&action=texts          → 500 にはならないが本文が空のページが返る
#
# ここで固定するのは次の 3 つ:
#
#   1. 呼べない action を指定しても 500 にならず、普通にページが出ること
#   2. **normalize と texts の本来の呼び出し経路が動き続けること**
#      (登録を消して直すと insert / replace / listedit / templateedit と
#       検索が壊れる。この変更でいちばん危ないのはここ)
#   3. 普通の action は今までどおり動くこと
#
# 資格情報をテストに置かずに済ませるため、複製したサイトの
# 「ログインしていない利用者」に write 権限を与えてから検査する。
#
# 権限を書き換えるので、必ず複製したサイトに対して実行する。
# 複製元には触らない。root で実行する必要がある。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
DISPATCH_TEST_SITE="${DISPATCH_TEST_SITE:-/var/www/html/nf-dispatch-test}"
DISPATCH_TEST_URL="${DISPATCH_TEST_URL:-http://localhost/nf-dispatch-test}"
WIKI_ADMIN="${WIKI_ADMIN:-admin}"

fail=0
total=0

cleanup() {
    if [[ "${KEEP:-0}" != "1" ]]; then
        sudo rm -rf "$DISPATCH_TEST_SITE" 2>/dev/null
    else
        echo
        echo "KEEP=1 のため検証サイトを残しました: $DISPATCH_TEST_SITE"
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
        "${DISPATCH_TEST_SITE}/dispatch-helper.php" \
        "${DISPATCH_TEST_SITE}/index.php" "$WIKI_ADMIN" "$@" 2>/dev/null
}

# CSRF ヘルパも使う (ログインしていない利用者に write 権限を与えるため)
csrf_helper() {
    sudo -u "$SITE_OWNER" php -d memory_limit=512M \
        "${DISPATCH_TEST_SITE}/csrf-helper.php" \
        "${DISPATCH_TEST_SITE}/index.php" "$WIKI_ADMIN" "$@" 2>/dev/null
}

value_of() {
    printf '%s\n' "$1" | sed -n "s/^$2=//p" | head -1
}

code() {
    curl -sk -o /dev/null -w '%{http_code}' "$@"
}

# 状態を変える action は POST + 同一オリジンでないと 403 になる (0.4.0)。
# ここで見たいのは「その先で落ちないこと」なので、正しいオリジンを付けて
# 検査そのものは通してから叩く。
post_same_origin() {
    curl -sk -o /dev/null -w '%{http_code}' -X POST -H "Origin: ${ORIGIN}" "$@"
}

# 本文が出ているか
has_body() {
    curl -sk "$@" | grep -c 'これは本文です' | head -1
}

if [[ ! -d "$NF_SITE" ]]; then
    echo "複製元がありません: $NF_SITE" >&2
    echo "tests/env.local の NF_SITE を設定してください。" >&2
    exit 1
fi

echo "複製元 = $NF_SITE"
echo "検証先 = $DISPATCH_TEST_SITE"
echo "URL    = $DISPATCH_TEST_URL"
echo

sudo rm -rf "$DISPATCH_TEST_SITE"
sudo cp -a "$NF_SITE" "$DISPATCH_TEST_SITE"
SITE_OWNER=$(sudo stat -c '%U' "${DISPATCH_TEST_SITE}/index.php")

# 複製元に配置済みのコードではなく、リポジトリの作業ツリーを検証する
# (search-index.sh と同じ理由)。
sudo rsync -a --delete "${REPO_ROOT}/NextForm/app/"      "${DISPATCH_TEST_SITE}/app/"
sudo rsync -a --delete "${REPO_ROOT}/NextForm/resource/" "${DISPATCH_TEST_SITE}/resource/"
sudo cp "${REPO_ROOT}/tests/dispatch-helper.php" "${REPO_ROOT}/tests/csrf-helper.php" \
        "${DISPATCH_TEST_SITE}/"
sudo chown -R "$SITE_OWNER" "${DISPATCH_TEST_SITE}/app" "${DISPATCH_TEST_SITE}/resource" \
                            "${DISPATCH_TEST_SITE}/dispatch-helper.php" \
                            "${DISPATCH_TEST_SITE}/csrf-helper.php"

out=$(csrf_helper guest-write)
if [[ "$(value_of "$out" saved)" != "1" ]]; then
    echo "ログインしていない利用者に write 権限を与えられませんでした。" >&2
    exit 1
fi

if [[ "$(code "${DISPATCH_TEST_URL}/")" != "200" ]]; then
    echo "検証サイトが $DISPATCH_TEST_URL で見えません。" >&2
    echo "tests/env.local の DISPATCH_TEST_URL を設定してください。" >&2
    exit 1
fi

ORIGIN=$(printf '%s' "$DISPATCH_TEST_URL" | sed -E 's#^(https?://[^/]+).*#\1#')
P="DispatchTest/Page"

helper make-page "$P" "これは本文です。" > /dev/null
helper make-page "DispatchTest/Table" '|a|bb|
|ccc|d|' > /dev/null
echo

echo "1. 呼べない action を指定しても 500 にならないこと"
# 比較の基準。存在しない action は昔から普通のページを返していた。
check_eq "存在しない action は 200 (基準)" "200" \
         "$(code "${DISPATCH_TEST_URL}/?${P}&action=bogus")"
check_eq "  本文が出る (基準)" "1" \
         "$(has_body "${DISPATCH_TEST_URL}/?${P}&action=bogus")"

check_eq "action=texts が 200"     "200" "$(code "${DISPATCH_TEST_URL}/?${P}&action=texts")"
check_eq "  本文が出る"            "1"   "$(has_body "${DISPATCH_TEST_URL}/?${P}&action=texts")"

check_eq "action=normalize が 200" "200" \
         "$(post_same_origin -d "action=normalize" "${DISPATCH_TEST_URL}/?${P}")"
check_eq "option=listedit&action=insert が 500 にならない" "200" \
         "$(post_same_origin -d "option=listedit" -d "action=insert" "${DISPATCH_TEST_URL}/?${P}")"

# 実体の無い登録は上流に 3 件あった。名前を数え上げるのではなく呼ぶ前に
# 実体を確かめる方式にしたので、知らなかった 1 件も同時に直っている。
# 版の比較は option=compare が担っており、こちらは生きている。
check_eq "option=history&action=diff が 500 にならない" "200" \
         "$(code "${DISPATCH_TEST_URL}/?${P}&option=history&action=diff")"
check_eq "版の比較 (option=compare) は動く" "200" \
         "$(code "${DISPATCH_TEST_URL}/?${P}&option=compare")"
echo

echo "2. 本来の呼び出し経路が動き続けること"
check_eq "handler_function() が normalize を引ける" "wiki_normalize" \
         "$(value_of "$(helper normalize-lookup "$P")" func)"
check_eq "handler_function() が texts を引ける"     "wiki_texts" \
         "$(value_of "$(helper texts-lookup "$P")" func)"

out=$(helper normalize-apply "DispatchTest/Table")
check_eq "normalize が実際に整形する"       "1" "$(value_of "$out" changed)"
check_eq "  表の桁が揃う"                   "1" "$(value_of "$out" aligned)"
check_eq "  整形結果が期待どおり"           '|a  |bb|/|ccc|d |' "$(value_of "$out" result)"

# texts は検索が使っている。引けなくなると検索が 0 件になる。
# ページ名は結果の HTML に複数回出るので、件数ではなく有無で見る。
hits=$(curl -sk "${DISPATCH_TEST_URL}/?option=search&query=%E3%81%93%E3%82%8C%E3%81%AF%E6%9C%AC%E6%96%87" \
       | grep -c "${P}")
check_eq "検索が本文を引ける" "yes" "$([[ "${hits:-0}" -ge 1 ]] && echo yes || echo no)"
echo

echo "3. 普通の action は今までどおり動くこと"
check_eq "action 指定なし"     "200" "$(code "${DISPATCH_TEST_URL}/?${P}")"
check_eq "action=show"         "200" "$(code "${DISPATCH_TEST_URL}/?${P}&action=show")"
check_eq "action=edit"         "200" "$(code "${DISPATCH_TEST_URL}/?${P}&action=edit")"
check_eq "action=source"       "200" "$(code "${DISPATCH_TEST_URL}/?${P}&action=source")"
check_eq "option=history"      "200" "$(code "${DISPATCH_TEST_URL}/?${P}&option=history")"

ticket=$(curl -sk "${DISPATCH_TEST_URL}/?${P}&action=edit" \
         | grep -o '<input[^>]*name="ticket"[^>]*>' \
         | sed -E 's/.*value="([^"]*)".*/\1/' | head -1)
curl -sk -o /dev/null -X POST -H "Origin: ${ORIGIN}" \
     -d "action=write" -d "ticket=${ticket}" \
     --data-urlencode "contents=書き換えました" "${DISPATCH_TEST_URL}/?${P}"
check_eq "action=write が通る" "書き換えました" \
         "$(value_of "$(helper page-body "$P")" body)"
helper cleanup > /dev/null
echo

echo "4. PHP の警告を出さないこと"
before=$(sudo wc -l "${PHP_ERROR_LOG:-/var/log/php-fpm/www-error.log}" 2>/dev/null | awk "{print \$1}")
code "${DISPATCH_TEST_URL}/?${P}&action=texts" > /dev/null
post_same_origin -d "action=normalize" "${DISPATCH_TEST_URL}/?${P}" > /dev/null
post_same_origin -d "option=listedit" -d "action=insert" "${DISPATCH_TEST_URL}/?${P}" > /dev/null
after=$(sudo wc -l "${PHP_ERROR_LOG:-/var/log/php-fpm/www-error.log}" 2>/dev/null | awk "{print \$1}")
check_eq "エラーログが増えない" "$before" "$after"
echo

if [[ $fail -eq 0 ]]; then
    printf '%d/%d 件すべて通りました。\n' "$total" "$total"
else
    printf '%d/%d 件が失敗しました。\n' "$fail" "$total"
fi
exit $((fail == 0 ? 0 : 1))
