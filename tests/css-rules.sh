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

    # 入力欄の枠。背景色と入力欄の背景色が近い色調では、枠が無いと入力欄が
    # どこにあるのか分からなくなる (色調の設定の識別子・表示名で実際に起きた)。
    check "テキスト入力欄に枠がある" \
          "bool(re.search(r'input\[type=.text.\][^{]*\{[^}]*border:\s*1px\s+solid\s+#[0-9a-f]{6}', css, re.S))"

    # &pre の長い行は横スクロールではなく折り返す。
    # ただしアスキーアート (pre.paa) は折り返すと絵が崩れるので除外する。
    check "pre が折り返す (white-space: pre-wrap)" \
          "bool(re.search(r'(^|\})\s*pre\s*\{[^}]*white-space:\s*pre-wrap', css, re.S))"
    check "pre が長い連続文字も折る (overflow-wrap)" \
          "bool(re.search(r'(^|\})\s*pre\s*\{[^}]*overflow-wrap:\s*break-word', css, re.S))"
    check "アスキーアート pre.paa は折り返さない" \
          "bool(re.search(r'pre\.paa\s*\{[^}]*white-space:\s*pre\s*;', css, re.S))"

    # 種別 markdown のページ。wiki の出力とは構造が違うので、当たる規則が
    # あることを名指しで見る (h2 以降はどのテーマでも規則が無かった)。
    check "Markdown の h2 に大きさが指定されている" \
          "bool(re.search(r'section\.markdown h2\s*\{[^}]*font-size', css, re.S))"
    check "Markdown の引用が pre 扱いのままになっていない" \
          "bool(re.search(r'section\.markdown blockquote\s*\{[^}]*white-space:\s*normal', css, re.S))"

    # ブロックごとの &pre(wrap) / &pre(nowrap) は、サイトの既定がどちらでも効く。
    check "ブロック指定 pre.wrap がある" \
          "bool(re.search(r'pre\.wrap\s*\{[^}]*white-space:\s*pre-wrap', css, re.S))"
    check "ブロック指定 pre.nowrap がある" \
          "bool(re.search(r'pre\.nowrap\s*\{[^}]*white-space:\s*pre\s*;', css, re.S))"
}

# 設定を既定から動かしたときだけ見る項目。
# 生成し直して確かめるので、既定の巡回とは別に呼ぶ。
rules_pre_scroll() {
    check "横スクロールを選ぶと pre が折り返さない" \
          "bool(re.search(r'(^|\})\s*pre\s*\{[^}]*white-space:\s*pre\s*;', css, re.S))"
    check "横スクロールでも pre.wrap は折り返す" \
          "bool(re.search(r'pre\.wrap\s*\{[^}]*white-space:\s*pre-wrap', css, re.S))"
}

rules_density_compact() {
    check "詰めると段落の余白が既定より狭い (24px -> 20px)" \
          "bool(re.search(r'margin-top:\s*20px', css))"
    check "詰めても余白が消えていない" \
          "not re.search(r'margin-top:\s*0px;\s*margin-bottom:\s*0px', css)"
}

rules_density_loose() {
    check "ゆったりで段落の余白が既定より広い (24px -> 28px)" \
          "bool(re.search(r'margin-top:\s*28px', css))"
}

# --- テーマ固有の項目 --------------------------------------------
# テーマを足したら rules_<テーマ名> を書く。無ければ共通項目だけを見る。
# そのテーマを「そのテーマたらしめている点」を 1〜2 件書けばよい。

# common/style/.standard.css の段組みを使うテーマ向け。
# サイドバーが本文の横に並ぶこと。
rules_standard_layout() {
    check "article.main が段組みになっている (float + 100% 未満)" \
          "bool(re.search(r'(^|\})\s*article\.main\s*\{[^}]*float:\s*right[^}]*width:\s*[0-9]+%', css, re.S))"
    check "article.side が左に回り込む" \
          "bool(re.search(r'article\.side\s*\{[^}]*float:\s*left', css, re.S))"
}

rules_basic() {
    rules_standard_layout
}

rules_plain() {
    rules_standard_layout
    check "見出しの下線が消えている (線と余白で段差を出す)" \
          "bool(re.search(r'(^|\})\s*h1\s*\{[^}]*border-bottom:\s*none', css, re.S))"
}

rules_docs() {
    rules_standard_layout
    check "本文の幅が止めてある (max-width)" \
          "bool(re.search(r'article\.main\s*>\s*section\.page[^{]*\{[^}]*max-width', css, re.S))"
    check "サイドが画面に留まる (position: sticky)" \
          "bool(re.search(r'article\.side\s*\{[^}]*position:\s*sticky', css, re.S))"
}

rules_card() {
    rules_standard_layout
    check "本文がカードになっている (角丸 + 影)" \
          "bool(re.search(r'article\.main\s*>\s*section\.page[^{]*\{[^}]*border-radius[^}]*box-shadow', css, re.S))"
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

# --- 既定から動かした設定 ------------------------------------------
# 生成し直さないと確かめられないので、代表して basic で見る。
# (どのテーマも同じ common/style/ を読むため、テーマごとに回す必要はない)

for pattern in pre-scroll density-compact density-loose; do
    echo "[設定] ${pattern}"
    if ! theme_lib_generate "$site" basic "$pattern" "$work/$pattern"; then
        fail=$((fail + 1))
        continue
    fi
    CSS="$work/$pattern/main.css"
    "rules_${pattern//-/_}"
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
