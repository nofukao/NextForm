<?php
/*
 * マニュアルの生成画面の跡地。
 *
 * 0.7.0 でマニュアルは組み込みになり、生成の操作が要らなくなった
 * (app/manual.inc を参照)。管理ツールの一覧 (option/admin.inc) からも外した。
 *
 * **それでもこのファイルを配布物から消してはいけない。**
 * app/tool/upgrade は「配布物に無いファイルは削除せず残す」方針なので
 * (利用者が置いたプラグインを巻き添えにしないため)、消すと 0.6.0 以前の
 * admin_manual.inc がアップグレード後もそのまま残り、$OPTIONS に
 * 登録されたまま、もう存在しない manual_build() を呼んで Fatal error になる。
 * 上書きして無害にするのが確実。
 *
 * ここへ来るのは、以前の URL をブックマークしていた人だけ。
 * 0.6.0 以前からのアップグレードを支えなくなったら、このファイルは消せる。
 */
$OPTIONS['admin_manual'] = array(
    'show' => 'admin_manual_show',
    );

function admin_manual_show(&$args, $dom) {
    auth_check(false, 'admin');
    title_set_l('Manual');
    ld($dom, 'The manual is built in now. Generating is no longer needed. See $pagename$.',
       array('pagename' => dom_pagename(MANUAL_PAGENAME . '/' . LANG)));
}
?>
