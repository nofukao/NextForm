#!/bin/bash
# アップグレード検証テスト
#
#   ./tests/upgrade.sh
#
# 環境変数:
#   SRC_SITE    複製元となる既存 toratorawiki (既定: /var/www/html/tora2)
#   TEST_SITE   検証用に作るサイト     (既定: /var/www/html/nf-upgrade-test)
#   TEST_URL    その URL               (既定: http://localhost/nf-upgrade-test)
#   KEEP=1      終了後に検証サイトを消さない (中身を見たいとき)
#
# 実在の 2015 年代インスタンス (tora2) を複製し、app/tool/upgrade を実際に
# 走らせて、
#
#   - 利用者のデータ (index.php / install-info.dat / storage/) が変わらないこと
#   - 利用者が app/ に置いたもの (プラグイン・独自テーマ) が残ること
#   - 静的 CSS が再生成されること   ← 忘れると CSS 修正が反映されない
#   - アップグレード後も HTTP 200 で PHP 警告ゼロで動くこと
#   - --dry-run が本当に何も書き換えないこと
#
# を確認する。複製元 (tora2) には一切触らない。
#
# root で実行する必要がある (apache 所有のサイトを複製・書き換えるため)。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# 向け先は tests/env.local に書く (Git には入らない)。
# 無い場合は下の既定値を使う。tests/env.local.example を参照。
[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
SRC_SITE="${SRC_SITE:-/var/www/html/tora2}"
TEST_SITE="${TEST_SITE:-/var/www/html/nf-upgrade-test}"
TEST_URL="${TEST_URL:-http://localhost/nf-upgrade-test}"
PHP_ERROR_LOG="${PHP_ERROR_LOG:-/var/log/php-fpm/www-error.log}"

# 手順書 (方法 B) の検証に使う 2 つめのサイト
SITE_B="${TEST_SITE}-manual"
URL_B="${TEST_URL}-manual"

DIST="$(mktemp -d)"
fail=0
total=0

cleanup() {
    rm -rf "$DIST"
    if [[ "${KEEP:-0}" != "1" ]]; then
        sudo rm -rf "$TEST_SITE" "${TEST_SITE}".backup-* "${TEST_SITE}".fullbackup \
                    "$SITE_B" "${SITE_B}".backup-* 2>/dev/null
    else
        echo
        echo "KEEP=1 のため検証サイトを残しました: $TEST_SITE  $SITE_B"
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

# $1 説明  $2 コマンド (成功すれば ok)
check_cmd() {
    total=$((total + 1))
    if eval "$2" > /dev/null 2>&1; then
        printf '  ok    %s\n' "$1"
    else
        printf '  FAIL  %s\n' "$1"
        fail=$((fail + 1))
    fi
}

# サイト全体の指紋。中身 + 所有者 + パーミッション + リンク先。
# シンボリックリンクも見る (-type f だけだと落ちる)。
fingerprint() {
    sudo find "$1" \( -type f -o -type l \) \
         -printf '%P %y %s %m %u:%g %l\n' 2>/dev/null | sort
}

if [[ ! -d "$SRC_SITE" ]]; then
    echo "複製元がありません: $SRC_SITE" >&2
    exit 1
fi

echo "複製元   = $SRC_SITE"
echo "検証先   = $TEST_SITE"
echo

# --- 配布物を作る -----------------------------------------------------------
# tar.gz を展開した状態を作業ツリーから再現する。生成物 (storage/ theme/
# install-info.dat) は配布物に入らないので除外する。
echo "[1] 配布物を用意"
rsync -a \
    --exclude '/storage/' \
    --exclude '/theme/' \
    --exclude '/install-info.dat' \
    "${REPO_ROOT}/NextForm/" "${DIST}/NextForm/"
if [[ ! -x "${DIST}/NextForm/app/tool/upgrade" && ! -f "${DIST}/NextForm/app/tool/upgrade" ]]; then
    echo "  app/tool/upgrade がありません" >&2
    exit 1
fi
echo "  ok"
echo

# --- 検証サイトを作る -------------------------------------------------------
echo "[2] 既存サイトを複製し、利用者の持ち物を植え込む"
sudo rm -rf "$TEST_SITE" "${TEST_SITE}".backup-*
sudo cp -a "$SRC_SITE" "$TEST_SITE"

# 利用者が app/ の中に置いたもの。アップグレードで消えてはいけない。
sudo tee "${TEST_SITE}/app/plugin/zz_upgrade_test.inc" > /dev/null <<'EOF'
<?php
// アップグレード検証用。消えてはいけない。
?>
EOF
sudo mkdir -p "${TEST_SITE}/app/theme/upgradetest"
sudo tee "${TEST_SITE}/app/theme/upgradetest/html.php" > /dev/null <<'EOF'
<?php /* アップグレード検証用の独自テーマ。消えてはいけない。 */ ?>
EOF
sudo tee "${TEST_SITE}/app/upgrade_test_note.txt" > /dev/null <<'EOF'
利用者が app/ 直下に置いた覚えのないファイル。消えてはいけない。
EOF

# 焼き付け済みのマニュアル。0.7.0 で組み込みになったので、もう表示には
# 使われない。ツールは件数を知らせるだけで**消さない**。
# 0.5.0 でページ名を変えているので、古いサイトには両方の名前が残っている。
# ディレクトリ名は bin2hex(ページ名)。
# meta の行は bin2hex(名前)=bin2hex(値) で保存される (storage_page_encode_meta)。
# 生の 'type=wiki' を書くと ascii_decode() の pack('H*') が警告を出す。
# 74797065=type / 77696b69=wiki
for MANUAL_PAGEID in 4e657874466f726d4d616e75616c 546f7261546f726157696b694d616e75616c; do
    sudo mkdir -p "${TEST_SITE}/storage/page/${MANUAL_PAGEID}"
    sudo tee "${TEST_SITE}/storage/page/${MANUAL_PAGEID}/head" > /dev/null <<'EOF'
74797065=77696b69

焼き付け済みのマニュアル。アップグレードで消えてはいけない。
EOF
done

# シンボリックリンク。移行してきたサイトには実際に紛れている。
# copy() はリンク先を読むため、リンク切れがあるとバックアップがそこで止まる
# (2026-08-09 に業務 wiki の移行で発生)。リンクは辿らず複製すること。
sudo ln -s '../this-does-not-exist/' "${TEST_SITE}/broken-link"
sudo ln -s 'app'                     "${TEST_SITE}/link-to-dir"
sudo ln -s 'index.php'               "${TEST_SITE}/link-to-file"
# 既定のバックアップはサイト直下を含まないので、app/ の中にも 1 本置く
sudo ln -s '../this-does-not-exist/' "${TEST_SITE}/app/broken-link-in-app"
sudo chown -Rh --reference="${TEST_SITE}/index.php" "${TEST_SITE}"
echo "  ok"
echo

BEFORE_INDEX=$(sudo md5sum "${TEST_SITE}/index.php" | cut -d' ' -f1)
BEFORE_INFO=$(sudo md5sum "${TEST_SITE}/install-info.dat" | cut -d' ' -f1)
BEFORE_STORAGE=$(sudo find "${TEST_SITE}/storage" -type f -exec md5sum {} + | sed "s|${TEST_SITE}/||" | sort | md5sum)
BEFORE_ALL=$(fingerprint "$TEST_SITE")
SITE_OWNER=$(sudo stat -c '%U:%G' "${TEST_SITE}/index.php")

# --- dry-run は何も変えない -------------------------------------------------
echo "[3] --dry-run"
sudo php "${DIST}/NextForm/app/tool/upgrade" "${TEST_SITE}/index.php" --dry-run > "${DIST}/dryrun.log" 2>&1
rc=$?
sed 's/^/  | /' "${DIST}/dryrun.log"
check_eq "--dry-run が正常終了する" "0" "$rc"
check_eq "--dry-run でサイトが1バイトも変わらない" "$BEFORE_ALL" "$(fingerprint "$TEST_SITE")"
check_cmd "--dry-run でバックアップを作らない" "! ls -d ${TEST_SITE}.backup-* 2>/dev/null | grep -q ."
echo

# --- 本番実行 ---------------------------------------------------------------
echo "[4] アップグレード実行"
sudo php "${DIST}/NextForm/app/tool/upgrade" "${TEST_SITE}/index.php" --yes > "${DIST}/upgrade.log" 2>&1
rc=$?
sed 's/^/  | /' "${DIST}/upgrade.log"
check_eq "正常終了する" "0" "$rc"
echo

# --- 利用者のデータが無傷であること -----------------------------------------
echo "[5] 利用者のデータ"
check_eq "index.php が変わらない (認証設定が消えない)" \
         "$BEFORE_INDEX" "$(sudo md5sum "${TEST_SITE}/index.php" | cut -d' ' -f1)"
check_eq "install-info.dat が変わらない" \
         "$BEFORE_INFO" "$(sudo md5sum "${TEST_SITE}/install-info.dat" | cut -d' ' -f1)"
check_cmd "焼き付け済みのマニュアルを消さない" \
          "sudo test -f '${TEST_SITE}/storage/page/4e657874466f726d4d616e75616c/head' && \
           sudo test -f '${TEST_SITE}/storage/page/546f7261546f726157696b694d616e75616c/head'"
check_eq "storage/ が変わらない" \
         "$BEFORE_STORAGE" \
         "$(sudo find "${TEST_SITE}/storage" -type f -exec md5sum {} + | sed "s|${TEST_SITE}/||" | sort | md5sum)"
check_eq "サイト側のシンボリックリンクが変わらない" \
         "$(echo "$BEFORE_ALL" | grep ' l ')" \
         "$(fingerprint "$TEST_SITE" | grep ' l ')"
echo

# --- 利用者が app/ に置いたものが残ること -----------------------------------
echo "[6] 利用者が app/ に置いたもの"
check_cmd "プラグイン app/plugin/zz_upgrade_test.inc が残る" \
          "sudo test -f '${TEST_SITE}/app/plugin/zz_upgrade_test.inc'"
check_cmd "独自テーマ app/theme/upgradetest/ が残る" \
          "sudo test -f '${TEST_SITE}/app/theme/upgradetest/html.php'"
check_cmd "見覚えのないファイル app/upgrade_test_note.txt が残る" \
          "sudo test -f '${TEST_SITE}/app/upgrade_test_note.txt'"
# docs/upgrade-guide.md の「古いマニュアルのページを片付ける」と 1 対 1。
# この案内は書き換えを終えた後に出るので、--dry-run のログには載らない。
check_cmd "焼き付け済みのマニュアルを知らせる" \
          "grep -q '古いマニュアルのページが残っています' '${DIST}/upgrade.log'"
check_cmd "  件数を出す (植えた 2 件)" \
          "grep -q '生成済みのページ (2 件)' '${DIST}/upgrade.log'"
check_cmd "  消さないと明言する" \
          "grep -q 'このツールはページを消しません' '${DIST}/upgrade.log'"
check_cmd "見覚えのないファイルが実行ログで報告される" \
          "grep -q 'upgrade_test_note.txt' '${DIST}/upgrade.log'"
echo

# --- 新しいコードが入ったこと -----------------------------------------------
echo "[7] 新しいコード"
check_cmd "app/version.inc に NEXTFORM_VERSION がある" \
          "sudo grep -q \"NEXTFORM_VERSION\" '${TEST_SITE}/app/version.inc'"
# Markdown ページは app/vendor/ の同梱ライブラリで描画する。
# アップグレードの対象ディレクトリ (UPGRADE_TARGET_DIRS) から app が外れたり、
# 除外規則が増えたりすると、コードだけ新しくなって Markdown ページが
# 500 になる。届いていることを名指しで確かめる。
check_cmd "同梱ライブラリ app/vendor/ が届く" \
          "sudo test -f ${TEST_SITE}/app/vendor/autoload.php && \
           sudo test -f ${TEST_SITE}/app/vendor/league/commonmark/src/MarkdownConverter.php"
check_cmd "同梱ライブラリのライセンス表記も届く" \
          "sudo test -f ${TEST_SITE}/app/vendor/league/commonmark/LICENSE"
check_cmd "app/tool/upgrade 自体も配置される (次回のアップグレード用)" \
          "sudo test -f '${TEST_SITE}/app/tool/upgrade'"
check_cmd "PHP 8 で消えた関数が残っていない (get_magic_quotes_gpc)" \
          "! sudo grep -rq 'get_magic_quotes_gpc' '${TEST_SITE}/app/'"
echo

# --- 静的 CSS が再生成されたこと --------------------------------------------
# ここが最大の落とし穴。app/theme/ はソースで、theme/ が生成物。
echo "[8] 静的 CSS の再生成"
check_cmd "theme/basic/style/main.css がある" \
          "sudo test -f '${TEST_SITE}/theme/basic/style/main.css'"
check_cmd "&pre の折り返し (white-space: pre-wrap) が反映されている" \
          "sudo grep -q 'pre-wrap' '${TEST_SITE}/theme/basic/style/main.css'"
check_cmd "サイドバー無効化ルールが出ていない" \
          "! sudo grep -qE 'article\.main[^}]*width: *100%' '${TEST_SITE}/theme/basic/style/main.css'"
check_cmd "theme/.htaccess が消えていない" \
          "sudo test -f '${TEST_SITE}/theme/.htaccess'"
echo

# --- 所有者 -----------------------------------------------------------------
echo "[9] 所有者"
BAD_OWNER=$(sudo find "${TEST_SITE}/app" "${TEST_SITE}/theme" \
                 \! -user "${SITE_OWNER%%:*}" -printf '%P\n' 2>/dev/null | head -5)
check_eq "app/ theme/ が元の所有者 (${SITE_OWNER}) のまま" "" "$BAD_OWNER"
echo

# --- バックアップ -----------------------------------------------------------
echo "[10] バックアップ"
BACKUP=$(sudo ls -d "${TEST_SITE}".backup-* 2>/dev/null | head -1)
check_cmd "バックアップが作られた" "test -n '$BACKUP'"
if [[ -n "$BACKUP" ]]; then
    check_cmd "バックアップにアップグレード前の app/version.inc がある" \
              "sudo test -f '${BACKUP}/app/version.inc' && ! sudo grep -q NEXTFORM_VERSION '${BACKUP}/app/version.inc'"
    # 既定では書き換える範囲 + 設定ファイルだけ。storage/ は触らないので入れない。
    # 業務 wiki の実測で storage/ 7.4 GB に対し書き換える範囲は 2 MB だった。
    check_cmd "既定では storage/ をコピーしない" \
              "! sudo test -e '${BACKUP}/storage'"
    check_cmd "書き換える app/ resource/ が控えてある" \
              "sudo test -d '${BACKUP}/app' && sudo test -d '${BACKUP}/resource'"
    check_cmd "再生成される theme/ が控えてある" \
              "sudo test -d '${BACKUP}/theme'"
    check_cmd "触らないが失うと痛い index.php / install-info.dat も控えてある" \
              "sudo test -f '${BACKUP}/index.php' && sudo test -f '${BACKUP}/install-info.dat'"
    # シンボリックリンクは辿らずリンクのまま複製する (cp -a と同じ)。
    # 辿ると (1) リンク切れで copy() が失敗してバックアップ全体が止まり、
    # (2) 外を指すリンクだと外にある実体まで抱え込む。
    check_eq  "app/ の中の壊れたリンクがリンクのまま複製される" \
              "../this-does-not-exist/" \
              "$(sudo readlink "${BACKUP}/app/broken-link-in-app" 2>/dev/null)"
    check_cmd "app/ の中の壊れたリンクがサイト側にも残る" \
              "sudo test -L '${TEST_SITE}/app/broken-link-in-app'"
fi

# --full-backup ならサイト全体。すでに更新済みなので --force で走らせる。
FULL="${TEST_SITE}.fullbackup"
sudo rm -rf "$FULL"
sudo php "${DIST}/NextForm/app/tool/upgrade" "${TEST_SITE}/index.php" \
     --yes --force --full-backup --backup-dir "$FULL" > "${DIST}/full.log" 2>&1
check_eq  "--full-backup が正常終了する" "0" "$?"
check_cmd "--full-backup なら storage/ も入る" "sudo test -d '${FULL}/storage/page'"
check_eq  "--full-backup でも壊れたリンクで止まらない" \
          "../this-does-not-exist/" "$(sudo readlink "${FULL}/broken-link" 2>/dev/null)"
check_eq  "ディレクトリへのリンクが実体コピーになっていない" \
          "l app" "$(sudo find "${FULL}" -maxdepth 1 -name link-to-dir -printf '%y %l' 2>/dev/null)"
check_eq  "ファイルへのリンクがリンクのまま複製される" \
          "index.php" "$(sudo readlink "${FULL}/link-to-file" 2>/dev/null)"
sudo rm -rf "$FULL"
echo

# --- 実際に動くこと ---------------------------------------------------------
echo "[11] 動作確認 ($TEST_URL)"
log_before=$(sudo cat "$PHP_ERROR_LOG" | wc -l)
code=$(curl -sk -o "${DIST}/top.html" -w '%{http_code}' "${TEST_URL}/")
check_eq "トップページが HTTP 200" "200" "$code"
code=$(curl -sk -o /dev/null -w '%{http_code}' "${TEST_URL}/theme/basic/style/main.css")
check_eq "静的 CSS が HTTP 200 で配信される" "200" "$code"
code=$(curl -sk -o /dev/null -w '%{http_code}' "${TEST_URL}/?option=admin_info")
check_eq "システム情報が HTTP 200" "200" "$code"
sleep 1
warnings=$(sudo tail -n "+$((log_before + 1))" "$PHP_ERROR_LOG" | grep -c . )
if [[ "$warnings" != "0" ]]; then
    echo "  --- PHP エラーログ ---"
    sudo tail -n "+$((log_before + 1))" "$PHP_ERROR_LOG" | sed 's/^/  | /' | head -20
fi
check_eq "PHP の警告が出ない" "0" "$warnings"
echo

# --- 二重適用の防止 ---------------------------------------------------------
echo "[12] 二重適用"
sudo php "${DIST}/NextForm/app/tool/upgrade" "${TEST_SITE}/index.php" --yes > "${DIST}/again.log" 2>&1
rc=$?
sed 's/^/  | /' "${DIST}/again.log"
check_cmd "同じ版へのアップグレードは中止される (終了コードが 0 以外)" "test $rc -ne 0"
echo

# --- 手作業手順 (docs/upgrade-guide.md 方法 B) -------------------------------
# 手順書のコマンドをそのまま実行する。ドキュメントは放っておくと実物とずれるので、
# スクリプトと同じように検証対象にしておく。
echo "[13] 手作業手順 (docs/upgrade-guide.md 方法 B)"
sudo rm -rf "$SITE_B" "${SITE_B}".backup-*
sudo cp -a "$SRC_SITE" "$SITE_B"
sudo tee "${SITE_B}/app/plugin/zz_upgrade_test.inc" > /dev/null <<'EOF'
<?php /* 手作業検証用。消えてはいけない。 */ ?>
EOF
sudo chown -Rh --reference="${SITE_B}/index.php" "$SITE_B"
B_INDEX=$(sudo md5sum "${SITE_B}/index.php" | cut -d' ' -f1)
B_OWNER=$(sudo stat -c '%U:%G' "${SITE_B}/index.php")

{
    # 手順 1 — バックアップ (書き換える範囲と設定ファイルだけ。storage/ は含めない)
    B="${SITE_B}.backup-$(date +%Y%m%d-%H%M%S)"
    sudo mkdir "$B" || exit 1
    ( cd "$SITE_B" && sudo cp -a app resource theme license.txt index.php install-info.dat .htaccess "$B"/ ) || exit 1
    sudo test -d "$B/app" && ! sudo test -e "$B/storage" || exit 1
    # 手順 2 — 見覚えのないファイルを確認する (表示するだけ)
    ( cd "$SITE_B" && sudo LC_ALL=C diff -rq app "${DIST}/NextForm/app" | grep '^Only in app' )
    # 手順 3 — コードを置き換える
    sudo cp -a "${DIST}/NextForm/app/."      "${SITE_B}/app/"      || exit 1
    sudo cp -a "${DIST}/NextForm/resource/." "${SITE_B}/resource/" || exit 1
    sudo cp -a "${DIST}/NextForm/license.txt" "${SITE_B}/"         || exit 1
    # 手順 4 — 所有者を戻す
    ( cd "$SITE_B" && sudo chown -R "$B_OWNER" app resource license.txt ) || exit 1
    # 手順 5 — 静的なテーマを再生成する
    sudo -u "${B_OWNER%%:*}" php "${SITE_B}/app/tool/update_wiki" "${SITE_B}/index.php" || exit 1
} > "${DIST}/manual.log" 2>&1
rc=$?
sed 's/^/  | /' "${DIST}/manual.log"
check_eq "手順が最後まで通る" "0" "$rc"
check_cmd "手順 2 が利用者のプラグインを 'Only in app' として挙げる" \
          "grep -q 'zz_upgrade_test.inc' '${DIST}/manual.log'"
check_cmd "app/version.inc に NEXTFORM_VERSION がある" \
          "sudo grep -q NEXTFORM_VERSION '${SITE_B}/app/version.inc'"
check_eq "index.php が変わらない" "$B_INDEX" "$(sudo md5sum "${SITE_B}/index.php" | cut -d' ' -f1)"
check_cmd "利用者のプラグインが残る" \
          "sudo test -f '${SITE_B}/app/plugin/zz_upgrade_test.inc'"
check_cmd "手順 5 で静的 CSS が再生成される" \
          "sudo grep -q 'pre-wrap' '${SITE_B}/theme/basic/style/main.css'"
B_BAD=$(sudo find "${SITE_B}/app" "${SITE_B}/theme" \! -user "${B_OWNER%%:*}" -printf '%P\n' 2>/dev/null | head -5)
check_eq "app/ theme/ が元の所有者 (${B_OWNER}) のまま" "" "$B_BAD"
code=$(curl -sk -o /dev/null -w '%{http_code}' "${URL_B}/")
check_eq "トップページが HTTP 200" "200" "$code"
echo

echo "----------------------------------------"
if [[ $fail -eq 0 ]]; then
    echo "全 ${total} 項目 ok"
    exit 0
fi
echo "${total} 項目中 ${fail} 件 失敗"
exit 1
