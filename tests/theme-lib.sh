# テーマの生成物を作るための共通処理。
# 単体では実行しない。tests/css-rules.sh と tests/theme-diff.sh が source する。
#
# app/theme/<名前>/style/*.css は PHP テンプレートで、theme_convert() が
# theme/<名前>/style/main.css を生成する。生成物は「管理画面で選ばれている
# テーマ」の分しか作られないため、配信中のサイトを見るだけでは他のテーマを
# 検査できない。ここでは作業ツリーの複製に対して生成を回し、テーマと設定の
# 組み合わせを網羅する。
#
# 複製を作るのは、作業ツリーの NextForm/theme/ を壊さないため
# (php -S で開発インスタンスを動かしていると、そこに実データがある)。

THEME_LIB_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
THEME_LIB_SITES=()

# 生成に使う設定の組み合わせ。
# 色調は全部、レイアウトと文字の大きさは端まで、背景画像は分岐が多いので必ず含める。
# 詰まり具合と整形済みテキストの折り返しは、CSS の数値そのものを動かすので入れる。
declare -A THEME_PATTERNS=(
  [default]=""
  [tone-yellow-pink]="THEME_TONE=yellow/pink"
  [tone-white-blue]="THEME_TONE=white/blue"
  [tone-black-blue]="THEME_TONE=black/blue"
  [tone-navy-yellow]="THEME_TONE=navy/yellow"
  [tone-gray-orange]="THEME_TONE=gray/orange"
  [tone-custom]="THEME_TONE=custom"
  [main-color-override]="THEME_COLOR_MAIN_CUSTOM=#123456"
  [layout-solid]="THEME_LAYOUT=solid"
  [side-right]="THEME_SIDE_PAGE_POSITION=right"
  [side-none]="SIDE_PAGENAME="
  [density-loose]="THEME_DENSITY=loose"
  [density-compact]="THEME_DENSITY=compact"
  [pre-scroll]="THEME_PRE_WRAP=scroll"
  [font-12]="THEME_FONT_SIZE=12"
  [font-20]="THEME_FONT_SIZE=20"
  [header-parts]="THEME_HEADER_HEIGHT=120 THEME_HEADER_SITE_NAME_ENABLE=false THEME_SEARCH_TOOL_ENABLE=false THEME_SITE_MENU_SIZE=small"
  [no-logo]="THEME_IMAGE_LOGO= THEME_IMAGE_ICON="
  [background-images]="THEME_LAYOUT=solid THEME_IMAGE_BACKGROUND_HEADER=@IMG@ THEME_IMAGE_BACKGROUND_BODY=@IMG@ THEME_IMAGE_BACKGROUND_OUTSIDE=@IMG@ THEME_IMAGE_BACKGROUND_BODY_REPEAT=repeat THEME_IMAGE_BACKGROUND_OUTSIDE_REPEAT=cover THEME_IMAGE_BACKGROUND_OUTSIDE_ATTACHMENT=fixed"
)

# 複製を作り、そのパスを返す。以降の生成はすべて複製の中で動く。
# theme_lib_make_site <複製元のルート (NextForm/ に相当)>
theme_lib_make_site() {
    local src="$1" site
    site="$(mktemp -d)"
    rsync -a --exclude '/storage/' --exclude '/theme/' "${src%/}/" "${site}/"
    THEME_LIB_SITES+=("$site")
    echo "$site"
}

theme_lib_cleanup() {
    local site
    for site in "${THEME_LIB_SITES[@]:-}"; do
        [[ -n "$site" ]] && rm -rf "$site"
    done
    THEME_LIB_SITES=()
}

# 検査できるテーマの一覧 (app/theme/*/html.php があるもの)。
# common/ は html.php を持たないのでここには出ない。
# theme_lib_themes [ルート]  既定は作業ツリー
theme_lib_themes() {
    local root="${1:-${THEME_LIB_ROOT}/NextForm}" file
    for file in "${root}"/app/theme/*/html.php; do
        [[ -f "$file" ]] || continue
        basename "$(dirname "$file")"
    done
}

theme_lib_pattern_names() {
    printf '%s\n' "${!THEME_PATTERNS[@]}" | sort
}

# theme_lib_generate <サイト> <テーマ> <パターン名> <出力先ディレクトリ>
# 生成した main.css / noscript.css / theme.js (あれば setup.js) を出力先へ置く。
# 生成に失敗したか、PHP が何か出力していたら 1 を返す。
theme_lib_generate() {
    local site="$1" theme="$2" pattern="$3" dst="$4"
    local consts="${THEME_PATTERNS[$pattern]//@IMG@/app/theme/$theme/image/logo.png}"
    local log
    log="$(mktemp)"

    rm -rf "${site:?}/theme"
    # shellcheck disable=SC2086
    if ! php "${THEME_LIB_ROOT}/tests/theme-css.php" \
             "$site" "$theme" theme $consts > "$log" 2>&1; then
        echo "生成に失敗: theme=$theme pattern=$pattern" >&2
        cat "$log" >&2
        rm -f "$log"
        return 1
    fi
    if [[ -s "$log" ]]; then
        echo "生成中に PHP が出力した: theme=$theme pattern=$pattern" >&2
        cat "$log" >&2
        rm -f "$log"
        return 1
    fi
    rm -f "$log"

    mkdir -p "$dst"
    cp "${site}/theme/${theme}/style/main.css"     "$dst/main.css"
    cp "${site}/theme/${theme}/style/noscript.css" "$dst/noscript.css"
    cp "${site}/theme/${theme}/script/theme.js"    "$dst/theme.js"
    # setup.js は必ずあるとは限らない (テーマが持っていれば生成される)
    if [[ -f "${site}/theme/${theme}/script/setup.js" ]]; then
        cp "${site}/theme/${theme}/script/setup.js" "$dst/setup.js"
    fi

    # 生成物に PHP のエラー出力が混ざっていないか (display_errors が On の環境)
    if grep -qE '^(PHP )?(Warning|Notice|Deprecated|Fatal error|Parse error): ' "$dst/main.css"; then
        echo "生成 CSS に PHP のエラー出力: theme=$theme pattern=$pattern" >&2
        return 1
    fi
    return 0
}
