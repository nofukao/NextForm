/*
 * diagram.js - draws Mermaid diagrams in Markdown pages of NextForm
 * Copyright (C) 2026 nofukao
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Additional permission under GNU GPL version 3 section 7
 *
 * If you modify this file, or any covered work, by linking or combining
 * it with Mermaid (https://github.com/mermaid-js/mermaid) or with any of
 * the components bundled in it, including the Eclipse Layout Kernel
 * (elkjs) and the libraries compiled into it (or modified versions of
 * those), containing parts covered by the terms of the Eclipse Public
 * License version 1.0 or 2.0 or of any other license under which those
 * components are distributed, the licensors of this file grant you
 * additional permission to convey the resulting work. Corresponding
 * Source for a non-source form of such a combination shall include the
 * source code for the parts of those components used as well as that
 * of the covered work.
 */

/*
 * Markdown のページの図 (```mermaid のコードブロック) を描く。
 *
 * サーバー (handler/markdown.inc の markdown_diagram_setup()) は、図のある
 * ページにだけこのファイルと <meta name="nextform-mermaid"> を足す。本文は
 * コードブロックのまま出るので、JavaScript が無効でも、読み込めなくても、
 * コードとして読める。
 *
 * **このファイルは nextform.js の関数を呼ばない。** Mermaid の中の ELK などは
 * EPL で GPLv3 と両立しない。Mermaid と組み合わさる GPL のコードをこのファイル
 * だけに閉じて、上の追加の許可を付けている (docs/development/
 * design-markdown-mermaid.md §2.6)。画面の文言はサーバーが <meta> で渡す。
 * 同じ理由で ES5 で書く。古いブラウザでも構文エラーにならず「描けない」と出せる。
 */
(function() {
    /* Mermaid の既定の上限 (maxTextSize)。超えるとコードが英語の赤い箱に化けるので、先に見る */
    var MAX_TEXT_SIZE = 50000;
    var ID_PREFIX = 'nf-diagram-';

    /*
     * ラベルに書ける HTML。strict でも Mermaid はラベルを DOMPurify の既定の
     * 設定に通すだけで、フォーム・入力欄・style・外部の画像が残る。飾りと
     * 数式 (KaTeX が出す MathML) だけにする。style と href と data- の属性は
     * 許さない (data- は部分編集が範囲を探す印 data-twp などを偽れる)。
     * この設定は図の中の %%{init}%% やフロントマターからは変えられない (SECURE_KEYS)。
     *
     * Mermaid の組み込みのアイコン (architecture の database など) も同じ消毒を
     * 通るので出ない。アイコンは色を各要素の style で付けていて、出すには
     * ラベルにも style を許すことになる (ページを覆う箱を置ける)。
     */
    var LABEL_TAGS = [
	'b', 'strong', 'i', 'em', 'u', 's', 'del', 'ins', 'strike', 'mark', 'small',
	'sub', 'sup', 'code', 'br', 'span', 'div', 'p'];
    var MATH_TAGS = [
	'math', 'semantics', 'mrow', 'mi', 'mo', 'mn', 'ms', 'mtext', 'mspace',
	'msup', 'msub', 'msubsup', 'mfrac', 'msqrt', 'mroot', 'mover', 'munder',
	'munderover', 'mtable', 'mtr', 'mtd', 'mlabeledtr', 'mstyle', 'mpadded',
	'mphantom', 'menclose', 'merror'];
    var LABEL_ATTRIBUTES = [
	'class', 'xmlns', 'display', 'mathvariant', 'mathcolor', 'mathbackground',
	'mathsize', 'stretchy', 'fence', 'separator', 'lspace', 'rspace', 'form',
	'largeop', 'movablelimits', 'symmetric', 'minsize', 'maxsize', 'accent',
	'accentunder', 'width', 'height', 'depth', 'voffset', 'linethickness',
	'displaystyle', 'scriptlevel', 'columnalign', 'rowalign', 'columnspacing',
	'rowspacing', 'columnlines', 'rowlines', 'frame', 'framespacing', 'notation',
	'align', 'encoding'];

    /*
     * 図の中の %%{init}%% やフロントマターで変えられない項目。Mermaid の既定の
     * 6 つに、CSS としてそのまま差し込まれるもの (url() で外部を読める) と、
     * ラベルの消毒の設定を足す。dompurifyConfig は Mermaid の既定の設定に無い
     * ので今は「知らない項目」として捨てられるが、Mermaid が既定に足すと図の
     * 中から上書きできるようになる。版に左右されないよう、ここで止める。
     */
    var SECURE_KEYS = [
	'secure', 'securityLevel', 'startOnLoad', 'maxTextSize',
	'suppressErrorRendering', 'maxEdges',
	'themeCSS', 'fontFamily', 'altFontFamily', 'dompurifyConfig'];

    var settings = null;
    var nextId = 0;

    function readSettings() {
	var meta = document.querySelector('meta[name="nextform-mermaid"]');
	if(!meta)
	    return null;
	return {
	    src: meta.getAttribute('content'),
	    error: meta.getAttribute('data-error') || '',
	    unsupported: meta.getAttribute('data-unsupported') || '',
	    tooLarge: meta.getAttribute('data-too-large') || ''
	};
    }

    /*
     * 図にするコードブロック。Markdown の本文の中の ```mermaid だけ。
     * Markdown のページに取り込んだ Wiki 記法のページの &pre(code,mermaid) は
     * (prettify.js が組み替えるので) 対象にしない。
     */
    function findBlocks() {
	var codes = document.querySelectorAll('section.markdown pre > code');
	var blocks = [];
	for(var i = 0; i < codes.length; i++) {
	    var code = codes[i];
	    var pre = code.parentNode;
	    if(!/(^|\s)language-mermaid(\s|$)/i.test(code.className))
		continue;
	    if(/(^|\s)prettyprint(\s|$)/.test(pre.className))
		continue;
	    blocks.push({pre: pre, code: code});
	}
	return blocks;
    }

    /*
     * prototype.js 1.7.2 が書き換えた組み込みを戻す。そのままでは 23 種類中
     * 8〜11 種類の図が誤りで止まり、流れ図の style にメソッドのソースが入った。
     *
     *   - Array.from を $A に置き換えている (Set・Map・{length: n} が空になる)
     *   - 配列の entries() が配列を返す (反復子を返さない)
     *   - Object.values が for...in で継承したものまで拾う
     *   - 配列・文字列などに足したメソッドが for...in で数えられる
     *
     * 前の 3 つは新しい iframe の組み込みを写し、最後のものは数えられないように
     * する。NextForm のスクリプトはこれらに頼っていない。図のあるページでだけ、
     * 1 度だけ行う。prototype.js の無いページでは何もしない。
     */
    var builtinsRestored = false;
    function restoreBuiltins() {
	if(builtinsRestored || typeof Prototype === 'undefined')
	    return;
	builtinsRestored = true;

	var frame = document.createElement('iframe');
	frame.style.display = 'none';
	document.documentElement.appendChild(frame);
	var clean = frame.contentWindow;
	Array.from = clean.Array.from;
	Array.prototype.entries = clean.Array.prototype.entries;
	Object.values = clean.Object.values;
	frame.parentNode.removeChild(frame);

	var prototypes = [Array.prototype, String.prototype, Number.prototype,
			  Function.prototype, Date.prototype, RegExp.prototype];
	for(var i = 0; i < prototypes.length; i++) {
	    var keys = Object.keys(prototypes[i]);
	    for(var j = 0; j < keys.length; j++)
		Object.defineProperty(prototypes[i], keys[j], {enumerable: false});
	}
    }

    /*
     * 読めたかは window.mermaid で見る。古いブラウザでは Mermaid が構文エラーに
     * なっても onload が呼ばれる。
     */
    function loadMermaid(callback) {
	var script = document.createElement('script');
	script.src = settings.src;
	script.async = true;
	script.onload = function() { callback(window.mermaid || null); };
	script.onerror = function() { callback(null); };
	document.head.appendChild(script);
    }

    function initializeMermaid(mermaid) {
	mermaid.initialize({
	    startOnLoad: false,
	    securityLevel: 'strict',
	    /* Mermaid の「爆弾の絵」を出さず、コードを残して自前で誤りを出す */
	    suppressErrorRendering: true,
	    secure: SECURE_KEYS,
	    dompurifyConfig: {
		ALLOWED_TAGS: LABEL_TAGS.concat(MATH_TAGS),
		ALLOWED_ATTR: LABEL_ATTRIBUTES,
		ALLOW_DATA_ATTR: false
	    }
	});
    }

    /* Mermaid は同じ id の要素を消すので、ページに無い id を使う */
    function newId() {
	var id;
	do {
	    id = ID_PREFIX + (nextId++);
	} while(document.getElementById(id));
	return id;
    }

    function showError(block, message) {
	var p = document.createElement('p');
	p.className = 'diagram_error';
	p.textContent = message.length > 300 ? message.substring(0, 300) + '…' : message;
	block.pre.parentNode.insertBefore(p, block.pre.nextSibling);
    }

    function errorMessage(error) {
	var message = error && error.message ? error.message : String(error);
	return settings.error + ' ' + message;
    }

    /*
     * 画面の上端にある本文の要素と、その位置。図に差し替わって高さが変わると、
     * 下にあるものがずれる。部分編集で保存すると nextform.js が読み込み直後に
     * スクロールの位置を戻す (scrollSetup()) が、そのときはまだコードなので、
     * 図より下を直していると見ていた箇所が動いてしまう。
     */
    function scrollAnchor() {
	if(window.pageYOffset <= 0)
	    return null;
	var elements = document.querySelectorAll('section.markdown > *');
	for(var i = 0; i < elements.length; i++) {
	    var rect = elements[i].getBoundingClientRect();
	    if(rect.bottom > 0)
		return {element: elements[i], top: rect.top};
	}
	return null;
    }

    function keepScroll(anchor) {
	if(!anchor)
	    return;
	var moved = anchor.element.getBoundingClientRect().top - anchor.top;
	if(moved !== 0)
	    window.scrollBy(0, moved);
    }

    /*
     * 1 つ描く。描く場所 (文字の幅を測る場所) は本文の中に置く。body の直下で
     * 測ると、本文の文字の設定と食い違ってラベルが切れることがある。
     */
    function render(mermaid, block, done) {
	var text = block.code.textContent;
	if(text.length > MAX_TEXT_SIZE) {
	    showError(block, settings.error + ' ' + settings.tooLarge);
	    done();
	    return;
	}
	var work = document.createElement('div');
	work.style.cssText = 'position: absolute; visibility: hidden; left: -10000px; top: 0; width: '
	    + block.pre.clientWidth + 'px';
	block.pre.parentNode.insertBefore(work, block.pre);

	var finish = function() {
	    if(work.parentNode)
		work.parentNode.removeChild(work);
	    done();
	};
	mermaid.render(newId(), text, work).then(function(result) {
	    var anchor = scrollAnchor();
	    /* <pre> は残す。部分編集の範囲 (data-twp / data-twl) が付いている */
	    block.pre.innerHTML = result.svg;
	    block.pre.className = (block.pre.className ? block.pre.className + ' ' : '') + 'diagram';
	    keepScroll(anchor);
	    finish();
	}, function(error) {
	    showError(block, errorMessage(error));
	    finish();
	});
    }

    function renderAll(mermaid, blocks) {
	var index = 0;
	var next = function() {
	    if(index >= blocks.length) {
		document.documentElement.setAttribute('data-diagrams', 'done');
		return;
	    }
	    var block = blocks[index++];
	    /* 図と図のあいだで処理を譲る。図の多いページでも画面が長く固まらないように */
	    render(mermaid, block, function() { setTimeout(next, 0); });
	};
	next();
    }

    function start() {
	settings = readSettings();
	if(!settings)
	    return;
	var blocks = findBlocks();
	if(blocks.length === 0)
	    return;
	restoreBuiltins();
	loadMermaid(function(mermaid) {
	    if(!mermaid) {
		for(var i = 0; i < blocks.length; i++)
		    showError(blocks[i], settings.unsupported);
		document.documentElement.setAttribute('data-diagrams', 'done');
		return;
	    }
	    initializeMermaid(mermaid);
	    renderAll(mermaid, blocks);
	});
    }

    if(document.readyState === 'loading')
	document.addEventListener('DOMContentLoaded', start);
    else
	start();
})();
