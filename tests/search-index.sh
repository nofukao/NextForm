#!/bin/bash
# 検索インデックスの整合テスト
#
#   ./tests/search-index.sh
#
# 環境変数:
#   NF_SITE            複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   SEARCH_TEST_SITE   検証用に作るサイト (既定: /var/www/html/nf-search-test)
#   WIKI_ADMIN         管理者ユーザー名   (既定: admin)
#   KEEP=1             終了後に検証サイトを消さない (中身を見たいとき)
#
# 検索インデックスは storage/cache/ の 2-gram 転置表で、ページの保存・削除の
# たびに「旧本文と新本文の差分」だけが当てられる。差分は索引の中身を確認せずに
# 当てられるため、一度ずれると自己修復しない。壊れても画面にもログにも
# 何も出ず、症状は「特定の語だけ検索に出てこない」になる。
#
# ここで固定するのは次の 3 つ:
#
#   1. 正常系では索引が本文と一致し続けること (編集 9 パターン / 削除 4 パターン)
#   2. 索引ファイルが読めなくなったときに何が起きるか
#   3. app/tool/search_index_check がその状態を検出できること
#
# 2 は現状の欠陥をそのまま記録している。読めない索引を「空」とみなして
# 全上書きするため、ページを 1 件保存しただけで、そのファイルに入っていた
# 全ページが巻き添えで消える。直したらこのテストの期待値も変える。
#
# 索引を壊すので、必ず複製したサイトに対して実行する。複製元には触らない。
# root で実行する必要がある (apache 所有のサイトを複製するため)。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# 向け先は tests/env.local に書く (Git には入らない)。
# 無い場合は下の既定値を使う。tests/env.local.example を参照。
[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
SEARCH_TEST_SITE="${SEARCH_TEST_SITE:-/var/www/html/nf-search-test}"
WIKI_ADMIN="${WIKI_ADMIN:-admin}"

fail=0
total=0

cleanup() {
    if [[ "${KEEP:-0}" != "1" ]]; then
        sudo rm -rf "$SEARCH_TEST_SITE" 2>/dev/null
    else
        echo
        echo "KEEP=1 のため検証サイトを残しました: $SEARCH_TEST_SITE"
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

# サイトの中でヘルパを実行し、key=value の出力を返す
helper() {
    sudo -u "$SITE_OWNER" php -d memory_limit=512M \
        "${SEARCH_TEST_SITE}/search-index-helper.php" \
        "${SEARCH_TEST_SITE}/index.php" "$WIKI_ADMIN" "$@" 2>/dev/null
}

# key=value の出力から値を取り出す
value_of() {
    printf '%s\n' "$1" | sed -n "s/^$2=//p" | head -1
}

if [[ ! -d "$NF_SITE" ]]; then
    echo "複製元がありません: $NF_SITE" >&2
    echo "tests/env.local の NF_SITE を設定してください。" >&2
    exit 1
fi

echo "複製元 = $NF_SITE"
echo "検証先 = $SEARCH_TEST_SITE"
echo

sudo rm -rf "$SEARCH_TEST_SITE"
sudo cp -a "$NF_SITE" "$SEARCH_TEST_SITE"
SITE_OWNER=$(sudo stat -c '%U' "${SEARCH_TEST_SITE}/index.php")

# 複製元に配置済みのコードではなく、リポジトリの作業ツリーを検証する。
# これをしないと app/ を直しても複製元へ deploy するまでテストに反映されず、
# 修正が効いていないのか反映されていないのか区別がつかなくなる。
# 範囲は deploy.sh と同じ (storage/ theme/ index.php install-info.dat は除く)。
sudo rsync -a --delete "${REPO_ROOT}/NextForm/app/"      "${SEARCH_TEST_SITE}/app/"
sudo rsync -a --delete "${REPO_ROOT}/NextForm/resource/" "${SEARCH_TEST_SITE}/resource/"
sudo cp "${REPO_ROOT}/tests/search-index-helper.php" "${SEARCH_TEST_SITE}/"
sudo chown -R "$SITE_OWNER" "${SEARCH_TEST_SITE}/app" "${SEARCH_TEST_SITE}/resource" \
                            "${SEARCH_TEST_SITE}/search-index-helper.php"

# 複製元に残っているずれを持ち込まないよう、まっさらな索引から始める。
# 検査ツールがこの状態を「問題なし」と言えることも同時に確かめている。
echo "索引を再構築しています.."
out=$(helper rebuild)
echo "  ${out}"
echo

echo "1. 検査ツールが健全な索引を通すこと"
out=$(cd "$SEARCH_TEST_SITE" && sudo -u "$SITE_OWNER" php -d memory_limit=512M \
      app/tool/search_index_check "${SEARCH_TEST_SITE}/index.php" 2>/dev/null)
rc=$?
check_eq "再構築直後は問題なし (終了コード 0)" "0" "$rc"

out=$(cd "$SEARCH_TEST_SITE" && sudo -u "$SITE_OWNER" php -d memory_limit=512M \
      app/tool/search_index_check "${SEARCH_TEST_SITE}/index.php" \
      --deep --user "$WIKI_ADMIN" 2>/dev/null)
rc=$?
check_eq "--deep でも問題なし (終了コード 0)" "0" "$rc"
check_eq "全ページの ngram が一致すると報告する" "yes" \
         "$(printf '%s' "$out" | grep -q 'すべて一致' && echo yes || echo no)"
echo

echo "2. 正常系では索引が本文と一致し続けること"
out=$(helper edit-consistency)
check_eq "編集 9 パターンを実行した"                "9" "$(value_of "$out" cases)"
check_eq "索引に足りない ngram が無い"              "0" "$(value_of "$out" missing)"
check_eq "索引に余分な ngram が無い"                "0" "$(value_of "$out" extra)"

out=$(helper delete-residue)
check_eq "削除 4 パターンを実行した"                "4" "$(value_of "$out" cases)"
check_eq "削除したページの ngram が残らない"        "0" "$(value_of "$out" residue)"
helper cleanup > /dev/null
echo

echo "3. 索引ファイルが読めなくなったときの挙動"
out=$(helper corrupt-bucket)
bucket=$(value_of "$out" bucket)
pages_before=$(value_of "$out" pages_before)
check_eq "壊す前のファイルに複数ページ入っている" "yes" \
         "$([[ "${pages_before:-0}" -gt 1 ]] && echo yes || echo no)"

out=$(cd "$SEARCH_TEST_SITE" && sudo -u "$SITE_OWNER" php -d memory_limit=512M \
      app/tool/search_index_check "${SEARCH_TEST_SITE}/index.php" 2>/dev/null)
rc=$?
check_eq "検査ツールが破損を検出する (終了コード 1)" "1" "$rc"
check_eq "破損の理由を報告する" "yes" \
         "$(printf '%s' "$out" | grep -q '読めない索引ファイル' && echo yes || echo no)"

# 読めない索引ファイルには書き戻さない。書き戻すと、そこに入っていた
# 全ページ分の登録が保存した 1 ページ分に置き換わって消えてしまう。
# 壊れたファイルは壊れたまま残し、再構築で回復できる状態を保つ。
# 利用者の編集自体は成功させる (索引のために保存を止めない)。
errfile=$(mktemp)
out=$(sudo -u "$SITE_OWNER" php -d memory_limit=512M \
      "${SEARCH_TEST_SITE}/search-index-helper.php" \
      "${SEARCH_TEST_SITE}/index.php" "$WIKI_ADMIN" save-one-page 2>"$errfile")
err=$(cat "$errfile"); rm -f "$errfile"
check_eq "破損していてもページの保存自体は成功する" "yes" "$(value_of "$out" saved)"
check_eq "読めなかったことをログに残す" "yes" \
         "$(printf '%s' "$err" | grep -q 'search index is unreadable' && echo yes || echo no)"

out=$(helper count-bucket "$bucket")
check_eq "読めない索引ファイルを上書きしない" "-1" "$(value_of "$out" pages_after)"
check_eq "壊れたファイルの中身が変わっていない" "500" "$(value_of "$out" bytes)"
echo

echo "4. 部分的な欠けは --deep でしか見つからないこと"
# 上の破損はファイルごと読めないので、ここでは「ファイルとしては正しいが
# 1 ページ分の ngram だけ抜けている」状態を意図的に作る。
# 索引更新が 1 回失われたときに実際に残るのはこの形。
helper rebuild > /dev/null
out=$(helper corrupt-bucket)   # 一番大きいファイルを選ぶためだけに使う
bucket=$(value_of "$out" bucket)
helper rebuild > /dev/null     # 壊した分を戻す
out=$(helper busiest-page-in-bucket "$bucket")
victim=$(value_of "$out" pagename)
out=$(helper drop-page-from-bucket "$bucket" "$victim")
check_eq "1 ページ分の ngram を抜いた" "yes" \
         "$([[ "$(value_of "$out" dropped)" -gt 0 ]] && echo yes || echo no)"

out=$(cd "$SEARCH_TEST_SITE" && sudo -u "$SITE_OWNER" php -d memory_limit=512M \
      app/tool/search_index_check "${SEARCH_TEST_SITE}/index.php" 2>/dev/null)
rc=$?
check_eq "既定の検査は部分的な欠けを見逃す (終了コード 0)" "0" "$rc"

out=$(cd "$SEARCH_TEST_SITE" && sudo -u "$SITE_OWNER" php -d memory_limit=512M \
      app/tool/search_index_check "${SEARCH_TEST_SITE}/index.php" \
      --deep --user "$WIKI_ADMIN" 2>/dev/null)
rc=$?
check_eq "--deep なら部分的な欠けを検出する (終了コード 1)" "1" "$rc"
check_eq "欠けているページ名を報告する" "yes" \
         "$(printf '%s' "$out" | grep -q "$victim" && echo yes || echo no)"
echo

if [[ $fail -eq 0 ]]; then
    printf '%d/%d 件すべて通りました。\n' "$total" "$total"
else
    printf '%d/%d 件が失敗しました。\n' "$fail" "$total"
fi
exit $((fail == 0 ? 0 : 1))
