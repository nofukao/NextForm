#!/bin/bash
# ゴールデンマスターテスト
#
#   ./tests/golden.sh              expected/ と比較する
#   ./tests/golden.sh --update     現在の出力を expected/ として保存する
#
# 環境変数:
#   BASE_URL   対象インスタンス (既定: http://localhost:8080)
#
# 前提: tests/golden/input/*.wiki が対象インスタンスに投入済みであること。
#   sudo -u apache php deploy/scripts/gen-pages.php \
#       /var/www/html/nextform/index.php --dir <input のコピー>
# (gen-pages.php とその入力は index.php と同じ所有者である必要がある。
#  app/tool/common が getmyuid() で所有者一致を要求するため。)

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# 向け先は tests/env.local に書く (Git には入らない)。
# 無い場合は下の既定値を使う。tests/env.local.example を参照。
[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
BASE_URL="${BASE_URL:-http://localhost:8080}"
GOLDEN_DIR="${REPO_ROOT}/tests/golden"
ACTUAL_DIR="${GOLDEN_DIR}/actual"
EXPECTED_DIR="${GOLDEN_DIR}/expected"

# 取得対象。<出力ファイル名>=<クエリ文字列>
#
# 対象はパーサの出力だけに絞る。?option=allpage や ?option=alltag のような
# 内容依存の一覧を混ぜると、インスタンスにページを 1 つ足しただけで落ちてしまい、
# 「振る舞いが変わったのか、データが変わっただけなのか」が区別できなくなる。
# 画面が壊れていないことは tests/smoke.sh 側で見る。
#
# フィクスチャは GoldenMaster/ 配下に置いてある。手動テストで作るページと
# 名前が衝突しないので、それ以外のページは自由に編集してよい。
TARGETS=(
    "Top=?GoldenMaster/Top"
    "Calendar=?GoldenMaster/Calendar"
    "Syntax=?GoldenMaster/Syntax"
    "Markdown=?GoldenMaster/Markdown"
)

# 比較対象を <article class="main"> の中だけに絞り、実行のたびに変わる値を潰す。
#
# ページ全体を比較すると、サイド列 (<article class="side">) やサイトナビ
# (<nav class="site_menu">) が入ってしまう。これらは Side ページを作る、
# マニュアルページを生成する、といった **フィクスチャと無関係な操作** で変わり、
# 「パーサの出力が変わったのか、周辺のデータが変わっただけなのか」が
# 区別できなくなる。article.main の中にはページツールと本文と meta があり、
# フィクスチャの描画結果はすべてそこに入っている。
normalize() {
    python3 -c '
import re, sys
html = sys.stdin.read()
m = re.search(r"<article class=\"main\">.*?</article>", html, re.S)
out = m.group(0) if m else "<article class=\"main\"> NOT FOUND </article>"
out = re.sub(r"data-ticket=\"[0-9a-f]{32}\"", "data-ticket=\"TICKET\"", out)
out = re.sub(r"\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}", "TIMESTAMP", out)
out = re.sub(r"(compare[12]time=)\d+", r"\1EPOCH", out)
out = re.sub(r"([&?]|&amp;)t=\d+", r"\1t=EPOCH", out)

# 「今日」に依存する 2 か所を潰す。
#
# フィクスチャは &calendar(2026-08) と月を固定してあるので表そのものは動かない
# が、その中で今日にあたるセルに today クラスが付き、「週」リンクの日付が
# 今日になる。expected を採取した日にしか通らないテストになってしまう。
# (2026-08-03 に採取したものが 08-09 に落ちた。)
out = re.sub(r"class=\"([^\"]*)\"",
             lambda m: "class=\"" + re.sub(r"\s*\btoday\b", "", m.group(1)).strip() + "\"",
             out)
out = re.sub(r"(calendar=)\d{4}-\d{2}-\d{2}(&amp;calendar_mode=weeks)", r"\1DATE\2", out)

# フィクスチャを投入した環境に依存する値を潰す。潰しておかないと、
# 別の環境で setup-fixtures.sh を流しただけでテストが落ちる。
#   - 作成者 / 最終更新者 … 管理者の名前は環境ごとに違う
#   - <time datetime="..."> … ページを投入した時刻
out = re.sub(r"(class=\"(?:create_user|last_modify_user)\">)[^<]*", r"\1USER", out)
out = re.sub(r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}[+-]\d{2}:\d{2}", "DATETIME", out)

sys.stdout.write(out)
'
}

UPDATE=0
[[ "${1:-}" == "--update" ]] && UPDATE=1

mkdir -p "$ACTUAL_DIR" "$EXPECTED_DIR"

echo "BASE_URL = $BASE_URL"
echo

fail=0
for target in "${TARGETS[@]}"; do
    name="${target%%=*}"
    query="${target#*=}"
    url="${BASE_URL}/${query}"

    code=$(curl -sk -o "${ACTUAL_DIR}/${name}.raw" -w '%{http_code}' "$url")
    if [[ "$code" != "200" ]]; then
        printf '%-12s FAIL  HTTP %s\n' "$name" "$code"
        fail=$((fail + 1))
        continue
    fi
    normalize < "${ACTUAL_DIR}/${name}.raw" > "${ACTUAL_DIR}/${name}.html"
    rm -f "${ACTUAL_DIR}/${name}.raw"

    if [[ $UPDATE -eq 1 ]]; then
        cp "${ACTUAL_DIR}/${name}.html" "${EXPECTED_DIR}/${name}.html"
        printf '%-12s saved (%s bytes)\n' "$name" "$(wc -c < "${EXPECTED_DIR}/${name}.html")"
        continue
    fi

    if [[ ! -f "${EXPECTED_DIR}/${name}.html" ]]; then
        printf '%-12s FAIL  expected/%s.html が無い (--update で作成)\n' "$name" "$name"
        fail=$((fail + 1))
    elif diff -q "${EXPECTED_DIR}/${name}.html" "${ACTUAL_DIR}/${name}.html" > /dev/null; then
        printf '%-12s ok\n' "$name"
    else
        printf '%-12s FAIL  差分あり\n' "$name"
        diff -u "${EXPECTED_DIR}/${name}.html" "${ACTUAL_DIR}/${name}.html" | head -40
        fail=$((fail + 1))
    fi
done

echo
if [[ $UPDATE -eq 1 ]]; then
    echo "expected/ を更新しました。内容を目視で確認してからコミットしてください。"
    exit 0
fi
if [[ $fail -eq 0 ]]; then
    echo "全 ${#TARGETS[@]} 件 一致"
    exit 0
fi
echo "${fail} / ${#TARGETS[@]} 件 失敗"
exit 1
