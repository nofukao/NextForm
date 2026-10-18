#!/bin/bash
# 履歴の「この版に戻す」のテスト
#
#   ./tests/history-restore.sh
#
# 環境変数:
#   NF_SITE            複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   RESTORE_TEST_SITE  検証用に作るサイト (既定: /var/www/html/nf-restore-test)
#   RESTORE_TEST_URL   その URL           (既定: http://localhost/nf-restore-test)
#   WIKI_ADMIN         管理者ユーザー名   (既定: admin)
#   KEEP=1             終了後に検証サイトを消さない
#
# 履歴の画面の旧版に「この版に戻す」を出し、確認の画面を経て、その版の本文と
# 種別で今のページを上書きする (option/restore.inc)。Wiki 記法から Markdown に
# 変換したページを元に戻すのにも使う。ここで固定するのは次のとおり:
#
#   1. 履歴の一覧と旧版の画面に「この版に戻す」が出て、確認の画面になること
#   2. 本文と種別が戻ること。題名は戻した種別の書き方で本文から取り直すこと
#      (種別の違う版にも戻せる)。戻す前の版は、同じ人が 15 分以内に続けて
#      保存したときでも履歴に残ること
#   3. 検索の索引が、戻した本文と一致すること (種別が変わっても古い語が残らない)
#   4. 種別 file の版は、中身と、中身に付く meta (Content-type・画像の寸法) が戻ること
#   5. 戻せないとき (GET、ロック中、書く権限が無い、確認の画面のあとに誰かが
#      編集した、ページが無い、版が無い) は何も変えないこと
#
# 資格情報をテストに置かずに済ませるため、複製したサイトの
# 「ログインしていない利用者」に write 権限を与えてから検査する。
#
# 権限と索引を書き換えるので、必ず複製したサイトに対して実行する。
# 複製元には触らない。root で実行する必要がある。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
RESTORE_TEST_SITE="${RESTORE_TEST_SITE:-/var/www/html/nf-restore-test}"
RESTORE_TEST_URL="${RESTORE_TEST_URL:-http://localhost/nf-restore-test}"
WIKI_ADMIN="${WIKI_ADMIN:-admin}"
PHP_ERROR_LOG="${PHP_ERROR_LOG:-/var/log/php-fpm/www-error.log}"

fail=0
total=0
WORK="$(mktemp -d)"

cleanup() {
    rm -rf "$WORK"
    if [[ "${KEEP:-0}" != "1" ]]; then
        sudo rm -rf "$RESTORE_TEST_SITE" 2>/dev/null
    else
        echo
        echo "KEEP=1 のため検証サイトを残しました: $RESTORE_TEST_SITE"
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
    sudo -u "$SITE_OWNER" php -d memory_limit=512M \
        "${RESTORE_TEST_SITE}/history-restore-helper.php" \
        "${RESTORE_TEST_SITE}/index.php" "$WIKI_ADMIN" "$@" 2>/dev/null
}

value_of() {
    printf '%s\n' "$1" | sed -n "s/^$2=//p" | head -1
}

code() {
    curl -sk -o /dev/null -w '%{http_code}' "$@"
}

sha_of_body() {
    value_of "$(helper body "$1")" body | base64 -d 2>/dev/null | sha256sum | cut -d' ' -f1
}

sha_of() { sha256sum < "$1" | cut -d' ' -f1; }

# 確認の画面を開き、HTML を $WORK/confirm に置く。$1 ページ名  $2 版の時刻
open_confirm() {
    curl -sk -o "$WORK/confirm" -w '%{http_code}' \
         "${RESTORE_TEST_URL}/?$1&option=restore&backuptime=$2"
}

# 確認の画面のフォームから ticket を取る
confirm_ticket() {
    grep -o '<input[^>]*name="ticket"[^>]*>' "$WORK/confirm" \
        | sed -E 's/.*value="([^"]*)".*/\1/' | head -1
}

# 「戻す」を送る。$1 ページ名  $2 版の時刻  $3 ticket。応答コードを返す
restore() {
    curl -sk -o "$WORK/body" -w '%{http_code}' -X POST -H "Origin: ${ORIGIN}" \
         -d "option=restore" -d "action=write" -d "backuptime=$2" -d "ticket=$3" \
         "${RESTORE_TEST_URL}/?$1"
}

said() {
    grep -qE "$1" "$WORK/body" && echo yes || echo no
}

if [[ ! -d "$NF_SITE" ]]; then
    echo "複製元がありません: $NF_SITE" >&2
    echo "tests/env.local の NF_SITE を設定してください。" >&2
    exit 1
fi

echo "複製元 = $NF_SITE"
echo "検証先 = $RESTORE_TEST_SITE"
echo "URL    = $RESTORE_TEST_URL"
echo

sudo rm -rf "$RESTORE_TEST_SITE"
sudo cp -a "$NF_SITE" "$RESTORE_TEST_SITE"
SITE_OWNER=$(sudo stat -c '%U' "${RESTORE_TEST_SITE}/index.php")

# 複製元に配置済みのコードではなく、リポジトリの作業ツリーを検証する
sudo rsync -a --delete "${REPO_ROOT}/NextForm/app/"      "${RESTORE_TEST_SITE}/app/"
sudo rsync -a --delete "${REPO_ROOT}/NextForm/resource/" "${RESTORE_TEST_SITE}/resource/"
sudo cp "${REPO_ROOT}/tests/history-restore-helper.php" "${RESTORE_TEST_SITE}/"
sudo chown -R "$SITE_OWNER" "${RESTORE_TEST_SITE}/app" "${RESTORE_TEST_SITE}/resource" \
                            "${RESTORE_TEST_SITE}/history-restore-helper.php"

# 置いたコードが PHP-FPM に載るのを待つ (tests/markdown.sh と同じ理由)
OPCACHE_FREQ=$(php -r 'echo (int)ini_get("opcache.revalidate_freq");' 2>/dev/null)
sleep $(( ${OPCACHE_FREQ:-2} + 1 ))

if [[ "$(value_of "$(helper guest write)" saved)" != "1" ]]; then
    echo "ログインしていない利用者に write 権限を与えられませんでした。" >&2
    exit 1
fi
if [[ "$(code "${RESTORE_TEST_URL}/")" != "200" ]]; then
    echo "検証サイトが $RESTORE_TEST_URL で見えません。" >&2
    echo "tests/env.local の RESTORE_TEST_URL を設定してください。" >&2
    exit 1
fi
ORIGIN=$(printf '%s' "$RESTORE_TEST_URL" | sed -E 's#^(https?://[^/]+).*#\1#')
log_before=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')
log_before="${log_before:-0}"

# --- 準備 ---------------------------------------------------------------------
# Doc: 旧版は Wiki 記法、今は Markdown (変換したページと同じ形)。
# 今の版は「ログインしていない利用者」が書く。戻すのも同じ利用者なので、
# 15 分以内に続けて保存した扱いになる (それでも戻す前の版が残ることを見る)。
FIX="${RESTORE_TEST_SITE}/restore-fixtures"
sudo mkdir -p "$FIX"
put() { sudo install -o "$SITE_OWNER" -m 644 "$1" "${FIX}/$(basename "$1")"; }
printf -- "&title{古い題名}\n*見出し\n旧い本文のことばウィキ\n" > "$WORK/v1.wiki"
printf -- "---\ntitle: 新しい題名\ntags: [あたらしい]\n---\n新しい本文のことばマークダウン\n" > "$WORK/v2.md"
printf -- "別の本文\n" > "$WORK/other.txt"
python3 - "$WORK/a.png" 40 30 "$WORK/b.png" 20 10 <<'PYEOF'
import struct, sys, zlib
def png(path, w, h):
    raw = b''.join(b'\x00' + bytes((x * 7 + y * 3) % 256 for x in range(w * 3)) for y in range(h))
    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                           + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
png(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]))
png(sys.argv[4], int(sys.argv[5]), int(sys.argv[6]))
PYEOF
for f in v1.wiki v2.md other.txt a.png b.png; do put "$WORK/$f"; done

D=RestoreTest/Doc
helper make-text "$D" wiki "${FIX}/v1.wiki" > /dev/null
# Wiki 記法はタグを本文に書かないので、meta に付ける (タグの画面と同じ経路)
helper set-tag "$D" ふるい > /dev/null
T1=$(value_of "$(helper backup-now "$D")" backuptime)
helper retype-as-guest "$D" markdown "${FIX}/v2.md" > /dev/null

F=RestoreTest/Doc/pic.png
helper make-file "$F" "${FIX}/a.png" image/png > /dev/null
TF=$(value_of "$(helper backup-now "$F")" backuptime)
sleep 1
helper make-file "$F" "${FIX}/b.png" image/png > /dev/null

# 版の種別が混ざった履歴を作るときに索引がずれるので、まっさらな索引から始める
helper rebuild > /dev/null
check_eq "(前提) 今は Markdown、旧版は Wiki 記法" "markdown|wiki" \
         "$(value_of "$(helper meta "$D" type)" meta)|$(value_of "$(helper backup-type "$D" "$T1")" type)"
check_eq "(前提) 索引が本文と一致している" "1" "$(value_of "$(helper index-ok "$D")" ok)"
echo

echo "1. 履歴の一覧と旧版の画面に「この版に戻す」が出て、確認の画面になること"
link="option=restore&amp;backuptime=${T1}"
# pipefail なので、grep -q が先に終わると curl の書き込みの失敗で偽になる。いったん受け取る
html=$(curl -sk "${RESTORE_TEST_URL}/?${D}&option=history")
check_eq "履歴の一覧に、旧版を戻すリンクがある" "yes" \
         "$([[ "$html" == *"$link"* ]] && echo yes || echo no)"
html=$(curl -sk "${RESTORE_TEST_URL}/?${D}&option=history&action=page&backuptime=${T1}")
check_eq "旧版の画面にも、その版を戻すリンクがある" "yes" \
         "$([[ "$html" == *"$link"* ]] && echo yes || echo no)"
check_eq "確認の画面が出る (200)" "200" "$(open_confirm "$D" "$T1")"
check_eq "  戻すボタンのあるフォームがある" "yes" \
         "$(grep -q 'name="action" value="write"' "$WORK/confirm" && grep -q 'type="submit"' "$WORK/confirm" && echo yes || echo no)"
echo

echo "2. 本文と種別が戻ること"
before_times=$(value_of "$(helper backup-times "$D")" times)
check_eq "GET では戻せない (403)" "403" \
         "$(code "${RESTORE_TEST_URL}/?${D}&option=restore&action=write&backuptime=${T1}&ticket=$(confirm_ticket)")"
check_eq "  何も変わらない" "markdown" "$(value_of "$(helper meta "$D" type)" meta)"

open_confirm "$D" "$T1" > /dev/null
check_eq "戻す: 保存後の移動 (302)" "302" "$(restore "$D" "$T1" "$(confirm_ticket)")"
check_eq "  本文が旧版と同じ" "$(sha_of "$WORK/v1.wiki")" "$(sha_of_body "$D")"
check_eq "  ページがあり、種別が旧版と同じ (wiki)" "1|wiki" \
         "$(value_of "$(helper exists "$D")" exists)|$(value_of "$(helper meta "$D" type)" meta)"
check_eq "  題名を Wiki 記法の本文から取り直す" "古い題名" "$(value_of "$(helper meta "$D" title)" meta)"
check_eq "  タグはその版のもの (Wiki 記法の版は本文にタグが無いので、版の meta から)" "ふるい" \
         "$(value_of "$(helper meta "$D" tag)" meta)"
check_eq "  タグ一覧の件数も合う" "1|0" \
         "$(value_of "$(helper alltag ふるい)" count)|$(value_of "$(helper alltag あたらしい)" count)"
after_times=$(value_of "$(helper backup-times "$D")" times)
newest=${after_times##*,}
check_eq "  戻す前の版 (Markdown) が履歴に残る (15 分以内に続けて保存しても)" "yes|markdown|1" \
         "$([[ "$after_times" == "${before_times},"* ]] && echo yes || echo no)|$(value_of "$(helper backup-type "$D" "$newest")" type)|$(value_of "$(helper exists "$D")" exists)"
check_eq "  残った版の本文は戻す前のもの" "$(sha_of "$WORK/v2.md")" \
         "$(value_of "$(helper backup-body "$D" "$newest")" body | base64 -d | sha256sum | cut -d' ' -f1)"
echo

S=RestoreTest/Second
helper make-text "$S" text "${FIX}/other.txt" > /dev/null
out=$(helper same-second-backup "$S" "${FIX}/v1.wiki" "${FIX}/other.txt")
check_eq "同じ秒にある版を、必ず残す保存で上書きしない" "1|1" "$(value_of "$out" added)|$(value_of "$out" kept)"
echo

echo "3. 検索の索引が、戻した本文と一致すること"
out=$(helper index-ok "$D")
check_eq "索引が戻した本文と一致する (足りない・余分な ngram が無い)" "1|0|0" \
         "$(value_of "$out" ok)|$(value_of "$out" missing)|$(value_of "$out" extra)"

open_confirm "$D" "$newest" > /dev/null
restore "$D" "$newest" "$(confirm_ticket)" > /dev/null
check_eq "Markdown の版に戻し直すと、題名とタグをフロントマターから取る" "markdown|新しい題名|あたらしい" \
         "$(value_of "$(helper meta "$D" type)" meta)|$(value_of "$(helper meta "$D" title)" meta)|$(value_of "$(helper meta "$D" tag)" meta)"
out=$(helper index-ok "$D")
check_eq "  索引が一致する" "1|0|0" "$(value_of "$out" ok)|$(value_of "$out" missing)|$(value_of "$out" extra)"
check_eq "  タグ一覧の件数も合う" "0|1" \
         "$(value_of "$(helper alltag ふるい)" count)|$(value_of "$(helper alltag あたらしい)" count)"
echo

echo "4. 種別 file の版は、中身と、中身に付く meta が戻ること"
open_confirm "$F" "$TF" > /dev/null
check_eq "戻す: 保存後の移動 (302)" "302" "$(restore "$F" "$TF" "$(confirm_ticket)")"
check_eq "  中身が旧版と同じ" "$(sha_of "$WORK/a.png")" "$(sha_of_body "$F")"
check_eq "  種別・Content-type・画像の寸法が旧版のもの" "file|image/png|40|30" \
         "$(value_of "$(helper meta "$F" type)" meta)|$(value_of "$(helper meta "$F" Content-type)" meta)|$(value_of "$(helper meta "$F" image_width)" meta)|$(value_of "$(helper meta "$F" image_height)" meta)"
times=$(value_of "$(helper backup-times "$F")" times)
check_eq "  戻す前の中身 (b.png) が履歴に残る" "$(sha_of "$WORK/b.png")" \
         "$(value_of "$(helper backup-body "$F" "${times##*,}")" body | base64 -d | sha256sum | cut -d' ' -f1)"

# 種別 file と text が混ざった履歴。file の版に戻すと file の meta が付き、
# text の版に戻すと落ちる
Z=RestoreTest/Doc/z.png
helper make-file "$Z" "${FIX}/a.png" image/png > /dev/null
TZ=$(value_of "$(helper backup-now "$Z")" backuptime)
sleep 1
helper retype-text "$Z" "${FIX}/other.txt" > /dev/null
open_confirm "$Z" "$TZ" > /dev/null
restore "$Z" "$TZ" "$(confirm_ticket)" > /dev/null
check_eq "text のページを file の版に戻す: 中身と file の meta" "file|image/png|40|$(sha_of "$WORK/a.png")" \
         "$(value_of "$(helper meta "$Z" type)" meta)|$(value_of "$(helper meta "$Z" Content-type)" meta)|$(value_of "$(helper meta "$Z" image_width)" meta)|$(sha_of_body "$Z")"
times=$(value_of "$(helper backup-times "$Z")" times)
open_confirm "$Z" "${times##*,}" > /dev/null
restore "$Z" "${times##*,}" "$(confirm_ticket)" > /dev/null
check_eq "  text の版に戻し直す: file の meta は落ちる" "text||$(sha_of "$WORK/other.txt")" \
         "$(value_of "$(helper meta "$Z" type)" meta)|$(value_of "$(helper meta "$Z" Content-type)" meta)|$(sha_of_body "$Z")"
echo

echo "5. 戻せないときは何も変えないこと"
cur=$(sha_of_body "$D")
times=$(value_of "$(helper backup-times "$D")" times)
first=${times%%,*}

helper lock "$D" > /dev/null
html=$(curl -sk "${RESTORE_TEST_URL}/?${D}&option=history")
check_eq "ロック中は、履歴の一覧に戻すリンクを出さない (書く権限だけの人)" "no" \
         "$([[ "$html" == *'option=restore'* ]] && echo yes || echo no)"
open_confirm "$D" "$first" > /dev/null
restore "$D" "$first" "$(confirm_ticket)" > /dev/null
check_eq "ロック中: 理由が出る" "yes" "$(said 'ロックされています|locked')"
check_eq "  何も変わらない" "$cur" "$(sha_of_body "$D")"
helper unlock "$D" > /dev/null

open_confirm "$D" "$first" > /dev/null
stale=$(confirm_ticket)
helper make-text "$D" markdown "${FIX}/other.txt" > /dev/null
cur=$(sha_of_body "$D")
restore "$D" "$first" "$stale" > /dev/null
check_eq "確認の画面のあとに誰かが編集した: 理由が出る" "yes" "$(said '衝突|Confliction')"
check_eq "  何も変わらない" "$cur" "$(sha_of_body "$D")"

open_confirm "$D" "$first" > /dev/null
ticket=$(confirm_ticket)
helper guest read > /dev/null
check_eq "書く権限が無い: ログインを求める (401)" "401" "$(restore "$D" "$first" "$ticket")"
check_eq "  何も変わらない" "$cur" "$(sha_of_body "$D")"
helper guest write > /dev/null

restore "$D" "9999999999" "$(confirm_ticket)" > /dev/null
check_eq "版が無い: 理由が出る" "yes" "$(said 'Backup not found|バックアップが見つかりません')"
check_eq "  何も変わらない" "$cur" "$(sha_of_body "$D")"
for bad in 0 abc; do
    open_confirm "$D" "$bad" > /dev/null
    check_eq "版の時刻が ${bad}: 確認の画面にフォームを出さない" "yes|no" \
             "$(grep -qE 'Backup not found|バックアップが見つかりません' "$WORK/confirm" && echo yes || echo no)|$(grep -q 'name="backuptime"' "$WORK/confirm" && echo yes || echo no)"
    before=$(value_of "$(helper backup-times "$D")" times)
    restore "$D" "$bad" "$ticket" > /dev/null
    check_eq "  送っても何も変わらない (本文も履歴も)" "$cur|$before" \
             "$(sha_of_body "$D")|$(value_of "$(helper backup-times "$D")" times)"
done

G=RestoreTest/Gone
helper make-text "$G" markdown "${FIX}/other.txt" > /dev/null
TG=$(value_of "$(helper backup-now "$G")" backuptime)
helper delete "$G" > /dev/null
restore "$G" "$TG" "" > /dev/null
check_eq "ページが無い (削除した): 戻さない" "0" "$(value_of "$(helper exists "$G")" exists)"
echo

echo "6. 同じ秒に続けて保存されても、古い ticket の保存を競合として断ること"
# 戻す画面に限らず、編集の画面すべての競合の判定 (page_write())。ticket が
# 「ページ名 + 保存した秒」だったころは、同じ秒の 2 回の保存で同じ値になり、
# 間に編集を始めた人の保存が、あとの保存を黙って上書きした
S=RestoreTest/SameSecond
helper make-text "$S" markdown "${FIX}/other.txt" > /dev/null
R=$(helper same-second-ticket "$S" "${FIX}/first.txt" "${FIX}/other.txt")
check_eq "2 回の保存が同じ秒に入る (検査の前提)" "1" "$(value_of "$R" same_second)"
check_eq "  古い ticket の保存は競合として断る" "1" "$(value_of "$R" rejected)"
echo

echo "7. PHP の警告が出ていないこと"
log_new=$(sudo tail -n +"$((log_before + 1))" "$PHP_ERROR_LOG" 2>/dev/null \
              | grep -F "$RESTORE_TEST_SITE" || true)
check_eq "検証サイトの警告が増えていない" "" "$log_new"
echo

if [[ $fail -eq 0 ]]; then
    printf '%d/%d 件すべて通りました。\n' "$total" "$total"
else
    printf '%d/%d 件が失敗しました。\n' "$fail" "$total"
fi
exit $((fail == 0 ? 0 : 1))
