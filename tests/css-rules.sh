#!/bin/bash
# 生成された静的 CSS のルール検査
#
#   ./tests/css-rules.sh
#
# 環境変数:
#   BASE_URL   対象インスタンス (既定: http://localhost:8080)
#
# app/theme/basic/style/*.css は PHP テンプレートで、theme_convert() が
# theme/basic/style/main.css を生成する。ゴールデンマスターは HTML しか見ないので
# 生成 CSS の崩れを検出できず、これまでサイドバーのレイアウトと編集画面の
# カーソルの 2 件を取り逃がしている。
#
# 全体のスナップショットは取らない。フォントサイズや配色は管理画面から変更
# できるため、それだけで落ちてしまう。過去に壊れた箇所を名指しで検査する。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# 向け先は tests/env.local に書く (Git には入らない)。
# 無い場合は下の既定値を使う。tests/env.local.example を参照。
[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"

BASE_URL="${BASE_URL:-http://localhost:8080}"
CSS="$(mktemp)"
trap 'rm -f "$CSS"' EXIT

code=$(curl -sk -o "$CSS" -w '%{http_code}' "${BASE_URL}/theme/basic/style/main.css?nocache=$RANDOM")
if [[ "$code" != "200" ]]; then
    echo "CSS を取得できません: HTTP $code" >&2
    exit 1
fi

echo "BASE_URL = $BASE_URL"
echo

fail=0

# $1 説明  $2 判定用の python 式 (css 変数が使える)
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

# サイドバーが本文の横に並ぶこと。
# install.inc が SIDE_PAGENAME を '' に上書きしていると、
# article.main が全幅になり Side が下へ回り込む。
check "article.main が段組みになっている (float + 100% 未満)" \
      "bool(re.search(r'(^|\})\s*article\.main\s*\{[^}]*float:\s*right[^}]*width:\s*[0-9]+%', css, re.S))"
check "サイドバー無効化ルールが出ていない" \
      "not re.search(r'article\.main\s*\{[^}]*margin-left:\s*0px;\s*width:\s*100%', css)"
check "article.side が左に回り込む" \
      "bool(re.search(r'article\.side\s*\{[^}]*float:\s*left', css, re.S))"

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

echo
if [[ $fail -eq 0 ]]; then
    echo "全項目 ok"
    exit 0
fi
echo "${fail} 件 失敗"
exit 1
