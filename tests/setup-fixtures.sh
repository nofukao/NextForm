#!/bin/bash
# ゴールデンマスターの入力ページを対象インスタンスに投入する。
#
#   ./tests/setup-fixtures.sh [index.php のパス]
#
# 既定の対象は /var/www/html/nextform/index.php。
# 拡張子がページ種別になる (.wiki -> wiki、.md -> markdown、それ以外 -> file)。
# 何度実行してもよい (同じページを上書きするだけ)。
# 手動テストでフィクスチャを壊してしまったときの復旧にも使う。
#
# なぜ一時ディレクトリに複製してから実行するのか:
#   app/tool/common の eval_index_php() が fileowner(index.php) === getmyuid()
#   を要求する。getmyuid() は実行プロセスではなく **スクリプトファイルの所有者**
#   を返すため、スクリプトと入力を対象インスタンスと同じ所有者にしておく必要が
#   ある。リポジトリは apache から読めない場所にあることが多いので複製する。

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# 向け先と管理者名は tests/env.local に書く (他のテストと同じ)。
# ここだけ読んでいなかったため、管理者名が admin 以外のサイトでは
# 認証に失敗して 1 件も書けなかった。しかも下で既存ページを先に消すので、
# フィクスチャが消えたままゴールデンマスターが落ちる状態になっていた。
[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"

INDEX_PATH="${1:-/var/www/html/nextform/index.php}"

if [[ ! -f "$INDEX_PATH" ]]; then
    echo "index.php が見つかりません: $INDEX_PATH" >&2
    exit 1
fi

OWNER="$(stat -c '%U' "$INDEX_PATH")"
WORK="$(mktemp -d)"
cleanup() { sudo rm -rf "$WORK"; }
trap cleanup EXIT

cp "${REPO_ROOT}/deploy/scripts/gen-pages.php" "$WORK/"
cp -r "${REPO_ROOT}/tests/golden/input" "$WORK/"
sudo chown -R "$OWNER" "$WORK"

echo "対象   : $INDEX_PATH (所有者 $OWNER)"
echo "投入元 : ${REPO_ROOT}/tests/golden/input"
echo

# 既存のフィクスチャページを storage から消してから投入する。
# 上書きすると page_write() がバックアップを 1 つ作り、「変更点」「履歴」の
# ツールリンクが disabled から有効に変わる。つまり実行回数で出力が変わり、
# ゴールデンマスターが 2 回目の実行で落ちてしまう。毎回まっさらから作る。
INSTANCE_DIR="$(dirname "$INDEX_PATH")"
INPUT_DIR="${REPO_ROOT}/tests/golden/input"
while IFS= read -r wiki; do
    pagename="${wiki#${INPUT_DIR}/}"
    pagename="${pagename%.wiki}"
    pagename="${pagename%.md}"
    hex="$(php -r 'echo bin2hex($argv[1]);' "$pagename")"
    target="${INSTANCE_DIR}/storage/page/${hex}"
    if [[ -d "$target" ]]; then
        sudo rm -rf "$target"
        echo "reset $pagename"
    fi
done < <(find "$INPUT_DIR" -type f)

# カレンダーの日のページ (GoldenMaster/Calendar/ の下) は、画面からの保存と同じ経路
# (app/tool/page。種別の normalize を通る) で書く。gen-pages.php は normalize を通さず、
# meta にタイトル・短いタイトルが入らないので、カレンダーのマス (既定はタイトル) に
# タイトルが出なくなる。ほかのフィクスチャは今までどおり gen-pages.php で入れる。
CALENDAR_DAYS="GoldenMaster/Calendar"
sudo rm -rf "$WORK/input/$CALENDAR_DAYS"

sudo -u "$OWNER" php "$WORK/gen-pages.php" "$INDEX_PATH" --dir "$WORK/input" \
     --user "${WIKI_ADMIN:-admin}"

for day in "$INPUT_DIR/$CALENDAR_DAYS"/*; do
    case "$day" in
        *.wiki) type=wiki ;;
        *.md)   type=markdown ;;
        *)      echo "カレンダーの日のページにできないファイルです: $day" >&2; exit 1 ;;
    esac
    name="${day#${INPUT_DIR}/}"
    name="${name%.*}"
    if ! ( cd "$INSTANCE_DIR" && sudo -u "$OWNER" php app/tool/page "$INDEX_PATH" write "$name" \
               --user "${WIKI_ADMIN:-admin}" --type "$type" < "$day" > /dev/null ); then
        echo "書けませんでした: $name" >&2
        exit 1
    fi
    echo "write $name [$type] (app/tool/page) .. ok"
done
