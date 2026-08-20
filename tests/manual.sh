#!/bin/bash
# 組み込みマニュアルのテスト
#
#   ./tests/manual.sh
#
# 環境変数:
#   NF_SITE            複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   MANUAL_TEST_SITE   検証用に作るサイト (既定: /var/www/html/nf-manual-test)
#   MANUAL_TEST_URL    その URL           (既定: http://localhost/nf-manual-test)
#   WIKI_ADMIN         管理者ユーザー名   (既定: admin)
#   KEEP=1             終了後に検証サイトを消さない
#
# マニュアルは上流から「管理者が ?option=admin_manual で 1 回押すと、
# storage の一般ページとして焼き付けられる」仕組みだった。焼き付けた先が
# storage なのでアップグレードの対象外になり、古いマニュアルが残り続ける。
# アンロックすれば編集もできてしまう。
#
# 焼き付けをやめ、MANUAL_PAGENAME 以下を実体を持たない読み取り専用の
# 仮想ページとして解決するようにした。ここで固定するのは次の 5 つ。
#
#   1. 生成の操作なしに最初からマニュアルが読めること
#   2. storage に実体が作られないこと (= アップグレードで取り残されない)
#   3. 一般ページから [[...]] でリンクが張れること
#   4. 書き換えられないこと (編集・削除・改名・施錠のすべて)
#   5. 検索の対象に残っていること。かつ一覧には出ないこと
#
# 権限を書き換えるので、必ず複製したサイトに対して実行する。
# 複製元には触らない。sudo が要る。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
MANUAL_TEST_SITE="${MANUAL_TEST_SITE:-/var/www/html/nf-manual-test}"
MANUAL_TEST_URL="${MANUAL_TEST_URL:-http://localhost/nf-manual-test}"
WIKI_ADMIN="${WIKI_ADMIN:-admin}"

# ページ名 NextFormManual を storage が使うディレクトリ名に直したもの。
# storage_page_get_dirpath() は bin2hex したページ名をそのまま使う。
MANUAL_PAGEID="4e657874466f726d4d616e75616c"

fail=0
total=0

cleanup() {
    if [[ "${KEEP:-0}" != "1" ]]; then
        sudo rm -rf "$MANUAL_TEST_SITE" 2>/dev/null
    else
        echo
        echo "KEEP=1 のため検証サイトを残しました: $MANUAL_TEST_SITE"
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
        "${MANUAL_TEST_SITE}/manual-helper.php" \
        "${MANUAL_TEST_SITE}/index.php" "$WIKI_ADMIN" "$@" 2>/dev/null
}

csrf_helper() {
    sudo -u "$SITE_OWNER" php -d memory_limit=512M \
        "${MANUAL_TEST_SITE}/csrf-helper.php" \
        "${MANUAL_TEST_SITE}/index.php" "$WIKI_ADMIN" "$@" 2>/dev/null
}

value_of() {
    printf '%s\n' "$1" | sed -n "s/^$2=//p" | head -1
}

code() {
    curl -sk -o /dev/null -w '%{http_code}' "$@"
}

# 文字列が含まれるか (yes/no)。
#
# `curl | grep -q ...` と書いてはいけない。grep -q は見つけた時点で終わるので
# curl が SIGPIPE で落ち、set -o pipefail のせいでパイプライン全体が失敗と
# 扱われる。**見つかったときだけ no になる**という分かりにくい壊れ方をする。
# 先に変数へ受けてから調べる。
contains() {
    case "$1" in
        *"$2"*) echo yes ;;
        *)      echo no  ;;
    esac
}

post_same_origin() {
    curl -sk -o /dev/null -w '%{http_code}' -X POST -H "Origin: ${ORIGIN}" "$@"
}

# storage に焼き付けられたマニュアルのディレクトリ数
storage_manual_dirs() {
    sudo find "${MANUAL_TEST_SITE}/storage/page" -maxdepth 1 -name "${MANUAL_PAGEID}*" \
        2>/dev/null | wc -l
}

if [[ ! -d "$NF_SITE" ]]; then
    echo "複製元がありません: $NF_SITE" >&2
    echo "tests/env.local の NF_SITE を設定してください。" >&2
    exit 1
fi

echo "複製元 = $NF_SITE"
echo "検証先 = $MANUAL_TEST_SITE"
echo "URL    = $MANUAL_TEST_URL"
echo

sudo rm -rf "$MANUAL_TEST_SITE"
sudo cp -a "$NF_SITE" "$MANUAL_TEST_SITE"
SITE_OWNER=$(sudo stat -c '%U' "${MANUAL_TEST_SITE}/index.php")

# 複製元に配置済みのコードではなく、リポジトリの作業ツリーを検証する
sudo rsync -a --delete "${REPO_ROOT}/NextForm/app/"      "${MANUAL_TEST_SITE}/app/"
sudo rsync -a --delete "${REPO_ROOT}/NextForm/resource/" "${MANUAL_TEST_SITE}/resource/"
sudo cp "${REPO_ROOT}/tests/manual-helper.php" "${REPO_ROOT}/tests/csrf-helper.php" \
        "${MANUAL_TEST_SITE}/"
sudo chown -R "$SITE_OWNER" "${MANUAL_TEST_SITE}/app" "${MANUAL_TEST_SITE}/resource" \
                            "${MANUAL_TEST_SITE}/manual-helper.php" \
                            "${MANUAL_TEST_SITE}/csrf-helper.php"

# 複製元に焼き付け済みのマニュアルが残っていると、実体が無いことを確かめられない。
# 複製した側から消してから始める (複製元には触らない)。
sudo rm -rf "${MANUAL_TEST_SITE}/storage/page/${MANUAL_PAGEID}"*
# マニュアルのキャッシュを消してから始める。毎回同じ状態から検証するため。
# キャッシュのファイル名は <bin2hex(ページ名)>-<bin2hex(キー)> で、
# ページ名が空なので先頭が '-' になる。
sudo rm -f "${MANUAL_TEST_SITE}/storage/cache/-6d616e75616c5f7061676573" \
           "${MANUAL_TEST_SITE}/storage/cache/-6d616e75616c5f696e646578" 2>/dev/null

out=$(csrf_helper guest-write)
if [[ "$(value_of "$out" saved)" != "1" ]]; then
    echo "ログインしていない利用者に write 権限を与えられませんでした。" >&2
    exit 1
fi

if [[ "$(code "${MANUAL_TEST_URL}/")" != "200" ]]; then
    echo "検証サイトが $MANUAL_TEST_URL で見えません。" >&2
    echo "tests/env.local の MANUAL_TEST_URL を設定してください。" >&2
    exit 1
fi

ORIGIN=$(printf '%s' "$MANUAL_TEST_URL" | sed -E 's#^(https?://[^/]+).*#\1#')
M="NextFormManual"
MJA="NextFormManual/ja"
echo

echo "1. 生成の操作なしに最初から読めること"
check_eq "storage に焼き付けが無い状態から始める" "0" "$(storage_manual_dirs)"
check_eq "page_is_exists('${M}')"    "1" "$(value_of "$(helper page-exists "$M")" exists)"
check_eq "page_is_exists('${MJA}')"  "1" "$(value_of "$(helper page-exists "$MJA")" exists)"
check_eq "マニュアルのページが 1 件以上ある" "yes" \
         "$([[ "$(value_of "$(helper manual-count)" count)" -ge 1 ]] && echo yes || echo no)"
check_eq "?${MJA} が 200"  "200" "$(code "${MANUAL_TEST_URL}/?${MJA}")"
# 'NextForm' だけだと画面の飾りにも出るので、マニュアル本文にしかない語で見る
check_eq "  本文が出る" "yes" \
         "$(contains "$(curl -sk "${MANUAL_TEST_URL}/?${MJA}")" 'Wikiエンジン')"
check_eq "存在しない子ページは存在しないまま" "0" \
         "$(value_of "$(helper page-exists "${M}/NoSuchPage")" exists)"
echo

echo "2. storage に実体が作られないこと"
curl -sk -o /dev/null "${MANUAL_TEST_URL}/?${MJA}"
curl -sk -o /dev/null "${MANUAL_TEST_URL}/?${M}"
check_eq "読んでも storage にディレクトリができない" "0" "$(storage_manual_dirs)"
check_eq "版 (バックアップ) を持たない" "0" "$(value_of "$(helper backup-count "$MJA")" count)"
check_eq "mtime はソースの更新時刻 (0 ではない)" "yes" \
         "$([[ "$(value_of "$(helper page-mtime "$MJA")" mtime)" -gt 0 ]] && echo yes || echo no)"
echo

echo "3. 一般ページから [[...]] でリンクが張れること"
helper make-page "ManualTest/Link" "[[${MJA}]]" > /dev/null
# 存在しないページへのリンクには not_exists が付く。マニュアルには付かないこと。
link_html=$(curl -sk "${MANUAL_TEST_URL}/?ManualTest/Link" \
            | grep -o '<a[^>]*data-link-pagename="[^"]*Manual[^"]*"[^>]*>' | head -1)
check_eq "リンクが描かれる" "yes" \
         "$([[ -n "$link_html" ]] && echo yes || echo no)"
check_eq "  not_exists が付かない" "no" "$(contains "$link_html" 'not_exists')"
echo

echo "4. 書き換えられないこと"
check_eq "page_write() が拒否する"  "0" "$(value_of "$(helper write-try "$MJA" "乗っ取り")" written)"
check_eq "page_delete() が拒否する" "0" "$(value_of "$(helper delete-try "$MJA")" deleted)"
check_eq "page_lock() が拒否する"   "0" "$(value_of "$(helper lock-try "$MJA")" locked)"
check_eq "page_rename() が拒否する" "0" \
         "$(value_of "$(helper rename-try "$MJA" "ManualTest/Stolen")" renamed)"
check_eq "  拒否した後も storage に実体ができない" "0" "$(storage_manual_dirs)"

# HTTP からも塞がっていること。編集画面は ticket を出さないので、
# 空の ticket で書き込みを試す。
post_same_origin -d "action=write" -d "ticket=" \
                 --data-urlencode "contents=乗っ取り" "${MANUAL_TEST_URL}/?${MJA}" > /dev/null
check_eq "HTTP の action=write でも実体ができない" "0" "$(storage_manual_dirs)"
check_eq "  本文が乗っ取られていない" "no" \
         "$(contains "$(value_of "$(helper page-head "$MJA")" head)" '乗っ取り')"
check_eq "action=edit が 500 にならない" "200" "$(code "${MANUAL_TEST_URL}/?${MJA}&action=edit")"
check_eq "option=history が 500 にならない" "200" \
         "$(code "${MANUAL_TEST_URL}/?${MJA}&option=history")"
echo

echo "5. 検索に残り、一覧には出ないこと"
# 「早見表」はマニュアルにしか出てこない語
check_eq "検索でマニュアルが引ける" "yes" \
         "$([[ "$(value_of "$(helper search '早見表')" manual)" -ge 1 ]] && echo yes || echo no)"
check_eq "索引に載っている" "1" "$(value_of "$(helper index-has "$MJA")" indexed)"
check_eq "索引の整合検査が幽霊を出さない" "0" "$(value_of "$(helper index-ghosts)" ghosts)"
check_eq "page_find() はマニュアルを返さない" "0" "$(value_of "$(helper find-count)" count)"
# ページ名だけを見る。'NextFormManual' の素の検索では、どの画面にも出る
# サイトメニューの「マニュアル」リンクを拾ってしまう。一覧の項目は
# dom_append_page_link() が data-link-pagename を付ける。
check_eq "?option=allpage に出ない" "no" \
         "$(contains "$(curl -sk "${MANUAL_TEST_URL}/?option=allpage")" \
                     "data-link-pagename=\"${M}\"")"
echo

echo "6. 焼き付け済みの古いページを片付けられること"
# 0.6.0 以前に生成したページが storage に残っているサイトを作る。
# 中身は組み込みに隠されて見えないが、一覧には名前が出るので消せなければ困る。
# (docs/upgrade-guide.md の「古いマニュアルのページを片付ける」と 1 対 1)
sudo mkdir -p "${MANUAL_TEST_SITE}/storage/page/${MANUAL_PAGEID}"
sudo tee "${MANUAL_TEST_SITE}/storage/page/${MANUAL_PAGEID}/head" > /dev/null <<'EOF'
74797065=77696b69

古い焼き付け。組み込みに隠されるが、消せなければならない。
EOF
sudo chown -R "$SITE_OWNER" "${MANUAL_TEST_SITE}/storage/page/${MANUAL_PAGEID}"
check_eq "焼き付け済みの写しがある" "1" "$(storage_manual_dirs)"
check_eq "  開くと組み込みの方が出る" "no" \
         "$(contains "$(curl -sk "${MANUAL_TEST_URL}/?${M}")" '古い焼き付け')"
check_eq "  写しは削除できる" "1" "$(value_of "$(helper delete-try "$M")" deleted)"
check_eq "  消した後も組み込みは読める" "1" \
         "$(value_of "$(helper page-exists "$M")" exists)"
# 削除は論理削除で、版を残したディレクトリが storage に残る。
# 完全削除まで通ることを見て、storage を元の状態に戻す。
check_eq "  完全削除もできる" "1" "$(value_of "$(helper truncate-try "$M")" truncated)"
check_eq "  storage から消える" "0" "$(storage_manual_dirs)"
# 写しの無いページ (純粋な組み込み) は今までどおり断る
check_eq "  写しの無いページは削除できない" "0" \
         "$(value_of "$(helper delete-try "$MJA")" deleted)"
echo

echo "7. ソースを変えると追随すること"
before_mtime=$(value_of "$(helper page-mtime "$MJA")" mtime)
sudo touch "${MANUAL_TEST_SITE}/app/manual/manual-before.txt"
after_mtime=$(value_of "$(helper page-mtime "$MJA")" mtime)
check_eq "ソースを touch すると mtime が進む" "yes" \
         "$([[ "$after_mtime" -gt "$before_mtime" ]] && echo yes || echo no)"
check_eq "  作り直した後も索引に載っている" "1" "$(value_of "$(helper index-has "$MJA")" indexed)"
check_eq "  作り直しても実体はできない" "0" "$(storage_manual_dirs)"
echo

echo "8. 廃止した画面が 500 にならないこと"
# 跡地の画面が残っている (配布物から消すと、古いファイルがアップグレード後も
# 残って manual_build() を呼び Fatal error になる)。admin を求めるので 401。
check_eq "?option=admin_manual"  "401" "$(code "${MANUAL_TEST_URL}/?option=admin_manual")"
check_eq "管理ツールに項目が無い" "no" \
         "$(contains "$(curl -sk "${MANUAL_TEST_URL}/?option=admin")" 'admin_manual')"
echo

echo "9. PHP の警告を出さないこと"
before=$(sudo wc -l "${PHP_ERROR_LOG:-/var/log/php-fpm/www-error.log}" 2>/dev/null | awk "{print \$1}")
code "${MANUAL_TEST_URL}/?${MJA}" > /dev/null
code "${MANUAL_TEST_URL}/?${M}" > /dev/null
code "${MANUAL_TEST_URL}/?${MJA}&action=edit" > /dev/null
code "${MANUAL_TEST_URL}/?option=admin_manual" > /dev/null
code "${MANUAL_TEST_URL}/?ManualTest/Link" > /dev/null
after=$(sudo wc -l "${PHP_ERROR_LOG:-/var/log/php-fpm/www-error.log}" 2>/dev/null | awk "{print \$1}")
check_eq "エラーログが増えない" "$before" "$after"

helper cleanup > /dev/null
echo

if [[ $fail -eq 0 ]]; then
    printf '%d/%d 件すべて通りました。\n' "$total" "$total"
else
    printf '%d/%d 件が失敗しました。\n' "$fail" "$total"
fi
exit $((fail == 0 ? 0 : 1))
