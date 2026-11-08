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
#   2. 同じ日に複数のページ (YYYY-MM-DD, YYYY-MM-DD_2 …)。既定 (show=title) では、ページのある日の
#      日付はただの文字で、タイトルを番号順に並べ、書ける人にだけ「＋」(次の番号を作るリンク) を出す。
#      拾う名前 (_2〜_9999、先頭の 0 なし)、消したページの番号を使わないこと、読めないページ
#   3. マスからどのページにもたどれること (タイトルの無いページ。目次の見出し・書き出しのリンク)。
#      カレンダーを開いても日のページを「読んだ」にしないこと。ダブルクリックの編集の属性
#   4. show=toc (今までの目次の表示) は今のまま。知らない show= の値は既定 (タイトル)
#   5. maxlength= (マスの文字数の上限)。ページごとに数え、見た目の 1 文字を 1 とする
#   6. &datepages が _N のページも拾うこと
#   7. PHP の警告が出ないこと
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

# <article class="main"> の中だけ。中に入れ子の <article> があっても (&datepages は各ページを
# <article> で囲む) 外側の </article> まで取る。サイドの一覧などは含めない
main_nested_html() {
    curl -sk "${URL}/?$1" | python3 -c '
import re, sys
s = sys.stdin.read()
i = s.find("<article class=\"main\">")
if i < 0:
    sys.exit()
depth = 0
for m in re.finditer(r"<article\b|</article>", s[i:]):
    depth += 1 if m.group(0) != "</article>" else -1
    if depth == 0:
        print(s[i:i + m.end()])
        break
'
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

# カレンダーの HTML $1 から、日のページ $2 のマス (<td>..</td>) の中身
cell_of() {
    printf '%s' "$1" | python3 -c '
import re, sys
m = re.search(r"<td[^>]*data-ref-pagename=\"%s\"[^>]*>(.*?)</td>" % re.escape(sys.argv[1]),
              sys.stdin.read(), re.S)
print(m.group(1) if m else "")
' "$2"
}

# カレンダーの HTML $1 から、日のページ $2 のマスの開きタグ (<td ...>)
td_of() {
    printf '%s' "$1" | python3 -c '
import re, sys
m = re.search(r"<td[^>]*data-ref-pagename=\"%s\"[^>]*>" % re.escape(sys.argv[1]), sys.stdin.read())
print(m.group(0) if m else "")
' "$2"
}

# マスの HTML $1 の日付 (<dt class="day">..</dt>)
day_of() {
    printf '%s' "$1" | python3 -c '
import re, sys
m = re.search(r"<dt[^>]*class=\"day\"[^>]*>.*?</dt>", sys.stdin.read(), re.S)
print(m.group(0) if m else "")
'
}

# マスの HTML $1 のタイトル (class に title を含むリンク) の文字を、並んだ順に | でつなぐ
titles_of() {
    printf '%s' "$1" | python3 -c '
import html, re, sys
s = sys.stdin.read()
ts = re.findall(r"<a [^>]*class=\"[^\"]*\btitle\b[^\"]*\"[^>]*>(.*?)</a>", s, re.S)
print("|".join(html.unescape(re.sub(r"<[^>]*>", "", t)) for t in ts))
'
}

# マスの HTML $1 のタイトルのリンクの title 属性 (無ければ空) を、並んだ順に | でつなぐ
title_attrs_of() {
    printf '%s' "$1" | python3 -c '
import html, re, sys
s = sys.stdin.read()
out = []
for tag in re.findall(r"<a [^>]*class=\"[^\"]*\btitle\b[^\"]*\"[^>]*>", s):
    m = re.search(r"\stitle=\"([^\"]*)\"", tag)
    out.append(html.unescape(m.group(1)) if m else "")
print("|".join(out))
'
}

# マスの HTML $1 のページの項目 (li) の data-ref-pagename を、並んだ順に空白でつなぐ
items_of() {
    printf '%s' "$1" | python3 -c '
import re, sys
print(" ".join(re.findall(r"<li[^>]*data-ref-pagename=\"([^\"]*)\"", sys.stdin.read())))
'
}

# マスの HTML $1 の項目ごとに「名前:そのページへのリンクがあるか (yes/no)」を空白でつなぐ
linked_items_of() {
    printf '%s' "$1" | python3 -c '
import re, sys
s = sys.stdin.read()
starts = [(m.start(), m.group(1)) for m in re.finditer(r"<li[^>]*data-ref-pagename=\"([^\"]*)\"", s)]
out = []
for i, (pos, name) in enumerate(starts):
    end = starts[i + 1][0] if i + 1 < len(starts) else len(s)
    part = s[pos:end]
    ok = re.search(r"<a [^>]*data-link-pagename=\"%s\"" % re.escape(name), part)
    out.append("%s:%s" % (name.rsplit("/", 1)[-1], "yes" if ok else "no"))
print(" ".join(out))
'
}

# マスの HTML $1 の「＋」(class に create を含むリンク) の行き先 (無ければ空)
create_of() {
    printf '%s' "$1" | python3 -c '
import html, re, sys
m = re.search(r"<a [^>]*class=\"[^\"]*\bcreate\b[^\"]*\"[^>]*>", sys.stdin.read())
h = re.search(r"\shref=\"([^\"]*)\"", m.group(0)) if m else None
print(html.unescape(h.group(1)) if h else "")
'
}

# マスの HTML $1 の「＋」の開きタグ
create_tag_of() {
    printf '%s' "$1" | python3 -c '
import re, sys
m = re.search(r"<a [^>]*class=\"[^\"]*\bcreate\b[^\"]*\"[^>]*>", sys.stdin.read())
print(m.group(0) if m else "")
'
}

# 文字列が含まれるか (yes/no)。curl | grep -q は pipefail で落ちるので使わない
contains() {
    case "$1" in
        *"$2"*) echo yes ;;
        *)      echo no  ;;
    esac
}

value_of() {
    printf '%s\n' "$1" | sed -n "s/^$2=//p" | head -1
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

# --- 2 から 6 で使う日のページ (2026-08) ---
D="$P/Diary"
w() { helper write "$@" > /dev/null; }
# 同じ日に 4 つ。番号順に並ぶ (_10 は _3 の後)
w "$D/2026-08-03"    markdown "$(printf -- '---\nshort_title: 一つ目\n---\n\n## 朝\n\n本文\n')"
w "$D/2026-08-03_2"  markdown "$(printf -- '---\ntitle: 2026-08-03 二つ目\n---\n\n本文\n')"
w "$D/2026-08-03_3"  wiki     "$(printf -- '&title(short){三つ目}\n*節\n本文\n')"
w "$D/2026-08-03_10" markdown "$(printf -- '---\nshort_title: 十番目\n---\n\n本文\n')"
# 1 つ目が無く、_9 と _10 だけの日
w "$D/2026-08-04_9"  markdown "$(printf -- '---\nshort_title: 九番目\n---\n\n本文\n')"
w "$D/2026-08-04_10" markdown "$(printf -- '---\nshort_title: 十番目の二\n---\n\n本文\n')"
# 拾わない名前だけの日
for n in 1 02 x 10000; do
    w "$D/2026-08-05_$n" markdown "$(printf -- '---\nshort_title: 拾わない\n---\n\n本文\n')"
done
# 間を消した日 (_2 を消しても _3 は出る。「＋」は _4)
w "$D/2026-08-06"    markdown "$(printf -- '---\nshort_title: 六日\n---\n\n本文\n')"
w "$D/2026-08-06_2"  markdown "$(printf -- '---\nshort_title: 消す\n---\n\n本文\n')"
w "$D/2026-08-06_3"  markdown "$(printf -- '---\nshort_title: 残る\n---\n\n本文\n')"
helper delete "$D/2026-08-06_2" > /dev/null
# いちばん大きい番号を消した日 (消した名前を使わない。「＋」は _3)
w "$D/2026-08-07"    markdown "$(printf -- '---\nshort_title: 七日\n---\n\n本文\n')"
w "$D/2026-08-07_2"  markdown "$(printf -- '---\nshort_title: 消す七日\n---\n\n本文\n')"
helper delete "$D/2026-08-07_2" > /dev/null
# 次の番号が 4 桁を超える日 (「＋」を出さない)
w "$D/2026-08-10"      markdown "$(printf -- '---\nshort_title: 十日\n---\n\n本文\n')"
w "$D/2026-08-10_9999" markdown "$(printf -- '---\nshort_title: 最後\n---\n\n本文\n')"
# タイトルの無いページ
w "$D/2026-08-11"   wiki     "$(printf -- '*見出しA\n本文\n')"
w "$D/2026-08-11_2" wiki     "$(printf -- '見出しの無いWiki\n')"
w "$D/2026-08-11_3" markdown "$(printf -- 'Markdownの書き出し。\n')"
w "$D/2026-08-11_4" text     "$(printf -- 'テキストの書き出し\n')"
w "$D/2026-08-11_5" markdown "$(printf -- '---\ntags: [x]\n---\n')"
# 読めないページ (未ログインの人に)
w "$D/2026-08-12"   markdown "$(printf -- '---\nshort_title: 読める\n---\n\n本文\n')"
w "$D/2026-08-12_2" markdown "$(printf -- '---\nshort_title: 読めない\n---\n\n本文\n')"
w "$D/2026-08-13_2" markdown "$(printf -- '---\nshort_title: 読めないだけ\n---\n\n本文\n')"
# 文字数の上限
w "$D/2026-08-14"   markdown "$(printf -- '---\nshort_title: あいうえお\n---\n\n本文\n')"
w "$D/2026-08-14_2" markdown "$(printf -- '---\nshort_title: ab\n---\n\n本文\n')"
w "$D/2026-08-14_3" markdown "$(printf -- '---\nshort_title: 👨‍👩‍👧x🇯🇵y\n---\n\n本文\n')"
w "$D/2026-08-17"   wiki     "$(printf -- '*見出しいち\n**見出しに\n*見出しさん\n')"

w "$P/Cal"      wiki "$(printf -- '&calendar([[%s]]);\n' "$D")"
w "$P/Toc"      wiki "$(printf -- '&calendar([[%s]],show=toc);\n' "$D")"
w "$P/Unknown"  wiki "$(printf -- '&calendar([[%s]],show=foo);\n' "$D")"
w "$P/Max"      wiki "$(printf -- '&calendar([[%s]],maxlength=3);\n' "$D")"
w "$P/Max7"     wiki "$(printf -- '&calendar([[%s]],maxlength=7);\n' "$D")"
w "$P/MaxToc"   wiki "$(printf -- '&calendar([[%s]],show=toc,maxlength=7);\n' "$D")"
w "$P/MaxZero"  wiki "$(printf -- '&calendar([[%s]],maxlength=0);\n' "$D")"
w "$P/MaxNeg"   wiki "$(printf -- '&calendar([[%s]],maxlength=-1);\n' "$D")"
w "$P/MaxStr"   wiki "$(printf -- '&calendar([[%s]],maxlength=20abc);\n' "$D")"
w "$P/Dates"    wiki "$(printf -- '&datepages([[%s]],all);\n' "$D")"
w "$P/Future"   wiki "$(printf -- '&datepages([[%s]],future,2026-08-04);\n' "$D")"
w "$P/Empty"    wiki "$(printf -- '&calendar([[]]);\n')"
check_eq "未ログインの人には書かせず、読めないページを決める" "saved=1" \
         "$(helper keepout "$D/2026-08-12_2" "$D/2026-08-13_2")"

GUEST="$(main_html "$P/Cal&calendar=2026-08")"
ADMIN="$(helper render "$P/Cal" 2026-08)"

echo "2. 同じ日に複数のページ (既定 = show=title)"
C="$(cell_of "$ADMIN" "$D/2026-08-03")"
check_eq "タイトルを番号順に並べる (_10 は _3 の後)" "一つ目|2026-08-03 二つ目|三つ目|十番目" "$(titles_of "$C")"
check_eq "  ページごとの項目に、そのページの名前 (ダブルクリックで編集するページ)" \
         "$D/2026-08-03 $D/2026-08-03_2 $D/2026-08-03_3 $D/2026-08-03_10" "$(items_of "$C")"
check_eq "  項目ごとに ticket が付く" "4" \
         "$(printf '%s' "$C" | grep -o '<li[^>]*data-ref-ticket="[^"]\+"' | wc -l)"
check_eq "  日付はリンクでない (Tab と Enter で浮き出しを開ける)" "no|yes|yes" \
         "$(contains "$(day_of "$C")" '<a ')|$(contains "$(day_of "$C")" 'tabindex="0"')|$(contains "$(day_of "$C")" 'role="button"')"
check_eq "  マスは 1 つ目の ticket を持つ" "yes" "$(contains "$(td_of "$ADMIN" "$D/2026-08-03")" 'data-ref-ticket="')"
check_eq "  「＋」は書ける人に、いちばん大きい番号の次 (_11)" "?$D/2026-08-03_11" "$(create_of "$C")"
T="$(create_tag_of "$C")"
check_eq "  「＋」は nofollow、説明つき、エクスポートがたどらない" "yes|yes|no" \
         "$(contains "$T" 'rel="nofollow"')|$(contains "$T" 'aria-label="')|$(contains "$T" 'data-link-pagename')"
check_eq "  未ログインの人 (書けない) には「＋」を出さない" "" "$(create_of "$(cell_of "$GUEST" "$D/2026-08-03")")"
check_eq "  未ログインの人にもタイトルは同じに並ぶ" "一つ目|2026-08-03 二つ目|三つ目|十番目" \
         "$(titles_of "$(cell_of "$GUEST" "$D/2026-08-03")")"
C="$(cell_of "$ADMIN" "$D/2026-08-04")"
check_eq "1 つ目が無く _9 と _10 だけの日もページのある日" "九番目|十番目の二|yes" \
         "$(titles_of "$C")|$(contains "$(td_of "$ADMIN" "$D/2026-08-04")" 'exists')"
check_eq "  マスに ticket は付けない (1 つ目が無い)。「＋」は _11" "no|?$D/2026-08-04_11" \
         "$(contains "$(td_of "$ADMIN" "$D/2026-08-04")" 'data-ref-ticket')|$(create_of "$C")"
C="$(cell_of "$ADMIN" "$D/2026-08-05")"
check_eq "_1・_02・_x・5 桁の番号は拾わない (ページの無い日)" "|no|" \
         "$(titles_of "$C")|$(contains "$(td_of "$ADMIN" "$D/2026-08-05")" 'exists')|$(create_of "$C")"
check_eq "  ページの無い日の日付は、1 つ目を作るリンクのまま" "yes|yes" \
         "$(contains "$(day_of "$C")" "href=\"?$D/2026-08-05\"")|$(contains "$(day_of "$C")" 'rel="nofollow"')"
C="$(cell_of "$ADMIN" "$D/2026-08-06")"
check_eq "間の _2 を消しても _3 は出る。「＋」は _4" "六日|残る|?$D/2026-08-06_4" "$(titles_of "$C")|$(create_of "$C")"
C="$(cell_of "$ADMIN" "$D/2026-08-07")"
check_eq "消したページの番号は使わない (_2 を消した日の「＋」は _3)" "七日|?$D/2026-08-07_3" "$(titles_of "$C")|$(create_of "$C")"
C="$(cell_of "$ADMIN" "$D/2026-08-10")"
check_eq "次の番号が 4 桁を超えるなら「＋」を出さない" "十日|最後|" "$(titles_of "$C")|$(create_of "$C")"
check_eq "読めないページは並べない (未ログイン)" "読める" "$(titles_of "$(cell_of "$GUEST" "$D/2026-08-12")")"
check_eq "  読める人には並ぶ" "読める|読めない" "$(titles_of "$(cell_of "$ADMIN" "$D/2026-08-12")")"
C="$(cell_of "$GUEST" "$D/2026-08-13")"
check_eq "  読めないページだけの日は、ページの無い日と同じ" "no||yes" \
         "$(contains "$(td_of "$GUEST" "$D/2026-08-13")" 'exists')|$(titles_of "$C")|$(contains "$(day_of "$C")" "href=\"?$D/2026-08-13\"")"
check_eq "  読める人には出る" "読めないだけ" "$(titles_of "$(cell_of "$ADMIN" "$D/2026-08-13")")"
check_eq "親が空のカレンダーでも開ける" "200" "$(curl -sk -o /dev/null -w '%{http_code}' "${URL}/?$P/Empty")"
echo

echo "3. マスからどのページにもたどれる"
C="$(cell_of "$GUEST" "$D/2026-08-11")"
check_eq "タイトルの無いページにも、そのページへのリンクがある" \
         "2026-08-11:yes 2026-08-11_2:yes 2026-08-11_3:yes 2026-08-11_4:yes 2026-08-11_5:yes" "$(linked_items_of "$C")"
check_eq "  見出しのあるページは目次 (見出しへのリンク)" "yes" \
         "$(contains "$C" "href=\"?$D/2026-08-11#")"
check_eq "  文字の無いページは、ページ名の最後の部分" "yes" "$(contains "$C" '>2026-08-11_5</a>')"
check_eq "カレンダーを開いても、日のページを「読んだ」にしない" "no" \
         "$(contains "$(value_of "$(helper views "$P/Cal")" views)" "$D/")"
echo

echo "4. show=toc (今までの目次の表示) と知らない値"
C="$(cell_of "$(main_html "$P/Toc&calendar=2026-08")" "$D/2026-08-03")"
check_eq "目次を出し、日付はリンク、2 つ目以降と「＋」は出ない" "yes|yes|no|" \
         "$(contains "$C" '朝')|$(contains "$(day_of "$C")" "href=\"?$D/2026-08-03\"")|$(contains "$C" '二つ目')|$(create_of "$C")"
check_eq "知らない show= の値は既定 (タイトル)" "一つ目|2026-08-03 二つ目|三つ目|十番目" \
         "$(titles_of "$(cell_of "$(main_html "$P/Unknown&calendar=2026-08")" "$D/2026-08-03")")"
echo

echo "5. maxlength= (マスの文字数の上限)"
C="$(cell_of "$(main_html "$P/Max&calendar=2026-08")" "$D/2026-08-14")"
check_eq "ページごとに切って「…」。見た目の 1 文字を 1 と数える" "あいう…|ab|👨‍👩‍👧x🇯🇵…" "$(titles_of "$C")"
check_eq "  切ったタイトルは title 属性に全文。切らなければ付けない" "あいうえお||👨‍👩‍👧x🇯🇵y" "$(title_attrs_of "$C")"
check_eq "  ちょうど上限の文字数なら切らない" "一つ目" \
         "$(titles_of "$(cell_of "$(main_html "$P/Max&calendar=2026-08")" "$D/2026-08-03")" | cut -d'|' -f1)"
for page in Max7 MaxToc; do
    C="$(cell_of "$(main_html "$P/$page&calendar=2026-08")" "$D/2026-08-17")"
    check_eq "目次を見出しの順に数えて切る ($page)" "yes|yes|no|yes" \
             "$(contains "$C" '見出しいち')|$(contains "$C" '見出…')|$(contains "$C" '>見出しさん')|$(contains "$C" 'title="見出しいち')"
done
for page in MaxZero MaxNeg MaxStr; do
    check_eq "上限にならない値は切らない ($page)" "あいうえお" \
             "$(titles_of "$(cell_of "$(main_html "$P/$page&calendar=2026-08")" "$D/2026-08-14")" | cut -d'|' -f1)"
done
echo

echo "6. &datepages が _N のページも拾う"
DP="$(main_nested_html "$P/Dates")"
check_eq "_N のページが出る" "yes|yes" \
         "$(contains "$DP" "data-link-pagename=\"$D/2026-08-03_2\"")|$(contains "$DP" "data-link-pagename=\"$D/2026-08-04_10\"")"
check_eq "  拾わない名前は出ない" "no|no|no" \
         "$(contains "$DP" "$D/2026-08-05_x\"")|$(contains "$DP" "$D/2026-08-05_1\"")|$(contains "$DP" "$D/2026-08-05_02\"")"
DP="$(main_nested_html "$P/Future")"
check_eq "future は日付の部分で比べる (2026-08-04_9 は出て、2026-08-03_2 は出ない)" "yes|no" \
         "$(contains "$DP" "data-link-pagename=\"$D/2026-08-04_9\"")|$(contains "$DP" "data-link-pagename=\"$D/2026-08-03_2\"")"
echo

echo "7. PHP の警告"
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
