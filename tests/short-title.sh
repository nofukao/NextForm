#!/bin/bash
# 短いタイトル (meta['short_title'])・カレンダーのタイトル・一覧の目次のテスト
#
#   ./tests/short-title.sh
#
# 環境変数:
#   NF_SITE                複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   SHORT_TITLE_TEST_SITE  検証用に作るサイト (既定: /var/www/html/nf-short-title-test)
#   SHORT_TITLE_TEST_URL   その URL           (既定: http://localhost/nf-short-title-test)
#   WIKI_ADMIN             管理者ユーザー名   (既定: admin)
#   PHP_ERROR_LOG          PHP のエラーログ   (既定: /var/log/php-fpm/www-error.log)
#   KEEP=1                 終了後に検証サイトを消さない
#
# 設計は docs/development/design-short-title.md。ここで固定するのは次のもの。
#
#   1. Markdown のフロントマターの short_title: が meta に入ること。title: でも先頭の # でも、
#      タイトルが無くても一緒に使える。消すと消える。並びの値は使わない
#   2. Wiki 記法の &title(short){..} が meta に入り、タイトルには触らないこと。消すと消える
#   3. &calendar(show=title) のセルが「短いタイトル → タイトル → 目次」の順になること。
#      既定と知らない値もタイトル (v0.22 から。今までの目次は show=toc)
#   4. &pages(show=toc) の項目に、閉じた折りたたみで目次が付くこと。見出しの無いページと
#      text のページには付かない。一覧を開いても各ページを「読んだ」ことにしない
#   5. show=toc で each= を書かなければ 50 件で区切ること。each=0 なら全件
#   6. メタ情報の画面に欄と断り書きが出ること。本文が決めているときは画面の値を使わない。
#      text のページでは付けられ、空にすると消える
#   7. 「この版に戻す」で短いタイトルも戻ること。&meta(short_title) が出ること。
#      取り込んだページの短いタイトルが外側のページに入らないこと。
#      Wiki 記法から Markdown への変換で、メタ情報の画面の値も short_title: になること。
#      読めないページは目次の一覧に出ないこと
#   8. PHP の警告が出ないこと (PHP-FPM のログと、CLI のヘルパの標準エラー)
#
# 作業中に見つけた別の問題の直し (docs/development/design-review-fixes.md) も、同じ複製で見る:
# &popularpages の区切り、メタ情報の画面で題名・説明を消す、読めない日のページがあるカレンダー、
# URL で指定する週表示の範囲の上限
#
# ページを作って消すので、必ず複製したサイトに対して実行する。
# 複製元には触らない。sudo が要る。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
SHORT_TITLE_TEST_SITE="${SHORT_TITLE_TEST_SITE:-/var/www/html/nf-short-title-test}"
SHORT_TITLE_TEST_URL="${SHORT_TITLE_TEST_URL:-http://localhost/nf-short-title-test}"
WIKI_ADMIN="${WIKI_ADMIN:-admin}"
PHP_ERROR_LOG="${PHP_ERROR_LOG:-/var/log/php-fpm/www-error.log}"
SITE="$SHORT_TITLE_TEST_SITE"
URL="$SHORT_TITLE_TEST_URL"

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

# ヘルパの標準エラーは捨てずに貯める。保存・メタ情報の画面・この版に戻す・変換は CLI の
# ヘルパで走るので、PHP-FPM のログには出ない警告をここで拾う (§8)
HELPER_ERR="$(mktemp)"
helper() {
    sudo -u "$SITE_OWNER" php -d memory_limit=512M \
        "${SITE}/short-title-helper.php" "${SITE}/index.php" "$WIKI_ADMIN" "$@" 2>>"$HELPER_ERR"
}

value_of() {
    printf '%s\n' "$1" | sed -n "s/^$2=//p" | head -1
}

meta_of() {
    value_of "$(helper meta "$1" "$2")" value
}

isset_of() {
    value_of "$(helper meta "$1" "$2")" isset
}

# 文字列が含まれるか (yes/no)。curl | grep -q は pipefail で落ちるので使わない
contains() {
    case "$1" in
        *"$2"*) echo yes ;;
        *)      echo no  ;;
    esac
}

# <article class="main"> の中だけ
main_html() {
    curl -sk "${URL}/?$1" \
        | tr '\n' '\001' \
        | sed -e 's/.*<article class="main">//' -e 's;</article>.*;;' \
        | tr '\001' '\n'
}

# カレンダーの HTML $1 から、日のページ $2 のセル (<td>..</td>) の中身
cell_of() {
    printf '%s' "$1" | python3 -c '
import re, sys
m = re.search(r"<td[^>]*data-ref-pagename=\"%s\"[^>]*>(.*?)</td>" % re.escape(sys.argv[1]),
              sys.stdin.read(), re.S)
print(m.group(1) if m else "")
' "$2"
}

# カレンダーの HTML $1 から、日のページ $2 のセル (<td>) の class
td_class_of() {
    printf '%s' "$1" | python3 -c '
import re, sys
m = re.search(r"<td([^>]*data-ref-pagename=\"%s\"[^>]*)>" % re.escape(sys.argv[1]), sys.stdin.read())
c = re.search(r"class=\"([^\"]*)\"", m.group(1)) if m else None
print(c.group(1) if c else "")
' "$2"
}

# カレンダーの HTML $1 の週の数 (tbody の行)
week_rows() {
    printf '%s' "$1" | python3 -c '
import re, sys
m = re.search(r"<table class=\"calendar[^\"]*\">.*?<tbody>(.*?)</tbody>", sys.stdin.read(), re.S)
print(len(re.findall(r"<tr\b", m.group(1))) if m else -1)
'
}

# 一覧の HTML $1 から、ページ $2 の項目。項目の中の目次の li には data-ref-pagename が
# 付かないので、次の項目の data-ref-pagename の手前までを返す
item_of() {
    printf '%s' "$1" | python3 -c '
import sys
s = sys.stdin.read()
i = s.find("data-ref-pagename=\"%s\"" % sys.argv[1])
if i < 0:
    print("")
else:
    j = s.find("data-ref-pagename=\"", i + 1)
    print(s[i:j] if j > 0 else s[i:])
' "$2"
}

# 一覧の HTML $1 に出た、名前が $2 で始まる項目の数
count_items() {
    printf '%s' "$1" | python3 -c '
import re, sys
print(len(re.findall(r"<li[^>]*data-ref-pagename=\"%s" % re.escape(sys.argv[1]), sys.stdin.read())))
' "$2"
}

# $1 の中で $2 が $3 より前にあるか (yes/no)
before() {
    printf '%s' "$1" | python3 -c '
import sys
s = sys.stdin.read()
a, b = s.find(sys.argv[1]), s.find(sys.argv[2])
print("yes" if 0 <= a < b else "no")
' "$2" "$3"
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
sudo cp "${REPO_ROOT}/tests/short-title-helper.php" "${SITE}/"
sudo chown -R "$SITE_OWNER" "${SITE}/app" "${SITE}/resource" "${SITE}/short-title-helper.php"

# 置いたコードが PHP-FPM に載るのを待つ (opcache の revalidate_freq。markdown.sh を見よ)
OPCACHE_FREQ=$(php -r 'echo (int)ini_get("opcache.revalidate_freq");' 2>/dev/null)
sleep $(( ${OPCACHE_FREQ:-2} + 1 ))

if [[ "$(curl -sk -o /dev/null -w '%{http_code}' "${URL}/")" != "200" ]]; then
    echo "検証サイトが $URL で見えません。" >&2
    echo "tests/env.local の SHORT_TITLE_TEST_URL を設定してください。" >&2
    exit 1
fi

log_before=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')
log_before="${log_before:-0}"

helper cleanup > /dev/null

P="ShortTitleTest"
D="$P/Cal"

# カレンダーの日のページ (2026-08)
helper write "$D/2026-08-03" markdown "$(printf -- '---\ntitle: 2026-08-03（月）長いタイトル\nshort_title: 短いタイトル md\n---\n\n## 作業\n\n本文\n')" > /dev/null
helper write "$D/2026-08-04" markdown "$(printf -- '---\ntitle: 2026-08-04（火）タイトルだけ\n---\n\n## 作業\n\n本文\n')" > /dev/null
helper write "$D/2026-08-05" wiki "$(printf -- '&title(short){短いタイトル wiki}\n&title{2026-08-05（水）Wiki のタイトル}\n*作業\n本文\n')" > /dev/null
helper write "$D/2026-08-06" wiki "$(printf -- '&title{2026-08-06（木）Wiki のタイトルだけ}\n*作業\n本文\n')" > /dev/null
helper write "$D/2026-08-07" markdown "$(printf -- '書き出し。\n\n## 見出しだけ\n\n本文\n')" > /dev/null
helper write "$D/2026-08-10" markdown "$(printf -- '見出しの無い日。\n')" > /dev/null
helper write "$D/2026-08-11" text "$(printf -- 'テキストのページ\n')" > /dev/null
helper write "$D/2026-08-12" markdown "$(printf -- '---\nshort_title: 先頭の # と一緒\n---\n\n# 2026-08-12（水）先頭の見出し\n\n## 作業\n\n本文\n')" > /dev/null

echo "1. Markdown のフロントマターの short_title:"
check_eq "title: と一緒に入る" "短いタイトル md" "$(meta_of "$D/2026-08-03" short_title)"
check_eq "  タイトルはそのまま" "2026-08-03（月）長いタイトル" "$(meta_of "$D/2026-08-03" title)"
check_eq "先頭の # と一緒に入る" "先頭の # と一緒" "$(meta_of "$D/2026-08-12" short_title)"
check_eq "  タイトルは先頭の # から" "2026-08-12（水）先頭の見出し" "$(meta_of "$D/2026-08-12" title)"
helper write "$P/MdOnly" markdown "$(printf -- '---\nshort_title: "引用符で囲む"\n---\n\n本文\n')" > /dev/null
check_eq "タイトルが無くても入る (両端の引用符は落とす)" "引用符で囲む" "$(meta_of "$P/MdOnly" short_title)"
check_eq "  タイトルは付かない" "0" "$(isset_of "$P/MdOnly" title)"
helper write "$P/MdOnly" markdown "$(printf -- '本文だけ\n')" > /dev/null
check_eq "書いていなければ消える" "0" "$(isset_of "$P/MdOnly" short_title)"
helper write "$P/MdList" markdown "$(printf -- '---\nshort_title:\n  - 並び\n  - の値\n---\n\n本文\n')" > /dev/null
check_eq "並びで書いた値は使わない" "0" "$(isset_of "$P/MdList" short_title)"
echo

echo "2. Wiki 記法の &title(short)"
check_eq "&title と一緒に入る" "短いタイトル wiki" "$(meta_of "$D/2026-08-05" short_title)"
check_eq "  タイトルは &title のまま" "2026-08-05（水）Wiki のタイトル" "$(meta_of "$D/2026-08-05" title)"
helper write "$P/WikiOnly" wiki "$(printf -- '&title(short){Wiki の短いタイトル}\n*節\n本文\n')" > /dev/null
check_eq "&title(short) だけでも入る" "Wiki の短いタイトル" "$(meta_of "$P/WikiOnly" short_title)"
check_eq "  タイトルは付かない" "0" "$(isset_of "$P/WikiOnly" title)"
W_HTML="$(main_html "$P/WikiOnly")"
check_eq "  本文にもページの見出しにも出ない" "no" "$(contains "$W_HTML" 'Wiki の短いタイトル')"
helper write "$P/WikiOnly" wiki "$(printf -- '&title(short, show){見せる短いタイトル}\n*節\n本文\n')" > /dev/null
check_eq "show を付けると本文に出る" "yes" "$(contains "$(main_html "$P/WikiOnly")" '見せる短いタイトル')"
check_eq "  meta にも入る" "見せる短いタイトル" "$(meta_of "$P/WikiOnly" short_title)"
helper write "$P/WikiOnly" wiki "$(printf -- '*節\n本文\n')" > /dev/null
check_eq "消すと消える" "0" "$(isset_of "$P/WikiOnly" short_title)"
echo

echo "3. &calendar(show=title)"
helper write "$P/Cal" wiki "$(printf -- '&calendar(show=title);\n')" > /dev/null
helper write "$P/CalDefault" wiki "$(printf -- '&calendar([[%s]]);\n' "$D")" > /dev/null
helper write "$P/CalUnknown" wiki "$(printf -- '&calendar([[%s]],show=foo);\n' "$D")" > /dev/null
CAL="$(main_html "$P/Cal&calendar=2026-08")"
C="$(cell_of "$CAL" "$D/2026-08-03")"
check_eq "短いタイトルを出す (ページへのリンク)" "yes|no" \
         "$(contains "$C" "href=\"?$D/2026-08-03\">短いタイトル md</a>")|$(contains "$C" 'class="summary"')"
check_eq "短いタイトルが無ければタイトル" "yes" \
         "$(contains "$(cell_of "$CAL" "$D/2026-08-04")" '>2026-08-04（火）タイトルだけ</a>')"
check_eq "Wiki のページの短いタイトル" "yes" \
         "$(contains "$(cell_of "$CAL" "$D/2026-08-05")" '>短いタイトル wiki</a>')"
check_eq "Wiki のページのタイトル" "yes" \
         "$(contains "$(cell_of "$CAL" "$D/2026-08-06")" '>2026-08-06（木）Wiki のタイトルだけ</a>')"
C="$(cell_of "$CAL" "$D/2026-08-07")"
check_eq "どちらも無ければ今の目次" "yes|yes" \
         "$(contains "$C" 'class="summary"')|$(contains "$C" '見出しだけ')"
C="$(cell_of "$(main_html "$P/CalDefault&calendar=2026-08")" "$D/2026-08-03")"
# v0.22 から既定は show=title (今までの目次は show=toc。tests/calendar.sh で見る)
check_eq "show を書かなければタイトル (既定)" "no|yes" \
         "$(contains "$C" 'class="summary"')|$(contains "$C" '短いタイトル md')"
C="$(cell_of "$(main_html "$P/CalUnknown&calendar=2026-08")" "$D/2026-08-03")"
check_eq "知らない値も既定 (タイトル)" "no|yes" \
         "$(contains "$C" 'class="summary"')|$(contains "$C" '短いタイトル md')"
# 週表示の範囲は URL で指定できる。上限なしだと誰でも重い表示を走らせられるので、URL の値は
# 0〜12 週に収める (docs/development/design-review-fixes.md の 6)
W="$P/Cal&calendar=2026-08-05&calendar_mode=weeks"
# (URL の 0 は「書いていない」と同じで既定の週の数になるので、上限は 12 と比べる)
WEEKS_MAX="$(week_rows "$(main_html "$W&calendar_behind=12&calendar_ahead=12")")"
check_eq "週表示: URL の範囲は上限に収める (200 で、前後 12 週と同じ週の数)" "200|$WEEKS_MAX" \
         "$(curl -sk -m 60 -o /dev/null -w '%{http_code}' "${URL}/?$W&calendar_behind=100000&calendar_ahead=100000")|$(week_rows "$(curl -sk -m 60 "${URL}/?$W&calendar_behind=100000&calendar_ahead=100000")")"
check_eq "  上限より狭い範囲はそのまま" "yes" \
         "$([[ "$(week_rows "$(main_html "$W&calendar_behind=2&calendar_ahead=1")")" -lt "$WEEKS_MAX" ]] && echo yes || echo no)"
# 本文で 12 週より広く書いた範囲は、URL (▲▼・移動) を通っても狭めない
helper write "$P/CalWide" wiki "$(printf -- '&calendar([[%s]],weeks,behind=20);\n' "$D")" > /dev/null
WIDE="$(week_rows "$(main_html "$P/CalWide&calendar=2026-08-05")")"
check_eq "  本文の範囲が 12 週より広ければ、URL の上限はその値 (▲で狭まらない)" "$WIDE|$WIDE" \
         "$(week_rows "$(main_html "$P/CalWide&calendar=2026-08-05&calendar_mode=weeks&calendar_behind=22")")|$(week_rows "$(main_html "$P/CalWide&calendar=2026-08-05&calendar_mode=weeks&calendar_behind=100000")")"
check_eq "  本文の behind= が数字でなくても保存でき、500 にしない" "1|200" \
         "$(value_of "$(helper write "$P/CalBad" wiki "$(printf -- '&calendar([[%s]],weeks,behind=abc,ahead=-2);\n' "$D")")" written)|$(curl -sk -o /dev/null -w '%{http_code}' "${URL}/?$P/CalBad")"
check_eq "  URL の calendar に並び (calendar[]=) を渡しても 500 にしない" "200" \
         "$(curl -sk -o /dev/null -w '%{http_code}' "${URL}/?$P/Cal&calendar%5B%5D=x")"
check_eq "  負の値は 0 週 (表が描ける。警告は §8 で見る)" "200|yes" \
         "$(curl -sk -o /dev/null -w '%{http_code}' "${URL}/?$W&calendar_behind=-5&calendar_ahead=-3")|$([[ "$(week_rows "$(main_html "$W&calendar_behind=-5&calendar_ahead=-3")")" -ge 1 ]] && echo yes || echo no)"
echo

echo "4. &pages(show=toc)"
helper write "$P/List" wiki "$(printf -- '&pages([[%s/]],reverse,show=toc);\n' "$D")" > /dev/null
LIST="$(main_html "$P/List")"
I="$(item_of "$LIST" "$D/2026-08-03")"
check_eq "項目は日付つきのタイトル" "yes|no" \
         "$(contains "$I" '>2026-08-03（月）長いタイトル</a>')|$(contains "$I" '短いタイトル md')"
check_eq "  その下に閉じた折りたたみ" "yes|yes|no" \
         "$(contains "$I" '<details class="pages_toc"><summary>目次</summary>')|$(before "$I" '長いタイトル</a>' '<details')|$(contains "$I" 'open')"
check_eq "  中身は目次 (見出しへのリンク)" "yes" \
         "$(contains "$I" "href=\"?$D/2026-08-03#")"
check_eq "Wiki のページでも見出しへのリンク" "yes|yes" \
         "$(contains "$(item_of "$LIST" "$D/2026-08-05")" '<details class="pages_toc">')|$(contains "$(item_of "$LIST" "$D/2026-08-05")" "href=\"?$D/2026-08-05#")"
I="$(item_of "$LIST" "$D/2026-08-10")"
check_eq "見出しの無いページには付けない" "yes|no" \
         "$(contains "$I" "href=\"?$D/2026-08-10\"")|$(contains "$I" '<details')"
I="$(item_of "$LIST" "$D/2026-08-11")"
check_eq "text のページには付けない" "yes|no" \
         "$(contains "$I" "href=\"?$D/2026-08-11\"")|$(contains "$I" '<details')"
check_eq "一覧を描いても、各ページを読んだことにしない" "" \
         "$(value_of "$(helper views "$P/List")" views)"
echo

echo "5. show=toc の区切り"
helper write-many "$P/Many" 52 > /dev/null
helper write "$P/ManyDefault" wiki "$(printf -- '&pages([[%s/Many/]],show=toc);\n' "$P")" > /dev/null
helper write "$P/ManyAll" wiki "$(printf -- '&pages([[%s/Many/]],show=toc,each=0);\n' "$P")" > /dev/null
helper write "$P/ManyThree" wiki "$(printf -- '&pages([[%s/Many/]],show=toc,each=3);\n' "$P")" > /dev/null
H="$(main_html "$P/ManyDefault")"
check_eq "each= を書かなければ 50 件で区切る" "50|yes" \
         "$(count_items "$H" "$P/Many/")|$(contains "$H" 'class="next"')"
check_eq "  次の画面に残り" "2" "$(count_items "$(main_html "$P/ManyDefault&pages=1")" "$P/Many/")"
H="$(main_html "$P/ManyAll")"
check_eq "each=0 なら全件" "52|no" \
         "$(count_items "$H" "$P/Many/")|$(contains "$H" 'class="next"')"
check_eq "each=3 なら 3 件" "3" "$(count_items "$(main_html "$P/ManyThree")" "$P/Many/")"
# 「次」のリンクの番号 (?pages=) は URL なので誰でも書き換えられる。数字でなければ 1 画面目
# &popularpages も同じ部品で区切る。「次」はそのページを指し、2 画面目が出ること
helper pageview "$D/2026-08-05" 20 > /dev/null
helper pageview "$D/2026-08-04" 10 > /dev/null
helper write "$P/Popular" wiki "$(printf -- '&popularpages([[%s/]],each=1,limit=3);\n' "$D")" > /dev/null
H="$(main_html "$P/Popular")"
check_eq "&popularpages の区切り: 1 画面目は 1 位、「次」はそのページの 2 画面目" "yes|yes" \
         "$(contains "$H" "data-ref-pagename=\"$D/2026-08-05\"")|$(contains "$H" "href=\"?$P/Popular&amp;pages=1\"")"
check_eq "  2 画面目は 2 位" "yes|no" \
         "$(contains "$(main_html "$P/Popular&pages=1")" "data-ref-pagename=\"$D/2026-08-04\"")|$(contains "$(main_html "$P/Popular&pages=1")" "data-ref-pagename=\"$D/2026-08-05\"")"
helper write "$P/PopularAll" wiki "$(printf -- '&popularpages([[%s/]]);\n' "$D")" > /dev/null
check_eq "  閲覧数の無いページは ?pages_show_hidden=true でも出さない (今までどおり)" "2|2" \
         "$(count_items "$(main_html "$P/PopularAll")" "$D/")|$(count_items "$(main_html "$P/PopularAll&pages_show_hidden=true")" "$D/")"
check_eq "  ?pages= に大きすぎる値でも 500 にしない" "200" \
         "$(curl -sk -o /dev/null -w '%{http_code}' "${URL}/?$P/ManyDefault&pages=9999999999999999999")"
check_eq "  本文の limit= が数字でなくても保存でき、500 にしない" "1|200" \
         "$(value_of "$(helper write "$P/BadLimit" wiki "$(printf -- '&pages([[%s/]],limit=abc);\n' "$D")")" written)|$(curl -sk -o /dev/null -w '%{http_code}' "${URL}/?$P/BadLimit")"
for q in abc "" -1; do
    H="$(main_html "$P/ManyDefault&pages=$q")"
    check_eq "  ?pages=$q でも 1 画面目 (500 にしない)" "200|50|yes" \
             "$(curl -sk -o /dev/null -w '%{http_code}' "${URL}/?$P/ManyDefault&pages=$q")|$(count_items "$H" "$P/Many/")|$(contains "$H" "data-ref-pagename=\"$P/Many/001\"")"
done
echo

echo "6. メタ情報の画面"
H="$(main_html "$D/2026-08-03&option=meta")"
check_eq "欄に今の値が出る" "yes" \
         "$(contains "$H" 'name="short_title" value="短いタイトル md"')"
check_eq "  Markdown では断り書きが出る" "yes" "$(contains "$H" 'フロントマターのshort_title:が優先されます')"
check_eq "  Wiki では断り書きが出る" "yes" \
         "$(contains "$(main_html "$D/2026-08-05&option=meta")" 'title(short)が優先されます')"
check_eq "  text では出ない" "yes|no" \
         "$(contains "$(main_html "$D/2026-08-11&option=meta")" 'name="short_title"')|$(contains "$(main_html "$D/2026-08-11&option=meta")" '優先されます')"
helper meta-screen "$D/2026-08-11" "テキストの短いタイトル" > /dev/null
check_eq "text のページで付けられる" "テキストの短いタイトル" "$(meta_of "$D/2026-08-11" short_title)"
check_eq "  カレンダーにも出る" "yes" \
         "$(contains "$(cell_of "$(main_html "$P/Cal&calendar=2026-08")" "$D/2026-08-11")" '>テキストの短いタイトル</a>')"
helper meta-screen "$D/2026-08-11" "" > /dev/null
check_eq "  空にすると消える" "0" "$(isset_of "$D/2026-08-11" short_title)"
helper meta-screen "$D/2026-08-03" "画面の値" > /dev/null
check_eq "Markdown で本文が決めていれば画面の値は使わない" "短いタイトル md" "$(meta_of "$D/2026-08-03" short_title)"
check_eq "  タイトルも変わらない" "2026-08-03（月）長いタイトル" "$(meta_of "$D/2026-08-03" title)"
helper meta-screen "$D/2026-08-05" "画面の値" > /dev/null
check_eq "Wiki で本文が決めていれば画面の値は使わない" "短いタイトル wiki" "$(meta_of "$D/2026-08-05" short_title)"
helper write "$P/MdPlain" markdown "$(printf -- '本文だけ\n')" > /dev/null
helper meta-screen "$P/MdPlain" "画面の値" > /dev/null
check_eq "本文が決めていなければ画面の値が入る" "画面の値" "$(meta_of "$P/MdPlain" short_title)"
helper write "$P/MdPlain" markdown "$(printf -- '本文を直した\n')" > /dev/null
check_eq "  本文を保存すると消える (本文が正)" "0" "$(isset_of "$P/MdPlain" short_title)"
# メタ情報の画面で題名と説明を空にすると消える。消した語は索引にも残らない
helper write "$P/MetaClear" wiki "$(printf -- '*節\n本文\n')" > /dev/null
helper meta-screen "$P/MetaClear" "" "消える題名ほげ" "消える説明ふが" > /dev/null
check_eq "画面で題名と説明を付けられる" "消える題名ほげ|消える説明ふが" \
         "$(meta_of "$P/MetaClear" title)|$(meta_of "$P/MetaClear" description)"
helper meta-screen "$P/MetaClear" "" "" "" > /dev/null
check_eq "  空にすると題名も説明も消える" "0|0" \
         "$(isset_of "$P/MetaClear" title)|$(isset_of "$P/MetaClear" description)"
check_eq "  消した語が検索の索引に残らない" "0" "$(value_of "$(helper index-stale "$P/MetaClear")" stale)"
helper write "$P/WikiPlain" wiki "$(printf -- '*節\n本文\n')" > /dev/null
helper meta-screen "$P/WikiPlain" "画面の値" > /dev/null
check_eq "Wiki でも本文が決めていなければ画面の値が入る" "画面の値" "$(meta_of "$P/WikiPlain" short_title)"
helper write "$P/WikiPlain" wiki "$(printf -- '*節\n本文を直した\n')" > /dev/null
check_eq "  本文を保存すると消える (本文が正)" "0" "$(isset_of "$P/WikiPlain" short_title)"
echo

echo "7. この版に戻す・&meta・変換"
helper write "$P/Restore" markdown "$(printf -- '---\nshort_title: 前の短いタイトル\n---\n\n本文\n')" > /dev/null
T="$(value_of "$(helper backup-now "$P/Restore")" backuptime)"
helper write "$P/Restore" markdown "$(printf -- '---\nshort_title: 今の短いタイトル\n---\n\n本文\n')" > /dev/null
helper restore "$P/Restore" "$T" > /dev/null
check_eq "Markdown のページは本文から戻る" "前の短いタイトル" "$(meta_of "$P/Restore" short_title)"
helper write "$P/RestoreWiki" wiki "$(printf -- '&title(short){前の Wiki}\n本文\n')" > /dev/null
T="$(value_of "$(helper backup-now "$P/RestoreWiki")" backuptime)"
helper write "$P/RestoreWiki" wiki "$(printf -- '本文だけ\n')" > /dev/null
helper restore "$P/RestoreWiki" "$T" > /dev/null
check_eq "Wiki 記法のページも本文から戻る" "前の Wiki" "$(meta_of "$P/RestoreWiki" short_title)"
helper write "$P/RestoreText" text "$(printf -- '本文\n')" > /dev/null
helper meta-screen "$P/RestoreText" "前のテキスト" > /dev/null
T="$(value_of "$(helper backup-now "$P/RestoreText")" backuptime)"
helper meta-screen "$P/RestoreText" "今のテキスト" > /dev/null
helper restore "$P/RestoreText" "$T" > /dev/null
check_eq "text のページは版の meta から戻る" "前のテキスト" "$(meta_of "$P/RestoreText" short_title)"
helper write "$P/Meta" wiki "$(printf -- '短いタイトル: &meta([[%s/2026-08-05]],short_title);\n' "$D")" > /dev/null
check_eq "&meta(short_title) が出る" "yes" \
         "$(contains "$(main_html "$P/Meta")" '短いタイトル: 短いタイトル wiki')"
# 取り込んだページ・中身ごとの一覧に出したページの短いタイトルは、外側のページに入らない
helper write "$P/Include" wiki "$(printf -- '&include([[%s/2026-08-05]]);\n&pages([[%s/]],show=contents,limit=3);\n' "$D" "$D")" > /dev/null
check_eq "&include・show=contents の先の短いタイトルは外側に入らない" "0" "$(isset_of "$P/Include" short_title)"
helper write "$P/Conv" wiki "$(printf -- '*節\n本文\n')" > /dev/null
helper meta-screen "$P/Conv" "画面の短いタイトル" > /dev/null
check_eq "変換: 本文に無ければメタ情報の値を short_title: にする" "yes" \
         "$(contains "$(value_of "$(helper to-markdown "$P/Conv")" markdown)" '---\nshort_title: 画面の短いタイトル\n---')"

# 読めないページ (ログインしていない利用者に読ませない)。権限を変えるので最後に行う
check_eq "読めないページは目次の一覧に出ない (権限を保存)" "1" \
         "$(value_of "$(helper keepout "$D/2026-08-03")" saved)"
LIST="$(main_html "$P/List")"
check_eq "  そのページの項目も目次も出ない" "no|no" \
         "$(contains "$LIST" "data-ref-pagename=\"$D/2026-08-03\"")|$(contains "$LIST" "?$D/2026-08-03#")"
check_eq "  ほかのページは出る" "yes" "$(contains "$LIST" "data-ref-pagename=\"$D/2026-08-05\"")"
# カレンダーは、日のページが 1 つ読めないだけでページ全体が読めなくなっていた
# (docs/development/design-review-fixes.md の 5)。読めない日は無いページと同じに描く
check_eq "読めない日のページがあってもカレンダーのページを開ける" "200" \
         "$(curl -sk -o /dev/null -w '%{http_code}' "${URL}/?$P/Cal&calendar=2026-08")"
CAL="$(main_html "$P/Cal&calendar=2026-08")"
C="$(cell_of "$CAL" "$D/2026-08-03")"
check_eq "  読めない日のマスは空 (exists も短いタイトルも ticket も出さない)" "no|no|no" \
         "$(contains " $(td_class_of "$CAL" "$D/2026-08-03") " ' exists ')|$(contains "$C" '短いタイトル md')|$(contains "$(printf '%s' "$CAL" | grep -o "<td[^>]*data-ref-pagename=\"$D/2026-08-03\"[^>]*>")" 'data-ref-ticket')"
check_eq "  ほかの日は出る" "yes" "$(contains "$(cell_of "$CAL" "$D/2026-08-05")" '>短いタイトル wiki</a>')"
echo

echo "8. PHP の警告が出ていないこと"
log_new=$(sudo tail -n +"$((log_before + 1))" "$PHP_ERROR_LOG" 2>/dev/null \
              | grep -F "$SITE" || true)
check_eq "検証サイトの警告が増えていない" "" "$log_new"
check_eq "ヘルパ (保存・メタ情報の画面・戻す・変換) の警告が無い" "" "$(cat "$HELPER_ERR")"
echo

helper cleanup > /dev/null

if [[ $fail -eq 0 ]]; then
    printf '%d/%d 件すべて通りました。\n' "$total" "$total"
else
    printf '%d/%d 件が失敗しました。\n' "$fail" "$total"
fi
exit $((fail == 0 ? 0 : 1))
