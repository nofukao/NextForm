#!/bin/bash
# HTTP スモークテスト
#
# 全 option 画面を巡回し、期待どおりの HTTP ステータスが返り、
# かつ PHP の警告が出ないことを確認する。
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
for path in "${REPO_ROOT}"/NextForm/app/option/*.inc; do
    opt=$(basename "$path" .inc)
    total=$((total + 1))
    want=200
    [[ -n "${EXPECT_401[$opt]:-}" ]] && want=401

    got=$(curl -sk -o /dev/null -w '%{http_code}' "${BASE_URL}/?option=${opt}")
    if [[ "$got" == "$want" ]]; then
        printf '%-24s %s\n' "$opt" "$got"
    else
        printf '%-24s %s  FAIL (期待 %s)\n' "$opt" "$got" "$want"
        fail=$((fail + 1))
    fi
done

echo
echo "=== この巡回で出た PHP の警告 ==="
warnings=$(sudo cat "$PHP_ERROR_LOG" | tail -n +$((baseline + 1)) \
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
[[ $fail -eq 0 ]] || exit 1
