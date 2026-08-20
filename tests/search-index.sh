#!/bin/bash
# 検索インデックスの整合テスト
#
#   ./tests/search-index.sh
#
# 環境変数:
#   NF_SITE            複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   SEARCH_TEST_SITE   検証用に作るサイト (既定: /var/www/html/nf-search-test)
#   SEARCH_TEST_URL    その URL           (既定: http://localhost/nf-search-test)
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
SEARCH_TEST_URL="${SEARCH_TEST_URL:-http://localhost/nf-search-test}"
WIKI_ADMIN="${WIKI_ADMIN:-admin}"
ORIGIN=$(printf '%s' "$SEARCH_TEST_URL" | sed -E 's#^(https?://[^/]+).*#\1#')

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

# 権限を書き換える汎用ヘルパ (再構築を HTTP で走らせる節だけが使う)
site_helper() {
    sudo -u "$SITE_OWNER" php \
        "${SEARCH_TEST_SITE}/site-helper.php" \
        "${SEARCH_TEST_SITE}/index.php" "$@" 2>/dev/null
}

# 時間のかかる処理を、ブラウザと同じように最後まで進める。
#   $1  最初の応答の HTML
# 結果は RQ_HTML (最後の画面) / RQ_CODES (各ステップのステータス) / RQ_STEPS。
run_queue_follow() {
    local body="$1" fields args line
    RQ_CODES=""
    RQ_STEPS=0
    while :; do
        fields="$(printf '%s' "$body" | python3 "${REPO_ROOT}/tests/form-scrape.py" \
                  --form run_queue)"
        [[ -z "$fields" ]] && break
        RQ_STEPS=$((RQ_STEPS + 1))
        if [[ $RQ_STEPS -gt 50 ]]; then
            RQ_CODES="${RQ_CODES} 終わらない"
            break
        fi
        args=()
        while IFS= read -r line; do
            args+=(--data-urlencode "${line%%=*}=$(printf '%s' "${line#*=}" | base64 -d)")
        done <<< "$fields"
        body="$(curl -sk -X POST -H "Origin: ${ORIGIN}" -w '\n%{http_code}' \
                "${args[@]}" "${SEARCH_TEST_URL}/")"
        RQ_CODES="${RQ_CODES} ${body##*$'\n'}"
        body="${body%$'\n'*}"
    done
    RQ_HTML="$body"
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
sudo cp "${REPO_ROOT}/tests/site-helper.php" "${SEARCH_TEST_SITE}/"
sudo chown -R "$SITE_OWNER" "${SEARCH_TEST_SITE}/app" "${SEARCH_TEST_SITE}/resource" \
                            "${SEARCH_TEST_SITE}/search-index-helper.php" \
                            "${SEARCH_TEST_SITE}/site-helper.php"

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
out=$(helper ghost-page-search)
check_eq "幽霊ページを索引に残せた (準備)"          "yes" "$(value_of "$out" in_index)"
check_eq "実在しないページを検索しても警告が出ない" "0"   "$(value_of "$out" warnings)"
check_eq "実在しないページは検索結果に出ない"       "0"   "$(value_of "$out" hits)"
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

# 管理画面 (?option=search_index) は HTTP 経由だと digest 認証が要るので、
# option の関数を CLI から直接呼んで描画結果を見る。
# 4. で壊したページがそのまま残っているのでそれを使う。
echo "5. 管理画面から検査して 1 ページだけ直せること"
out=$(helper render-screen show)
check_eq "既定の画面が集合比較の結果を出す" "yes" \
         "$(printf '%s' "$out" | grep -q 'インデックス上のページ' && echo yes || echo no)"
check_eq "既定の画面は部分的な欠けを見逃すと断る" "yes" \
         "$(printf '%s' "$out" | grep -q '詳しく検査する' && echo yes || echo no)"

out=$(helper render-screen check)
check_eq "詳しい検査が欠けているページを挙げる" "yes" \
         "$(printf '%s' "$out" | grep -q "$victim" && echo yes || echo no)"
check_eq "そのページに直すボタンが付く" "yes" \
         "$(printf '%s' "$out" | grep -q 'search_index_repair' && echo yes || echo no)"

out=$(helper render-screen repair "$victim")
check_eq "直すと入れ直したと表示する" "yes" \
         "$(printf '%s' "$out" | grep -q '入れ直しました' && echo yes || echo no)"
check_eq "直した直後の再検査で一致する" "yes" \
         "$(printf '%s' "$out" | grep -q '一致しています' && echo yes || echo no)"

out=$(cd "$SEARCH_TEST_SITE" && sudo -u "$SITE_OWNER" php -d memory_limit=512M \
      app/tool/search_index_check "${SEARCH_TEST_SITE}/index.php" \
      --deep --user "$WIKI_ADMIN" 2>/dev/null)
rc=$?
check_eq "修理後は --deep でも問題なし (終了コード 0)" "0" "$rc"
echo

# 再構築は「別名で作って最後に差し替える」方式。中断されても、それまでの
# 索引がそのまま使われ続ける。先に全部消す方式だと、中断した時点で索引が
# 空のまま残り、再構築する前より悪い状態になっていた。
echo "6. 再構築が中断されても今の索引が生き残ること"
helper rebuild > /dev/null
out=$(helper count-rebuild-files)
index_before=$(value_of "$out" index_files)
check_eq "再構築後に作りかけのファイルが残らない" "0" "$(value_of "$out" rebuild_files)"

out=$(helper rebuild 3)
check_eq "3 件だけ処理して中断した" "no" "$(value_of "$out" finalized)"
out=$(helper count-rebuild-files)
check_eq "作りかけのファイルができている" "yes" \
         "$([[ "$(value_of "$out" rebuild_files)" -gt 0 ]] && echo yes || echo no)"
check_eq "使われている索引は減っていない" "$index_before" "$(value_of "$out" index_files)"

out=$(cd "$SEARCH_TEST_SITE" && sudo -u "$SITE_OWNER" php -d memory_limit=512M \
      app/tool/search_index_check "${SEARCH_TEST_SITE}/index.php" \
      --deep --user "$WIKI_ADMIN" 2>/dev/null)
rc=$?
check_eq "中断された直後でも検索は正常なまま (終了コード 0)" "0" "$rc"

helper rebuild > /dev/null
out=$(helper count-rebuild-files)
check_eq "やり直せば作りかけは片付く" "0" "$(value_of "$out" rebuild_files)"
out=$(cd "$SEARCH_TEST_SITE" && sudo -u "$SITE_OWNER" php -d memory_limit=512M \
      app/tool/search_index_check "${SEARCH_TEST_SITE}/index.php" \
      --deep --user "$WIKI_ADMIN" 2>/dev/null)
rc=$?
check_eq "やり直した後も問題なし (終了コード 0)" "0" "$rc"
echo

# 問い合わせ側だけが language_sanitize() を通っていて、索引側と照合側は
# 通っていなかった。全角スペースを含むフレーズは索引の ngram と一致せず、
# 本文に確かに書いてあっても必ず 0 件になる。
echo "7. 全角スペースを含むフレーズが引けること"
out=$(helper search-count "SearchIndexTest/Fullwidth" \
      "* 全角スペース
本文に 設定　方法 と書いてあります。" '"設定　方法"')
check_eq "引用符付きのフレーズが 1 語として解釈される" "1" "$(value_of "$out" words)"
check_eq "全角スペースのフレーズでページが引ける" "1" "$(value_of "$out" hits)"
helper cleanup > /dev/null

# 全角と半角を揃えた結果、どちらで書いても、どちらで問い合わせても引ける。
out=$(helper search-count "SearchIndexTest/Halfwidth" \
      "* 半角スペース
本文に 設定 方法 と書いてあります。" '"設定 方法"')
check_eq "半角スペースのフレーズも引ける" "1" "$(value_of "$out" hits)"

out=$(helper search-count "SearchIndexTest/Fullwidth2" \
      "* 全角スペース
本文に 設定　方法 と書いてあります。" '"設定 方法"')
check_eq "半角で問い合わせても全角で書いたページが引ける" "2" "$(value_of "$out" hits)"
helper cleanup > /dev/null
echo

echo "8. ブラウザと同じ経路で再構築が最後まで走ること"
# 再構築は 1 リクエストでは終わらない。上流は続きを
# <meta http-equiv="refresh"> で呼んでいたが、それは GET なので
# 0.4.0 の CSRF 対策に弾かれ、**1 ページも処理されずに 403 で終わっていた**。
# 1 回目のリクエストは準備しかしないため、症状は「実行を押すと即エラー」。
# ここまでの検査はどれも run_queue を通らない CLI 経由なので、
# この節だけがブラウザの経路 (POST -> 続きも POST) を通る。
#
# 資格情報をテストに置かずに済ませるため、ログインしていない利用者に
# admin 権限を与えてから叩く。
check_eq "ログインしていない利用者に admin を与えた" "1" \
         "$(value_of "$(site_helper guest-admin)" saved)"
check_eq "検証サイトが HTTP で見える" "200" \
         "$(curl -sk -o /dev/null -w '%{http_code}' "${SEARCH_TEST_URL}/")"

# 直す相手を作る。1 ページ分の ngram を抜いた状態から始める。
helper rebuild > /dev/null
out=$(helper corrupt-bucket)
bucket=$(value_of "$out" bucket)
helper rebuild > /dev/null
victim=$(value_of "$(helper busiest-page-in-bucket "$bucket")" pagename)
helper drop-page-from-bucket "$bucket" "$victim" > /dev/null
(cd "$SEARCH_TEST_SITE" && sudo -u "$SITE_OWNER" php -d memory_limit=512M \
 app/tool/search_index_check "${SEARCH_TEST_SITE}/index.php" \
 --deep --user "$WIKI_ADMIN" > /dev/null 2>&1)
check_eq "始める前は索引がずれている (終了コード 1)" "1" "$?"

first=$(curl -sk -X POST -H "Origin: ${ORIGIN}" -w '\n%{http_code}' \
        --data-urlencode "option=search_index" --data-urlencode "action=write" \
        "${SEARCH_TEST_URL}/")
check_eq "実行の POST が通る" "200" "${first##*$'\n'}"
first="${first%$'\n'*}"
check_eq "続きはフォームで返る (GET ではない)" "1" \
         "$(printf '%s' "$first" | grep -c 'class="run_queue"')"
check_eq "JavaScript を切っていても押せる" "1" \
         "$(printf '%s' "$first" | grep -c '<noscript><input type="submit"')"

run_queue_follow "$first"
check_eq "途中で拒否されない"   ""     "$(printf '%s' "$RQ_CODES" | tr ' ' '\n' | grep -v '^200$' | tr '\n' ' ' | sed 's/ *$//')"
check_eq "2 回以上に分けて走る" "yes"  "$([[ "$RQ_STEPS" -ge 1 ]] && echo yes || echo no)"
check_eq "最後まで走る"         "1"    "$(printf '%s' "$RQ_HTML" | grep -c '完了')"

(cd "$SEARCH_TEST_SITE" && sudo -u "$SITE_OWNER" php -d memory_limit=512M \
 app/tool/search_index_check "${SEARCH_TEST_SITE}/index.php" \
 --deep --user "$WIKI_ADMIN" > /dev/null 2>&1)
check_eq "索引が本文と一致する (終了コード 0)" "0" "$?"
echo

if [[ $fail -eq 0 ]]; then
    printf '%d/%d 件すべて通りました。\n' "$total" "$total"
else
    printf '%d/%d 件が失敗しました。\n' "$fail" "$total"
fi
exit $((fail == 0 ? 0 : 1))
