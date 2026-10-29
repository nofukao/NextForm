# tests/mermaid.sh から BROWSER=1 のときに読む (単独では動かさない)。
#
# 描画はブラウザでしか確かめられないので、検証サイトに見るページを作り、URL を出す。
# Mermaid を上げたとき (deploy/scripts/update-mermaid.sh) にも流す。
#
#   MermaidCheck/互換           図の種類ごとの見本 (tests/mermaid-check/corpus.md)
#   MermaidCheck/安全           図の中に書かれても困らないことの見本 (security.md)
#   MermaidCheck/誤りと部分編集  誤りの出し方・大きすぎる図・部分編集・保存後の位置
#   resource/mermaid-check/compare.html
#                               互換 (prototype.js あり・なしの描き比べ)・組み込み・
#                               安全を自動で調べて表にする
#   resource/mermaid-check/clean.html
#                               互換のページから prototype.js・nextform.js・theme.js
#                               を抜いた写し (compare.html が比べる相手)

# 複製した静的 CSS は複製元のもの。作業ツリーのテーマ (pre.diagram) で作り直す
sudo -u "$SITE_OWNER" php "${SITE}/app/tool/update_wiki" "${SITE}/index.php" > /dev/null \
    || { echo "テーマを作り直せませんでした" >&2; exit 1; }

write_page write "MermaidCheck/互換" "$(cat "${REPO_ROOT}/tests/mermaid-check/corpus.md")"
write_page write "MermaidCheck/安全" "$(cat "${REPO_ROOT}/tests/mermaid-check/security.md")"

LARGE="$(python3 -I -c "print('flowchart TD'); print('\n'.join('    A%d --> B%d' % (i, i) for i in range(3000)))")"
write_page write "MermaidCheck/誤りと部分編集" "$(cat <<EOF
---
title: 図の誤りと部分編集の確かめ
---

## 描ける図

ダブルクリックすると、図の原文 (コードブロック) が開く。

\`\`\`mermaid
sequenceDiagram
    participant A as 依頼する側
    participant B as 作る側
    A->>B: 依頼書を読んで
    Note over B: 利用者が設計を承認
    B-->>A: 新しい版を出した
\`\`\`

## 書き方の誤り

コードが残り、その下に「図を描けませんでした」と誤りの文が出る。

\`\`\`mermaid
flowchart TD
    A[閉じていない --> B
\`\`\`

## 知らない種類

\`\`\`mermaid
notADiagram
    x --> y
\`\`\`

## 大きすぎる図

5 万字を超えるので描かない。コードが残り、「図が大きすぎます」と出る。

\`\`\`mermaid
${LARGE}
\`\`\`

## 長い本文

図より下のこの節の段落をダブルクリックして直し、保存する。保存したあと、
見ていた段落が画面の同じ位置にあること (図に差し替わった分だけずれないこと)。

$(for i in $(seq 1 30); do printf '段落 %d。図より下にある本文。部分編集で直して保存したときの位置を見るための文。\n\n' "$i"; done)
EOF
)"

sudo mkdir -p "${SITE}/resource/mermaid-check"
sudo cp "${REPO_ROOT}/tests/mermaid-check/compare.html" "${SITE}/resource/mermaid-check/"
page_html "MermaidCheck/$(python3 -I -c 'import urllib.parse; print(urllib.parse.quote("互換"))')" \
    | python3 -I -c '
import re, sys
html = sys.stdin.read()
n = 0
for name in ("resource/script/prototype\\.js", "resource/script/nextform\\.js", "theme/[^/]+/script/theme\\.js"):
    html, k = re.subn(r"<script[^>]*src=\"%s[^\"]*\"[^>]*>\s*</script>" % name, "", html)
    n += k
if n != 3:
    sys.exit("prototype.js・nextform.js・theme.js を抜けませんでした (%d)" % n)
html = html.replace("<head>", "<head>\n<base href=\"../../\" />", 1)
sys.stdout.write(html)
' > "$WORK/clean.html" || { echo "clean.html を作れませんでした" >&2; exit 1; }
sudo cp "$WORK/clean.html" "${SITE}/resource/mermaid-check/clean.html"
sudo chown -R "$SITE_OWNER" "${SITE}/resource/mermaid-check"

encode() {
    python3 -I -c 'import sys, urllib.parse; print("/".join(urllib.parse.quote(p) for p in sys.argv[1].split("/")))' "$1"
}

cat <<EOF
  検証サイトを残しました。ブラウザ (Chrome・Firefox・iPhone か iPad の Safari) で開いて
  確かめてください。終わったら sudo rm -rf ${SITE} で消します。

  1. 自動の確かめ (互換・組み込み・安全)。上に「すべて ok」と出ること
       ${URL}/resource/mermaid-check/compare.html
  2. 図の種類ごとの見た目 (日本語・数式を含む)。崩れていないこと
       ${URL}/?$(encode "MermaidCheck/互換")
  3. 安全の見本。アラートが出ない・フォームや入力欄・ページを覆う箱が出ないこと
       ${URL}/?$(encode "MermaidCheck/安全")
  4. 誤りと部分編集 (管理者でログインして)
       ${URL}/?$(encode "MermaidCheck/誤りと部分編集")
       - 誤りの図・知らない種類・大きすぎる図は、コードが残り、下に誤りの文が出る
       - 描ける図をダブルクリックすると原文が開き、保存すると描き直される
       - 「長い本文」の段落を部分編集して保存すると、見ていた段落が同じ位置にある
       - 編集画面の「プレビュー」で図が描かれる
       - JavaScript を切ると、どの図もコードのまま読める
  5. 見た目の設定
       - 色調を「ブラック/ブルー」にしても図が読める (白い下地)
       - 携帯の幅で、広い図が本文の幅に縮む。印刷 (プレビュー) で図が切れない
EOF
