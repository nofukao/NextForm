#!/bin/bash
# 検索のテキストキャッシュのテスト
#
#   ./tests/search-cache.sh
#
# 環境変数:
#   NF_SITE            複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   CACHE_TEST_SITE    検証用に作るサイト (既定: /var/www/html/nf-cache-test)
#   WIKI_ADMIN         管理者ユーザー名   (既定: admin)
#   KEEP=1             終了後に検証サイトを消さない
#
# search_page_match() は候補ページごとに wiki_texts() で本文をフルパースして
# いた。実測 (1372 ページ) で検索時間の 92〜95% がここに使われており、
# 索引の引き込みは 0.1〜2.9ms でしかなかった。**遅いのは索引ではなく、
# そのあとの本文の再パース。** 抽出結果をページごとにキャッシュする。
#
# ここで固定するのは次の 4 つ:
#
#   1. **キャッシュの有無で検索結果が変わらないこと** (件数・順序・スコア)
#   2. 古いキャッシュが使われないこと (編集・削除が即反映される)
#   3. キャッシュが壊れていても正しい結果が返ること
#      (見つからない・読めない・古い、のどれでも再計算するだけで済む。
#       間違えると結果が変わる検索インデックスとは性質が違う)
#   4. 索引を作る経路がキャッシュを書かないこと
#      (page_write() は cache_delete() のあとに旧本文の ngram を計算するので、
#       そこで書くと古い内容が残る)
#
# キャッシュを壊す検査を含むので、必ず複製したサイトに対して実行する。
# 複製元には触らない。root で実行する必要がある。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
CACHE_TEST_SITE="${CACHE_TEST_SITE:-/var/www/html/nf-cache-test}"
WIKI_ADMIN="${WIKI_ADMIN:-admin}"

fail=0
total=0

cleanup() {
    if [[ "${KEEP:-0}" != "1" ]]; then
        sudo rm -rf "$CACHE_TEST_SITE" 2>/dev/null
    else
        echo
        echo "KEEP=1 のため検証サイトを残しました: $CACHE_TEST_SITE"
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
        "${CACHE_TEST_SITE}/search-cache-helper.php" \
        "${CACHE_TEST_SITE}/index.php" "$WIKI_ADMIN" "$@" 2>/dev/null
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
echo "検証先 = $CACHE_TEST_SITE"
echo

sudo rm -rf "$CACHE_TEST_SITE"
sudo cp -a "$NF_SITE" "$CACHE_TEST_SITE"
SITE_OWNER=$(sudo stat -c '%U' "${CACHE_TEST_SITE}/index.php")

# 複製元に配置済みのコードではなく、リポジトリの作業ツリーを検証する
# (search-index.sh と同じ理由)。
sudo rsync -a --delete "${REPO_ROOT}/NextForm/app/"      "${CACHE_TEST_SITE}/app/"
sudo rsync -a --delete "${REPO_ROOT}/NextForm/resource/" "${CACHE_TEST_SITE}/resource/"
sudo cp "${REPO_ROOT}/tests/search-cache-helper.php" "${CACHE_TEST_SITE}/"
sudo chown -R "$SITE_OWNER" "${CACHE_TEST_SITE}/app" "${CACHE_TEST_SITE}/resource" \
                            "${CACHE_TEST_SITE}/search-cache-helper.php"

# 複製元に残っている索引のずれを持ち込まないよう、まっさらな索引から始める
# (search-index.sh と同じ理由)。複製元には &time を含むページなど、
# 作った時刻によって本文が変わるものがあり、そのままだと --deep が
# この変更とは無関係なずれを報告する。
echo "索引を再構築しています.."
echo "  $(helper rebuild)"
echo

# 検索の対象になるページを置く。複製元のページに引っかからないよう、
# このテストでしか使わない語を使う。語の出方を変えて、件数だけでなく
# 順序 (スコア) の違いも見えるようにする。
WORD1="Sctestalpha"
WORD2="Sctestbeta"
helper write-page "SearchCacheTest/A" "${WORD1} ${WORD2} 本文です。${WORD1} ${WORD1}" > /dev/null
helper write-page "SearchCacheTest/B" "${WORD1} ${WORD2}" > /dev/null
helper write-page "SearchCacheTest/C" "* ${WORD1}
表もいれておく
|列1|列2|
|あ|い|
${WORD2} の語も入れる" > /dev/null
helper clear-cache > /dev/null
check_eq "このテスト用の語は複製元に無い" "3" \
         "$(value_of "$(helper result "$WORD1")" count)"
echo

echo "1. キャッシュの有無で検索結果が変わらないこと"
cold=$(helper result "$WORD1")
warm=$(helper result "$WORD1")
check_eq "1 回目に結果が出る" "3" "$(value_of "$cold" count)"
check_eq "2 回目も件数が同じ" "$(value_of "$cold" count)" "$(value_of "$warm" count)"
check_eq "順序とスコアも同じ"  "$(value_of "$cold" digest)" "$(value_of "$warm" digest)"
check_eq "先頭の 1 件も同じ"   "$(value_of "$cold" first)"  "$(value_of "$warm" first)"

# 語を変えても、キャッシュを消してから引いた結果と一致すること
warm2=$(helper result "$WORD2")
helper clear-cache > /dev/null
cold2=$(helper result "$WORD2")
check_eq "別の語でも件数が一致" "$(value_of "$warm2" count)"  "$(value_of "$cold2" count)"
check_eq "別の語でも順序が一致" "$(value_of "$warm2" digest)" "$(value_of "$cold2" digest)"

# 2 語の問い合わせ
warm3=$(helper result "$WORD1 $WORD2")
helper clear-cache > /dev/null
cold3=$(helper result "$WORD1 $WORD2")
check_eq "2 語でも件数が一致" "$(value_of "$warm3" count)"  "$(value_of "$cold3" count)"
check_eq "2 語でも順序が一致" "$(value_of "$warm3" digest)" "$(value_of "$cold3" digest)"
echo

echo "2. 古いキャッシュが使われないこと"
helper result "$WORD1" > /dev/null
check_eq "検索するとキャッシュができる" "1" \
         "$(value_of "$(helper cache-of 'SearchCacheTest/A')" exists)"

helper write-page "SearchCacheTest/A" "Sctestgamma だけにしました" > /dev/null
check_eq "保存でそのページのキャッシュが消える" "0" \
         "$(value_of "$(helper cache-of 'SearchCacheTest/A')" exists)"
check_eq "消した語では引けなくなる" "2" \
         "$(value_of "$(helper result "$WORD1")" count)"
check_eq "新しい語で引ける"         "1" \
         "$(value_of "$(helper result 'Sctestgamma')" count)"

helper result "Sctestgamma" > /dev/null
helper delete-page "SearchCacheTest/A" > /dev/null
check_eq "削除でキャッシュが消える" "0" \
         "$(value_of "$(helper cache-of 'SearchCacheTest/A')" exists)"
check_eq "削除したページは出てこない" "0" \
         "$(value_of "$(helper result 'Sctestgamma')" count)"
echo

echo "3. キャッシュが壊れていても正しい結果が返ること"
helper write-page "SearchCacheTest/A" "${WORD1} ${WORD2} 本文です。${WORD1} ${WORD1}" > /dev/null
good=$(helper result "$WORD1")

helper clear-cache > /dev/null
check_eq "キャッシュを全部消しても同じ結果" "$(value_of "$good" digest)" \
         "$(value_of "$(helper result "$WORD1")" digest)"

helper result "$WORD1" > /dev/null
check_eq "壊す準備ができた" "1" \
         "$(value_of "$(helper corrupt-cache 'SearchCacheTest/B')" corrupted)"
check_eq "読めないキャッシュがあっても同じ結果" "$(value_of "$good" digest)" \
         "$(value_of "$(helper result "$WORD1")" digest)"
check_eq "  読めなかったものは作り直される" "1" \
         "$(value_of "$(helper cache-of 'SearchCacheTest/B')" exists)"
check_eq "  作り直したあとも同じ結果" "$(value_of "$good" digest)" \
         "$(value_of "$(helper result "$WORD1")" digest)"
echo

echo "4. 索引を作る経路がキャッシュを書かないこと"
helper clear-cache > /dev/null
out=$(helper index-add "SearchCacheTest/C")
check_eq "索引を作る前にキャッシュは無い" "0" "$(value_of "$out" before)"
check_eq "索引を作ってもキャッシュは増えない" "0" "$(value_of "$out" after)"
echo

echo "5. 索引の整合に影響しないこと"
rc=$(cd "$CACHE_TEST_SITE" && sudo -u "$SITE_OWNER" php -d memory_limit=512M \
     app/tool/search_index_check "${CACHE_TEST_SITE}/index.php" \
     --deep --user "$WIKI_ADMIN" > /dev/null 2>&1; echo $?)
check_eq "--deep が問題なしと言う (終了コード 0)" "0" "$rc"
echo

echo "6. 速くなっていること"
helper clear-cache > /dev/null
cold_ms=$(value_of "$(helper time "$WORD1")" ms)
warm_ms=$(value_of "$(helper time "$WORD1")" ms)
printf '  1 回目 %s ms / 2 回目 %s ms\n' "$cold_ms" "$warm_ms"
check_eq "2 回目が 1 回目より遅くならない" "yes" \
         "$([[ "${warm_ms:-0}" -le "${cold_ms:-0}" ]] && echo yes || echo no)"
helper cleanup > /dev/null
echo

if [[ $fail -eq 0 ]]; then
    printf '%d/%d 件すべて通りました。\n' "$total" "$total"
else
    printf '%d/%d 件が失敗しました。\n' "$fail" "$total"
fi
exit $((fail == 0 ? 0 : 1))
