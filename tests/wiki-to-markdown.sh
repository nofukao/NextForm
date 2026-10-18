#!/bin/bash
# Wiki 記法のページを Markdown に変換する機能のテスト
#
#   ./tests/wiki-to-markdown.sh            # 検査
#   ./tests/wiki-to-markdown.sh --update   # 変換の期待値 (tests/wiki-to-markdown/expected/) を作り直す
#
# 環境変数:
#   NF_SITE             複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   WIKI_MD_TEST_SITE   検証用に作るサイト (既定: /var/www/html/nf-wiki-md-test)
#   WIKI_MD_TEST_URL    その URL           (既定: http://localhost/nf-wiki-md-test)
#   WIKI_ADMIN          管理者ユーザー名   (既定: admin)
#   KEEP=1              終了後に検証サイトを消さない
#
# 変換の規則は NextForm/app/handler/wiki/wiki_markdown.inc、設計は
# docs/development/design-wiki-to-markdown.md の 3e。ここで固定するのは次のとおり:
#
#   1. 入力 (tests/wiki-to-markdown/input/*.wiki) を変換した結果が、期待値
#      (expected/*.md) と 1 バイトも変わらないこと。落ちるもの・原文の枠で残るものの
#      報告も期待値 (expected/*.report) と同じこと
#   2. 変換した Markdown を表示したとき、見出し・リンク・表などが期待どおりに出ること
#
# 権限を書き換えるので、必ず複製したサイトに対して実行する。root で実行する必要がある。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
WIKI_MD_TEST_SITE="${WIKI_MD_TEST_SITE:-/var/www/html/nf-wiki-md-test}"
WIKI_MD_TEST_URL="${WIKI_MD_TEST_URL:-http://localhost/nf-wiki-md-test}"
WIKI_ADMIN="${WIKI_ADMIN:-admin}"
PHP_ERROR_LOG="${PHP_ERROR_LOG:-/var/log/php-fpm/www-error.log}"
FIXTURES="${REPO_ROOT}/tests/wiki-to-markdown"

UPDATE=0
[[ "${1:-}" == "--update" ]] && UPDATE=1

fail=0
total=0
WORK="$(mktemp -d)"

cleanup() {
    rm -rf "$WORK"
    if [[ "${KEEP:-0}" != "1" ]]; then
        sudo rm -rf "$WIKI_MD_TEST_SITE" 2>/dev/null
    else
        echo
        echo "KEEP=1 のため検証サイトを残しました: $WIKI_MD_TEST_SITE"
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
        "${WIKI_MD_TEST_SITE}/wiki-to-markdown-helper.php" \
        "${WIKI_MD_TEST_SITE}/index.php" "$WIKI_ADMIN" "$@" 2>/dev/null
}

value_of() {
    printf '%s\n' "$1" | sed -n "s/^$2=//p" | head -1
}

# 変換の道具 (読み取り専用)
convert() {
    (cd "$WIKI_MD_TEST_SITE" && sudo -u "$SITE_OWNER" php -d memory_limit=512M \
        app/tool/wiki_to_markdown "${WIKI_MD_TEST_SITE}/index.php" "$@" --user "$WIKI_ADMIN" 2>&1)
}

if [[ ! -d "$NF_SITE" ]]; then
    echo "複製元がありません: $NF_SITE" >&2
    exit 1
fi

echo "複製元 = $NF_SITE"
echo "検証先 = $WIKI_MD_TEST_SITE"
echo "URL    = $WIKI_MD_TEST_URL"
echo

sudo rm -rf "$WIKI_MD_TEST_SITE"
sudo cp -a "$NF_SITE" "$WIKI_MD_TEST_SITE"
SITE_OWNER=$(sudo stat -c '%U' "${WIKI_MD_TEST_SITE}/index.php")
sudo rsync -a --delete "${REPO_ROOT}/NextForm/app/"      "${WIKI_MD_TEST_SITE}/app/"
sudo rsync -a --delete "${REPO_ROOT}/NextForm/resource/" "${WIKI_MD_TEST_SITE}/resource/"
sudo cp "${REPO_ROOT}/tests/wiki-to-markdown-helper.php" "${WIKI_MD_TEST_SITE}/"
sudo chown -R "$SITE_OWNER" "${WIKI_MD_TEST_SITE}/app" "${WIKI_MD_TEST_SITE}/resource" \
                            "${WIKI_MD_TEST_SITE}/wiki-to-markdown-helper.php"
OPCACHE_FREQ=$(php -r 'echo (int)ini_get("opcache.revalidate_freq");' 2>/dev/null)
sleep $(( ${OPCACHE_FREQ:-2} + 1 ))
log_before=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')
log_before="${log_before:-0}"

FIX="${WIKI_MD_TEST_SITE}/wiki-md-fixtures"
sudo mkdir -p "$FIX"
for f in "${FIXTURES}"/input/*.wiki; do
    sudo install -o "$SITE_OWNER" -m 644 "$f" "${FIX}/$(basename "$f")"
done

echo "1. 変換した結果が期待値と同じこと"
for f in "${FIXTURES}"/input/*.wiki; do
    name="$(basename "$f" .wiki)"
    helper write-wiki "WikiMdTest/${name}" "${FIX}/${name}.wiki" > /dev/null
done
# タグは Wiki 記法の本文に書かないので、タグの画面と同じ経路で付ける
helper set-tags WikiMdTest/Full タグA "タグ, B" > /dev/null

for f in "${FIXTURES}"/input/*.wiki; do
    name="$(basename "$f" .wiki)"
    convert "WikiMdTest/${name}" > "${WORK}/${name}.md"
    convert "WikiMdTest/${name}" --report > "${WORK}/${name}.report"
    for kind in md report; do
        expected="${FIXTURES}/expected/${name}.${kind}"
        if [[ $UPDATE -eq 1 ]]; then
            cp "${WORK}/${name}.${kind}" "$expected"
            continue
        fi
        if diff -u "$expected" "${WORK}/${name}.${kind}" > "${WORK}/diff" 2>&1; then
            check_eq "${name}.${kind}" "same" "same"
        else
            check_eq "${name}.${kind}" "same" "差分あり"
            sed 's/^/        /' "${WORK}/diff" | head -40
        fi
    done
done
if [[ $UPDATE -eq 1 ]]; then
    echo "expected/ を更新しました。内容を目視で確認してからコミットしてください。"
fi
echo

if [[ $UPDATE -eq 1 ]]; then
    exit 0
fi

# --- 画面 -----------------------------------------------------------------
if [[ "$(value_of "$(helper guest write)" saved)" != "1" ]]; then
    echo "ログインしていない利用者に write 権限を与えられませんでした。" >&2
    exit 1
fi
if [[ "$(curl -sk -o /dev/null -w '%{http_code}' "${WIKI_MD_TEST_URL}/")" != "200" ]]; then
    echo "検証サイトが $WIKI_MD_TEST_URL で見えません。tests/env.local の WIKI_MD_TEST_URL を設定してください。" >&2
    exit 1
fi
ORIGIN=$(printf '%s' "$WIKI_MD_TEST_URL" | sed -E 's#^(https?://[^/]+).*#\1#')

contains() { case "$1" in *"$2"*) echo yes ;; *) echo no ;; esac; }
sha_of_body() { value_of "$(helper body "$1")" body | base64 -d 2>/dev/null | sha256sum | cut -d' ' -f1; }
open_confirm() {
    curl -sk -o "$WORK/confirm" -w '%{http_code}' "${WIKI_MD_TEST_URL}/?$1&option=tomarkdown"
}
confirm_ticket() {
    grep -o '<input[^>]*name="ticket"[^>]*>' "$WORK/confirm" | sed -E 's/.*value="([^"]*)".*/\1/' | head -1
}
convert_page() {
    curl -sk -o "$WORK/body" -w '%{http_code}' -X POST -H "Origin: ${ORIGIN}" \
         -d "option=tomarkdown" -d "action=write" -d "ticket=$2" "${WIKI_MD_TEST_URL}/?$1"
}
said() { grep -qE "$1" "$WORK/body" && echo yes || echo no; }

echo "2. 種別 wiki のページのページメニューに「Markdownに変換」が出ること"
P=WikiMdTest/Full
html=$(curl -sk "${WIKI_MD_TEST_URL}/?${P}")
check_eq "種別 wiki のページに出る" "yes" "$(contains "$html" 'option=tomarkdown')"
echo

echo "3. 確認の画面に、変換後の本文と、落ちるもの・原文の枠で残るものが出ること"
check_eq "確認の画面 (200)" "200" "$(open_confirm "$P")"
C="$(cat "$WORK/confirm")"
check_eq "  変換後の本文 (読むだけの欄)" "yes|yes" \
         "$(contains "$C" 'readonly')|$(contains "$C" '[PAGES children]')"
check_eq "  落ちるもの (関連リスト) と、原文の枠で残るもの (&amp;calendar)" "yes|yes" \
         "$(contains "$C" '関連リスト')|$(contains "$C" '&amp;calendar')"
check_eq "  戻し方の説明" "yes" "$(contains "$C" 'この版に戻す')"
open_confirm WikiMdTest/Heading > /dev/null
check_eq "  飛び先が変わる見出しを出す (変わらない見出しは出さない)" "yes|no" \
         "$(contains "$(cat "$WORK/confirm")" '<li>計算 ')|$(contains "$(cat "$WORK/confirm")" '<li>そのままの節</li>')"
echo

echo "4. 変換して保存すること"
open_confirm "$P" > /dev/null
ticket=$(confirm_ticket)
check_eq "GET では変換しない (403)" "403" \
         "$(curl -sk -o /dev/null -w '%{http_code}' "${WIKI_MD_TEST_URL}/?${P}&option=tomarkdown&action=write&ticket=${ticket}")"
check_eq "  種別は wiki のまま" "wiki" "$(value_of "$(helper meta "$P" type)" meta)"
before_sha=$(sha_of_body "$P")
before_tags=$(value_of "$(helper tags "$P")" tags)
before_times=$(value_of "$(helper backup-times "$P")" times)
check_eq "変換する: 保存後の移動 (302)" "302" "$(convert_page "$P" "$ticket")"
check_eq "  種別が Markdown になる" "markdown" "$(value_of "$(helper meta "$P" type)" meta)"
check_eq "  題名を先頭の # から取る" "いろいろな記法" "$(value_of "$(helper meta "$P" title)" meta)"
check_eq "  タグを残す (フロントマターの tags: から取る)" "$before_tags" "$(value_of "$(helper tags "$P")" tags)"
after_times=$(value_of "$(helper backup-times "$P")" times)
T="${after_times##*,}"
check_eq "  変換前の版 (wiki) が履歴に残る" "yes|wiki|$before_sha" \
         "$([[ "$after_times" != "$before_times" ]] && echo yes || echo no)|$(value_of "$(helper backup-type "$P" "$T")" type)|$(value_of "$(helper backup-body "$P" "$T")" body | base64 -d | sha256sum | cut -d' ' -f1)"
body=$(value_of "$(helper body "$P")" body | base64 -d)
footer="このページは、wikiページから変換して作りました。[(変換前の版)](?WikiMdTest/Full&option=history&action=page&backuptime=${T})"
check_eq "  本文は変換した結果と、末尾の一文 (その版へのリンク)" \
         "$(cat "${FIXTURES}/expected/Full.md")"$'\n\n'"${footer}" "$body"
check_eq "  末尾のリンクで変換前の版が開ける" "200|yes" \
         "$(curl -sk -o "$WORK/old" -w '%{http_code}' "${WIKI_MD_TEST_URL}/?WikiMdTest/Full&option=history&action=page&backuptime=${T}")|$(grep -q '畳んだ中身' "$WORK/old" && echo yes || echo no)"
check_eq "  検索の索引が本文と一致する" "1" "$(value_of "$(helper index-ok "$P")" ok)"
html=$(curl -sk "${WIKI_MD_TEST_URL}/?${P}")
check_eq "  Markdown として描かれる (表・目次・折りたたみ・末尾のリンク)" "yes|yes|yes|yes" \
         "$(contains "$html" '<table')|$(contains "$html" 'class="summary"')|$(contains "$html" '<details')|$(contains "$html" "backuptime=${T}")"
check_eq "  ページメニューに「Markdownに変換」は出なくなる" "no" "$(contains "$html" 'option=tomarkdown')"
echo

echo "5. 変換前の版に戻せること (履歴の「この版に戻す」)"
curl -sk -o "$WORK/rconfirm" "${WIKI_MD_TEST_URL}/?${P}&option=restore&backuptime=${T}"
rticket=$(grep -o '<input[^>]*name="ticket"[^>]*>' "$WORK/rconfirm" | sed -E 's/.*value="([^"]*)".*/\1/' | head -1)
check_eq "戻す: 保存後の移動 (302)" "302" \
         "$(curl -sk -o /dev/null -w '%{http_code}' -X POST -H "Origin: ${ORIGIN}" -d option=restore -d action=write -d "backuptime=${T}" -d "ticket=${rticket}" "${WIKI_MD_TEST_URL}/?${P}")"
check_eq "  種別・本文・題名が変換前に戻る" "wiki|$before_sha|いろいろな記法" \
         "$(value_of "$(helper meta "$P" type)" meta)|$(sha_of_body "$P")|$(value_of "$(helper meta "$P" title)" meta)"
check_eq "  タグも戻る" "$before_tags" "$(value_of "$(helper tags "$P")" tags)"
echo

echo "6. 同じ人が続けて保存したすぐあとでも、変換前の版が残ること"
G=WikiMdTest/Guest
helper write-wiki-guest "$G" "${FIX}/Clean.wiki" > /dev/null
sleep 1
helper write-wiki-guest "$G" "${FIX}/Lossy.wiki" > /dev/null
open_confirm "$G" > /dev/null
before_times=$(value_of "$(helper backup-times "$G")" times)
convert_page "$G" "$(confirm_ticket)" > /dev/null
after_times=$(value_of "$(helper backup-times "$G")" times)
check_eq "変換前の版 (Lossy) が履歴に残る" "markdown|$(sha256sum < "${FIXTURES}/input/Lossy.wiki" | cut -d' ' -f1)" \
         "$(value_of "$(helper meta "$G" type)" meta)|$(value_of "$(helper backup-body "$G" "${after_times##*,}")" body | base64 -d | sha256sum | cut -d' ' -f1)"
echo

echo "7. 変換できないときは何も変えないこと"
L=WikiMdTest/Clean
helper lock "$L" > /dev/null
open_confirm "$L" > /dev/null
convert_page "$L" "$(confirm_ticket)" > /dev/null
check_eq "ロック中 (書く権限だけの人): 理由が出る・変えない" "yes|wiki" \
         "$(said 'ロックされています|locked')|$(value_of "$(helper meta "$L" type)" meta)"
helper unlock "$L" > /dev/null
open_confirm "$L" > /dev/null
stale=$(confirm_ticket)
helper write-wiki "$L" "${FIX}/Clean.wiki" > /dev/null
convert_page "$L" "$stale" > /dev/null
check_eq "確認の画面のあとに誰かが編集した: 理由が出る・変えない" "yes|wiki" \
         "$(said '衝突|Confliction')|$(value_of "$(helper meta "$L" type)" meta)"
gsha=$(sha_of_body "$G")
convert_page "$G" "$(value_of "$(helper meta "$G" ticket)" meta)" > /dev/null
check_eq "種別 wiki でないページ: 理由が出る・変えない" "yes|markdown|$gsha" \
         "$(said '種別wikiのページだけ|Only pages of type wiki')|$(value_of "$(helper meta "$G" type)" meta)|$(sha_of_body "$G")"
helper guest read > /dev/null
check_eq "書く権限が無い: ログインを求める (401)" "401" "$(convert_page "$L" "$(value_of "$(helper meta "$L" ticket)" meta)")"
helper guest write > /dev/null
echo

echo "9. PHP の警告が出ていないこと"
log_new=$(sudo tail -n +"$((log_before + 1))" "$PHP_ERROR_LOG" 2>/dev/null \
              | grep -F "$WIKI_MD_TEST_SITE" || true)
check_eq "検証サイトの警告が増えていない" "" "$log_new"
echo

if [[ $fail -eq 0 ]]; then
    printf '%d/%d 件すべて通りました。\n' "$total" "$total"
else
    printf '%d/%d 件が失敗しました。\n' "$fail" "$total"
fi
exit $((fail == 0 ? 0 : 1))
