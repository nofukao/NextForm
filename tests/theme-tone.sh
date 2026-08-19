#!/bin/bash
# 色調 (トーン) の保存・読み込みのテスト
#
#   ./tests/theme-tone.sh
#
# 環境変数:
#   NF_SITE           複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   THEME_TEST_SITE   検証用に作るサイト (既定: /var/www/html/nf-theme-test)
#   THEME_TEST_URL    その URL           (既定: http://localhost/nf-theme-test)
#   KEEP=1            終了後に検証サイトを消さない
#
# 色調は JSON ファイル 1 つ = 1 色調で、2 つのディレクトリから読む。
#
#   app/tone/<識別子>.json       配布物。組み込み。書き換えない
#   storage/tone/<識別子>.json   そのサイトが保存したもの
#
# 同じ識別子なら storage が app を隠す。組み込みを上書きでき、storage 側を
# 消せば戻る。この「隠す」関係と、ファイルの形式が守られることを固定する。
#
# 設定と権限を書き換えるので、必ず複製したサイトに対して実行する。
# 複製元には触らない。root で実行する必要がある。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
THEME_TEST_SITE="${THEME_TEST_SITE:-/var/www/html/nf-theme-test}"
THEME_TEST_URL="${THEME_TEST_URL:-http://localhost/nf-theme-test}"
PHP_ERROR_LOG="${PHP_ERROR_LOG:-/var/log/php-fpm/www-error.log}"

fail=0
total=0

cleanup() {
    if [[ "${KEEP:-0}" != "1" ]]; then
        sudo rm -rf "$THEME_TEST_SITE" 2>/dev/null
    else
        echo
        echo "KEEP=1 のため検証サイトを残しました: $THEME_TEST_SITE"
    fi
}
trap cleanup EXIT

# $1 説明  $2 期待値  $3 実測値
check_eq() {
    total=$((total + 1))
    if [[ "$2" == "$3" ]]; then
        printf '  ok    %s\n' "$1"
    else
        printf '  FAIL  %s\n        期待: %s\n        実測: %s\n' "$1" "$2" "$3"
        fail=$((fail + 1))
    fi
}

helper() {
    sudo -u "$SITE_OWNER" php \
        "${THEME_TEST_SITE}/theme-helper.php" \
        "${THEME_TEST_SITE}/index.php" "$@" 2>/dev/null
}

value_of() {
    printf '%s\n' "$1" | sed -n "s/^$2=//p" | head -1
}

code() {
    curl -sk -o /dev/null -w '%{http_code}' "$@"
}

# 「外観の設定」をブラウザと同じように送る。
#   $1 以降  追加で送る値 (name=value)。const_THEME_TONE などを上書きできる
apply_theme_setup() {
    local args=() line key value
    while IFS= read -r line; do
        key="${line%%=*}"
        value="$(printf '%s' "${line#*=}" | base64 -d)"
        args+=(--data-urlencode "const_${key}=${value}")
    done < <(helper values theme)
    local extra
    for extra in "$@"; do
        args+=(--data-urlencode "$extra")
    done
    curl -sk -o /dev/null -L -X POST -H "Origin: ${ORIGIN}" \
         --data-urlencode "option=admin_setup_theme" \
         --data-urlencode "apply=true" \
         "${args[@]}" "${THEME_TEST_URL}/"
}

# 保存された色調のファイル
tone_file() {
    echo "${THEME_TEST_SITE}/storage/tone/$1.json"
}

tone_exists() {
    sudo test -f "$(tone_file "$1")" && echo 1 || echo 0
}

# 保存された色調の値を 1 つ取る。$1 識別子  $2 jq 相当のキー列
tone_value() {
    sudo cat "$(tone_file "$1")" 2>/dev/null | python3 -c '
import json, sys
try:
    tone = json.load(sys.stdin)
except Exception:
    sys.exit(0)
for key in sys.argv[1:]:
    tone = tone.get(key, "")
    if not isinstance(tone, dict):
        break
print(tone if not isinstance(tone, dict) else len(tone))
' "${@:2}"
}

# 設定画面の「色調」の選択肢に出ている表示名
tone_option_name() {
    curl -sk "${THEME_TEST_URL}/?option=admin_setup_theme" \
        | grep -o "<option value=\"$1\"[^>]*>[^<]*</option>" \
        | sed -E 's/.*>([^<]*)<.*/\1/' | head -1
}

if [[ ! -d "$NF_SITE" ]]; then
    echo "複製元がありません: $NF_SITE" >&2
    exit 1
fi

echo "複製元 = $NF_SITE"
echo "検証先 = $THEME_TEST_SITE"
echo "URL    = $THEME_TEST_URL"
echo

sudo rm -rf "$THEME_TEST_SITE"
sudo cp -a "$NF_SITE" "$THEME_TEST_SITE"
SITE_OWNER=$(sudo stat -c '%U' "${THEME_TEST_SITE}/index.php")

sudo rsync -a --delete "${REPO_ROOT}/NextForm/app/"      "${THEME_TEST_SITE}/app/"
sudo rsync -a --delete "${REPO_ROOT}/NextForm/resource/" "${THEME_TEST_SITE}/resource/"
sudo cp "${REPO_ROOT}/tests/theme-helper.php" "${THEME_TEST_SITE}/"
sudo chown -R "$SITE_OWNER" "${THEME_TEST_SITE}/app" "${THEME_TEST_SITE}/resource" \
                            "${THEME_TEST_SITE}/theme-helper.php"

out=$(helper guest-admin)
if [[ "$(value_of "$out" saved)" != "1" ]]; then
    echo "ログインしていない利用者に admin 権限を与えられませんでした。" >&2
    exit 1
fi

if [[ "$(code "${THEME_TEST_URL}/")" != "200" ]]; then
    echo "検証サイトが $THEME_TEST_URL で見えません。" >&2
    exit 1
fi

ORIGIN=$(printf '%s' "$THEME_TEST_URL" | sed -E 's#^(https?://[^/]+).*#\1#')

log_before=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')

echo "1. 組み込みの色調"
check_eq "6 つある" "6" "$(ls "${REPO_ROOT}/NextForm/app/tone/"*.json | wc -l)"
check_eq "選択肢に出る" "ベージュ/グリーン" "$(tone_option_name beige-green)"
check_eq "storage には無い" "0" "$(tone_exists beige-green)"
echo

echo "2. いまの色を名前を付けて保存する"
apply_theme_setup "const_THEME_TONE=navy-yellow" \
                  "tone_save=1" "tone_id=testtone" "tone_name=テスト色調"
check_eq "ファイルができる"       "1"          "$(tone_exists testtone)"
check_eq "表示名が入る"           "テスト色調" "$(tone_value testtone names ja)"
check_eq "25 色が入る"            "25"         "$(tone_value testtone colors)"
check_eq "選んだ色調の色になる"   "#000d40"    "$(tone_value testtone colors THEME_COLOR_BACKGROUND)"
check_eq "選択肢に出る"           "テスト色調" "$(tone_option_name testtone)"
# storage は Web から見えてはいけない。色調も storage の下なので同じ扱いになる
check_eq "Web からは読めない"     "403"        \
         "$(code "${THEME_TEST_URL}/storage/tone/testtone.json")"
echo

echo "3. 個別に設定した色を保存する"
apply_theme_setup "const_THEME_TONE=custom" \
                  "const_THEME_CUSTOM_COLOR_BACKGROUND=#123456" \
                  "tone_save=1" "tone_id=custom-tone" "tone_name=手で作った色"
check_eq "入力した色が入る" "#123456" "$(tone_value custom-tone colors THEME_COLOR_BACKGROUND)"
echo

echo "4. 組み込みと同じ識別子で保存すると隠す"
apply_theme_setup "const_THEME_TONE=custom" \
                  "const_THEME_CUSTOM_COLOR_BACKGROUND=#abcdef" \
                  "tone_save=1" "tone_id=beige-green" "tone_name=上書きベージュ"
check_eq "storage にできる"   "1"                "$(tone_exists beige-green)"
check_eq "選択肢が入れ替わる" "上書きベージュ"   "$(tone_option_name beige-green)"
check_eq "組み込みは残る"     "beige/green"      \
         "$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["names"]["en"])' \
            "${REPO_ROOT}/NextForm/app/tone/beige-green.json")"
echo

echo "5. 保存した色調を消すと組み込みに戻る"
apply_theme_setup "tone_delete=1" "tone_delete_id=beige-green"
check_eq "ファイルが消える" "0"                 "$(tone_exists beige-green)"
check_eq "表示名が戻る"     "ベージュ/グリーン" "$(tone_option_name beige-green)"
echo

echo "6. 使えない識別子は保存しない"
for bad in "../evil" "日本語" "" "a/b"; do
    apply_theme_setup "tone_save=1" "tone_id=${bad}" "tone_name=x" > /dev/null
done
check_eq "storage に増えない" "2" \
         "$(sudo ls "${THEME_TEST_SITE}/storage/tone/" | wc -l)"
check_eq "storage の外に書かない" "0" \
         "$(sudo find "${THEME_TEST_SITE}" -name 'evil*' | wc -l)"
echo

echo "7. PHP の警告を出さない"
log_after=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')
check_eq "エラーログが増えない" "$log_before" "$log_after"
echo

if [[ $fail -eq 0 ]]; then
    printf '%d/%d 件すべて通りました。\n' "$total" "$total"
else
    printf '%d/%d 件が失敗しました。\n' "$fail" "$total"
fi
exit $((fail == 0 ? 0 : 1))
