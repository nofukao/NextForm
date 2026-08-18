#!/bin/bash
# テーマの生成物が変わっていないことを確かめる
#
#   ./tests/theme-diff.sh             main と現在の作業ツリーを比べる
#   ./tests/theme-diff.sh v0.4.1      比べる相手を指定する
#
# 指定した ref を一時 worktree に取り出し、そちらと作業ツリーの両方で
# テーマの静的ファイルを生成して突き合わせる。設定は tests/theme-lib.sh の
# パターン表 (色調・レイアウト・文字の大きさ・背景画像) を全部回す。
#
# 振る舞いを変えないはずの整理 (共通部分の抽出、ファイルの移動、コメントの追記)
# を「1 バイトも変わらない」ことで担保するために使う。見た目を変える変更では
# 当然差分が出るので、その差分が意図したものかを読むために使う。
#
# tests/css-rules.sh との違い:
#   css-rules.sh   壊れやすい箇所を名指しで検査する。設定を変えても落ちない
#   theme-diff.sh  生成物を丸ごと比べる。意図した変更でも差分として出る

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
. "${REPO_ROOT}/tests/theme-lib.sh"

REF="${1:-main}"

if ! git -C "$REPO_ROOT" rev-parse --verify --quiet "${REF}^{commit}" > /dev/null; then
    echo "そんな ref はありません: ${REF}" >&2
    exit 1
fi

work="$(mktemp -d)"
ref_tree="$(mktemp -d)"
rmdir "$ref_tree"
cleanup() {
    theme_lib_cleanup
    git -C "$REPO_ROOT" worktree remove --force "$ref_tree" 2> /dev/null
    rm -rf "$work" "$ref_tree"
}
trap cleanup EXIT

echo "比較対象: ${REF} ($(git -C "$REPO_ROOT" log -1 --format='%h %s' "$REF"))"
echo "作業ツリー: $(git -C "$REPO_ROOT" branch --show-current)"
echo

git -C "$REPO_ROOT" worktree add --detach --quiet "$ref_tree" "$REF" || exit 1

ref_site="$(theme_lib_make_site "${ref_tree}/NextForm")"
new_site="$(theme_lib_make_site "${REPO_ROOT}/NextForm")"

mapfile -t ref_themes < <(theme_lib_themes "${ref_tree}/NextForm")
mapfile -t new_themes < <(theme_lib_themes "${REPO_ROOT}/NextForm")

fail=0
patterns=()
mapfile -t patterns < <(theme_lib_pattern_names)

for theme in "${new_themes[@]}"; do
    if ! printf '%s\n' "${ref_themes[@]}" | grep -qx "$theme"; then
        printf '  新規  %-8s (%s には無いので比べない)\n' "$theme" "$REF"
        continue
    fi
    changed=0
    for pattern in "${patterns[@]}"; do
        if ! theme_lib_generate "$ref_site" "$theme" "$pattern" "$work/ref/$theme/$pattern" \
           || ! theme_lib_generate "$new_site" "$theme" "$pattern" "$work/new/$theme/$pattern"; then
            fail=$((fail + 1))
            changed=1
            continue
        fi
        if ! diff -r -q "$work/ref/$theme/$pattern" "$work/new/$theme/$pattern" > /dev/null; then
            printf '  差分  %-8s %s\n' "$theme" "$pattern"
            diff -r -u "$work/ref/$theme/$pattern" "$work/new/$theme/$pattern" \
                | sed -n '1,40p' | sed 's/^/        /'
            changed=1
            fail=$((fail + 1))
        fi
    done
    [[ $changed -eq 0 ]] && printf '  同一  %-8s (%d パターン)\n' "$theme" "${#patterns[@]}"
done

for theme in "${ref_themes[@]}"; do
    if ! printf '%s\n' "${new_themes[@]}" | grep -qx "$theme"; then
        printf '  消失  %-8s (%s にはあった)\n' "$theme" "$REF"
        fail=$((fail + 1))
    fi
done

echo
if [[ $fail -eq 0 ]]; then
    echo "生成物に変化なし"
    exit 0
fi
echo "${fail} 件 差分あり"
exit 1
