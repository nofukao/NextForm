#!/bin/bash
# 生成された静的 CSS のルール検査
#
#   ./tests/css-rules.sh              全テーマ + 配信中のサイト
#   ./tests/css-rules.sh basic plain  テーマを絞る
#
# 環境変数:
#   BASE_URL   配信中のサイト (既定: http://localhost:8080)
#
# app/theme/<名前>/style/*.css は PHP テンプレートで、theme_convert() が
# theme/<名前>/style/main.css を生成する。ゴールデンマスターは HTML しか見ないので
# 生成 CSS の崩れを検出できず、これまでサイドバーのレイアウトと編集画面の
# カーソルの 2 件を取り逃がしている。
#
# 検査は 2 段階ある。
#
#   ローカル生成   作業ツリーの複製に対して全テーマ分の CSS を生成して検査する。
#                  静的ファイルは「選ばれているテーマ」の分しか作られないため、
#                  配信中のサイトを見るだけでは他のテーマを検査できない。
#   配信中         実際に配信されている CSS を検査する。サイトの設定
#                  (SIDE_PAGENAME など) が絡む項目はこちらでしか見られない。
#
# 全体のスナップショットは取らない。フォントサイズや配色は管理画面から変更
# できるため、それだけで落ちてしまう。過去に壊れた箇所を名指しで検査する。
# 生成物が変わっていないことを丸ごと確かめたいときは tests/theme-diff.sh を使う。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# 向け先は tests/env.local に書く (Git には入らない)。
# 無い場合は下の既定値を使う。tests/env.local.example を参照。
[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
. "${REPO_ROOT}/tests/theme-lib.sh"

BASE_URL="${BASE_URL:-http://localhost:8080}"
CSS=""
fail=0

# $1 説明  $2 判定用の python 式 ($CSS を読んだ css 変数が使える)
check() {
    local desc="$1" expr="$2"
    if python3 -c "
import re, sys
css = open('$CSS', encoding='utf-8').read()
css = re.sub(r'/\*.*?\*/', '', css, flags=re.S)
sys.exit(0 if ($expr) else 1)
"; then
        printf '  ok    %s\n' "$desc"
    else
        printf '  FAIL  %s\n' "$desc"
        fail=$((fail + 1))
    fi
}

# --- どのテーマでも成り立つべき項目 ------------------------------
# 実体は app/theme/common/style/ にあり、全テーマが同じものを読む。

rules_common() {
    # 編集画面のカーソルが行頭で見えなくなる問題 (Chrome)。
    # padding が 0 だとキャレットが内容ボックスの左端に描かれ、Chrome でクリップされる。
    check "編集用 textarea に左パディングがある" \
          "bool(re.search(r'form\.text_edit\s*>\s*textarea\s*\{[^}]*padding:[^;}]*\b([1-9][0-9]*)px\s*(;|\})', css, re.S))"
    check "編集用 textarea が border-box (幅がはみ出さない)" \
          "bool(re.search(r'form\.text_edit\s*>\s*textarea\s*\{[^}]*box-sizing:\s*border-box', css, re.S))"

    # &pre の長い行は横スクロールではなく折り返す。
    # ただしアスキーアート (pre.paa) は折り返すと絵が崩れるので除外する。
    check "pre が折り返す (white-space: pre-wrap)" \
          "bool(re.search(r'(^|\})\s*pre\s*\{[^}]*white-space:\s*pre-wrap', css, re.S))"
    check "pre が長い連続文字も折る (overflow-wrap)" \
          "bool(re.search(r'(^|\})\s*pre\s*\{[^}]*overflow-wrap:\s*break-word', css, re.S))"
    check "アスキーアート pre.paa は折り返さない" \
          "bool(re.search(r'pre\.paa\s*\{[^}]*white-space:\s*pre\s*;', css, re.S))"
}

# --- テーマ固有の項目 --------------------------------------------
# テーマを足したら rules_<テーマ名> を書く。無ければ共通項目だけを見る。

# サイドバーが本文の横に並ぶこと (basic は float の段組み)。
rules_basic() {
    check "article.main が段組みになっている (float + 100% 未満)" \
          "bool(re.search(r'(^|\})\s*article\.main\s*\{[^}]*float:\s*right[^}]*width:\s*[0-9]+%', css, re.S))"
    check "article.side が左に回り込む" \
          "bool(re.search(r'article\.side\s*\{[^}]*float:\s*left', css, re.S))"
}

# --- 配信中のサイトでしか見られない項目 --------------------------
# サイトの設定に依存するので、生成しただけでは判定できない。

rules_live() {
    # install.inc が SIDE_PAGENAME を '' に上書きしていると、
    # article.main が全幅になり Side が下へ回り込む。
    check "サイドバー無効化ルールが出ていない" \
          "not re.search(r'article\.main\s*\{[^}]*margin-left:\s*0px;\s*width:\s*100%', css)"
}

# --- ローカル生成 -------------------------------------------------

if [[ $# -gt 0 ]]; then
    themes=("$@")
else
    mapfile -t themes < <(theme_lib_themes)
fi

work="$(mktemp -d)"
trap 'theme_lib_cleanup; rm -rf "$work"' EXIT
site="$(theme_lib_make_site "${REPO_ROOT}/NextForm")"

for theme in "${themes[@]}"; do
    echo "[ローカル生成] ${theme}"
    if ! theme_lib_generate "$site" "$theme" default "$work/$theme"; then
        fail=$((fail + 1))
        continue
    fi
    CSS="$work/$theme/main.css"
    rules_common
    if declare -F "rules_${theme}" > /dev/null; then
        "rules_${theme}"
    else
        echo "  --    ${theme} 固有の検査はまだ無い (rules_${theme} を書く)"
    fi
    echo
done

# --- 配信中のサイト -----------------------------------------------

echo "[配信中] ${BASE_URL}"
html="$(curl -sk "${BASE_URL}/")"
css_path="$(sed -n 's|.*href="\(theme/[^"]*/style/main\.css\)[^"]*".*|\1|p' <<< "$html" | head -1)"
if [[ -z "$css_path" ]]; then
    echo "  FAIL  トップページが theme/*/style/main.css を参照していない"
    fail=$((fail + 1))
else
    live_theme="$(basename "$(dirname "$(dirname "$css_path")")")"
    CSS="$work/live-main.css"
    code=$(curl -sk -o "$CSS" -w '%{http_code}' "${BASE_URL}/${css_path}?nocache=$RANDOM")
    if [[ "$code" != "200" ]]; then
        echo "  FAIL  CSS を取得できない: HTTP $code (${css_path})"
        fail=$((fail + 1))
    else
        echo "  テーマ: ${live_theme}"
        rules_common
        if declare -F "rules_${live_theme}" > /dev/null; then
            "rules_${live_theme}"
        fi
        rules_live
    fi
fi

echo
if [[ $fail -eq 0 ]]; then
    echo "全項目 ok"
    exit 0
fi
echo "${fail} 件 失敗"
exit 1
