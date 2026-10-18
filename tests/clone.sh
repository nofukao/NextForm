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
#   6. ログインが必要なページは、そのサイトのユーザー名とパスワードを入れれば
#      写せること (ダイジェスト認証)。違えば断ること。パスワードを残さないこと
#   7. 「子ページを含む」を選ぶと、孫以下も含めて木ごと写せること。
#      「添付ファイルを含む」を外すと、どの深さの添付も写さないこと。
#      子ページを含められるのはログインした人だけであること。
#      木全体の上限 (ページ数・添付の数・合計の大きさ・取る時間) を超えたら、
#      また同時に別のクローンが動いていたら、何も書かないこと
#   8. クローン元が古い版 (添付の一覧に大きさが無い) でも写せ、大きすぎる添付は
#      転送の途中で打ち切って断ること。子ページも写せること
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
# $3 $4 クローン元のユーザー名とパスワード (省略すると空欄で送る)
clone_to() {
    curl -sk -o "$WORK/body" -D "$WORK/headers" -w '%{http_code}' -X POST \
         -H "Origin: ${DST_ORIGIN}" \
         -d "option=clone" -d "action=write" --data-urlencode "source_url=$2" \
         --data-urlencode "source_user=${3:-}" --data-urlencode "source_password=${4:-}" \
         "${CLONE_DST_URL}/?$1"
}

# チェックボックスを付けてクローンする。クローン先にはクローン用の利用者で
# ログインする (login=1 の Cookie があると、サーバーが必ずダイジェスト認証を求める)。
# $1 クローン先のページ名  $2 クローン元の URL  $3 添付 (on/off)  $4 子ページ (on/off)
# $5 $6 クローン元のユーザー名とパスワード (省略すると空欄)
# CLONE_AS_GUEST=1 のときはログインせずに送る
clone_tree_to() {
    local opts=(-d "clone_options=1")
    [[ "$3" == on ]] && opts+=(-d "clone_attachments=on")
    [[ "$4" == on ]] && opts+=(-d "clone_subpages=on")
    local login=(-b "login=1" --digest -u "clonewriter:${PASS}")
    [[ "${CLONE_AS_GUEST:-0}" == 1 ]] && login=()
    curl -sk -o "$WORK/body" -D "$WORK/headers" -w '%{http_code}' -X POST "${login[@]}" \
         -H "Origin: ${DST_ORIGIN}" \
         -d "option=clone" -d "action=write" "${opts[@]}" --data-urlencode "source_url=$2" \
         --data-urlencode "source_user=${5:-}" --data-urlencode "source_password=${6:-}" \
         "${CLONE_DST_URL}/?$1"
}

# クローン先の index.php で上限を変える (set_const より先に define する)。
# 引数が無ければ元に戻す。置いたあと PHP-FPM に載るのを待つ
set_dst_limit() {
    sudo cp "${CLONE_DST_SITE}/index.php.orig" "${CLONE_DST_SITE}/index.php"
    if [[ $# -gt 0 ]]; then
        sudo sed -i "s|^// custom settings here.*|&\ndefine('$1', $2);|" "${CLONE_DST_SITE}/index.php"
    fi
    wait_opcache
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
printf -- "# 木の根\n\n[子](./Child)\n\n![図](./t.png)\n" > "$WORK/tree.txt"
printf -- "*子のページ\n[[孫:./Grand]]\n" > "$WORK/child.txt"
printf -- "孫のページ\n" > "$WORK/grand.txt"
printf -- "木の中の読めないページ\n" > "$WORK/tsecret.txt"
printf -- "親の無い添付\n" > "$WORK/orphan.txt"
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
for f in md.txt wiki.txt text.txt sub.txt big.txt secret.txt a.txt ja.txt file.bin big.bin img.png \
         tree.txt child.txt grand.txt tsecret.txt orphan.txt; do
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
src make-file CloneTest/Secret/a.txt "${FIX}/a.txt" text/plain > /dev/null
src make-file CloneTest/Secret/img.png "${FIX}/img.png" image/png > /dev/null
# 子ページを含めて写す木。Missing は無いページで、その下の o.txt は親の無い添付
src make-text CloneTest/Tree markdown "${FIX}/tree.txt" > /dev/null
src make-file CloneTest/Tree/t.png "${FIX}/img.png" image/png > /dev/null
src make-text CloneTest/Tree/Child wiki "${FIX}/child.txt" > /dev/null
src make-file CloneTest/Tree/Child/a.txt "${FIX}/a.txt" text/plain > /dev/null
src make-text CloneTest/Tree/Child/Grand text "${FIX}/grand.txt" > /dev/null
src make-file CloneTest/Tree/Child/Grand/g.bin "${FIX}/file.bin" application/octet-stream > /dev/null
src make-text CloneTest/Tree/Secret markdown "${FIX}/tsecret.txt" > /dev/null
src make-file CloneTest/Tree/Missing/o.txt "${FIX}/orphan.txt" text/plain > /dev/null
src guest read CloneTest/Secret 'CloneTest/Secret/*' CloneTest/Tree/Secret 'CloneTest/Tree/Secret/*' > /dev/null
# ログインしてクローンするための利用者。パスワードは実行のたびに作る使い捨て
PASS=$(python3 -c 'import secrets; print(secrets.token_hex(12))')
src set-user clonereader "$PASS" read > /dev/null
src set-user clonenoread "$PASS" read CloneTest/Secret 'CloneTest/Secret/*' > /dev/null
dst guest write > /dev/null
dst set-user clonewriter "$PASS" write > /dev/null
dst set-allowed-hosts '' > /dev/null
sudo cp -p "${CLONE_DST_SITE}/index.php" "${CLONE_DST_SITE}/index.php.orig"

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

echo "6. ログインが必要なページは、ユーザー名とパスワードを入れれば写せること"
html=$(curl -sk "${CLONE_DST_URL}/?Dst/New&option=clone")
check_eq "クローンの画面にユーザー名とパスワードの欄がある" "yes|yes" \
         "$([[ "$html" == *'name="source_user"'* ]] && echo yes || echo no)|$([[ "$html" =~ type=\"password\"[^\>]*name=\"source_password\"|name=\"source_password\"[^\>]*type=\"password\" ]] && echo yes || echo no)"
# クローンのフォームの入力欄を、出てくる順に並べる (hidden は除く)
form_order=$(printf '%s' "$html" | python3 -c '
import sys
from html.parser import HTMLParser
class P(HTMLParser):
    def __init__(self):
        super().__init__(); self.inside = False; self.items = []
    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag == "form" and "clone" in (a.get("class") or "").split():
            self.inside = True
        elif tag == "input" and self.inside and a.get("type") != "hidden":
            self.items.append(a.get("name") or a.get("type"))
    def handle_endtag(self, tag):
        if tag == "form": self.inside = False
p = P(); p.feed(sys.stdin.read()); print(",".join(p.items))
')
check_eq "  「クローン」ボタンは URL の入力欄のすぐ後ろ (選択肢・ユーザー名・パスワードより前)" \
         "source_url,submit,clone_attachments,source_user,source_password" "$form_order"
check_eq "  https の画面では暗号化の注意を出さない" "no" \
         "$([[ "$html" == *'https でない'* || "$html" == *'not https'* ]] && echo yes || echo no)"

code=$(clone_to Dst/Secret "${CLONE_SRC_URL}/?CloneTest/Secret" clonereader "$PASS")
check_eq "正しいユーザー名とパスワード: 保存後の移動 (302)" "302" "$code"
check_eq "  本文が同じ" "$(src_sha_of_body CloneTest/Secret)" "$(sha_of_body Dst/Secret)"
check_eq "  添付もログインして取る" \
         "$(sha256sum < "$WORK/a.txt" | cut -d' ' -f1)|$(sha256sum < "$WORK/img.png" | cut -d' ' -f1)" \
         "$(sha_of_body Dst/Secret/a.txt)|$(sha_of_body Dst/Secret/img.png)"

code=$(clone_to Dst/SecretWrong "${CLONE_SRC_URL}/?CloneTest/Secret" clonereader "wrong-${PASS}")
check_eq "パスワードが違う: 画面に戻る (200)" "200" "$code"
check_eq "  理由が出る" "yes" "$(said 'ユーザー名かパスワードが違います|user name or password .*wrong')"
check_eq "  何も書かない" "0|" "$(value_of "$(dst exists Dst/SecretWrong)" exists)|$(children_of Dst/SecretWrong)"
check_eq "  画面に戻ったとき、ユーザー名は残し、パスワードは残さない" "yes|no" \
         "$(grep -q 'value="clonereader"' "$WORK/body" && echo yes || echo no)|$(grep -qF "$PASS" "$WORK/body" && echo yes || echo no)"

code=$(clone_to Dst/SecretNoRead "${CLONE_SRC_URL}/?CloneTest/Secret" clonenoread "$PASS")
check_eq "読む権限の無いユーザー: 理由が出る" "yes" "$(said '読む権限がありません|no permission to read')"
check_eq "  何も書かない" "0|" "$(value_of "$(dst exists Dst/SecretNoRead)" exists)|$(children_of Dst/SecretNoRead)"

code=$(clone_to Dst/TextWithLogin "${CLONE_SRC_URL}/?CloneTest/Text" clonereader "$PASS")
check_eq "ログインが要らないページに入れても写せる" "$(src_sha_of_body CloneTest/Text)" "$(sha_of_body Dst/TextWithLogin)"

check_eq "クローン先の storage にパスワードが残っていない" "" \
         "$(sudo grep -rlF "$PASS" "${CLONE_DST_SITE}/storage" 2>/dev/null)"
echo

echo "7. 子ページを含めて写すこと"
# 画面のチェックボックスを「名前=on/off」で並べる
checkboxes() {
    python3 -c '
import sys
from html.parser import HTMLParser
class P(HTMLParser):
    def __init__(self):
        super().__init__(); self.items = []
    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag == "input" and a.get("type") == "checkbox" and (a.get("name") or "").startswith("clone_"):
            self.items.append(a["name"] + "=" + ("on" if "checked" in a else "off"))
p = P(); p.feed(sys.stdin.read()); print(",".join(p.items))
'
}
# $1 のページを表示して、$2 へのリンクがあるか (exists / not_exists / none)
link_state() {
    curl -sk "${CLONE_DST_URL}/?$1" | python3 -c '
import sys
from html.parser import HTMLParser
target = sys.argv[1]
class P(HTMLParser):
    def __init__(self):
        super().__init__(); self.state = "none"
    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag == "a" and a.get("data-link-pagename") == target and self.state == "none":
            self.state = "not_exists" if "not_exists" in (a.get("class") or "").split() else "exists"
p = P(); p.feed(sys.stdin.read()); print(p.state)
' "$2"
}
sha_of() { sha256sum < "$1" | cut -d' ' -f1; }

# 強制終了で残った一時ファイルに見立てる。次のクローンが片付ける
sudo install -o "$SITE_OWNER" -m 600 /dev/null "${CLONE_DST_SITE}/storage/cache/clone-stale"

html=$(curl -sk "${CLONE_DST_URL}/?Dst/New&option=clone")
check_eq "ログインしていない人の画面: 添付は最初からオン、子ページの選択肢は出ない" \
         "clone_attachments=on" "$(printf '%s' "$html" | checkboxes)"
html=$(curl -sk -b "login=1" --digest -u "clonewriter:${PASS}" "${CLONE_DST_URL}/?Dst/New&option=clone")
check_eq "ログインした人の画面: 子ページの選択肢が出る (最初はオフ)" \
         "clone_attachments=on,clone_subpages=off" "$(printf '%s' "$html" | checkboxes)"

code=$(CLONE_AS_GUEST=1 clone_tree_to Dst/GuestTree "${CLONE_SRC_URL}/?CloneTest/Tree" on on)
check_eq "ログインしていない人が子ページを含めて送る: 画面に戻る (200)" "200" "$code"
check_eq "  理由が出る" "yes" "$(said 'ログインした人だけ|Only logged-in users')"
check_eq "  何も書かない" "0|" "$(value_of "$(dst exists Dst/GuestTree)" exists)|$(children_of Dst/GuestTree)"

code=$(clone_tree_to Dst/Tree "${CLONE_SRC_URL}/?CloneTest/Tree" on on)
check_eq "子ページと添付を含める: 保存後の移動 (302)" "302" "$code"
check_eq "  孫以下と、それぞれの添付を写す (読めないページは抜ける。親の無い添付も写す)" \
         "Dst/Tree/Child|Dst/Tree/Child/Grand|Dst/Tree/Child/Grand/g.bin|Dst/Tree/Child/a.txt|Dst/Tree/Missing/o.txt|Dst/Tree/t.png|" \
         "$(children_of Dst/Tree)"
check_eq "  本文が同じ (根・子・孫)" \
         "$(src_sha_of_body CloneTest/Tree)|$(src_sha_of_body CloneTest/Tree/Child)|$(src_sha_of_body CloneTest/Tree/Child/Grand)" \
         "$(sha_of_body Dst/Tree)|$(sha_of_body Dst/Tree/Child)|$(sha_of_body Dst/Tree/Child/Grand)"
check_eq "  種別が同じ (Markdown・wiki・text)" "markdown|wiki|text" \
         "$(value_of "$(dst meta Dst/Tree type)" meta)|$(value_of "$(dst meta Dst/Tree/Child type)" meta)|$(value_of "$(dst meta Dst/Tree/Child/Grand type)" meta)"
check_eq "  添付が同じ" \
         "$(sha_of "$WORK/img.png")|$(sha_of "$WORK/a.txt")|$(sha_of "$WORK/file.bin")|$(sha_of "$WORK/orphan.txt")" \
         "$(sha_of_body Dst/Tree/t.png)|$(sha_of_body Dst/Tree/Child/a.txt)|$(sha_of_body Dst/Tree/Child/Grand/g.bin)|$(sha_of_body Dst/Tree/Missing/o.txt)"
check_eq "  木の中どうしの ./ のリンクがつながる (Markdown・wiki)" "exists|exists" \
         "$(link_state Dst/Tree Dst/Tree/Child)|$(link_state Dst/Tree/Child Dst/Tree/Child/Grand)"

code=$(clone_tree_to Dst/TreeLogin "${CLONE_SRC_URL}/?CloneTest/Tree" on on clonereader "$PASS")
check_eq "クローン元にログインすれば、読めなかった子ページも写す" "yes" \
         "$([[ "$(children_of Dst/TreeLogin)" == *'Dst/TreeLogin/Secret|'* ]] && echo yes || echo no)"

code=$(clone_tree_to Dst/TreeNoAtt "${CLONE_SRC_URL}/?CloneTest/Tree" off on)
check_eq "子ページを含め、添付を含めない: どの深さの添付も写さない" \
         "302|Dst/TreeNoAtt/Child|Dst/TreeNoAtt/Child/Grand|" "${code}|$(children_of Dst/TreeNoAtt)"
code=$(clone_tree_to Dst/TreeOne "${CLONE_SRC_URL}/?CloneTest/Tree" on off)
check_eq "子ページを含めず、添付を含める: 直下の添付だけ (今までと同じ)" \
         "302|Dst/TreeOne/t.png|" "${code}|$(children_of Dst/TreeOne)"
code=$(clone_tree_to Dst/TreeBare "${CLONE_SRC_URL}/?CloneTest/Tree" off off)
check_eq "どちらも含めない: 本文だけ" "302|1|" \
         "${code}|$(value_of "$(dst exists Dst/TreeBare)" exists)|$(children_of Dst/TreeBare)"

# $1 説明  $2 クローン先  $3 出るはずの文言 (正規表現)
refuse_tree() {
    local code
    code=$(clone_tree_to "$2" "${CLONE_SRC_URL}/?CloneTest/Tree" on on)
    check_eq "$1: 画面に戻る (200)" "200" "$code"
    check_eq "  理由が出る" "yes" "$(said "$3")"
    check_eq "  何も書かない" "0|" "$(value_of "$(dst exists "$2")" exists)|$(children_of "$2")"
}
dst make-text Dst/TreeDup/Child/Grand text "${CLONE_DST_SITE}/exists.txt" > /dev/null
code=$(clone_tree_to Dst/TreeDup "${CLONE_SRC_URL}/?CloneTest/Tree" on on)
check_eq "子孫の写す先に同じ名前のページがある: 理由に名前が出る" "yes" "$(said 'Dst/TreeDup/Child/Grand')"
check_eq "  何も書かない (もとからあったページだけ)" "0|Dst/TreeDup/Child/Grand|" \
         "$(value_of "$(dst exists Dst/TreeDup)" exists)|$(children_of Dst/TreeDup)"

set_dst_limit CLONE_MAX_PAGES 1
refuse_tree "子ページの数が上限を超える" Dst/LimPages '子ページが多すぎます|too many subpages'
set_dst_limit CLONE_MAX_TOTAL_ATTACHMENTS 3
refuse_tree "添付の総数が上限を超える" Dst/LimFiles '全部で多すぎます|too many attached files in total'
set_dst_limit CLONE_MAX_TOTAL_BYTES 2000
refuse_tree "合計の大きさが上限を超える" Dst/LimBytes '合計で大きすぎます|too large in total'
set_dst_limit CLONE_MAX_FETCH_SECONDS 0
refuse_tree "取るのに時間がかかりすぎる" Dst/LimTime '時間がかかりすぎました|took too long'
set_dst_limit

# 別のクローンが動いているあいだ (ロックを持っているあいだ) は断る
sudo flock "${CLONE_DST_SITE}/storage/cache/clone.lock" sleep 5 &
holder=$!
sleep 1
refuse_tree "同時に別のクローンが動いている" Dst/Busy 'ほかのクローンが動いています|Another clone is in progress'
wait "$holder" 2>/dev/null

check_eq "強制終了で残った一時ファイルを、次のクローンが片付ける" "0" \
         "$(sudo find "${CLONE_DST_SITE}/storage/cache" -maxdepth 1 -name 'clone-stale' | wc -l)"
echo

echo "8. クローン元が古い版 (${OLD_REF}) でも写せること"
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
code=$(clone_to Dst/OldSecret "${CLONE_SRC_URL}/?CloneTest/Secret" clonereader "$PASS")
check_eq "ログインが必要なページも、ユーザー名とパスワードで写せる" \
         "$(src_sha_of_body CloneTest/Secret)|$(sha256sum < "$WORK/a.txt" | cut -d' ' -f1)" \
         "$(sha_of_body Dst/OldSecret)|$(sha_of_body Dst/OldSecret/a.txt)"
code=$(clone_tree_to Dst/OldTree "${CLONE_SRC_URL}/?CloneTest/Tree" on on)
check_eq "子ページも含めて写せる" \
         "302|Dst/OldTree/Child|Dst/OldTree/Child/Grand|Dst/OldTree/Child/Grand/g.bin|Dst/OldTree/Child/a.txt|Dst/OldTree/Missing/o.txt|Dst/OldTree/t.png|" \
         "${code}|$(children_of Dst/OldTree)"
echo

echo "9. 一時ファイルを残さないこと"
check_eq "クローン先の storage/cache/ に clone-* が無い" "0" \
         "$(sudo find "${CLONE_DST_SITE}/storage/cache" -maxdepth 1 -name 'clone-*' | wc -l)"
echo

echo "10. PHP の警告が出ていないこと"
log_new=$(sudo tail -n +"$((log_before + 1))" "$PHP_ERROR_LOG" 2>/dev/null \
              | grep -F -e "$CLONE_SRC_SITE" -e "$CLONE_DST_SITE" || true)
check_eq "検証サイトの警告が増えていない" "" "$log_new"
check_eq "PHP のエラーログにパスワードが出ていない" "" \
         "$(sudo tail -n +"$((log_before + 1))" "$PHP_ERROR_LOG" 2>/dev/null | grep -F "$PASS" || true)"
echo

if [[ $fail -eq 0 ]]; then
    printf '%d/%d 件すべて通りました。\n' "$total" "$total"
else
    printf '%d/%d 件が失敗しました。\n' "$fail" "$total"
fi
exit $((fail == 0 ? 0 : 1))
