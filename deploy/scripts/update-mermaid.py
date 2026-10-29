#!/usr/bin/env python3
"""
同梱する Mermaid を取り直して NextForm/resource/script/mermaid/ を作り直す。
deploy/scripts/update-mermaid.sh から呼ぶ (直接呼んでもよい)。

    update-mermaid.py                 mermaid.lock の版を取り直す
    update-mermaid.py --pin 12.3.0    版を変える (mermaid.lock を書き換える)

置くもの:
    mermaid.min.js            npm の mermaid の dist/mermaid.min.js (手を加えない)
    LICENSE                   Mermaid の MIT
    THIRD-PARTY-LICENSES.txt  mermaid.min.js に組み込まれている部品の一覧と条文

**部品の一覧は自前で作る。** npm の mermaid は Mermaid 自身の LICENSE しか持たず、
mermaid.min.js にも条文がほとんど残っていない。同じ tgz の非圧縮版 dist/mermaid.js に
残っている元のパスの注記 (// ../../node_modules/.pnpm/<名前>@<版>/...) と
__commonJS のキーから拾う。拾えないものが 2 つあり、別に取りに行く:

  - @mermaid-js/parser は組み込み済みの形で入っていて、中の langium などに注記が無い。
    Mermaid の同じタグのソースから parser の版を引き、その tgz の注記から拾う。
  - elkjs は Java (ELK) を GWT で JavaScript にしたもので、EMF・Xtext の実行用
    ライブラリ・Guava・GWT を中に組み込んでいる。elkjs の同じタグの build.gradle から
    版を引き、Maven Central の POM でライセンスを確かめる。

**許すライセンスの表に無いものがあれば止まる。** 原則は GPLv3 と両立するもの。
EPL は elkjs とその中の部品に限って認める (CLAUDE.md の同梱物の規約、
docs/development/design-markdown-mermaid.md §2.6・§3.7)。

ダウンロードは curl で行う。urllib はこの開発機で 1 件 30 秒かかった (IPv6 を待つ)。
"""

import argparse
import base64
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import xml.etree.ElementTree as ElementTree

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
LOCK_PATH = os.path.join(REPO_ROOT, 'deploy', 'scripts', 'mermaid.lock')
DEST = os.path.join(REPO_ROOT, 'NextForm', 'resource', 'script', 'mermaid')

REGISTRY = 'https://registry.npmjs.org'
MAVEN = 'https://repo1.maven.org/maven2'
SPDX_TEXT = 'https://raw.githubusercontent.com/spdx/license-list-data/main/text/'

# GPLv3 と両立するもの。SPDX の OR は、どれか 1 つがここにあればよい
ALLOWED = {'MIT', 'ISC', 'BSD-2-Clause', 'BSD-3-Clause', 'Apache-2.0', 'MPL-2.0', '0BSD'}
# 規約で例外として認めたもの (elkjs とその中の部品だけ)
EPL = {'EPL-1.0', 'EPL-2.0'}

# 元のパスの注記と __commonJS のキーに出る pnpm の置き場所
PNPM_PATTERN = re.compile(
    r'node_modules/\.pnpm/([^/"\s]+)/node_modules/((?:@[^/"\s]+/)?[^/"\s]+)/')


def fail(message):
    print('止めます: ' + message, file=sys.stderr)
    sys.exit(1)


def curl(url, dest=None):
    command = ['curl', '-fsSL', '--retry', '2', '--max-time', '120', url]
    if dest is not None:
        command[1:1] = ['-o', dest]
    result = subprocess.run(command, capture_output=True)
    if result.returncode != 0:
        fail('取れません: %s (%s)' % (url, result.stderr.decode('utf-8', 'replace').strip()))
    return result.stdout


def fetch_json(url):
    return json.loads(curl(url).decode('utf-8'))


def integrity_of(path):
    with open(path, 'rb') as f:
        return 'sha512-' + base64.b64encode(hashlib.sha512(f.read()).digest()).decode('ascii')


def sha512_hex(data):
    return hashlib.sha512(data).hexdigest()


def read_lock():
    lock = {}
    with open(LOCK_PATH, encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith('#'):
                key, _, value = line.partition('=')
                lock[key.strip()] = value.strip()
    for key in ('version', 'tgz', 'js'):
        if not lock.get(key) and key != 'js':
            fail('mermaid.lock に %s がありません' % key)
    return lock


def write_lock(lock):
    with open(LOCK_PATH, encoding='utf-8') as f:
        lines = f.read().split('\n')
    out = []
    for line in lines:
        key = line.partition('=')[0].strip()
        if not line.startswith('#') and key in lock:
            line = '%s=%s' % (key, lock[key])
        out.append(line)
    with open(LOCK_PATH, 'w', encoding='utf-8') as f:
        f.write('\n'.join(out))


def npm_metadata(name, version):
    return fetch_json('%s/%s/%s' % (REGISTRY, name.replace('/', '%2F'), version))


def npm_download(name, version, workdir):
    """npm の tgz を取り、登録所の integrity で確かめる。(パス, メタデータ) を返す"""
    meta = npm_metadata(name, version)
    path = os.path.join(workdir, '%s-%s.tgz' % (name.replace('/', '+'), version))
    curl(meta['dist']['tarball'], path)
    if integrity_of(path) != meta['dist']['integrity']:
        fail('%s@%s の sha512 が登録所と合いません' % (name, version))
    return path, meta


def tar_read(path, member):
    with tarfile.open(path) as tar:
        try:
            f = tar.extractfile(member)
        except KeyError:
            return None
        return f.read() if f is not None else None


def tar_texts(path, predicate):
    """名前が predicate に合う通常のファイルを (名前, 中身) で返す。展開はしない"""
    found = []
    with tarfile.open(path) as tar:
        for member in tar.getmembers():
            if member.isfile() and predicate(member.name):
                found.append((member.name, tar.extractfile(member).read()))
    return found


def pnpm_packages(text):
    """注記とキーから (名前, 版) の集まりを作る"""
    packages = set()
    for directory, name in PNPM_PATTERN.findall(text):
        prefix = name.replace('/', '+') + '@'
        if not directory.startswith(prefix):
            fail('読めない置き場所です: %s' % directory)
        # 版の後ろに _<相手>@<版> (peer) や _patch_hash=... が付くことがある
        packages.add((name, directory[len(prefix):].split('_')[0]))
    return packages


def license_of(meta):
    value = meta.get('license')
    if isinstance(value, dict):
        value = value.get('type')
    if not value and isinstance(meta.get('licenses'), list):
        value = ' OR '.join(l.get('type', '') for l in meta['licenses'] if isinstance(l, dict))
    return value or None


def guess_license(text):
    """package.json に書いていない部品のために、条文から見分ける"""
    if 'Permission is hereby granted, free of charge' in text:
        return 'MIT'
    if 'Permission to use, copy, modify, and/or distribute this software' in text:
        return 'ISC'
    return None


def license_options(expression):
    return [part.strip() for part in re.sub(r'[()]', '', expression).split(' OR ')]


def is_license_file(member):
    base = member.split('/')[-1]
    return member.count('/') == 1 and re.match(r'(?i)^(licen[cs]e|copying|notice)', base) is not None


def maven_license(group, artifact, version, depth=0):
    """POM (無ければ親の POM) の <licenses> から SPDX の名前を出す"""
    url = '%s/%s/%s/%s/%s-%s.pom' % (MAVEN, group.replace('.', '/'), artifact, version, artifact, version)
    root = ElementTree.fromstring(curl(url))
    names = [e.text.strip() for e in root.iter() if e.tag.endswith('}name') and
             e.text and _under(root, e, 'license')]
    if names:
        name = names[0]
        if 'Eclipse Public License' in name:
            return 'EPL-2.0' if '2.0' in name else 'EPL-1.0'
        if 'Apache' in name:
            return 'Apache-2.0'
        # GWT の POM は名前だけで、指す先 (gwtproject.org/terms.html) に
        # 「GWT は Apache License 2.0」とある
        if name == 'GWT Terms':
            return 'Apache-2.0'
        fail('POM のライセンスを読めません: %s (%s)' % (name, url))
    parent = [e for e in root if e.tag.endswith('}parent')]
    if not parent or depth > 4:
        fail('POM にライセンスがありません: %s' % url)
    get = lambda tag: [e.text for e in parent[0] if e.tag.endswith('}' + tag)][0]
    return maven_license(get('groupId'), get('artifactId'), get('version'), depth + 1)


def _under(root, element, tag):
    for parent in root.iter():
        if element in list(parent):
            return parent.tag.endswith('}' + tag)
    return False


def elkjs_components(version):
    """elkjs が中に組み込んでいる Java の部品。build.gradle の versions から版を引く"""
    gradle = curl('https://raw.githubusercontent.com/kieler/elkjs/%s/build.gradle' % version).decode('utf-8')
    versions = dict(re.findall(r"^\s*(emfGwt|gwt|guava|xtext):\s*'([^']+)'", gradle, re.M))
    if set(versions) != {'emfGwt', 'gwt', 'guava', 'xtext'}:
        fail('elkjs %s の build.gradle から版を読めません: %s' % (version, versions))
    minor = '.'.join(version.split('.')[:2])
    components = [
        ('Eclipse Layout Kernel (ELK) %s' % minor, None,
         'https://github.com/eclipse/elk (the %s release line; elkjs keeps the minor version of ELK)\n'
         '    https://github.com/kieler/elkjs/tree/%s' % (minor, version)),
    ]
    maven = [
        ('EMF for GWT', 'com.genmymodel.emf.gwt', ['emf-common', 'emf-ecore'], versions['emfGwt']),
        ('Xtend/Xbase runtime library', None, None, versions['xtext']),
        ('Guava', 'com.google.guava', ['guava'], versions['guava']),
        ('GWT (gwt-user)', 'org.gwtproject', ['gwt-user'], versions['gwt']),
    ]
    for label, group, artifacts, v in maven:
        if label.startswith('Xtend'):
            coords = [('org.eclipse.xtend', 'org.eclipse.xtend.lib'), ('org.eclipse.xtext', 'org.eclipse.xtext.xbase.lib')]
        else:
            coords = [(group, a) for a in artifacts]
        licenses = {maven_license(g, a, v) for g, a in coords}
        if len(licenses) != 1:
            fail('%s のライセンスがそろっていません: %s' % (label, licenses))
        sources = '\n    '.join('%s/%s/%s/%s/%s-%s-sources.jar' % (MAVEN, g.replace('.', '/'), a, v, a, v)
                               for g, a in coords)
        components.append(('%s %s' % (label, v), licenses.pop(), sources))
    return components


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[1])
    parser.add_argument('--pin', metavar='VERSION', help='版を変えて mermaid.lock を書き換える')
    args = parser.parse_args()

    lock = read_lock()
    if args.pin:
        meta = npm_metadata('mermaid', args.pin)
        lock = {'version': args.pin, 'tgz': meta['dist']['integrity'], 'js': ''}
    version = lock['version']

    workdir = tempfile.mkdtemp(prefix='nf-mermaid-')
    try:
        print('取得中 mermaid@%s ...' % version)
        tgz = os.path.join(workdir, 'mermaid.tgz')
        curl('%s/mermaid/-/mermaid-%s.tgz' % (REGISTRY, version), tgz)
        if integrity_of(tgz) != lock['tgz']:
            fail('mermaid-%s.tgz の sha512 が mermaid.lock と合いません' % version)

        minified = tar_read(tgz, 'package/dist/mermaid.min.js')
        unminified = tar_read(tgz, 'package/dist/mermaid.js')
        mermaid_license = tar_read(tgz, 'package/LICENSE')
        if minified is None or unminified is None or mermaid_license is None:
            fail('tgz に dist/mermaid.min.js・dist/mermaid.js・LICENSE のどれかがありません')
        if args.pin:
            lock['js'] = sha512_hex(minified)
        elif sha512_hex(minified) != lock['js']:
            fail('mermaid.min.js の sha512 が mermaid.lock と合いません')

        print('部品を洗い出し中 ...')
        packages = pnpm_packages(unminified.decode('utf-8'))
        parser_json = curl('https://raw.githubusercontent.com/mermaid-js/mermaid/mermaid@%s/packages/parser/package.json'
                           % version)
        parser_version = json.loads(parser_json)['version']
        parser_tgz, _ = npm_download('@mermaid-js/parser', parser_version, workdir)
        packages.add(('@mermaid-js/parser', parser_version))
        for _, text in tar_texts(parser_tgz, lambda n: n.startswith('package/dist/') and n.endswith('.mjs')):
            packages |= pnpm_packages(text.decode('utf-8'))

        records = []
        for name, v in sorted(packages):
            path, meta = npm_download(name, v, workdir)
            texts = tar_texts(path, is_license_file)
            license = license_of(meta)
            if not license:
                guesses = {guess_license(t.decode('utf-8', 'replace')) for _, t in texts} - {None}
                if len(guesses) != 1:
                    fail('%s@%s のライセンスが分かりません' % (name, v))
                license = guesses.pop()
            author = meta.get('author')
            if isinstance(author, dict):
                author = author.get('name')
            records.append({'name': name, 'version': v, 'license': license, 'author': author,
                            'texts': [(n.split('/')[-1], t.decode('utf-8', 'replace')) for n, t in texts]})

        elk = [r for r in records if r['name'] == 'elkjs']
        components = elkjs_components(elk[0]['version']) if elk else []

        print('ライセンスを確かめ中 ...')
        for r in records:
            options = license_options(r['license'])
            if any(o in ALLOWED for o in options):
                continue
            if r['name'] == 'elkjs' and any(o in EPL for o in options):
                continue
            fail('%s@%s のライセンス %s は認めていません' % (r['name'], r['version'], r['license']))
        for label, license, _ in components:
            if license is not None and license not in ALLOWED and license not in EPL:
                fail('elkjs の中の %s のライセンス %s は認めていません' % (label, license))
        missing = [r for r in records if not r['texts']]
        standard = {}
        for license in sorted({c[1] for c in components if c[1]} |
                              {license_options(r['license'])[0] for r in missing}):
            standard[license] = curl(SPDX_TEXT + license + '.txt').decode('utf-8')

        write_output(version, mermaid_license, minified, records, components, standard)
        write_lock(lock)
    finally:
        shutil.rmtree(workdir, ignore_errors=True)

    print()
    print('置いたもの: %s' % os.path.relpath(DEST, REPO_ROOT))
    for name in sorted(os.listdir(DEST)):
        print('  %-28s %9d bytes' % (name, os.path.getsize(os.path.join(DEST, name))))
    print('部品 %d 個 + elkjs の中の %d 個' % (len(records), len(components)))
    print()
    print('ブラウザでの確かめ (tests/mermaid.sh が出すページ) を流してから、コミットしてください:')
    print('  git add deploy/scripts/mermaid.lock NextForm/resource/script/mermaid')


def write_output(version, mermaid_license, minified, records, components, standard):
    if os.path.isdir(DEST):
        shutil.rmtree(DEST)
    os.makedirs(DEST)
    with open(os.path.join(DEST, 'mermaid.min.js'), 'wb') as f:
        f.write(minified)
    with open(os.path.join(DEST, 'LICENSE'), 'wb') as f:
        f.write(mermaid_license)

    rule = '=' * 78
    out = []
    out.append('Third-party components bundled in mermaid.min.js (Mermaid %s)' % version)
    out.append('')
    out.append('This file is generated by deploy/scripts/update-mermaid.sh in the NextForm')
    out.append('repository. Do not edit it by hand. Mermaid itself is under the MIT License')
    out.append('(see LICENSE in this directory). mermaid.min.js is the unmodified')
    out.append('dist/mermaid.min.js of the npm package mermaid@%s.' % version)
    out.append('')
    out.append('Components and their licenses:')
    out.append('')
    for r in records:
        out.append('  %-48s %-14s %s' % (r['name'], r['version'], r['license']))
    if components:
        out.append('')
        out.append('Contained in elkjs (compiled from Java with GWT):')
        out.append('')
        for label, license, _ in components:
            out.append('  %-63s %s' % (label, license or '(as elkjs)'))
    if components:
        out.append('')
        out.append(rule)
        out.append('elkjs (Eclipse Public License) and the libraries compiled into it')
        out.append(rule)
        out.append('')
        out.append('elkjs and the libraries compiled into it are distributed here in object')
        out.append('code form (JavaScript generated by GWT, then bundled and minified by')
        out.append('Mermaid). They are not covered by the GNU General Public License of')
        out.append('NextForm; they remain under their own licenses below. NextForm calls them')
        out.append('only from resource/script/diagram.js, which carries an additional')
        out.append('permission under section 7 of the GNU GPL version 3.')
        out.append('')
        out.append('The source code is available from:')
        out.append('')
        elk = [r for r in records if r['name'] == 'elkjs'][0]
        out.append('  elkjs %s (%s)' % (elk['version'], elk['license']))
        out.append('    https://www.npmjs.com/package/elkjs/v/%s' % elk['version'])
        for label, license, sources in components:
            out.append('  %s (%s)' % (label, license or elk['license']))
            out.append('    ' + sources)
    for r in records:
        out.append('')
        out.append(rule)
        out.append('%s %s (%s)' % (r['name'], r['version'], r['license']))
        out.append(rule)
        if r['texts']:
            for name, text in r['texts']:
                out.append('')
                out.append('--- %s ---' % name)
                out.append('')
                out.append(text.rstrip())
        else:
            out.append('')
            out.append('The package contains no license file. The standard text of %s follows.'
                       % license_options(r['license'])[0])
            if r['author']:
                out.append('Author (from package.json): %s' % r['author'])
            out.append('')
            out.append(standard[license_options(r['license'])[0]].rstrip())
    for license in sorted(set(standard) & ({c[1] for c in components} | EPL)):
        out.append('')
        out.append(rule)
        out.append('%s (full text, for the components contained in elkjs)' % license)
        out.append(rule)
        out.append('')
        out.append(standard[license].rstrip())
    with open(os.path.join(DEST, 'THIRD-PARTY-LICENSES.txt'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(out) + '\n')


if __name__ == '__main__':
    main()
