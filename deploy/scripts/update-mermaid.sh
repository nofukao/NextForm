#!/bin/bash
# 同梱する Mermaid を取り直して NextForm/resource/script/mermaid/ を作り直す
#
#   ./deploy/scripts/update-mermaid.sh               mermaid.lock の版を取り直す
#   ./deploy/scripts/update-mermaid.sh --pin 12.3.0  版を変える (mermaid.lock を書き換える)
#
# update-vendor.sh (PHP のライブラリ) のブラウザ版。Mermaid は npm にしか無いが、
# node も npm も要らない。npm の登録所から tgz を curl で取り、sha512 を
# mermaid.lock と突き合わせ、dist/mermaid.min.js を手を加えずに置く。
#
# 上流に脆弱性が出たら (リリースのたびに勧告を見る。docs/development/workflow.md §6)
# --pin で版を上げ、tests/mermaid.sh が出すブラウザの確認を流してからコミットする。
#
# 中身は update-mermaid.py。部品のライセンスの洗い出しと、認めていない
# ライセンスが混ざっていたら止まる検査をする。

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

for command in curl python3; do
    if ! command -v "$command" > /dev/null; then
        echo "$command が要ります (開発機のみ。利用者には不要)" >&2
        exit 1
    fi
done

exec python3 -I "${REPO_ROOT}/deploy/scripts/update-mermaid.py" "$@"
