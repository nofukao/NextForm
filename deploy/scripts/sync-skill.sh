#!/bin/bash
# AI 向けの手順を、組み込みマニュアルからスキルへ写す
#
#   ./deploy/scripts/sync-skill.sh
#
# 正本は NextForm/app/manual/AI.md (組み込みマニュアルの NextFormManual/AI)。
# plugins/nextform/skills/nextform-pages/SKILL.md は、先頭の name / description の
# あとに同じ本文を置く。プラグインはプラグインのフォルダーしか写さないので、
# AI.md を参照させられず、写しを持つしかない。
#
# AI.md を直したらこれを流す。食い違ったままだと tests/manual.sh が止める。
# SKILL.md の先頭 (--- で囲んだ name / description) はそのまま残す。

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
AI_MD="${REPO_ROOT}/NextForm/app/manual/AI.md"
SKILL_MD="${REPO_ROOT}/plugins/nextform/skills/nextform-pages/SKILL.md"

if [[ "$(head -1 "$SKILL_MD")" != "---" ]]; then
    echo "SKILL.md の先頭が --- ではありません: $SKILL_MD" >&2
    exit 1
fi

tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT

# 先頭の --- から次の --- までを残し、空行 1 つのあとに AI.md を続ける
awk 'NR == 1 { print; next }
     { print }
     $0 == "---" { exit }' "$SKILL_MD" > "$tmp"
if [[ "$(tail -1 "$tmp")" != "---" ]]; then
    echo "SKILL.md の先頭の --- が閉じていません: $SKILL_MD" >&2
    exit 1
fi
printf '\n' >> "$tmp"
cat "$AI_MD" >> "$tmp"

if cmp -s "$tmp" "$SKILL_MD"; then
    echo "SKILL.md は AI.md と同じです。"
else
    cp "$tmp" "$SKILL_MD"
    echo "SKILL.md を AI.md から書き直しました。"
fi
