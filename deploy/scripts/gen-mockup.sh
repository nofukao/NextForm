#!/bin/bash
# テーマ候補の見本を静的 HTML として書き出す
#
#   ./deploy/scripts/gen-mockup.sh [出力先]     既定: /var/www/html/moc
#
# 手で書いた HTML ではなく、配信中のサイトから実際の出力を取ってきて、
# 読み込む CSS だけをテーマごとに差し替える。デザインの判断を実物で行うため。
#
# 出力先は root 所有の場所を想定しているので sudo を使う。
# 生成物はリポジトリに入れない (このスクリプトだけを追跡する)。

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
BASE_URL="${BASE_URL:-http://localhost:8080}"
OUT="${1:-/var/www/html/moc}"

THEMES=(basic plain docs dense card)
TONES=("beige/green" "white/blue" "black/blue")

# 見本にするページ: ファイル名|取得するクエリ|見出し
# 見出しに空白を入れないこと (一覧を空白区切りで渡している)
PAGES=(
    "index|?Top|トップページ"
    "syntax|?GoldenMaster/Syntax|記法いろいろ"
    "calendar|?GoldenMaster/Calendar|カレンダー"
    "parts|?option=cssexample|画面部品の見本"
)

tone_id() { echo "${1//\//-}"; }

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

echo "取得元: ${BASE_URL}"
echo "出力先: ${OUT}"
echo

# --- 素材の HTML を取ってくる ------------------------------------
for entry in "${PAGES[@]}"; do
    IFS='|' read -r name query _ <<< "$entry"
    curl -sk "${BASE_URL}/${query}" -o "${tmp}/${name}.html"
    [[ -s "${tmp}/${name}.html" ]] || { echo "取得できない: ${query}" >&2; exit 1; }
done

# --- テーマごとに CSS を作る --------------------------------------
site="$(mktemp -d)"
rsync -a --exclude '/storage/' --exclude '/theme/' "${REPO_ROOT}/NextForm/" "${site}/"
trap 'rm -rf "$tmp" "$site"' EXIT

sudo mkdir -p "$OUT"
sudo rm -rf "${OUT:?}"/*

for theme in "${THEMES[@]}"; do
    echo "  ${theme}"
    stage="${tmp}/stage/${theme}"
    mkdir -p "$stage"
    for tone in "${TONES[@]}"; do
        rm -rf "${site:?}/theme"
        php "${REPO_ROOT}/tests/theme-css.php" "$site" "$theme" theme "THEME_TONE=${tone}" > /dev/null
        if [[ ! -d "${stage}/theme" ]]; then
            cp -a "${site}/theme" "${stage}/theme"
        fi
        cp "${site}/theme/${theme}/style/main.css" \
           "${stage}/theme/${theme}/style/main-$(tone_id "$tone").css"
    done
    cp -a "${REPO_ROOT}/NextForm/resource" "${stage}/resource"

    # --- HTML を組み立てる
    for entry in "${PAGES[@]}"; do
        IFS='|' read -r name query title <<< "$entry"
        python3 "${REPO_ROOT}/deploy/scripts/gen-mockup.py" \
            "${tmp}/${name}.html" "${stage}/${name}.html" \
            "$theme" "$name" "$title" "${THEMES[*]}" "${TONES[*]}" "${PAGES[*]}"
    done
done

python3 "${REPO_ROOT}/deploy/scripts/gen-mockup.py" --index "${tmp}/stage/index.html" \
        "${THEMES[*]}" "${TONES[*]}" "${PAGES[*]}"

sudo cp -a "${tmp}/stage/." "${OUT}/"
sudo chown -R apache:apache "$OUT"
sudo chmod -R a+rX "$OUT"

echo
echo "完了。"
for theme in "${THEMES[@]}"; do
    echo "  ${BASE_URL%/*}/moc/${theme}/"
done
