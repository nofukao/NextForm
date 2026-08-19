#!/bin/bash
# テーマ切り替えのテスト
#
#   ./tests/theme-switch.sh
#
# 環境変数:
#   NF_SITE           複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   THEME_TEST_SITE   検証用に作るサイト (既定: /var/www/html/nf-theme-test)
#   THEME_TEST_URL    その URL           (既定: http://localhost/nf-theme-test)
#   KEEP=1            終了後に検証サイトを消さない
#
# テーマの CSS は PHP テンプレートで、theme_convert() が設定を埋め込んで
# theme/<名前>/ に静的ファイルを書き出す。ブラウザが読むのはこの生成物なので、
# 生成されていないテーマを選ぶと CSS が 404 になり、画面は素の HTML になる。
#
# v0.5 でテーマが 4 つになるまで、テーマは 1 つしか無く、生成はインストーラと
# 「外観の設定」の保存でしか行われなかった。サイト設定でテーマを選ぶ経路には
# 生成が無かったため、**テーマを変えると必ず画面が崩れた**。
#
# ここで固定するのは次の 3 つ:
#
#   1. サイト設定でテーマを変えると、そのテーマの静的ファイルが生成されること
#   2. 生成物を失っても、サイト設定を開けば作り直されること
#   3. 切り替えの前後で PHP の警告が出ないこと
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

# サイト設定のフォームをブラウザと同じように送る。$1 が選ぶテーマ。
apply_theme() {
    local theme="$1"
    local args=() line key value
    while IFS= read -r line; do
        key="${line%%=*}"
        value="$(printf '%s' "${line#*=}" | base64 -d)"
        [[ "$key" == "THEME" ]] && continue
        args+=(--data-urlencode "const_${key}=${value}")
    done < <(helper values site)
    curl -sk -o /dev/null -L -X POST -H "Origin: ${ORIGIN}" \
         --data-urlencode "option=admin_setup_site" \
         --data-urlencode "apply=true" \
         --data-urlencode "const_THEME=${theme}" \
         "${args[@]}" "${THEME_TEST_URL}/"
}

# 生成物が置かれたか
generated() {
    sudo test -f "${THEME_TEST_SITE}/theme/$1/style/main.css" && echo 1 || echo 0
}

# いま表示されている画面が読んでいる CSS
current_css() {
    curl -sk "${THEME_TEST_URL}/" \
        | sed -n 's/.*<link rel="stylesheet"[^>]*href="\([^"?]*main\.css\)[^"]*".*/\1/p' | head -1
}

if [[ ! -d "$NF_SITE" ]]; then
    echo "複製元がありません: $NF_SITE" >&2
    echo "tests/env.local の NF_SITE を設定してください。" >&2
    exit 1
fi

echo "複製元 = $NF_SITE"
echo "検証先 = $THEME_TEST_SITE"
echo "URL    = $THEME_TEST_URL"
echo

sudo rm -rf "$THEME_TEST_SITE"
sudo cp -a "$NF_SITE" "$THEME_TEST_SITE"
SITE_OWNER=$(sudo stat -c '%U' "${THEME_TEST_SITE}/index.php")

# 複製元に配置済みのコードではなく、リポジトリの作業ツリーを検証する
# (csrf.sh と同じ理由)。
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
    echo "tests/env.local の THEME_TEST_URL を設定してください。" >&2
    exit 1
fi

ORIGIN=$(printf '%s' "$THEME_TEST_URL" | sed -E 's#^(https?://[^/]+).*#\1#')

# 選べるテーマ (app/theme/<名前>/html.php があるもの)。テーマが増えたら
# 自動で対象になる。common は html.php を持たないので入らない。
THEMES=()
for dir in "${REPO_ROOT}"/NextForm/app/theme/*/html.php; do
    THEMES+=("$(basename "$(dirname "$dir")")")
done

log_before=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')

echo "1. サイト設定でテーマを変えると静的ファイルが生成される"
for theme in "${THEMES[@]}"; do
    apply_theme "$theme"
    check_eq "$theme に切り替えられる"            "$theme" "$(value_of "$(helper theme)" theme)"
    check_eq "$theme の静的ファイルが生成される"  "1"      "$(generated "$theme")"
    check_eq "$theme の CSS が配信される"         "200"    \
             "$(code "${THEME_TEST_URL}/theme/${theme}/style/main.css")"
    check_eq "$theme の画面がその CSS を読む"     "theme/${theme}/style/main.css" "$(current_css)"
done
echo

echo "2. 生成物を失っても作り直される"
# 配布物の入れ替えや theme/ の消し込みで生成物だけが無くなることがある。
# 設定は正しいのに画面だけ崩れるため、気付いたときに直せる経路が要る。
# いま使っているテーマ (上のループで最後に選んだもの) の生成物を消す
CURRENT="${THEMES[$((${#THEMES[@]} - 1))]}"
sudo rm -rf "${THEME_TEST_SITE}/theme/${CURRENT}"
check_eq "生成物が無い"           "0"   "$(generated "$CURRENT")"
code "${THEME_TEST_URL}/?option=admin_setup_site" > /dev/null
check_eq "サイト設定を開くと戻る" "1"   "$(generated "$CURRENT")"
check_eq "CSS が配信される"       "200" \
         "$(code "${THEME_TEST_URL}/theme/${CURRENT}/style/main.css")"
echo

echo "3. 切り替えで PHP の警告を出さない"
log_after=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')
check_eq "エラーログが増えない" "$log_before" "$log_after"
echo

if [[ $fail -eq 0 ]]; then
    printf '%d/%d 件すべて通りました。\n' "$total" "$total"
else
    printf '%d/%d 件が失敗しました。\n' "$fail" "$total"
fi
exit $((fail == 0 ? 0 : 1))
