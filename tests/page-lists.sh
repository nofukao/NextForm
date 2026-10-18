#!/bin/bash
# 一覧の画面 (削除されたページ・タグ一覧) に出るものと、その件数のテスト
#
#   ./tests/page-lists.sh
#
# 環境変数:
#   NF_SITE           複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   LISTS_TEST_SITE   検証用に作るサイト (既定: /var/www/html/nf-lists-test)
#   LISTS_TEST_URL    その URL           (既定: http://localhost/nf-lists-test)
#   WIKI_ADMIN        管理者ユーザー名   (既定: admin)
#   KEEP=1            終了後に検証サイトを消さない
#
# ここで固定するのは次の 3 つ。
#
#   1. 削除されたページの一覧に、読む権限の無いページの名前を出さないこと
#      (ページごとの権限で隠した場所の名前が、消したあとに見えていた)
#   2. タグ一覧の件数が、削除とリネームで合ったままになること
#      (削除しても減らず、検索インデックスの再構築まで多いままだった)。
#      作り直した件数と、書き込みのたびに直した件数が一致すること
#   3. タグ一覧に、読む権限の無いページのタグを出さず、件数にも数えないこと
#      (件数の控えはサイト全体で数えてあるので、ページごとの権限があると
#      読めないページの分まで見えていた)。読める人には全体の件数を出すこと
#
# ページと権限を書き換えるので、必ず複製したサイトに対して実行する。
# 複製元には触らない。root で実行する必要がある。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
LISTS_TEST_SITE="${LISTS_TEST_SITE:-/var/www/html/nf-lists-test}"
LISTS_TEST_URL="${LISTS_TEST_URL:-http://localhost/nf-lists-test}"
WIKI_ADMIN="${WIKI_ADMIN:-admin}"
PHP_ERROR_LOG="${PHP_ERROR_LOG:-/var/log/php-fpm/www-error.log}"

fail=0
total=0

cleanup() {
    if [[ "${KEEP:-0}" != "1" ]]; then
        sudo rm -rf "$LISTS_TEST_SITE" 2>/dev/null
    else
        echo
        echo "KEEP=1 のため検証サイトを残しました: $LISTS_TEST_SITE"
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
        "${LISTS_TEST_SITE}/page-lists-helper.php" \
        "${LISTS_TEST_SITE}/index.php" "$WIKI_ADMIN" "$@" 2>/dev/null
}

value_of() {
    printf '%s\n' "$1" | sed -n "s/^$2=//p" | head -1
}

code() {
    curl -sk -o /dev/null -w '%{http_code}' "$@"
}

# 削除されたページの一覧 (ログインしていない人の画面) に、そのページ名が出るか
deleted_listed() {
    curl -sk "${LISTS_TEST_URL}/?option=deletedpage" \
        | grep -o 'data-link-pagename="[^"]*"' | grep -qxF "data-link-pagename=\"$1\"" \
        && echo yes || echo no
}

# タグ一覧 (ログインしていない人の画面) のタグの件数。出ていなければ "なし"
alltag_shown() {
    curl -sk "${LISTS_TEST_URL}/?option=alltag" | python3 -c '
import html, re, sys
page = sys.stdin.read()
for tag, num in re.findall(r"<td class=\"tags\"><a [^>]*>([^<]*)</a></td><td class=\"numeric\">([0-9]+)</td>", page):
    if html.unescape(tag) == sys.argv[1]:
        print(num)
        sys.exit(0)
print("なし")
' "$1"
}

if [[ ! -d "$NF_SITE" ]]; then
    echo "複製元がありません: $NF_SITE" >&2
    echo "tests/env.local の NF_SITE を設定してください。" >&2
    exit 1
fi

echo "複製元 = $NF_SITE"
echo "検証先 = $LISTS_TEST_SITE"
echo "URL    = $LISTS_TEST_URL"
echo

sudo rm -rf "$LISTS_TEST_SITE"
sudo cp -a "$NF_SITE" "$LISTS_TEST_SITE"
SITE_OWNER=$(sudo stat -c '%U' "${LISTS_TEST_SITE}/index.php")

# 複製元に配置済みのコードではなく、リポジトリの作業ツリーを検証する
sudo rsync -a --delete "${REPO_ROOT}/NextForm/app/"      "${LISTS_TEST_SITE}/app/"
sudo rsync -a --delete "${REPO_ROOT}/NextForm/resource/" "${LISTS_TEST_SITE}/resource/"
sudo cp "${REPO_ROOT}/tests/page-lists-helper.php" "${LISTS_TEST_SITE}/"
sudo chown -R "$SITE_OWNER" "${LISTS_TEST_SITE}/app" "${LISTS_TEST_SITE}/resource" \
                            "${LISTS_TEST_SITE}/page-lists-helper.php"

# 置いたコードが PHP-FPM に載るのを待つ (tests/markdown.sh と同じ理由)
OPCACHE_FREQ=$(php -r 'echo (int)ini_get("opcache.revalidate_freq");' 2>/dev/null)
sleep $(( ${OPCACHE_FREQ:-2} + 1 ))

if [[ "$(code "${LISTS_TEST_URL}/")" != "200" ]]; then
    echo "検証サイトが $LISTS_TEST_URL で見えません。" >&2
    echo "tests/env.local の LISTS_TEST_URL を設定してください。" >&2
    exit 1
fi
log_before=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')
log_before="${log_before:-0}"

# --- 準備 ---------------------------------------------------------------------
# 複製元にもとからあるタグの件数は、ここでは見ない。このテストのタグだけを見る。
#   Open        きょうつう ひらく     残す (誰でも読める)
#   Secret      きょうつう ひみつ     残す (ログインしていない人は読めない)
#   GoneOpen    けす                 消す (誰でも読める)
#   SecretGone  ひみつ               消す (ログインしていない人は読めない)
#   Moved       うごく               Moved2 にリネームする
helper cleanup > /dev/null
helper make ListTest/Open きょうつう ひらく > /dev/null
helper make ListTest/Secret きょうつう ひみつ > /dev/null
helper make ListTest/GoneOpen けす > /dev/null
helper make ListTest/SecretGone ひみつ > /dev/null
helper make ListTest/Moved うごく > /dev/null

echo "1. タグ一覧の件数が、削除とリネームで合ったままになる"
check_eq "作った時点: きょうつう 2・ひみつ 2・けす 1" "2|2|1" \
         "$(value_of "$(helper alltag きょうつう)" count)|$(value_of "$(helper alltag ひみつ)" count)|$(value_of "$(helper alltag けす)" count)"
helper delete ListTest/GoneOpen > /dev/null
helper delete ListTest/SecretGone > /dev/null
check_eq "消したページのタグは数えない (けす 0)" "0" "$(value_of "$(helper alltag けす)" count)"
check_eq "消したページの分だけ減る (ひみつ 1)" "1" "$(value_of "$(helper alltag ひみつ)" count)"
check_eq "リネームする" "1" "$(value_of "$(helper rename ListTest/Moved ListTest/Moved2)" renamed)"
check_eq "リネームしても件数は変わらない (うごく 1)" "1" "$(value_of "$(helper alltag うごく)" count)"
before=$(helper alltags | grep -E '^(きょうつう|ひらく|ひみつ|けす|うごく)=' | paste -sd' ' -)
helper rebuild > /dev/null
after=$(helper alltags | grep -E '^(きょうつう|ひらく|ひみつ|けす|うごく)=' | paste -sd' ' -)
check_eq "作り直した件数と一致する" "$after" "$before"
echo

echo "2. 削除されたページの一覧に、読めないページの名前を出さない"
check_eq "ログインしていない人は ListTest/Secret* を読めない" "1" \
         "$(value_of "$(helper guest-keepout 'ListTest/Secret*')" saved)"
check_eq "読めるページの名前は出る (GoneOpen)" "yes" "$(deleted_listed ListTest/GoneOpen)"
check_eq "読めないページの名前は出ない (SecretGone)" "no" "$(deleted_listed ListTest/SecretGone)"
check_eq "リネームの元の名前は出る (Moved)" "yes" "$(deleted_listed ListTest/Moved)"
echo

echo "3. タグ一覧に、読めないページのタグを出さない"
check_eq "読めるページだけ数える (きょうつう 1)" "1" "$(alltag_shown きょうつう)"
check_eq "読めるページのタグは出る (ひらく 1)" "1" "$(alltag_shown ひらく)"
check_eq "読めないページにしか無いタグは出ない (ひみつ)" "なし" "$(alltag_shown ひみつ)"
check_eq "消したページのタグは出ない (けす)" "なし" "$(alltag_shown けす)"
check_eq "ログインしていない人に何も読めない権限を与える" "1" "$(value_of "$(helper guest keepout)" saved)"
check_eq "何も読めない人には何も出ない (きょうつう)" "なし" "$(alltag_shown きょうつう)"
check_eq "ログインしていない人に管理者の権限を与える" "1" "$(value_of "$(helper guest admin)" saved)"
check_eq "全部読める人には全体の件数 (きょうつう 2)" "2" "$(alltag_shown きょうつう)"
check_eq "全部読める人には全体の件数 (ひみつ 1)" "1" "$(alltag_shown ひみつ)"
echo

echo "4. PHP の警告を出さない"
helper cleanup > /dev/null
log_after=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')
check_eq "エラーログが増えない" "$log_before" "${log_after:-0}"
echo

if [[ $fail -eq 0 ]]; then
    printf '%d/%d 件すべて通りました。\n' "$total" "$total"
else
    printf '%d/%d 件が失敗しました。\n' "$fail" "$total"
fi
exit $((fail == 0 ? 0 : 1))
