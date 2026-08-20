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
# 画面は「色調の設定」(?option=admin_setup_tone) 1 枚で、25 色を直接扱う。
# 組み込みや保存した色調は「読み込む」で入力欄に流し込んでから調整する。
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
        "${THEME_TEST_SITE}/site-helper.php" \
        "${THEME_TEST_SITE}/index.php" "$@" 2>/dev/null
}

value_of() {
    printf '%s\n' "$1" | sed -n "s/^$2=//p" | head -1
}

code() {
    curl -sk -o /dev/null -w '%{http_code}' "$@"
}

# 「色調の設定」をブラウザと同じように送る。
#   $1 以降  追加で送る値 (tone_save=1 など)
apply_tone_setup() {
    local args=() line key value
    while IFS= read -r line; do
        key="${line%%=*}"
        value="$(printf '%s' "${line#*=}" | base64 -d)"
        args+=(--data-urlencode "const_${key}=${value}")
    done < <(helper values tone)
    local extra
    for extra in "$@"; do
        args+=(--data-urlencode "$extra")
    done
    curl -sk -L -X POST -H "Origin: ${ORIGIN}" \
         --data-urlencode "option=admin_setup_tone" \
         --data-urlencode "apply=true" \
         "${args[@]}" "${THEME_TEST_URL}/"
}

# 画面に出ているフォームの値を、ブラウザと同じようにそのまま送り返す。
#   $1 以降  追加・上書きする値 (name=value)
apply_rendered_setup() {
    local args=() line key value
    while IFS= read -r line; do
        key="${line%%=*}"
        value="$(printf '%s' "${line#*=}" | base64 -d)"
        args+=(--data-urlencode "${key}=${value}")
    done < <(curl -sk "${THEME_TEST_URL}/?option=admin_setup_tone" | scrape_form)
    local extra
    for extra in "$@"; do
        args+=(--data-urlencode "$extra")
    done
    curl -sk -o /dev/null -L -X POST -H "Origin: ${ORIGIN}" \
         --data-urlencode "option=admin_setup_tone" \
         --data-urlencode "apply=true" \
         "${args[@]}" "${THEME_TEST_URL}/"
}

# フォームの入力欄を `名前=base64(値)` で出す
scrape_form() {
    python3 "${REPO_ROOT}/tests/form-scrape.py"
}

# $1 の HTML に出ている値をそのまま送り返す ($2 以降で上書きできる)
apply_html_setup() {
    local html="$1"; shift
    local args=() line key value
    while IFS= read -r line; do
        key="${line%%=*}"
        value="$(printf '%s' "${line#*=}" | base64 -d)"
        args+=(--data-urlencode "${key}=${value}")
    done < <(printf '%s' "$html" | scrape_form)
    local extra
    for extra in "$@"; do
        args+=(--data-urlencode "$extra")
    done
    curl -sk -o /dev/null -L -X POST -H "Origin: ${ORIGIN}" \
         --data-urlencode "option=admin_setup_tone" \
         --data-urlencode "apply=true" \
         "${args[@]}" "${THEME_TEST_URL}/"
}

# $1 の HTML に出ている入力欄の値
html_value() {
    printf '%s' "$1" | grep -o "<input[^>]*name=\"$2\"[^>]*>" \
        | sed -E 's/.*value="([^"]*)".*/\1/' | head -1
}

# 「保存した色調の削除」で選ばれている値 (空なら空文字)
tone_delete_selected() {
    curl -sk "${THEME_TEST_URL}/?option=admin_setup_tone" \
        | python3 -c '
import re, sys
html = sys.stdin.read()
select = re.search(r"<select name=\"tone_delete_id\">(.*?)</select>", html, re.S)
if not select:
    print("(欄が無い)")
    sys.exit(0)
option = re.search(r"<option value=\"([^\"]*)\"[^>]*selected", select.group(1))
print(option.group(1) if option else "(選択なし)")
'
}

# 「色調の読み込み」の選択肢の識別子 ($1 番目)
tone_load_option() {
    curl -sk "${THEME_TEST_URL}/?option=admin_setup_tone" \
        | python3 "${REPO_ROOT}/tests/form-scrape.py" tone_load_id \
        | sed -n "$1p"
}

# 画面に出ている入力欄の値
rendered_value() {
    curl -sk "${THEME_TEST_URL}/?option=admin_setup_tone" \
        | grep -o "<input[^>]*name=\"$1\"[^>]*>" \
        | sed -E 's/.*value="([^"]*)".*/\1/' | head -1
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
    curl -sk "${THEME_TEST_URL}/?option=admin_setup_tone" \
        | grep -o "<option value=\"$1\"[^>]*>[^<]*</option>" \
        | sed -E 's/.*>([^<]*) \([^)]*\)<.*/\1/' | head -1
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
sudo cp "${REPO_ROOT}/tests/site-helper.php" "${THEME_TEST_SITE}/"
sudo chown -R "$SITE_OWNER" "${THEME_TEST_SITE}/app" "${THEME_TEST_SITE}/resource" \
                            "${THEME_TEST_SITE}/site-helper.php"

# 複製元が保存した色調を引き継ぐと並び順や件数が読めなくなる。空から始める。
sudo rm -rf "${THEME_TEST_SITE}/storage/tone"

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

# 色調は組み込みのものから始める (複製元が何を使っていても同じ結果にする)
helper set-tone beige-green > /dev/null

log_before=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')

# 生成物は「いま使っているテーマ」の下にできる (複製元のテーマに従う)
current_theme="$(value_of "$(helper theme)" theme)"
theme_css="${THEME_TEST_SITE}/theme/${current_theme:-basic}/style/main.css"

css_has() {
    sudo grep -q "$1" "$theme_css" && echo 1 || echo 0
}

echo "1. 組み込みの色調"
check_eq "6 つある"       "6"                 "$(ls "${REPO_ROOT}/NextForm/app/tone/"*.json | wc -l)"
check_eq "読み込みに出る" "ベージュ/グリーン" "$(tone_option_name beige-green)"
check_eq "storage には無い" "0"               "$(tone_exists beige-green)"
echo

echo "2. 色調の設定は最初から 25 色を出す"
check_eq "色調を選ぶ欄が無い"     "0"  \
         "$(curl -sk "${THEME_TEST_URL}/?option=admin_setup_tone" \
            | grep -c 'name="const_THEME_TONE"')"
check_eq "25 色ある"              "25" \
         "$(curl -sk "${THEME_TEST_URL}/?option=admin_setup_tone" \
            | grep -o 'name="const_THEME_CUSTOM_COLOR_[A-Z_]*"' | sort -u | wc -l)"
check_eq "外観の設定に色が無い"   "0"  \
         "$(curl -sk "${THEME_TEST_URL}/?option=admin_setup_theme" \
            | grep -c 'name="const_THEME_CUSTOM_COLOR_')"
echo

echo "3. 色調を読み込む"
loaded=$(apply_tone_setup "tone_load=1" "tone_load_id=navy-yellow")
check_eq "入力欄がその色になる" "#000d40" \
         "$(html_value "$loaded" const_THEME_CUSTOM_COLOR_BACKGROUND)"
check_eq "まだ適用はされない"   "0"       "$(css_has '#000d40')"
check_eq "保存もされない"       "0"       "$(tone_exists navy-yellow)"
echo

echo "4. 読み込んだ色を適用する"
# 読み込んだ画面の値をそのまま送り返す = ブラウザで「適用」を押すのと同じ
apply_html_setup "$loaded"
check_eq "生成された CSS に出る" "1"       "$(css_has '#000d40')"
check_eq "入力欄にも残る"        "#000d40" "$(rendered_value const_THEME_CUSTOM_COLOR_BACKGROUND)"
echo

echo "5. 適用は上にもある"
# 25 色は縦に長い。上の方を直したときに一番下まで送らずに済むようにする。
check_eq "適用ボタンが 2 つある" "2" \
         "$(curl -sk "${THEME_TEST_URL}/?option=admin_setup_tone" \
            | grep -o 'value="適用"' | wc -l)"
check_eq "どちらにも注意書きが付く" "2" \
         "$(curl -sk "${THEME_TEST_URL}/?option=admin_setup_tone" \
            | grep -o '(適用後ブラウザを再読み込みして下さい)' | wc -l)"
echo

echo "6. いまの色を名前を付けて保存する"
apply_tone_setup "tone_save=1" "tone_id=testtone" "tone_name=テスト色調" > /dev/null
check_eq "ファイルができる"   "1"          "$(tone_exists testtone)"
check_eq "表示名が入る"       "テスト色調" "$(tone_value testtone names ja)"
check_eq "25 色が入る"        "25"         "$(tone_value testtone colors)"
check_eq "入力欄の色が入る"   "#000d40"    "$(tone_value testtone colors THEME_COLOR_BACKGROUND)"
check_eq "読み込みに出る"     "テスト色調" "$(tone_option_name testtone)"
# storage は Web から見えてはいけない。色調も storage の下なので同じ扱いになる
check_eq "Web からは読めない" "403"        \
         "$(code "${THEME_TEST_URL}/storage/tone/testtone.json")"
echo

echo "7. 手で決めた色を保存する"
apply_tone_setup "const_THEME_CUSTOM_COLOR_BACKGROUND=#123456" \
                 "tone_save=1" "tone_id=custom-tone" "tone_name=手で作った色" > /dev/null
check_eq "入力した色が入る"      "#123456" "$(tone_value custom-tone colors THEME_COLOR_BACKGROUND)"
check_eq "適用もされる"          "1"       "$(css_has '#123456')"
echo

echo "8. 後に保存したものほど上に出る"
check_eq "保存した色調が先頭"     "custom-tone" "$(tone_load_option 1)"
check_eq "その次が 1 つ前のもの"  "testtone"    "$(tone_load_option 2)"
check_eq "組み込みはその後ろ"     "beige-green" "$(tone_load_option 3)"
echo

echo "9. 組み込みと同じ識別子で保存すると隠す"
apply_tone_setup "tone_save=1" "tone_id=beige-green" "tone_name=上書きベージュ" > /dev/null
check_eq "storage にできる"   "1"                "$(tone_exists beige-green)"
check_eq "読み込みが変わる"   "上書きベージュ"   "$(tone_option_name beige-green)"
check_eq "組み込みは残る"     "beige/green"      \
         "$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["names"]["en"])' \
            "${REPO_ROOT}/NextForm/app/tone/beige-green.json")"

apply_tone_setup "tone_delete=1" "tone_delete_id=beige-green" > /dev/null
check_eq "消すと組み込みに戻る" "ベージュ/グリーン" "$(tone_option_name beige-green)"
check_eq "ファイルが消える"     "0"                 "$(tone_exists beige-green)"
echo

echo "10. 削除はうっかり押しても起きない"
# 選択欄の既定が保存済みの色調だと、押しただけで 1 つ消える。
check_eq "既定は空"           ""   "$(tone_delete_selected)"
check_eq "選択肢には出る"     "1"  \
         "$(curl -sk "${THEME_TEST_URL}/?option=admin_setup_tone" \
            | grep -c 'option value="custom-tone"' | head -1)"
body=$(apply_tone_setup "tone_delete=1" "tone_delete_id=")
check_eq "選ばずに押すと断る" "1"  "$(printf '%s' "$body" | grep -c '削除する色調が選ばれていません')"
check_eq "何も消えない"       "2"  "$(sudo ls "${THEME_TEST_SITE}/storage/tone/" | wc -l)"
echo

echo "11. 使えない識別子は保存しない"
for bad in "../evil" "日本語" "" "a/b"; do
    apply_tone_setup "tone_save=1" "tone_id=${bad}" "tone_name=x" > /dev/null
done
check_eq "storage に増えない" "2" \
         "$(sudo ls "${THEME_TEST_SITE}/storage/tone/" | wc -l)"
check_eq "storage の外に書かない" "0" \
         "$(sudo find "${THEME_TEST_SITE}" -name 'evil*' | wc -l)"
echo

echo "12. 色調のファイルを失っても色は決まる"
# 上流から引き継いだサイトは THEME_TONE に色調の名前を持っている。
# その色調が無くなっても、色が 1 つも決まらない状態にはしない。
helper set-tone custom-tone > /dev/null
sudo rm -f "$(tone_file custom-tone)"
check_eq "無くなったと知らせる" "1" \
         "$(curl -sk "${THEME_TEST_URL}/?option=admin_setup_tone" \
            | grep -c "custom-tone" | head -1)"
code "${THEME_TEST_URL}/?option=admin_setup_tone&apply=theme" > /dev/null
check_eq "作り直すと既定に落ちる" "1" "$(css_has '#fbf6ea')"
echo

echo "13. PHP の警告を出さない"
log_after=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')
check_eq "エラーログが増えない" "$log_before" "$log_after"
echo

if [[ $fail -eq 0 ]]; then
    printf '%d/%d 件すべて通りました。\n' "$total" "$total"
else
    printf '%d/%d 件が失敗しました。\n' "$fail" "$total"
fi
exit $((fail == 0 ? 0 : 1))
