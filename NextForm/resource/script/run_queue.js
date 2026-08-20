/*
 * 時間のかかる処理 (検索インデックスの再構築、他 wiki からのインポート) の
 * 続きを送る。
 *
 * サーバは 10 秒ほどで区切って画面を返し、続きを送るためのフォームを
 * 置いてくる (app/util.inc の run_queue)。それをそのまま送信する。
 * 続きは状態を変えるので POST でなければならない (app/csrf.inc)。
 *
 * このファイルが読めなかったときは、フォームの中の <noscript> にある
 * 「続ける」を押せば同じことが起きる。
 */
window.addEventListener('load', function() {
    var form = document.querySelector('form.run_queue');
    if(form)
	form.submit();
});
