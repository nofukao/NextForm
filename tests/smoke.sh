#!/bin/bash
# HTTP スモークテスト
#
# 全 option 画面とページの主な経路を巡回し、期待どおりの HTTP ステータスが
# 返り、かつ PHP の警告が出ないことを確認する。
#
#   ./tests/smoke.sh
#
# 環境変数:
#   BASE_URL   対象インスタンス (既定: http://localhost:8080)
#
# 前提: /etc/php.d/99-nextform-dev.ini で error_reporting = E_ALL に
#       なっていること (docs/setup-guide.md §2.1)。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# 向け先は tests/env.local に書く (Git には入らない)。
# 無い場合は下の既定値を使う。tests/env.local.example を参照。
[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
BASE_URL="${BASE_URL:-http://localhost:8080}"
PHP_ERROR_LOG="${PHP_ERROR_LOG:-/var/log/php-fpm/www-error.log}"

# 未ログインでは 401 が正しい応答の画面。それ以外は 200 を期待する。
declare -A EXPECT_401=(
    [admin_analyze]=1 [admin_manual]=1 [admin_permissions]=1
    [admin_user]=1    [login]=1
)

baseline=$(sudo cat "$PHP_ERROR_LOG" | wc -l)

echo "BASE_URL = $BASE_URL"
echo

fail=0
total=0

# $1 表示名  $2 URL の後ろ  $3 期待する HTTP ステータス
hit() {
    local got
    total=$((total + 1))
    got=$(curl -sk -o /dev/null -w '%{http_code}' "${BASE_URL}/$2")
    if [[ "$got" == "$3" ]]; then
        printf '%-24s %s\n' "$1" "$got"
    else
        printf '%-24s %s  FAIL (期待 %s)\n' "$1" "$got" "$3"
        fail=$((fail + 1))
    fi
}

for path in "${REPO_ROOT}"/NextForm/app/option/*.inc; do
    opt=$(basename "$path" .inc)
    want=200
    [[ -n "${EXPECT_401[$opt]:-}" ]] && want=401
    hit "$opt" "?option=${opt}" "$want"
done

# option の画面だけを回っていると、ページそのものの経路に入らない。
# 画像の縮小は twpage:// ストリームを通るので、ここでしか出ない警告があった
# (PageContentStream の動的プロパティ。v0.6.1 で修正)。
#
# 縮小した画像はキャッシュに残る。毎回同じ寸法を頼むと 2 回目からは
# キャッシュを返すだけになり、縮小の経路を通らない。寸法を変えて必ず通す。
echo
IMAGE_PAGE="GoldenMaster/Markdown/portforward01.png"
RESIZE=$(( RANDOM % 24 + 8 ))
hit "画像ページ"     "?${IMAGE_PAGE}" 200
hit "画像の原本"     "?${IMAGE_PAGE}&action=raw" 200
hit "画像の縮小"     "?${IMAGE_PAGE}&action=raw&width=${RESIZE}&height=${RESIZE}" 200
hit "Wiki ページ"    "?GoldenMaster/Syntax" 200
hit "Markdown ページ" "?GoldenMaster/Markdown" 200
hit "ページの目次"   "?GoldenMaster/Syntax&option=summary" 200
hit "ページの原本"   "?GoldenMaster/Syntax&action=source" 200

echo
echo "=== この巡回で出た PHP の警告 ==="
# エラーログは同じサーバの全サイトが共有している。巡回した先の分だけを見る。
# (BASE_URL が php -S を指しているときは、警告はログではなく標準エラーに出るので
#  ここでは何も拾えない。判定は Apache 配下のインスタンスに対して行うこと。)
SITE_DIR=$(basename "$BASE_URL")
warnings=$(sudo cat "$PHP_ERROR_LOG" | tail -n +$((baseline + 1)) \
    | grep -F "/var/www/html/${SITE_DIR}/" \
    | sed -E 's/^\[[^]]*\] //; s| in /var/www/html/[^/]+/| @ |')

if [[ -z "$warnings" ]]; then
    echo "なし"
else
    echo "$warnings" | sort | uniq -c | sort -rn
    echo
    echo "合計 $(echo "$warnings" | wc -l) 件"
fi

echo
echo "=== HTTP ステータス: $((total - fail)) / ${total} 件 一致 ==="
[[ $fail -eq 0 && -z "$warnings" ]] || exit 1
