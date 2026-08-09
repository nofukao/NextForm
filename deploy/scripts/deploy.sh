#!/bin/bash
# NextForm を Apache の DocumentRoot へ配置する
#
#   ./deploy/scripts/deploy.sh          2 回目以降 (index.php を保護)
#   ./deploy/scripts/deploy.sh --init   初回 (index.php も配置。インストーラを動かす)
#
# 除外するもの:
#   storage/           wiki の実データ (インスタンス固有)
#   theme/             app/theme/ からビルドされた静的 CSS/JS
#   install-info.dat   インストール時の環境記録
#   index.php          インストール後にカスタム設定が書き込まれる (--init 時のみ配置)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC="${REPO_ROOT}/NextForm/"
DEST="${DEST:-/var/www/html/nextform}"

# パターンは必ず先頭 / でアンカーする。アンカーしないと rsync は任意の階層で
# マッチしてしまい、'theme/' が app/theme/ (テーマのソース) まで除外する。
EXCLUDES=(
    --exclude '/storage/'
    --exclude '/theme/'
    --exclude '/install-info.dat'
)

if [[ "${1:-}" == "--init" ]]; then
    echo "初回配置: index.php も含めて配置します"
else
    echo "更新配置: index.php は保護します (初回は --init を付けてください)"
    EXCLUDES+=(--exclude '/index.php')
fi

echo "  ${SRC}"
echo "  -> ${DEST}/"

sudo mkdir -p "${DEST}"
sudo rsync -a --delete "${EXCLUDES[@]}" "${SRC}" "${DEST}/"
sudo chown -R apache:apache "${DEST}"

echo "完了。"
echo
echo "app/theme/ の CSS を変更した場合は、管理画面で静的 CSS を再生成してください:"
echo "  ?option=admin_setup の「テーマ」を無変更のまま「適用」"
