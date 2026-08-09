#!/bin/bash
# 配布用の tar.gz を作る。
#
#   ./deploy/scripts/make-dist.sh            HEAD から作る
#   ./deploy/scripts/make-dist.sh v0.1       tag から作る
#
# 上流と同じ「展開するとトップに index.php がある」形式になる。
#
# git archive を使い、**Git が追跡しているファイルだけ**を固める。
# tar で NextForm/ をそのまま固めると、.gitignore で無視しているだけの
# 生成物 (theme/ storage/ install-info.dat) や作業中のファイルが
# 紛れ込む。実際、開発中に php -S で動かした残骸の theme/ が
# 作業ツリーに残っていたことがある。

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

REF="${1:-HEAD}"
OUT="NextForm.tar.gz"

if ! git rev-parse --verify --quiet "${REF}^{commit}" > /dev/null; then
    echo "そのようなコミット/タグはありません: $REF" >&2
    exit 1
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

git archive --format=tar --prefix=NextForm/ "${REF}:NextForm" | tar x -C "$WORK"

# アップグレード手順書を同梱する。
# 正本は docs/upgrade-guide.md で、配布物では UPGRADE.md という名前にする。
# tar.gz を展開しただけの人が、リポジトリを見に行かなくても読めるようにするため。
# 二重管理にならないよう、ここでコピーする以外の実体は持たない。
git show "${REF}:docs/upgrade-guide.md" > "${WORK}/NextForm/UPGRADE.md"
touch -r "${WORK}/NextForm/index.php" "${WORK}/NextForm/UPGRADE.md"

# --sort/--owner/--group は、同じ ref からは同じ tar.gz ができるようにするため。
tar czf "$OUT" --sort=name --owner=0 --group=0 --numeric-owner -C "$WORK" NextForm

# 一覧は一度だけ取って使い回す。tar に grep -q をつなぐと、grep が先に
# 終了して tar が SIGPIPE で落ち、pipefail がそれを失敗として拾ってしまう。
LIST="$(tar tzf "$OUT")"

echo "作成: $OUT  ($(stat -c %s "$OUT") bytes, ref=$REF)"
echo
echo "内容の確認:"
echo "$LIST" | wc -l | sed 's/^/  ファイル数: /'
echo "$LIST" | grep -E '^NextForm/[^/]*$' | sed 's/^/  /'

# 生成物や実行時データが混ざっていないこと
if echo "$LIST" | grep -qE '^NextForm/(theme|storage)/|^NextForm/install-info\.dat'; then
    echo
    echo "エラー: 生成物が含まれています" >&2
    exit 1
fi

# アップグレードに必要なものが入っていること
for required in NextForm/UPGRADE.md NextForm/app/tool/upgrade NextForm/app/version.inc; do
    if ! echo "$LIST" | grep -qx "$required"; then
        echo
        echo "エラー: $required が含まれていません" >&2
        exit 1
    fi
done
