#!/bin/bash
# 添付画面からのダウンロードのテスト
#
#   ./tests/attach-download.sh
#
# 環境変数:
#   NF_SITE                    複製元にする NextForm インスタンス (既定: /var/www/html/nextform)
#   ATTACH_DOWNLOAD_TEST_SITE  検証用に作るサイト (既定: /var/www/html/nf-attach-download-test)
#   ATTACH_DOWNLOAD_TEST_URL   その URL           (既定: http://localhost/nf-attach-download-test)
#   WIKI_ADMIN                 管理者ユーザー名   (既定: admin)
#   KEEP=1                     終了後に検証サイトを消さない
#
# 添付画面 (?ページ&option=attach) の「添付ファイル」の一覧で、ファイルに
# チェックを付けて「ダウンロード」を押すと、1 つならそのファイルを、2 つ以上なら
# ZIP 1 つにまとめて返す。ZIP は zip 拡張を使わずに NextForm が組み立てる
# (project-overview.md §7)。ここで固定するのは次のとおり:
#
#   1. 一覧は「チェックボックス | 添付ファイル名 | 保存日時 | サイズ」の表で、
#      読めないファイルは出ないこと
#   2. 2 つ以上なら ZIP になり、名前 (日本語を含む) と中身が元のファイルと一致すること
#   3. 1 つならそのファイルが元のバイト列のまま返ること (画像も縮小しない)
#   4. 何も選ばないとき、このページの添付でないものを選んだときは返さないこと
#   5. 読めないファイルを選んだら、要求ごと断ること
#   6. GET では返さないこと (CSRF 対策の対象)
#   7. フォームの送り方を Base64 にしたサイトでも、包んだ要求で返ること
#
# 見出しの行の「全部を選ぶ」チェックボックスは JavaScript が差し込むので、
# ブラウザで確かめる。ここでは見出しの先頭の欄が空で、並べ替えの対象から
# 外れている (nosort) ことだけを見る。
#
# 権限を書き換えるので、必ず複製したサイトに対して実行する。
# 複製元には触らない。root で実行する必要がある。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

[[ -f "${REPO_ROOT}/tests/env.local" ]] && . "${REPO_ROOT}/tests/env.local"
NF_SITE="${NF_SITE:-/var/www/html/nextform}"
ATTACH_DOWNLOAD_TEST_SITE="${ATTACH_DOWNLOAD_TEST_SITE:-/var/www/html/nf-attach-download-test}"
ATTACH_DOWNLOAD_TEST_URL="${ATTACH_DOWNLOAD_TEST_URL:-http://localhost/nf-attach-download-test}"
WIKI_ADMIN="${WIKI_ADMIN:-admin}"
PHP_ERROR_LOG="${PHP_ERROR_LOG:-/var/log/php-fpm/www-error.log}"

SITE="$ATTACH_DOWNLOAD_TEST_SITE"
URL="$ATTACH_DOWNLOAD_TEST_URL"

fail=0
total=0
WORK="$(mktemp -d)"

cleanup() {
    rm -rf "$WORK"
    if [[ "${KEEP:-0}" != "1" ]]; then
        sudo rm -rf "$SITE" 2>/dev/null
    else
        echo
        echo "KEEP=1 のため検証サイトを残しました: $SITE"
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
        "${SITE}/attach-download-helper.php" \
        "${SITE}/index.php" "$WIKI_ADMIN" "$@" 2>/dev/null
}

value_of() {
    printf '%s\n' "$1" | sed -n "s/^$2=//p" | head -1
}

b64() {
    printf '%s' "$1" | base64 -w0
}

# 応答のヘッダの値 (大文字小文字を区別しない)。$1 ヘッダのファイル  $2 名前
header_of() {
    sed -n "s/^$2: *//Ip" "$1" | tr -d '\r' | tail -1
}

# ダウンロードを POST する。応答の本文を $WORK/body、ヘッダを $WORK/headers に置き、
# 応答コードを返す。$1 以降は curl の引数
download() {
    curl -sk -o "$WORK/body" -D "$WORK/headers" -w '%{http_code}' -X POST \
         -H "Origin: ${ORIGIN}" "$@" "${URL}/?${P_PAGE}"
}

# 本文が ZIP なら、中の名前と中身の sha256 を「名前<TAB>sha256」で並べる。
# CRC も検査する (壊れていれば BAD を出す)。
zip_listing() {
    python3 - "$WORK/body" <<'PYEOF'
import hashlib, sys, zipfile
try:
    z = zipfile.ZipFile(sys.argv[1])
except zipfile.BadZipFile:
    print('NOT A ZIP')
    sys.exit(0)
bad = z.testzip()
if bad is not None:
    print('BAD CRC ' + bad)
for info in z.infolist():
    print('%s\t%s' % (info.filename, hashlib.sha256(z.read(info)).hexdigest()))
PYEOF
}

sha_of() {
    sha256sum "$1" | cut -d' ' -f1
}

if [[ ! -d "$NF_SITE" ]]; then
    echo "複製元がありません: $NF_SITE" >&2
    echo "tests/env.local の NF_SITE を設定してください。" >&2
    exit 1
fi

echo "複製元 = $NF_SITE"
echo "検証先 = $SITE"
echo "URL    = $URL"
echo

sudo rm -rf "$SITE"
sudo cp -a "$NF_SITE" "$SITE"
SITE_OWNER=$(sudo stat -c '%U' "${SITE}/index.php")

# 複製元に配置済みのコードではなく、リポジトリの作業ツリーを検証する
sudo rsync -a --delete "${REPO_ROOT}/NextForm/app/"      "${SITE}/app/"
sudo rsync -a --delete "${REPO_ROOT}/NextForm/resource/" "${SITE}/resource/"
sudo cp "${REPO_ROOT}/tests/attach-download-helper.php" "${SITE}/"
sudo chown -R "$SITE_OWNER" "${SITE}/app" "${SITE}/resource" \
                            "${SITE}/attach-download-helper.php"

# 置いたコードが PHP-FPM に載るのを待つ (tests/markdown.sh と同じ理由)
OPCACHE_FREQ=$(php -r 'echo (int)ini_get("opcache.revalidate_freq");' 2>/dev/null)
sleep $(( ${OPCACHE_FREQ:-2} + 1 ))

if [[ "$(value_of "$(helper guest-write)" saved)" != "1" ]]; then
    echo "ログインしていない利用者に write 権限を与えられませんでした。" >&2
    exit 1
fi
helper set-encoding '' > /dev/null

if [[ "$(curl -sk -o /dev/null -w '%{http_code}' "${URL}/")" != "200" ]]; then
    echo "検証サイトが $URL で見えません。" >&2
    echo "tests/env.local の ATTACH_DOWNLOAD_TEST_URL を設定してください。" >&2
    exit 1
fi

helper cleanup > /dev/null

ORIGIN=$(printf '%s' "$URL" | sed -E 's#^(https?://[^/]+).*#\1#')
log_before=$(sudo wc -l "$PHP_ERROR_LOG" 2>/dev/null | awk '{print $1}')
log_before="${log_before:-0}"

P_PAGE="AttachDownloadTest/Page"
P_OTHER="AttachDownloadTest/Other"
JA_NAME="日本語 の名前.txt"

# --- 準備: ページを作り、ファイルを添付する ---------------------------------
printf "テキスト '--aa'\n" > "$WORK/a.txt"
printf "日本語の名前のファイル\n" > "$WORK/ja.txt"
printf "読めないファイル\n" > "$WORK/secret.txt"
printf "よそのページの添付\n" > "$WORK/other.txt"
# 縮小がかかる大きさの PNG (PAGE_FILE_IMAGE_RESIZE は 640x640)
python3 - "$WORK/image.png" <<'PYEOF'
import struct, sys, zlib
w, h = 800, 700
raw = b''.join(b'\x00' + bytes((x * 7 + y * 3) % 256 for x in range(w * 3)) for y in range(h))
def chunk(t, d):
    return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) \
    + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')
open(sys.argv[1], 'wb').write(png)
PYEOF

for p in "$P_PAGE" "$P_OTHER"; do
    curl -sk -o /dev/null -X POST -H "Origin: ${ORIGIN}" -d "action=write" \
         --data-urlencode "contents=添付の置き場" "${URL}/?${p}"
done
curl -sk -o /dev/null -X POST -H "Origin: ${ORIGIN}" \
     -F "option=attach" -F "action=write" \
     -F "files[]=@${WORK}/a.txt;type=text/plain" \
     -F "files[]=@${WORK}/ja.txt;type=text/plain;filename=${JA_NAME}" \
     -F "files[]=@${WORK}/image.png;type=image/png" \
     -F "files[]=@${WORK}/secret.txt;type=text/plain" \
     "${URL}/?${P_PAGE}"
curl -sk -o /dev/null -X POST -H "Origin: ${ORIGIN}" \
     -F "option=attach" -F "action=write" \
     -F "files[]=@${WORK}/other.txt;type=text/plain" \
     "${URL}/?${P_OTHER}"
# secret.txt だけ、ログインしていない利用者から読めなくする
helper guest-write "${P_PAGE}/secret.txt" > /dev/null

echo "1. 一覧の各ファイルにチェックボックスが付くこと"
attach_html=$(curl -sk "${URL}/?${P_PAGE}&option=attach")
boxes=$(printf '%s' "$attach_html" | grep -o '<input[^>]*name="download\[\]"[^>]*>' \
            | sed -E 's/.*value="([^"]*)".*/\1/' | sort | tr '\n' '|')
check_eq "読めるファイルにだけチェックボックスがある" \
         "$(printf '%s\n' a.txt image.png "$JA_NAME" | sort | tr '\n' '|')" "$boxes"
# チェックボックスを囲むフォームの method と hidden、送信ボタンを並べる
form_info=$(printf '%s' "$attach_html" | python3 -c '
import sys
from html.parser import HTMLParser
class P(HTMLParser):
    def __init__(self):
        super().__init__(); self.forms = []; self.cur = None
    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag == "form":
            self.cur = {"method": (a.get("method") or "").upper(), "inputs": []}
            self.forms.append(self.cur)
        elif tag == "input" and self.cur is not None:
            self.cur["inputs"].append(a)
    def handle_endtag(self, tag):
        if tag == "form": self.cur = None
p = P(); p.feed(sys.stdin.read())
for f in p.forms:
    if any(i.get("name") == "download[]" for i in f["inputs"]):
        h = {i.get("name"): i.get("value") for i in f["inputs"] if i.get("type") == "hidden"}
        s = [i.get("name") for i in f["inputs"] if i.get("type") == "submit"]
        print("method=%s option=%s action=%s submit=%s" % (f["method"], h.get("option"), h.get("action"), ",".join(s)))
')
check_eq "チェックボックスはダウンロードのフォームの中にあり、POST で送る" \
         "method=POST option=attach action=download submit=download_files" "$form_info"

# ダウンロードのフォームの中の表を「見出し」と「行ごとのセル」に分けて出す。
# 1 列目はチェックボックスの値、2 列目はリンクの文字、ほかはセルの文字。
table_info=$(printf '%s' "$attach_html" | python3 -c '
import sys
from html.parser import HTMLParser
class P(HTMLParser):
    def __init__(self):
        super().__init__()
        self.form = False; self.table = False; self.cell = None
        self.heads = []; self.rows = []
    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag == "form" and "attach_download" in (a.get("class") or "").split():
            self.form = True
        elif self.form and tag == "table":
            self.table = True
        elif self.table and tag == "tr":
            self.row = []
        elif self.table and tag in ("th", "td"):
            self.cell = ""
        elif self.cell is not None and tag == "input" and a.get("type") == "checkbox":
            self.cell += "[x:%s]" % a.get("value")
        elif self.cell is not None and tag == "a":
            self.cell += "[a:"
    def handle_endtag(self, tag):
        if tag == "form": self.form = False
        elif tag == "table": self.table = False
        elif self.cell is not None and tag == "a":
            self.cell += "]"
        elif self.cell is not None and tag in ("th", "td"):
            self.row.append(self.cell.strip()); self.cell = None
        elif self.table and tag == "tr":
            (self.heads if not self.rows and not any(c.startswith("[x:") for c in self.row) and not self.heads else self.rows).append(self.row)
    def handle_data(self, data):
        if self.cell is not None: self.cell += data
p = P(); p.feed(sys.stdin.read())
for h in p.heads: print("head\t" + "\t".join(h))
for r in p.rows: print("row\t" + "\t".join(r))
')
check_eq "表の見出しは「(空) | 添付ファイル名 | 保存日時 | サイズ」" "yes" \
         "$(printf '%s\n' "$table_info" | grep -qE $'^head\t\t(添付ファイル名|Attached file name)\t(保存日時|Saved at)\t(サイズ|Size)$' && echo yes || echo no)"
# 見出しの先頭の欄には JavaScript が「全部を選ぶ」チェックボックスを差し込む。
# この欄を押しても並べ替えないよう nosort を付ける (nextform.js の tableSortSetup())。
check_eq "  見出しの先頭の欄は並べ替えの対象にしない (nosort)" "yes" \
         "$(printf '%s' "$attach_html" | grep -q '<thead><tr><th class="nosort"></th><th>' && echo yes || echo no)"
expected_rows=""
for name in a.txt image.png "$JA_NAME"; do
    case "$name" in
        a.txt)     size=$(stat -c %s "$WORK/a.txt") ;;
        image.png) size=$(stat -c %s "$WORK/image.png") ;;
        *)         size=$(stat -c %s "$WORK/ja.txt") ;;
    esac
    mtime=$(value_of "$(helper mtime-text "${P_PAGE}/${name}")" mtime)
    expected_rows+=$(printf 'row\t[x:%s]\t[a:%s]\t%s\t%s\n' "$name" "$name" "$mtime" "$(python3 -c 'import sys; print("{:,}".format(int(sys.argv[1])))' "$size")")
    expected_rows+=$'\n'
done
check_eq "各行はチェックボックス・ファイル名 (リンク)・保存日時・バイト数 (桁区切り)" \
         "$(printf '%s' "$expected_rows" | sort)" "$(printf '%s\n' "$table_info" | grep '^row' | sort)"
echo

echo "2. 2 つ以上なら ZIP になり、名前と中身が元のファイルと一致すること"
code=$(download -d "option=attach" -d "action=download" \
                --data-urlencode "download[]=a.txt" \
                --data-urlencode "download[]=${JA_NAME}" \
                --data-urlencode "download[]=image.png")
check_eq "応答は 200" "200" "$code"
check_eq "  Content-Type は application/zip" "application/zip" "$(header_of "$WORK/headers" Content-Type)"
check_eq "  ダウンロードとして返し、ZIP の名前はページ名の末尾" \
         "attachment; filename=\"Page.zip\"; filename*=UTF-8''Page.zip" \
         "$(header_of "$WORK/headers" Content-Disposition)"
# web サーバによっては Content-Length を落として chunked で返す (この開発環境の
# Apache + PHP-FPM がそう)。出す値 (zip_prepare()) を CLI で取って本文と比べ、
# ヘッダが届いていればそれも比べる。
body_size=$(stat -c %s "$WORK/body")
check_eq "  ZIP の大きさの計算 (Content-Length に出す値) が本文と合う" "$body_size" \
         "$(value_of "$(helper zip-length "$P_PAGE" a.txt "$JA_NAME" image.png)" length)"
content_length=$(header_of "$WORK/headers" Content-Length)
check_eq "  Content-Length が届いていれば本文と合う" "$body_size" "${content_length:-$body_size}"
check_eq "  中の名前と中身が元のファイルと一致する (CRC も正しい)" \
         "$(printf 'a.txt\t%s\n%s\t%s\nimage.png\t%s' "$(sha_of "$WORK/a.txt")" "$JA_NAME" "$(sha_of "$WORK/ja.txt")" "$(sha_of "$WORK/image.png")")" \
         "$(zip_listing)"
echo

echo "3. 1 つならそのファイルが元のバイト列のまま返ること"
code=$(download -d "option=attach" -d "action=download" --data-urlencode "download[]=image.png")
check_eq "応答は 200" "200" "$code"
check_eq "  ダウンロードとして返す" \
         "attachment; filename=\"image.png\"; filename*=UTF-8''image.png" \
         "$(header_of "$WORK/headers" Content-Disposition)"
check_eq "  画像も縮小せず元のバイト列のまま" "$(sha_of "$WORK/image.png")" "$(sha_of "$WORK/body")"
code=$(download -d "option=attach" -d "action=download" --data-urlencode "download[]=${JA_NAME}")
check_eq "日本語の名前のファイルも返す" "$(sha_of "$WORK/ja.txt")" "$(sha_of "$WORK/body")"
check_eq "  名前は UTF-8 で伝える" \
         "attachment; filename=\"_______.txt\"; filename*=UTF-8''%E6%97%A5%E6%9C%AC%E8%AA%9E%20%E3%81%AE%E5%90%8D%E5%89%8D.txt" \
         "$(header_of "$WORK/headers" Content-Disposition)"
echo

echo "4. 何も選ばないとき、このページの添付でないものを選んだときは返さないこと"
code=$(download -d "option=attach" -d "action=download")
check_eq "何も選ばない: 画面に戻る (200)" "200" "$code"
check_eq "  ダウンロードにならない" "" "$(header_of "$WORK/headers" Content-Disposition)"
check_eq "  案内が出る" "yes" \
         "$(grep -qE 'ダウンロードするファイルを選んでください|Select the files to download' "$WORK/body" && echo yes || echo no)"
for bad in "../Other/other.txt" "Other/other.txt" "nothere.txt" "."; do
    code=$(download -d "option=attach" -d "action=download" \
                    --data-urlencode "download[]=a.txt" --data-urlencode "download[]=${bad}")
    check_eq "「${bad}」を混ぜる: ダウンロードにならない" "" "$(header_of "$WORK/headers" Content-Disposition)"
    check_eq "  見つからないと出る" "yes" \
             "$(grep -qE '選んだファイルが見つかりません|The selected file was not found' "$WORK/body" && echo yes || echo no)"
done
echo

echo "5. 読めないファイルを選んだら、要求ごと断ること"
code=$(download -d "option=attach" -d "action=download" \
                --data-urlencode "download[]=a.txt" --data-urlencode "download[]=secret.txt")
check_eq "ログインを求められる (401)" "401" "$code"
check_eq "  ZIP にならない" "NOT A ZIP" "$(zip_listing)"
echo

echo "6. GET では返さないこと"
code=$(curl -sk -o "$WORK/body" -D "$WORK/headers" -w '%{http_code}' \
            "${URL}/?${P_PAGE}&option=attach&action=download&download%5B%5D=a.txt&download%5B%5D=image.png")
check_eq "GET は 403" "403" "$code"
check_eq "  ダウンロードにならない" "" "$(header_of "$WORK/headers" Content-Disposition)"
echo

echo "7. フォームの送り方を Base64 にしたサイトでも返すこと"
check_eq "(準備) 設定を base64 にする" "1" "$(value_of "$(helper set-encoding base64)" saved)"
code=$(download --data-urlencode "option=$(b64 attach)" --data-urlencode "action=$(b64 download)" \
                --data-urlencode "download[]=$(b64 a.txt)" \
                --data-urlencode "download[]=$(b64 "$JA_NAME")" \
                --data-urlencode "download_files=$(b64 'ダウンロード')" \
                -d "post_encoding=base64")
check_eq "応答は 200" "200" "$code"
check_eq "  中の名前と中身が元のファイルと一致する" \
         "$(printf 'a.txt\t%s\n%s\t%s' "$(sha_of "$WORK/a.txt")" "$JA_NAME" "$(sha_of "$WORK/ja.txt")")" \
         "$(zip_listing)"
helper set-encoding '' > /dev/null
echo

echo "8. PHP の警告が出ていないこと"
log_new=$(sudo tail -n +"$((log_before + 1))" "$PHP_ERROR_LOG" 2>/dev/null | grep -F "$SITE" || true)
check_eq "この検証サイトの警告が増えていない" "" "$log_new"
echo

helper cleanup > /dev/null

if [[ $fail -eq 0 ]]; then
    printf '%d/%d 件すべて通りました。\n' "$total" "$total"
else
    printf '%d/%d 件が失敗しました。\n' "$fail" "$total"
fi
exit $((fail == 0 ? 0 : 1))
