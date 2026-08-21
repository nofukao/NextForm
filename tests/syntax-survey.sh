#!/bin/bash
# wiki 記法の棚卸しツールのテスト
#
#   ./tests/syntax-survey.sh
#
# 環境変数:
#   NF_SITE            複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   SURVEY_TEST_SITE   検証用に作るサイト (既定: /var/www/html/nf-survey-test)
#   WIKI_ADMIN         管理者ユーザー名   (既定: admin)
#   KEEP=1             終了後に検証サイトを消さない
#
# app/tool/wiki_syntax_survey は「wiki 記法のページを Markdown に変換したら
# 何が失われるか」を数える道具で、記法を一本化するかどうかの判断材料になる。
# 数え方を間違えると、その判断ごと間違える。ここで固定するのは次の 4 つ。
#
#   1. 記法を数える。正規表現ではなくパーサに数えさせているので、
#      &pre の中に書かれた &calendar は「使われている」に入らない
#   2. 別名を展開先で分類する。&direct は中身がリンクなので写せる側に入る
#   3. ページを 3 つに分ける (そのまま / 一部落ちる / 写せない)
#   4. 写せないページがあれば終了コード 1 を返す
#
# ページを投入するので、必ず複製したサイトに対して実行する。
# 複製元には触らない。sudo が要る。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# 向け先は tests/env.local に書く (Git には入らない)。
[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
SURVEY_TEST_SITE="${SURVEY_TEST_SITE:-/var/www/html/nf-survey-test}"
WIKI_ADMIN="${WIKI_ADMIN:-admin}"

fail=0
total=0

cleanup() {
    if [[ "${KEEP:-0}" != "1" ]]; then
        sudo rm -rf "$SURVEY_TEST_SITE" 2>/dev/null
    else
        echo
        echo "KEEP=1 のため検証サイトを残しました: $SURVEY_TEST_SITE"
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

if [[ ! -d "$NF_SITE" ]]; then
    echo "複製元がありません: $NF_SITE" >&2
    echo "tests/env.local の NF_SITE を設定してください。" >&2
    exit 1
fi

echo "複製元 = $NF_SITE"
echo "検証先 = $SURVEY_TEST_SITE"
echo

sudo rm -rf "$SURVEY_TEST_SITE"
sudo cp -a "$NF_SITE" "$SURVEY_TEST_SITE"
SITE_OWNER=$(sudo stat -c '%U' "${SURVEY_TEST_SITE}/index.php")

# 複製元に配置済みのコードではなく、リポジトリの作業ツリーを検証する
# (search-index.sh と同じ理由)。
sudo rsync -a --delete "${REPO_ROOT}/NextForm/app/"      "${SURVEY_TEST_SITE}/app/"
sudo rsync -a --delete "${REPO_ROOT}/NextForm/resource/" "${SURVEY_TEST_SITE}/resource/"
sudo cp "${REPO_ROOT}/deploy/scripts/gen-pages.php" "${SURVEY_TEST_SITE}/"
sudo cp -r "${REPO_ROOT}/tests/syntax-survey-input" "${SURVEY_TEST_SITE}/"
sudo chown -R "$SITE_OWNER" "${SURVEY_TEST_SITE}/app" "${SURVEY_TEST_SITE}/resource" \
                            "${SURVEY_TEST_SITE}/gen-pages.php" \
                            "${SURVEY_TEST_SITE}/syntax-survey-input"

sudo -u "$SITE_OWNER" php "${SURVEY_TEST_SITE}/gen-pages.php" \
     "${SURVEY_TEST_SITE}/index.php" --dir "${SURVEY_TEST_SITE}/syntax-survey-input" \
     --user "$WIKI_ADMIN" > /dev/null

survey() {
    sudo -u "$SITE_OWNER" php "${SURVEY_TEST_SITE}/app/tool/wiki_syntax_survey" \
         "${SURVEY_TEST_SITE}/index.php" --base SurveyTest --user "$WIKI_ADMIN" "$@" 2>&1
}

CSV="$(survey --csv)"
RC_TEXT="$(survey --list 0)"
RC=$?

# CSV の 1 行から欄を取り出す。$1 記法名  $2 欄番号 (2=分類 3=出現 4=ページ)
field() {
    printf '%s\n' "$CSV" | awk -F, -v n="$1" -v c="$2" '$1 == n { print $c }'
}

echo "1. 記法を使っているページを数えること"
check_eq "節はどのページにもある"       "5" "$(field section 4)"
check_eq "箇条書きは 1 ページ"          "1" "$(field list 4)"
check_eq "表は 1 ページ"                "1" "$(field table 4)"
check_eq "関連リストは 1 ページ"        "1" "$(field deflist 4)"
check_eq "&include は 1 ページ"         "1" "$(field include 4)"
check_eq "&calendar は 1 ページ"        "1" "$(field calendar 4)"
check_eq "&title は 5 ページ全部にある" "5" "$(field title 4)"
# 「箇所」は塊と行の両方を数える。3 行の表なら 表 1 + 行 3 で 4。
check_eq "表の箇所は 塊 1 + 行 3"       "4" "$(field table 3)"

echo
echo "2. 整形済みテキストの中は記法として数えないこと"
# SurveyTest/Literal は &calendar と &include を行頭スペースで書いている。
# 正規表現で探す方式だと、これを「使っている」と数えてしまう。
check_eq "&calendar を使うページは 1 件だけ" "1" "$(field calendar 4)"
check_eq "&include を使うページは 1 件だけ"  "1" "$(field include 4)"

echo
echo "3. 分類"
check_eq "節はそのまま写せる"           "a" "$(field section 2)"
check_eq "表は一部が落ちる"             "b" "$(field table 2)"
check_eq "関連リストは一部が落ちる"     "b" "$(field deflist 2)"
check_eq "&include は写せない"          "c" "$(field include 2)"
check_eq "&direct は別名を展開して写せる側" "a" "$(field direct 2)"

echo
echo "4. ページの数え分け"
ratio() { printf '%s\n' "$RC_TEXT" | sed -n "s/^$1 *: \([0-9]*\) .*/\1/p"; }
check_eq "対象は 5 ページ" "5" "$(printf '%s\n' "$RC_TEXT" | sed -n 's/^種別 wiki のページ: \([0-9]*\) .*/\1/p')"
check_eq "そのまま写せるページ" "3" "$(ratio 'そのまま写せるページ')"
check_eq "一部が落ちるページ"   "1" "$(ratio '一部が落ちるページ  ')"
check_eq "写せないページ"       "1" "$(ratio '写せないページ      ')"
check_eq "写せないページ名が出る" "1" \
         "$(printf '%s\n' "$RC_TEXT" | grep -c 'SurveyTest/Blocked')"

echo
echo "5. 終了コード"
survey --csv > /dev/null; rc_blocked=$?
check_eq "写せないページがあれば 1" "1" "$rc_blocked"
sudo -u "$SITE_OWNER" php "${SURVEY_TEST_SITE}/app/tool/wiki_syntax_survey" \
     "${SURVEY_TEST_SITE}/index.php" --base SurveyTest/Clean --user "$WIKI_ADMIN" \
     > /dev/null 2>&1
check_eq "写せるページだけなら 0" "0" "$?"

echo
echo "6. 複製元に触っていないこと"
SURVEY_HEX="$(php -r 'echo bin2hex("SurveyTest");')"
check_eq "複製元にテストページが無い" "0" \
         "$(sudo find "${NF_SITE}/storage/page" -maxdepth 1 -name "${SURVEY_HEX}*" 2>/dev/null | wc -l)"

echo
if [[ $fail -eq 0 ]]; then
    echo "${total}/${total} 件すべて通りました。"
    exit 0
fi
echo "${total} 件中 ${fail} 件 失敗"
exit 1
