#!/bin/bash
# カレンダー (&calendar) のテスト
#
#   ./tests/calendar.sh
#
# 環境変数:
#   NF_SITE             複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   CALENDAR_TEST_SITE  検証用に作るサイト (既定: /var/www/html/nf-calendar-test)
#   CALENDAR_TEST_URL   その URL           (既定: http://localhost/nf-calendar-test)
#   WIKI_ADMIN          管理者ユーザー名   (既定: admin)
#   PHP_ERROR_LOG       PHP のエラーログ   (既定: /var/log/php-fpm/www-error.log)
#   KEEP=1              終了後に検証サイトを消さない
#
# 設計は docs/development/design-calendar-day-pages.md。ここで固定するのは次のもの。
#
#   1. URL の日付 (?calendar=) を、サイトの時間帯にかかわらずその日として読むこと (§10.2)。
#      複製したサイトの時間帯をニューヨークにして見る。以前は 2026-08-06 を
#      2026-08-06-01-01 と補い、最後の -01 を UTC-1 時間と読んで前の日になっていた
#      (月の表示で ▶ を押しても次の月へ進めなかった)。夏時間の切り替えをまたいでも、
#      表の日付が重なったり抜けたりしないこと
#   2. PHP の警告が出ないこと
#
# ページを作るので、必ず複製したサイトに対して実行する。
# 複製元には触らない。sudo が要る。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
CALENDAR_TEST_SITE="${CALENDAR_TEST_SITE:-/var/www/html/nf-calendar-test}"
CALENDAR_TEST_URL="${CALENDAR_TEST_URL:-http://localhost/nf-calendar-test}"
WIKI_ADMIN="${WIKI_ADMIN:-admin}"
PHP_ERROR_LOG="${PHP_ERROR_LOG:-/var/log/php-fpm/www-error.log}"
SITE="$CALENDAR_TEST_SITE"
URL="$CALENDAR_TEST_URL"

fail=0
total=0

cleanup() {
    rm -f "${HELPER_ERR:-}"
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

# ヘルパの標準エラーは捨てずに貯める (保存は CLI で走るので、PHP-FPM のログには出ない)
HELPER_ERR="$(mktemp)"
helper() {
    sudo -u "$SITE_OWNER" php -d memory_limit=512M \
        "${SITE}/calendar-helper.php" "${SITE}/index.php" "$WIKI_ADMIN" "$@" 2>>"$HELPER_ERR"
}

# <article class="main"> の中だけ
main_html() {
    curl -sk "${URL}/?$1" \
        | tr '\n' '\001' \
        | sed -e 's/.*<article class="main">//' -e 's;</article>.*;;' \
        | tr '\001' '\n'
}

# カレンダーの HTML $1 の表の日付のまとめ。
#   <その月のマスの最初>..<最後>|dup=<重なった日の数>|gap=<抜けた日の数>
# 月の表示では前後の月のマス (other_month) を除いた範囲、週の表示ではすべてのマス。
# 重なり・抜けは、表のマスを並んだ順に見て数える。
cal_summary() {
    printf '%s' "$1" | python3 -c '
import datetime, re, sys
cells = re.findall(r"<td class=\"([^\"]*)\" data-ref-pagename=\"[^\"]*/(\d{4}-\d{2}-\d{2})\"", sys.stdin.read())
if not cells:
    print("none")
    sys.exit()
days = [datetime.date.fromisoformat(d) for _, d in cells]
dup = len(days) - len(set(days))
gap = sum(1 for a, b in zip(days, days[1:]) if (b - a).days > 1)
inner = [d for c, d in cells if "other_month" not in c.split()]
print("%s..%s|dup=%d|gap=%d" % (inner[0], inner[-1], dup, gap))
'
}

# カレンダーの HTML $1 の見出しの ▶ (次の月) の行き先の calendar=
next_month_of() {
    printf '%s' "$1" | python3 -c '
import re, sys
m = re.search(r"<a href=\"[^\"]*[?&;]calendar=([0-9-]+)[^\"]*\" class=\"button next\"", sys.stdin.read())
print(m.group(1) if m else "")
'
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
sudo cp "${REPO_ROOT}/tests/calendar-helper.php" "${SITE}/"
sudo chown -R "$SITE_OWNER" "${SITE}/app" "${SITE}/resource" "${SITE}/calendar-helper.php"

# サイトの時間帯をニューヨーク (UTC-5、夏時間は UTC-4) にする。サイト設定より先に
# index.php の定義が効く (setup_local() は定義済みの値を上書きしない)
sudo sed -i "/define('DEFAULT_TIMEZONE'/d" "${SITE}/index.php"
sudo sed -i "0,/^<?php/s##<?php\ndefine('DEFAULT_TIMEZONE', 'America/New_York');#" "${SITE}/index.php"
if ! sudo grep -q "define('DEFAULT_TIMEZONE', 'America/New_York')" "${SITE}/index.php"; then
    echo "複製した index.php の時間帯を変えられませんでした。" >&2
    exit 1
fi

# 置いたコードが PHP-FPM に載るのを待つ (opcache の revalidate_freq。markdown.sh を見よ)
OPCACHE_FREQ=$(php -r 'echo (int)ini_get("opcache.revalidate_freq");' 2>/dev/null)
sleep $(( ${OPCACHE_FREQ:-2} + 1 ))

if [[ "$(curl -sk -o /dev/null -w '%{http_code}' "${URL}/")" != "200" ]]; then
    echo "検証サイトが $URL で見えません。" >&2
    echo "tests/env.local の CALENDAR_TEST_URL を設定してください。" >&2
    exit 1
fi

log_before=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')
log_before="${log_before:-0}"

P="CalendarTest"
helper write "$P/Month" wiki "$(printf -- '&calendar;\n')" > /dev/null
helper write "$P/Week" wiki "$(printf -- '&calendar(weeks,behind=0,ahead=0);\n')" > /dev/null
helper write "$P/Weeks" wiki "$(printf -- '&calendar(weeks,behind=3,ahead=3);\n')" > /dev/null

echo "1. URL の日付 (サイトの時間帯はニューヨーク)"
AUG="$(main_html "$P/Month&calendar=2026-08")"
check_eq "calendar=2026-08 は 8 月を出す" "2026-08-01..2026-08-31|dup=0|gap=0" "$(cal_summary "$AUG")"
NEXT="$(next_month_of "$AUG")"
check_eq "  ▶ の行き先は 9 月の 1 日" "2026-09-01" "$NEXT"
check_eq "  ▶ を押すと 9 月を出す (次の月へ進める)" "2026-09-01..2026-09-30|dup=0|gap=0" \
         "$(cal_summary "$(main_html "$P/Month&calendar=${NEXT:-x}")")"
check_eq "calendar=2026 は 1 月を出す" "2026-01-01..2026-01-31|dup=0|gap=0" \
         "$(cal_summary "$(main_html "$P/Month&calendar=2026")")"
check_eq "calendar=2026-8 (月を 1 桁で) も 8 月" "2026-08-01..2026-08-31|dup=0|gap=0" \
         "$(cal_summary "$(main_html "$P/Month&calendar=2026-8")")"
check_eq "calendar=2026/8/6 (区切りが /) も 8 月" "2026-08-01..2026-08-31|dup=0|gap=0" \
         "$(cal_summary "$(main_html "$P/Month&calendar=2026/8/6")")"
# 2026-08-09 は日曜日。前の日に読むと、前の週 (08-02〜08-08) が出る
check_eq "週の表示: calendar=2026-08-09 はその週を出す" "2026-08-09..2026-08-15|dup=0|gap=0" \
         "$(cal_summary "$(main_html "$P/Week&calendar=2026-08-09")")"
# 夏時間の切り替え (2026-03-08 と 2026-11-01)。表のマスは起点から 24 時間ずつ足すので、
# 起点が 0 時や 23 時台だと、切り替えをまたいだところで日付が重なったり抜けたりする
check_eq "夏時間の始まりをまたぐ月 (3 月)" "2026-03-01..2026-03-31|dup=0|gap=0" \
         "$(cal_summary "$(main_html "$P/Month&calendar=2026-03")")"
check_eq "夏時間の終わりをまたぐ月 (11 月)" "2026-11-01..2026-11-30|dup=0|gap=0" \
         "$(cal_summary "$(main_html "$P/Month&calendar=2026-11")")"
check_eq "夏時間の始まりをまたぐ週の表示" "2026-02-15..2026-04-04|dup=0|gap=0" \
         "$(cal_summary "$(main_html "$P/Weeks&calendar=2026-03-08")")"
check_eq "夏時間の終わりをまたぐ週の表示" "2026-10-11..2026-11-28|dup=0|gap=0" \
         "$(cal_summary "$(main_html "$P/Weeks&calendar=2026-11-01")")"
check_eq "読めない値は今日にする (200 で表が出る)" "200|yes" \
         "$(curl -sk -o /dev/null -w '%{http_code}' "${URL}/?$P/Month&calendar=abc")|$([[ "$(cal_summary "$(main_html "$P/Month&calendar=abc")")" != none ]] && echo yes || echo no)"
echo

echo "2. PHP の警告"
log_after=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')
log_after="${log_after:-0}"
NEW_LOG="$(sudo tail -n +"$((log_before + 1))" "$PHP_ERROR_LOG" 2>/dev/null | grep -v '^$' || true)"
check_eq "PHP-FPM のログに新しい警告が出ない" "" "$(printf '%s' "$NEW_LOG" | head -5)"
check_eq "CLI のヘルパが警告を出さない" "" "$(head -5 "$HELPER_ERR")"
echo

if [[ $fail -eq 0 ]]; then
    echo "全 ${total} 件 通過"
    exit 0
else
    echo "${fail} / ${total} 件 失敗"
    exit 1
fi
