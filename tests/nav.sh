#!/bin/bash
# 画面の道筋 (見出しの後ろの「(管理ツール/サイト設定)」) のテスト
#
#   ./tests/nav.sh
#
# 環境変数:
#   NF_SITE         複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   NAV_TEST_SITE   検証用に作るサイト (既定: /var/www/html/nf-nav-test)
#   NAV_TEST_URL    その URL           (既定: http://localhost/nf-nav-test)
#   KEEP=1          終了後に検証サイトを消さない
#
# 管理ツールや個人設定の下の画面には、見出しの後ろに道筋が出る。
# 一般のページが &title を付けたときと同じ小さな括弧書きで、実体は
# <small class="actual_title"> (app/title.inc)。
# 親子は各 option のファイルの $OPTION_PATHS に書いてある (app/option.inc)。
#
# パスワード設定のように入口が複数ある画面は、どこから来たかで親が変わる。
# それを見るには実際にログインする必要があるので、検証用の利用者を
# 毎回その場で作る (パスワードは使い捨て。リポジトリには置かない)。
#
# 権限とパスワードを書き換えるので、必ず複製したサイトに対して実行する。
# 複製元には触らない。root で実行する必要がある。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
NAV_TEST_SITE="${NAV_TEST_SITE:-/var/www/html/nf-nav-test}"
NAV_TEST_URL="${NAV_TEST_URL:-http://localhost/nf-nav-test}"
PHP_ERROR_LOG="${PHP_ERROR_LOG:-/var/log/php-fpm/www-error.log}"

# 検証用の利用者。パスワードは実行のたびに作り、画面にも出さない。
NAV_USER="navtestuser"
NAV_PASS="$(head -c 18 /dev/urandom | base64 | tr -d '/+=')"

fail=0
total=0

cleanup() {
    if [[ "${KEEP:-0}" != "1" ]]; then
        sudo rm -rf "$NAV_TEST_SITE" 2>/dev/null
    else
        echo
        echo "KEEP=1 のため検証サイトを残しました: $NAV_TEST_SITE"
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
    sudo -u "$SITE_OWNER" php \
        "${NAV_TEST_SITE}/site-helper.php" \
        "${NAV_TEST_SITE}/index.php" "$@" 2>/dev/null
}

value_of() {
    printf '%s\n' "$1" | sed -n "s/^$2=//p" | head -1
}

code() {
    curl -sk -o /dev/null -w '%{http_code}' "$@"
}

# $1 URL の後ろ  $2 "auth" ならログインして取る
fetch() {
    if [[ "${2:-}" == "auth" ]]; then
        # login=1 のクッキーが無いとサイトが 401 を返さず、curl が
        # 資格情報を送らない (app/auth.inc の auth_digest_get_user)。
        curl -sk --digest -u "${NAV_USER}:${NAV_PASS}" -b login=1 "${NAV_TEST_URL}/$1"
    else
        curl -sk "${NAV_TEST_URL}/$1"
    fi
}

# 見出しに出ている道筋を "名前|URL" の行で出す
path_of() {
    fetch "$1" "${2:-}" | python3 -c '
import html, re, sys
page = sys.stdin.read()
small = re.search(r"<small class=\"actual_title\">(.*?)</small>", page, re.S)
if not small:
    sys.exit(0)
for href, name in re.findall(r"<a href=\"([^\"]*)\"[^>]*>([^<]*)</a>", small.group(1)):
    print("%s|%s" % (html.unescape(name), html.unescape(href)))
'
}

# 道筋を「A/B/C」の 1 行にする
path_names() {
    path_of "$1" "${2:-}" | cut -d'|' -f1 | paste -sd/ -
}

# 道筋の $2 番目のリンク先
path_uri() {
    path_of "$1" "${3:-}" | sed -n "$2p" | cut -d'|' -f2
}

if [[ ! -d "$NF_SITE" ]]; then
    echo "複製元がありません: $NF_SITE" >&2
    exit 1
fi

echo "複製元 = $NF_SITE"
echo "検証先 = $NAV_TEST_SITE"
echo "URL    = $NAV_TEST_URL"
echo

sudo rm -rf "$NAV_TEST_SITE"
sudo cp -a "$NF_SITE" "$NAV_TEST_SITE"
SITE_OWNER=$(sudo stat -c '%U' "${NAV_TEST_SITE}/index.php")

sudo rsync -a --delete "${REPO_ROOT}/NextForm/app/"      "${NAV_TEST_SITE}/app/"
sudo rsync -a --delete "${REPO_ROOT}/NextForm/resource/" "${NAV_TEST_SITE}/resource/"
sudo cp "${REPO_ROOT}/tests/site-helper.php" "${NAV_TEST_SITE}/"
sudo chown -R "$SITE_OWNER" "${NAV_TEST_SITE}/app" "${NAV_TEST_SITE}/resource" \
                            "${NAV_TEST_SITE}/site-helper.php"

out=$(helper guest-admin)
if [[ "$(value_of "$out" saved)" != "1" ]]; then
    echo "ログインしていない利用者に admin 権限を与えられませんでした。" >&2
    exit 1
fi
out=$(helper set-password "$NAV_USER" "$NAV_PASS")
if [[ "$(value_of "$out" saved)" != "1" ]]; then
    echo "検証用の利用者を作れませんでした。" >&2
    exit 1
fi

if [[ "$(code "${NAV_TEST_URL}/")" != "200" ]]; then
    echo "検証サイトが $NAV_TEST_URL で見えません。" >&2
    exit 1
fi
if [[ "$(fetch "?option=config" auth | grep -c 'option=password')" == "0" ]]; then
    echo "検証用の利用者でログインできませんでした。" >&2
    exit 1
fi

log_before=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')

echo "1. 管理ツール直下の 8 画面に道筋が出る"
check_eq "サイト設定"       "管理ツール/サイト設定"       "$(path_names '?option=admin_setup_site')"
check_eq "外観の設定"       "管理ツール/外観の設定"       "$(path_names '?option=admin_setup_theme')"
check_eq "色調の設定"       "管理ツール/色調の設定"       "$(path_names '?option=admin_setup_tone')"
check_eq "ユーザーの管理"   "管理ツール/ユーザーの管理"   "$(path_names '?option=admin_user')"
check_eq "システム情報"     "管理ツール/システム情報"     "$(path_names '?option=admin_info')"
check_eq "検索インデックス" "管理ツール/検索インデックス" "$(path_names '?option=search_index')"
check_eq "インポート"       "管理ツール/他のwikiからページをインポート" \
                            "$(path_names '?option=admin_import_otherwiki')"
check_eq "アクセス解析"     "管理ツール/アクセス解析"     "$(path_names '?option=admin_analyze')"
echo

echo "2. 管理ツールへ戻れる"
check_eq "1 つ目が管理ツール" "?option=admin"            "$(path_uri '?option=admin_setup_site' 1)"
check_eq "戻り先が開ける"     "200" \
         "$(code "${NAV_TEST_URL}/$(path_uri '?option=admin_setup_site' 1)")"
check_eq "自分自身も出る"     "?option=admin_setup_site" "$(path_uri '?option=admin_setup_site' 2)"
echo

echo "3. 孫は 2 段になる"
# 権限設定はユーザーの管理から入る。1 クリックで戻る先も管理ツールではない。
check_eq "3 つ並ぶ"   "管理ツール/ユーザーの管理/権限設定" "$(path_names '?option=admin_permissions')"
check_eq "2 つ目が親" "?option=admin_user"                 "$(path_uri '?option=admin_permissions' 2)"
echo

echo "4. 管理ツール自身は自分だけ"
check_eq "1 つだけ"   "管理ツール"    "$(path_names '?option=admin')"
check_eq "自分を指す" "?option=admin" "$(path_uri '?option=admin' 1)"
echo

echo "5. 個人設定も同じ"
check_eq "個人設定"           "個人設定"       "$(path_names '?option=config')"
check_eq "パスワード設定"     "個人設定/パスワード設定" \
                              "$(path_names '?option=password' auth)"
check_eq "メールアドレス設定" "個人設定/メールアドレス設定" \
                              "$(path_names '?option=email' auth)"
check_eq "個人設定へ戻れる"   "?option=config" "$(path_uri '?option=password' 1 auth)"
echo

echo "6. 入口によって親が変わる"
# 他人のパスワードはユーザーの管理から入る。個人設定へ戻しても意味が無い。
check_eq "他人の分は管理ツールの下" "管理ツール/ユーザーの管理/パスワード設定" \
         "$(path_names "?option=password&user=${WIKI_ADMIN:-admin}" auth)"
check_eq "自分の分は個人設定の下"   "個人設定/パスワード設定" \
         "$(path_names "?option=password&user=${NAV_USER}" auth)"
echo

echo "7. 戻る先が無いときは出さない"
# パスワード再発行のリンクはログインしていない状態で開く。
check_eq "未ログインのパスワード設定" "" "$(path_names '?option=password')"
check_eq "未ログインの個人設定"       "個人設定" "$(path_names '?option=config')"
echo

echo "8. 関係の無い画面には出ない"
check_eq "全ページ一覧" "" "$(path_names '?option=allpage')"
check_eq "検索"         "" "$(path_names '?option=search')"
echo

echo "9. 見出しの本文は消えていない"
# 道筋は後ろに足すもので、もとの見出しを置き換えてはいけない。
check_eq "見出しが残る" "1" \
         "$(fetch '?option=admin_setup_tone' | grep -c '<h1>色調の設定 <small class="actual_title">')"
echo

echo "10. PHP の警告を出さない"
log_after=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')
check_eq "エラーログが増えない" "$log_before" "$log_after"
echo

if [[ $fail -eq 0 ]]; then
    printf '%d/%d 件すべて通りました。\n' "$total" "$total"
else
    printf '%d/%d 件が失敗しました。\n' "$fail" "$total"
fi
exit $((fail == 0 ? 0 : 1))
