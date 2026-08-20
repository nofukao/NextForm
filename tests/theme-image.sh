#!/bin/bash
# サイトのロゴ画像・お気に入りアイコンのテスト
#
#   ./tests/theme-image.sh
#
# 環境変数:
#   NF_SITE           複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   THEME_TEST_SITE   検証用に作るサイト (既定: /var/www/html/nf-theme-test)
#   THEME_TEST_URL    その URL           (既定: http://localhost/nf-theme-test)
#   KEEP=1            終了後に検証サイトを消さない
#
# ロゴとアイコンは 3 つの状態を持つ。
#
#   設定値が無い   テーマ同梱の画像 (theme/<テーマ>/image/logo.svg)
#   設定値がパス   上げた画像 (theme/logo.<拡張子>。全テーマ共通)
#   設定値が空     出さない
#
# 上流には「削除」しか無く、それは空文字を保存する = 出さない だった。
# 空文字も「設定された値」なので、一度差し替えると**同梱の画像に戻せなかった**。
# 「標準に戻す」(設定値そのものを消す) と「表示しない」を分けてある。
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
IMAGE="${REPO_ROOT}/tests/golden/input/GoldenMaster/Markdown/portforward01.png"

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

# 「外観の設定」を、画面に出ている値そのままで送り返す (multipart)。
#   $1 以降  curl の -F に渡す追加の値
apply_theme_form() {
    local args=() line key value
    while IFS= read -r line; do
        key="${line%%=*}"
        value="$(printf '%s' "${line#*=}" | base64 -d)"
        args+=(-F "${key}=${value}")
    done < <(curl -sk "${THEME_TEST_URL}/?option=admin_setup_theme" \
             | python3 "${REPO_ROOT}/tests/form-scrape.py")
    local extra
    for extra in "$@"; do
        args+=(-F "$extra")
    done
    curl -sk -o /dev/null -L -X POST -H "Origin: ${ORIGIN}" \
         -F "option=admin_setup_theme" -F "apply=true" \
         "${args[@]}" "${THEME_TEST_URL}/"
}

# 保存されている設定値 (未設定なら "(unset)")
saved_value() {
    value_of "$(helper saved "$1")" saved
}

# トップページの <img class="logo"> の src (無ければ空)
logo_src() {
    curl -sk "${THEME_TEST_URL}/" \
        | grep -o '<img class="logo"[^>]*src="[^"]*"' \
        | sed -E 's/.*src="([^"]*)".*/\1/' | head -1
}

# トップページの favicon の href (無ければ空)
icon_href() {
    curl -sk "${THEME_TEST_URL}/" \
        | grep -o '<link rel="icon"[^>]*href="[^"]*"' \
        | sed -E 's/.*href="([^"]*)".*/\1/' | head -1
}

uploaded_exists() {
    sudo test -f "${THEME_TEST_SITE}/theme/$1" && echo 1 || echo 0
}

if [[ ! -d "$NF_SITE" ]]; then
    echo "複製元がありません: $NF_SITE" >&2
    exit 1
fi
if [[ ! -f "$IMAGE" ]]; then
    echo "テスト用の画像がありません: $IMAGE" >&2
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
theme="$(value_of "$(helper theme)" theme)"
theme="${theme:-basic}"

# 複製元が画像を差し替えていても同じ結果になるよう、まず標準に戻す
apply_theme_form "const_THEME_IMAGE_LOGO_action=default" \
                 "const_THEME_IMAGE_ICON_action=default" > /dev/null

log_before=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')

echo "1. 初期状態はテーマ同梱の画像"
check_eq "設定値が無い"     "(unset)"                       "$(saved_value THEME_IMAGE_LOGO)"
check_eq "ロゴが出る"       "theme/${theme}/image/logo.svg" "$(logo_src)"
check_eq "アイコンが出る"   "theme/${theme}/image/favicon.ico" "$(icon_href)"
echo

echo "2. 画像を上げる"
apply_theme_form "const_THEME_IMAGE_LOGO=@${IMAGE}" > /dev/null
check_eq "設定値が変わる"   "theme/logo.png" "$(saved_value THEME_IMAGE_LOGO)"
check_eq "ファイルができる" "1"              "$(uploaded_exists logo.png)"
check_eq "上げた画像が出る" "theme/logo.png" "$(logo_src)"
check_eq "配信される"       "200"            "$(code "${THEME_TEST_URL}/theme/logo.png")"
echo

echo "3. 標準に戻す"
apply_theme_form "const_THEME_IMAGE_LOGO_action=default" > /dev/null
check_eq "設定値が消える"       "(unset)"                       "$(saved_value THEME_IMAGE_LOGO)"
check_eq "同梱の画像に戻る"     "theme/${theme}/image/logo.svg" "$(logo_src)"
check_eq "上げた画像は消える"   "0"                             "$(uploaded_exists logo.png)"
echo

echo "4. 表示しない"
apply_theme_form "const_THEME_IMAGE_LOGO_action=none" > /dev/null
check_eq "設定値が空になる" ""  "$(saved_value THEME_IMAGE_LOGO)"
check_eq "ロゴが出ない"     ""  "$(logo_src)"
echo

echo "5. 表示しないからも標準に戻せる"
apply_theme_form "const_THEME_IMAGE_LOGO_action=default" > /dev/null
check_eq "同梱の画像に戻る" "theme/${theme}/image/logo.svg" "$(logo_src)"
echo

echo "6. そのままを選ぶと何も起きない"
apply_theme_form "const_THEME_IMAGE_LOGO=@${IMAGE}" > /dev/null
apply_theme_form > /dev/null
check_eq "上げた画像のまま" "theme/logo.png" "$(logo_src)"
apply_theme_form "const_THEME_IMAGE_LOGO_action=" > /dev/null
check_eq "空の指定でも変わらない" "theme/logo.png" "$(logo_src)"
echo

echo "7. お気に入りアイコン (favicon) も同じ"
apply_theme_form "const_THEME_IMAGE_ICON=@${IMAGE}" > /dev/null
check_eq "上げた画像が出る" "theme/favicon.png" "$(icon_href)"
apply_theme_form "const_THEME_IMAGE_ICON_action=default" > /dev/null
check_eq "同梱の画像に戻る" "theme/${theme}/image/favicon.ico" "$(icon_href)"
check_eq "設定値が消える"   "(unset)"                          "$(saved_value THEME_IMAGE_ICON)"
echo

echo "8. 画面に選べるものが並ぶ"
# 先頭は「そのまま」(空の値)。form-scrape.py は空の選択肢を出さない。
form_html="$(curl -sk "${THEME_TEST_URL}/?option=admin_setup_theme")"
select_values() {
    printf '%s' "$form_html" | python3 "${REPO_ROOT}/tests/form-scrape.py" "$1" \
        | tr '\n' ' ' | sed 's/ $//'
}
check_eq "ロゴ"     "default none" "$(select_values const_THEME_IMAGE_LOGO_action)"
check_eq "アイコン" "default none" "$(select_values const_THEME_IMAGE_ICON_action)"
check_eq "favicon と書いてある" "1" \
         "$(printf '%s' "$form_html" | grep -c 'お気に入りアイコン (favicon)')"
echo

echo "9. PHP の警告を出さない"
log_after=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')
check_eq "エラーログが増えない" "$log_before" "$log_after"
echo

if [[ $fail -eq 0 ]]; then
    printf '%d/%d 件すべて通りました。\n' "$total" "$total"
else
    printf '%d/%d 件が失敗しました。\n' "$fail" "$total"
fi
exit $((fail == 0 ? 0 : 1))
