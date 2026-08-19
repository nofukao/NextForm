#!/bin/bash
# 同梱するライブラリを取り直して NextForm/app/vendor/ を作り直す
#
#   ./deploy/scripts/update-vendor.sh
#
# NextForm は「tar.gz を展開するだけで動く」配布形式を守るため、
# 利用者に composer install を要求しない。代わりに、ここで取得したものを
# app/vendor/ として配布物に同梱する。
#
# 版は composer.lock で固定する。上流に脆弱性が出たら、
#   composer update league/commonmark
# のあとにこのスクリプトを流し、差分をコミットする。
#
# composer.json / composer.lock / リポジトリ直下の vendor/ は配布物に入らない
# (make-dist.sh が NextForm/ だけを固める)。

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DEST="${REPO_ROOT}/NextForm/app/vendor"

cd "$REPO_ROOT"

if ! command -v composer > /dev/null; then
    echo "composer が要ります (開発機のみ。利用者には不要)" >&2
    exit 1
fi

echo "取得中 ..."
composer install --no-dev --optimize-autoloader --no-interaction

# 中身は削らない。tests/ や docs/ は配布物に元から含まれておらず、
# 削れるのは README と CHANGELOG 程度 (約 100KB)。
# ライセンス表示を含むファイルを取りこぼす危険のほうが大きい。
echo "複製中 -> ${DEST}"
rm -rf "$DEST"
mkdir -p "$DEST"
rsync -a --exclude '.git' "${REPO_ROOT}/vendor/" "${DEST}/"

echo
echo "同梱したもの:"
composer show --no-dev 2>/dev/null | awk '{printf "  %-32s %s\n", $1, $2}'
echo
printf '  合計 %s / %d ファイル\n' \
       "$(du -sh "$DEST" | cut -f1)" "$(find "$DEST" -type f | wc -l)"
echo
echo "確認してからコミットしてください:"
echo "  git add composer.json composer.lock NextForm/app/vendor"
