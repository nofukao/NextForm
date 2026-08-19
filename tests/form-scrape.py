# 標準入力の HTML から設定フォームの入力欄を取り出し、
# `名前=base64(値)` の行で出す。tests/theme-tone.sh が使う。
#
# ブラウザが送るものと同じ内容を組み立てるためのもので、
# ファイル選択・チェックボックス・送信ボタンは送らない。
import base64
import re
import sys

html = sys.stdin.read()
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
