# 標準入力の HTML から設定フォームの入力欄を取り出し、
# `名前=base64(値)` の行で出す。tests/theme-tone.sh が使う。
#
# ブラウザが送るものと同じ内容を組み立てるためのもので、
# ファイル選択・チェックボックス・送信ボタンは送らない。
#
# 引数に select の名前を渡すと、代わりにその選択肢の値を上から順に出す。
import base64
import re
import sys

html = sys.stdin.read()

if len(sys.argv) > 1:
    select = re.search(r'<select name="%s">(.*?)</select>' % re.escape(sys.argv[1]),
                       html, re.S)
    if select:
        for value in re.findall(r'<option value="([^"]*)"', select.group(1)):
            if value != '':
                print(value)
    sys.exit(0)

values = []

for match in re.finditer(r'<input\b[^>]*>', html):
    tag = match.group(0)
    name = re.search(r'name="([^"]*)"', tag)
    if not name or not name.group(1).startswith('const_'):
        continue
    if re.search(r'type="(file|checkbox|submit)"', tag):
        continue
    value = re.search(r'value="([^"]*)"', tag)
    values.append((name.group(1), value.group(1) if value else ''))

for match in re.finditer(r'<select\b[^>]*>.*?</select>', html, re.S):
    tag = match.group(0)
    name = re.search(r'name="([^"]*)"', tag)
    if not name or not name.group(1).startswith('const_'):
        continue
    selected = re.search(r'<option value="([^"]*)"[^>]*selected', tag)
    if not selected:
        selected = re.search(r'<option value="([^"]*)"', tag)
    values.append((name.group(1), selected.group(1) if selected else ''))

for name, value in values:
    print('%s=%s' % (name, base64.b64encode(value.encode()).decode()))
