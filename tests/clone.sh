#!/bin/bash
# 他の NextForm のページのクローンのテスト
#
#   ./tests/clone.sh
#
# 環境変数:
#   NF_SITE          複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   CLONE_SRC_SITE   クローン元として作るサイト (既定: /var/www/html/nf-clone-src)
#   CLONE_SRC_URL    その URL           (既定: http://localhost/nf-clone-src)
#   CLONE_DST_SITE   クローン先として作るサイト (既定: /var/www/html/nf-clone-dst)
#   CLONE_DST_URL    その URL           (既定: http://localhost/nf-clone-dst)
#   WIKI_ADMIN       管理者ユーザー名   (既定: admin)
#   KEEP=1           終了後に検証サイトを消さない
#
# 新規ページの種別で「他のNextFormページのクローン」を選ぶと、URL を入れて
# 他の NextForm のページを本文と添付ごと写せる (option/clone.inc)。
# クローン先のサーバーがクローン元へ HTTP で取りに行くので、サイトを 2 つ作り、
# 実際の HTTP で確かめる。ここで固定するのは次のとおり:
#
#   1. 新規ページの種別に選択肢が出て、選ぶとクローンの画面になること
#   2. 既定ではプライベートなネットワークのホストから取らず、
#      「クローン元として許すホスト」に書けば取れること (URL でもホスト名でも)
#   3. 種別ごと (Markdown / wiki / text / file) に、本文と添付が 1 バイトも
#      変わらずにクローンされること。添付以外の子ページや、ほかのページの添付は
#      写さないこと
#   4. URL の書き方 (http からの転送、index.php つき、ページ名の符号化) を問わないこと
#   5. 写せないとき (URL の誤り、NextForm でない、ページが無い、ログインが要る、
#      添付が大きすぎる、同じ名前のページがある、書く権限が無い、GET) は
#      何も書かないこと
#   6. クローン元が古い版 (添付の一覧に大きさが無い) でも写せ、大きすぎる添付は
#      転送の途中で打ち切って断ること
#
# クローン先には添付の大きさの上限を 1MB にする .user.ini を置く
# (max_upload_file_size() が見る upload_max_filesize)。
#
# 権限と設定を書き換えるので、必ず複製したサイトに対して実行する。
# 複製元には触らない。root で実行する必要がある。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
CLONE_SRC_SITE="${CLONE_SRC_SITE:-/var/www/html/nf-clone-src}"
CLONE_SRC_URL="${CLONE_SRC_URL:-http://localhost/nf-clone-src}"
CLONE_DST_SITE="${CLONE_DST_SITE:-/var/www/html/nf-clone-dst}"
CLONE_DST_URL="${CLONE_DST_URL:-http://localhost/nf-clone-dst}"
WIKI_ADMIN="${WIKI_ADMIN:-admin}"
PHP_ERROR_LOG="${PHP_ERROR_LOG:-/var/log/php-fpm/www-error.log}"

# 古い版のクローン元として使う版 (添付の一覧が大きさを返さない)
OLD_REF="v0.10.0"

fail=0
total=0
WORK="$(mktemp -d)"

cleanup() {
    rm -rf "$WORK"
    if [[ "${KEEP:-0}" != "1" ]]; then
        sudo rm -rf "$CLONE_SRC_SITE" "$CLONE_DST_SITE" 2>/dev/null
    else
        echo
        echo "KEEP=1 のため検証サイトを残しました: $CLONE_SRC_SITE $CLONE_DST_SITE"
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

# $1 サイトのパス  $2 以降 検査名と引数
helper_at() {
    local site="$1"
    shift
    sudo -u "$SITE_OWNER" php -d memory_limit=512M \
        "${site}/clone-helper.php" "${site}/index.php" "$WIKI_ADMIN" "$@" 2>/dev/null
}
src() { helper_at "$CLONE_SRC_SITE" "$@"; }
dst() { helper_at "$CLONE_DST_SITE" "$@"; }

value_of() {
    printf '%s\n' "$1" | sed -n "s/^$2=//p" | head -1
}

sha_of_body() {
    value_of "$(dst body "$1")" body | base64 -d 2>/dev/null | sha256sum | cut -d' ' -f1
}

src_sha_of_body() {
    value_of "$(src body "$1")" body | base64 -d 2>/dev/null | sha256sum | cut -d' ' -f1
}

# クローン先でクローンを実行する。応答の本文を $WORK/body、ヘッダを $WORK/headers に置き、
# 応答コードを返す。$1 クローン先のページ名  $2 クローン元の URL
clone_to() {
    curl -sk -o "$WORK/body" -D "$WORK/headers" -w '%{http_code}' -X POST \
         -H "Origin: ${DST_ORIGIN}" \
         -d "option=clone" -d "action=write" --data-urlencode "source_url=$2" \
         "${CLONE_DST_URL}/?$1"
}

# 応答の本文にその文言があるか (yes/no)。日本語と英語のどちらでもよい
said() {
    grep -qE "$1" "$WORK/body" && echo yes || echo no
}

children_of() {
    dst children "$1" | sed -n 's/^child=//p' | tr '\n' '|'
}

if [[ ! -d "$NF_SITE" ]]; then
    echo "複製元がありません: $NF_SITE" >&2
    echo "tests/env.local の NF_SITE を設定してください。" >&2
    exit 1
fi

echo "複製元   = $NF_SITE"
echo "クローン元 = $CLONE_SRC_SITE ($CLONE_SRC_URL)"
echo "クローン先 = $CLONE_DST_SITE ($CLONE_DST_URL)"
echo

for site in "$CLONE_SRC_SITE" "$CLONE_DST_SITE"; do
    sudo rm -rf "$site"
    sudo cp -a "$NF_SITE" "$site"
done
SITE_OWNER=$(sudo stat -c '%U' "${CLONE_SRC_SITE}/index.php")

# 複製元に配置済みのコードではなく、リポジトリの作業ツリーを検証する
install_code() {
    local site="$1" from="$2"
    sudo rsync -a --delete "${from}/app/"      "${site}/app/"
    sudo rsync -a --delete "${from}/resource/" "${site}/resource/"
    sudo cp "${REPO_ROOT}/tests/clone-helper.php" "${site}/"
    sudo chown -R "$SITE_OWNER" "${site}/app" "${site}/resource" "${site}/clone-helper.php"
}
install_code "$CLONE_SRC_SITE" "${REPO_ROOT}/NextForm"
install_code "$CLONE_DST_SITE" "${REPO_ROOT}/NextForm"
printf 'upload_max_filesize = 1M\n' > "$WORK/user.ini"
sudo install -o "$SITE_OWNER" -m 644 "$WORK/user.ini" "${CLONE_DST_SITE}/.user.ini"

# 置いたコードが PHP-FPM に載るのを待つ (tests/markdown.sh と同じ理由)
OPCACHE_FREQ=$(php -r 'echo (int)ini_get("opcache.revalidate_freq");' 2>/dev/null)
wait_opcache() { sleep $(( ${OPCACHE_FREQ:-2} + 1 )); }
wait_opcache

for url in "$CLONE_SRC_URL" "$CLONE_DST_URL"; do
    if [[ "$(curl -sk -o /dev/null -w '%{http_code}' "${url}/")" != "200" ]]; then
        echo "検証サイトが $url で見えません。" >&2
        echo "tests/env.local の CLONE_SRC_URL / CLONE_DST_URL を設定してください。" >&2
        exit 1
    fi
done

DST_ORIGIN=$(printf '%s' "$CLONE_DST_URL" | sed -E 's#^(https?://[^/]+).*#\1#')
SRC_HOST=$(printf '%s' "$CLONE_SRC_URL" | sed -E 's#^https?://([^/:]+).*#\1#')
SRC_ROOT=$(printf '%s' "$CLONE_SRC_URL" | sed -E 's#^(https?://[^/]+).*#\1#')
log_before=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')
log_before="${log_before:-0}"

# --- 準備: クローン元にページと添付を作る ----------------------------------
FIX="${CLONE_SRC_SITE}/clone-fixtures"
sudo mkdir -p "$FIX"
put() { sudo install -o "$SITE_OWNER" -m 644 "$1" "${FIX}/$(basename "$1")"; }

printf -- "---\ntitle: クローンの題名\ntags: [クローン]\n---\n本文 '--aa' <script>x</script>\n\n![画像](./img.png)\n\n[よそ](../Other/x.png)\n" > "$WORK/md.txt"
printf -- "&title{ウィキの題名}\n*見出し\n&include(./a.txt)\n" > "$WORK/wiki.txt"
printf -- "そのまま '--\n<b>x</b>\n" > "$WORK/text.txt"
printf -- "子ページ\n" > "$WORK/sub.txt"
printf -- "大きい添付\n" > "$WORK/big.txt"
printf -- "読めない\n" > "$WORK/secret.txt"
printf -- "添付 a\n" > "$WORK/a.txt"
printf -- "日本語の名前の添付\n" > "$WORK/ja.txt"
head -c 3000 /dev/urandom > "$WORK/file.bin"
head -c 1572864 /dev/urandom > "$WORK/big.bin"
python3 - "$WORK/img.png" <<'PYEOF'
import struct, sys, zlib
w, h = 40, 30
raw = b''.join(b'\x00' + bytes((x * 7 + y * 3) % 256 for x in range(w * 3)) for y in range(h))
def chunk(t, d):
    return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
open(sys.argv[1], 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                              + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
PYEOF
for f in md.txt wiki.txt text.txt sub.txt big.txt secret.txt a.txt ja.txt file.bin big.bin img.png; do
    put "$WORK/$f"
done

src cleanup CloneTest > /dev/null
dst cleanup Dst > /dev/null
src make-text CloneTest/Md markdown "${FIX}/md.txt" > /dev/null
src make-file CloneTest/Md/img.png "${FIX}/img.png" image/png > /dev/null
src make-file "CloneTest/Md/日本語 名前.txt" "${FIX}/ja.txt" text/plain > /dev/null
src make-text CloneTest/Md/Sub markdown "${FIX}/sub.txt" > /dev/null
src make-text CloneTest/Wiki wiki "${FIX}/wiki.txt" > /dev/null
src make-file CloneTest/Wiki/a.txt "${FIX}/a.txt" text/plain > /dev/null
src make-text CloneTest/Text text "${FIX}/text.txt" > /dev/null
src make-file CloneTest/File.bin "${FIX}/file.bin" application/octet-stream > /dev/null
src make-text CloneTest/Big markdown "${FIX}/big.txt" > /dev/null
src make-file CloneTest/Big/big.bin "${FIX}/big.bin" application/octet-stream > /dev/null
src make-text CloneTest/Secret markdown "${FIX}/secret.txt" > /dev/null
src guest read CloneTest/Secret > /dev/null
dst guest write > /dev/null
dst set-allowed-hosts '' > /dev/null

echo "1. 新規ページの種別に選択肢が出て、選ぶとクローンの画面になること"
html=$(curl -sk "${CLONE_DST_URL}/?option=newpage")
check_eq "新規ページの画面に「他のNextFormページのクローン」がある" "yes" \
         "$([[ "$html" == *'<option value="clone">'* ]] && echo yes || echo no)"
html=$(curl -sk "${CLONE_DST_URL}/?Dst/New")
check_eq "まだ無いページの「ページ種別」にもある" "yes" \
         "$([[ "$html" == *'<option value="clone">'* ]] && echo yes || echo no)"
location=$(curl -sk -o /dev/null -D - "${CLONE_DST_URL}/?page=Dst%2FNew&newpage_type=clone" \
               | sed -n 's/^location: *//Ip' | tr -d '\r')
check_eq "選んで作成するとクローンの画面に移る" "yes" \
         "$([[ "$location" == *'option=clone'* ]] && echo yes || echo no)"
html=$(curl -sk "${CLONE_DST_URL}/?Dst/New&option=clone")
check_eq "クローンの画面に URL の入力欄がある" "yes" \
         "$([[ "$html" == *'name="source_url"'* ]] && echo yes || echo no)"
echo

echo "2. 既定ではプライベートなネットワークのホストから取らないこと"
code=$(clone_to Dst/Md "${CLONE_SRC_URL}/?CloneTest/Md")
check_eq "画面に戻る (200)" "200" "$code"
check_eq "  理由が出る" "yes" "$(said 'プライベートなネットワーク|private network')"
check_eq "  何も書かない" "0" "$(value_of "$(dst exists Dst/Md)" exists)"
# 許すホストには、ホスト名だけでなく URL も書ける (サイトの URL を貼る人が多い)。
# URL はホストの部分だけを見る。ほかの行があっても、パスが付いていてもよい。
check_eq "(準備) 許すホストに URL で書く" "1" \
         "$(value_of "$(dst set-allowed-hosts "$(printf 'https://other.example.invalid/nm/\n%s/nf-clone-src/' "$SRC_ROOT")")" saved)"
echo

echo "3. 種別ごとに、本文と添付が変わらずにクローンされること"
code=$(clone_to Dst/Md "${CLONE_SRC_URL}/?CloneTest/Md")
check_eq "Markdown: 保存後の移動 (302)" "302" "$code"
check_eq "  本文が同じ" "$(src_sha_of_body CloneTest/Md)" "$(sha_of_body Dst/Md)"
check_eq "  種別は Markdown" "markdown" "$(value_of "$(dst meta Dst/Md type)" meta)"
check_eq "  題名が本文から取られている" "クローンの題名" "$(value_of "$(dst meta Dst/Md title)" meta)"
check_eq "  直下の添付だけを写す (子ページやほかのページの添付は写さない)" \
         "Dst/Md/img.png|Dst/Md/日本語 名前.txt|" "$(children_of Dst/Md)"
check_eq "  画像が同じ" "$(sha256sum < "$WORK/img.png" | cut -d' ' -f1)" "$(sha_of_body Dst/Md/img.png)"
check_eq "  画像の Content-type" "image/png" "$(value_of "$(dst meta Dst/Md/img.png Content-type)" meta)"
check_eq "  日本語の名前の添付が同じ" "$(sha256sum < "$WORK/ja.txt" | cut -d' ' -f1)" \
         "$(sha_of_body "Dst/Md/日本語 名前.txt")"

code=$(clone_to Dst/Wiki "${CLONE_SRC_URL}/?CloneTest/Wiki")
check_eq "wiki: 保存後の移動 (302)" "302" "$code"
check_eq "  本文が同じ" "$(src_sha_of_body CloneTest/Wiki)" "$(sha_of_body Dst/Wiki)"
check_eq "  種別は wiki" "wiki" "$(value_of "$(dst meta Dst/Wiki type)" meta)"
check_eq "  題名が本文から取られている" "ウィキの題名" "$(value_of "$(dst meta Dst/Wiki title)" meta)"
check_eq "  添付が同じ" "$(sha256sum < "$WORK/a.txt" | cut -d' ' -f1)" "$(sha_of_body Dst/Wiki/a.txt)"

code=$(clone_to Dst/Text "${CLONE_SRC_URL}/?CloneTest/Text")
check_eq "text: 保存後の移動 (302)" "302" "$code"
check_eq "  本文が同じ" "$(src_sha_of_body CloneTest/Text)" "$(sha_of_body Dst/Text)"
check_eq "  種別は text" "text" "$(value_of "$(dst meta Dst/Text type)" meta)"

code=$(clone_to Dst/File.bin "${CLONE_SRC_URL}/?CloneTest/File.bin")
check_eq "file: 保存後の移動 (302)" "302" "$code"
check_eq "  中身が同じ" "$(sha256sum < "$WORK/file.bin" | cut -d' ' -f1)" "$(sha_of_body Dst/File.bin)"
check_eq "  種別は file" "file" "$(value_of "$(dst meta Dst/File.bin type)" meta)"
echo

echo "4. URL の書き方を問わないこと"
check_eq "(準備) 許すホストをホスト名だけで書く" "1" "$(value_of "$(dst set-allowed-hosts "$SRC_HOST")" saved)"
code=$(clone_to Dst/TextBareHost "${CLONE_SRC_URL}/?CloneTest/Text")
check_eq "許すホストをホスト名だけで書いても取れる" "$(src_sha_of_body CloneTest/Text)" "$(sha_of_body Dst/TextBareHost)"
http_url=$(printf '%s' "$CLONE_SRC_URL" | sed -E 's#^https://#http://#')
code=$(clone_to Dst/TextHttp "${http_url}/?CloneTest/Text")
check_eq "http から https への転送をたどる" "$(src_sha_of_body CloneTest/Text)" "$(sha_of_body Dst/TextHttp)"
code=$(clone_to Dst/TextIndex "${CLONE_SRC_URL}/index.php?CloneTest/Text&action=edit")
check_eq "index.php つき、余分な引数つき" "$(src_sha_of_body CloneTest/Text)" "$(sha_of_body Dst/TextIndex)"
code=$(clone_to Dst/TextEncoded "${CLONE_SRC_URL}/?CloneTest%2FText")
check_eq "ページ名が符号化されている" "$(src_sha_of_body CloneTest/Text)" "$(sha_of_body Dst/TextEncoded)"
echo

echo "5. 写せないときは何も書かないこと"
# $1 説明  $2 クローン先  $3 URL  $4 出るはずの文言 (正規表現)
refuse() {
    local code
    code=$(clone_to "$2" "$3")
    check_eq "$1: 画面に戻る (200)" "200" "$code"
    check_eq "  理由が出る" "yes" "$(said "$4")"
    check_eq "  何も書かない" "0|" "$(value_of "$(dst exists "$2")" exists)|$(children_of "$2")"
}
refuse "URL でない" Dst/Bad1 "クローン元" 'NextFormのページのURLを入力|Enter the URL of a NextForm page'
refuse "http(s) でない" Dst/Bad2 "ftp://${SRC_HOST}/?CloneTest/Md" 'NextFormのページのURLを入力|Enter the URL of a NextForm page'
refuse "ページ名が無い" Dst/Bad3 "${CLONE_SRC_URL}/" 'NextFormのページのURLを入力|Enter the URL of a NextForm page'
refuse "NextForm でない" Dst/Bad4 "${SRC_ROOT}/nf-clone-no-such-dir/?CloneTest/Md" 'NextFormのページではありません|not a page of NextForm'
refuse "ページが無い" Dst/Bad5 "${CLONE_SRC_URL}/?CloneTest/Nothing" 'クローン元のページが見つかりません|page to clone was not found'
refuse "ログインが要る" Dst/Bad6 "${CLONE_SRC_URL}/?CloneTest/Secret" 'ログインが必要|require login'
# 一覧に大きさがあるときは転送の前に断り、理由に大きさが出る。転送の途中で
# 打ち切ったとき (古い版) は大きさが分からないので出ない。
refuse "添付がサイズ上限を超える (転送の前に断る)" Dst/Big "${CLONE_SRC_URL}/?CloneTest/Big" 'big\.bin \([0-9.]+[KMGT]?i?B\)'

printf 'もとからある\n' > "$WORK/exists.txt"
sudo install -o "$SITE_OWNER" -m 644 "$WORK/exists.txt" "${CLONE_DST_SITE}/exists.txt"
dst make-text Dst/Exists markdown "${CLONE_DST_SITE}/exists.txt" > /dev/null
before=$(sha_of_body Dst/Exists)
code=$(clone_to Dst/Exists "${CLONE_SRC_URL}/?CloneTest/Md")
check_eq "同じ名前のページがある: 理由が出る" "yes" "$(said '既に存在するページです|Page already exists')"
check_eq "  元のページは変わらない" "$before" "$(sha_of_body Dst/Exists)"
check_eq "  添付も足さない" "" "$(children_of Dst/Exists)"

code=$(curl -sk -o /dev/null -w '%{http_code}' \
            "${CLONE_DST_URL}/?Dst/ByGet&option=clone&action=write&source_url=$(printf '%s' "${CLONE_SRC_URL}/?CloneTest/Md" | python3 -c 'import sys,urllib.parse;print(urllib.parse.quote(sys.stdin.read(),safe=""))')")
check_eq "GET では動かない (403)" "403" "$code"
check_eq "  何も書かない" "0" "$(value_of "$(dst exists Dst/ByGet)" exists)"

dst guest read > /dev/null
code=$(clone_to Dst/NoPermission "${CLONE_SRC_URL}/?CloneTest/Md")
check_eq "書く権限が無い: ログインを求める (401)" "401" "$code"
check_eq "  何も書かない" "0|" "$(value_of "$(dst exists Dst/NoPermission)" exists)|$(children_of Dst/NoPermission)"
dst guest write > /dev/null
echo

echo "6. クローン元が古い版 (${OLD_REF}) でも写せること"
git -C "$REPO_ROOT" archive "$OLD_REF" NextForm/app NextForm/resource | tar -x -C "$WORK"
install_code "$CLONE_SRC_SITE" "${WORK}/NextForm"
wait_opcache
list=$(curl -sk "${CLONE_SRC_URL}/?CloneTest/Md&option=attach&action=list")
check_eq "(前提) 古い版の添付の一覧には大きさが無い" "no" \
         "$([[ "$list" == *'"size"'* ]] && echo yes || echo no)"
code=$(clone_to Dst/OldMd "${CLONE_SRC_URL}/?CloneTest/Md")
check_eq "Markdown: 保存後の移動 (302)" "302" "$code"
check_eq "  本文が同じ" "$(src_sha_of_body CloneTest/Md)" "$(sha_of_body Dst/OldMd)"
check_eq "  添付が同じ" "$(sha256sum < "$WORK/img.png" | cut -d' ' -f1)" "$(sha_of_body Dst/OldMd/img.png)"
refuse "大きすぎる添付は転送の途中で打ち切る" Dst/OldBig "${CLONE_SRC_URL}/?CloneTest/Big" 'big\.bin (が|exceeds)'
echo

echo "7. 一時ファイルを残さないこと"
check_eq "クローン先の storage/cache/ に clone-* が無い" "0" \
         "$(sudo find "${CLONE_DST_SITE}/storage/cache" -maxdepth 1 -name 'clone-*' | wc -l)"
echo

echo "8. PHP の警告が出ていないこと"
log_new=$(sudo tail -n +"$((log_before + 1))" "$PHP_ERROR_LOG" 2>/dev/null \
              | grep -F -e "$CLONE_SRC_SITE" -e "$CLONE_DST_SITE" || true)
check_eq "検証サイトの警告が増えていない" "" "$log_new"
echo

if [[ $fail -eq 0 ]]; then
    printf '%d/%d 件すべて通りました。\n' "$total" "$total"
else
    printf '%d/%d 件が失敗しました。\n' "$fail" "$total"
fi
exit $((fail == 0 ? 0 : 1))
