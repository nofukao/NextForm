#!/usr/bin/env python3
# gen-mockup.sh から呼ばれる。配信中の HTML を、モックアップ用に書き換える。
#
#   gen-mockup.py <入力 html> <出力 html> <テーマ> <ページ名> <見出し> \
#                 "<テーマ一覧>" "<トーン一覧>" "<ページ定義一覧>"
#   gen-mockup.py --index <出力 html> "<テーマ一覧>" "<トーン一覧>" "<ページ定義一覧>"
#
# やること:
#   ・読み込む CSS と JS の URL を、そのテーマのものに差し替える
#   ・トーンを切り替えられるように、CSS を id つきの link にする
#   ・テーマ / トーン / ページを行き来する小さなバーを足す
import re
import sys


def tone_id(tone):
    return tone.replace('/', '-')


def parse_pages(spec):
    out = []
    for entry in spec.split():
        name, query, title = entry.split('|')
        out.append((name, query, title))
    return out


BAR_STYLE = '''
#mockbar {
    position: fixed; left: 0; right: 0; bottom: 0; z-index: 9999;
    background: #16181c; color: #e8e8e8; font: 12px/1.6 system-ui, sans-serif;
    padding: 6px 10px; display: flex; gap: 18px; flex-wrap: wrap; align-items: center;
    box-shadow: 0 -1px 6px rgba(0,0,0,.35);
}
#mockbar b { color: #9aa0a6; font-weight: normal; margin-right: 4px; }
#mockbar a, #mockbar button {
    color: #e8e8e8; background: #2a2e35; border: none; border-radius: 4px;
    padding: 3px 9px; margin-right: 4px; text-decoration: none; cursor: pointer;
    font: inherit;
}
#mockbar a.on, #mockbar button.on { background: #4c8bf5; color: #fff; }
#mockbar a:hover, #mockbar button:hover { background: #3a4049; }
body { padding-bottom: 44px !important; }
'''


def bar(theme, page, themes, tones, pages):
    parts = ['<div id="mockbar">']
    parts.append('<span><b>テーマ</b>')
    for t in themes:
        cls = ' class="on"' if t == theme else ''
        parts.append(f'<a href="../{t}/{page}.html"{cls}>{t}</a>')
    parts.append('</span>')

    parts.append('<span><b>色調</b>')
    for i, tone in enumerate(tones):
        cls = ' class="on"' if i == 0 else ''
        parts.append(
            f'<button{cls} onclick="mockTone(this, \'{tone_id(tone)}\')">{tone}</button>')
    parts.append('</span>')

    parts.append('<span><b>ページ</b>')
    for name, _query, title in pages:
        cls = ' class="on"' if name == page else ''
        parts.append(f'<a href="{name}.html"{cls}>{title}</a>')
    parts.append('</span>')

    parts.append('<span><b></b><a href="../index.html">一覧へ</a></span>')
    parts.append('</div>')
    parts.append(f'''<script>
function mockTone(button, tone) {{
    document.getElementById('mock-css').href =
        'theme/{theme}/style/main-' + tone + '.css';
    var buttons = button.parentNode.getElementsByTagName('button');
    for(var i = 0; i < buttons.length; i++) buttons[i].className = '';
    button.className = 'on';
}}
</script>''')
    return '\n'.join(parts)


def build_page(src, dst, theme, page, _title, themes, tones, pages):
    html = open(src, encoding='utf-8').read()

    # 配信元のテーマ名 (通常 basic) を、このテーマのものへ差し替える
    html = re.sub(r'theme/[^/"]+/', f'theme/{theme}/', html)

    # CSS はトーンを差し替えられるように id を付ける
    first = tone_id(tones[0])
    html = re.sub(
        r'<link rel="stylesheet" type="text/css" href="theme/[^"]*main\.css[^"]*"[^>]*/?>',
        f'<link id="mock-css" rel="stylesheet" type="text/css" '
        f'href="theme/{theme}/style/main-{first}.css" media="all" />',
        html, count=1)

    html = html.replace('</head>', f'<style>{BAR_STYLE}</style>\n</head>', 1)
    html = html.replace('</body>', bar(theme, page, themes, tones, pages) + '\n</body>', 1)
    open(dst, 'w', encoding='utf-8').write(html)


def build_index(dst, themes, tones, pages):
    rows = []
    for t in themes:
        links = ' '.join(
            f'<a href="{t}/{name}.html">{title}</a>' for name, _q, title in pages)
        rows.append(f'<tr><th><a href="{t}/index.html">{t}</a></th><td>{links}</td></tr>')
    tone_list = '、'.join(tones)
    page_list = '\n'.join(rows)
    open(dst, 'w', encoding='utf-8').write(f'''<!DOCTYPE html>
<html lang="ja"><head><meta charset="utf-8" />
<meta name="viewport" content="width=device-width" />
<title>NextForm テーマ候補</title>
<style>
body {{ font: 15px/1.9 system-ui, "Noto Sans JP", sans-serif; margin: 40px auto; max-width: 780px;
       padding: 0 20px; color: #1c1f23; background: #fafafa; }}
h1 {{ font-size: 22px; margin-bottom: 4px; }}
p.note {{ color: #5c6370; margin-top: 0; }}
table {{ border-collapse: collapse; width: 100%; margin-top: 24px; }}
th, td {{ border-bottom: 1px solid #dcdfe4; padding: 12px 8px; text-align: left; vertical-align: top; }}
th {{ width: 6em; font-size: 17px; }}
a {{ color: #1a56c4; text-decoration: none; margin-right: 12px; }}
a:hover {{ text-decoration: underline; }}
dl {{ margin-top: 28px; }} dt {{ font-weight: bold; margin-top: 12px; }}
dd {{ margin: 0 0 0 1.2em; color: #40454c; }}
</style></head><body>
<h1>NextForm テーマ候補</h1>
<p class="note">実際の wiki の出力に、テーマごとの CSS を当てたもの。
各ページ下のバーでテーマ・色調・ページを切り替えられる。色調は {tone_list} を用意した。</p>
<table>{page_list}</table>
<dl>
<dt>basic</dt><dd>現行。比較の基準。</dd>
<dt>plain</dt><dd>線と余白。塗りをやめ、細い線と広い行間で読ませる。</dd>
<dt>docs</dt><dd>技術文書。本文の幅を止め、見出しの深さで階層を示し、サイドを画面に留める。</dd>
<dt>dense</dt><dd>高密度。行間と余白を詰めて 1 画面に多く入れる。議事録・作業ログ向け。</dd>
<dt>card</dt><dd>ノート風。本文とサイドをカードに載せ、角を丸めて浮かせる。</dd>
</dl>
</body></html>''')


if __name__ == '__main__':
    if sys.argv[1] == '--index':
        build_index(sys.argv[2], sys.argv[3].split(), sys.argv[4].split(),
                    parse_pages(sys.argv[5]))
    else:
        build_page(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5],
                   sys.argv[6].split(), sys.argv[7].split(), parse_pages(sys.argv[8]))
