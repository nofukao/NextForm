<?php
$WIKI_OPERATORS_FUNCTION['export'] = true;
function wiki_operator_export(&$context, $dom, $tree) {
    if(!empty($context['texts_only']))
	return;

    $pagename = $context['page']['name'];
    $export_directory = '';
    $url = '';
    $label = l('Export');
    $is_force = false;
    $is_quiet = false;
    wiki_parse_parameters(
	$context, $tree,
	array(
	    'pagename' => &$pagename,
	    'equalities' => array(
		'directory' => &$export_directory,
		'url' => &$url,
		'label' => &$label,
		),
	    'long_flags' => array(
		'force' => &$is_force,
		'quiet' => &$is_quiet,
		)
	    )
	);

    $form = dom_append_form($dom, array(
				'page' => $pagename,
				'option' => 'export',
				'action' => 'write',
				'force' => $is_force,
				'quiet' => $is_quiet,
				'url' => $url,
				'back_page' => $context['page']['name'],
				'directory' => $export_directory),
			    'export', 'POST');
    dom_set_position($form, $tree);
    $input = dom_append_child($form, 'input', '',
			      array('type' => 'submit', 'value' => $label));
    if(EXPORT_ENABLE !== 'true' || !auth_has_permission(false, 'export')) {
	dom_set_attribute($input, 'disabled', 'disabled');
    }

    wiki_convert_from_tokens($context, $dom, $tree->target);
}
/**
@manual $manualpage$/ja/Wiki/CheatSheet/FunctionAdvanced
|[[エクスポート実行ボタン:../../Function/export]]|&&export(&[&[ページ名&]&], directory=ディレクトリ);|
*/
/**
@manual $manualpage$/en/Wiki/CheatSheet/FunctionAdvanced
|export button|&&export(&[&[page name&]&], directory=destination directory path);|
*/
/**
@manual $manualpage$/ja/Wiki/Function/export
&title &&export

*概要
 &export([[ページ名]], directory=ディレクトリ, url=エクスポート先URL, label=ラベル, force, quiet);
エクスポートを行うボタンを表示します．
エクスポート機能については[[こちら:../../../Export]]を参照して下さい．

*パラメータ
:&[&[ページ名&]&]|エクスポートの設定表のあるページを指定します．省略した場合は現在のページになります．
:{directory=ディレクトリ|エクスポート先のディレクトリを指定します．
ディレクトリは[[設定項目:../../../Administration/Settings]]の''EXPORT_DIR_PATH''から見たパスになります．}
:{url=エクスポート先URL|このパラメータはエクスポートの処理自体に何ら影響を及ぼしません．
このパラメータを用いてエクスポート先のディレクトリに対応するURLを明示すると，
エクスポートの結果の画面でファイル名が実際のエクスポート先へのリンクになります．
これによりエクスポートしたサイトのページを簡単に確認できるようになります．}
:label=ラベル|ボタンに表示される文字を指定します．
:{force|エクスポート機能では通常，ページとファイルの更新日時を比較して必要がなければエクスポートをスキップします．
このパラメータを指定するとスキップをすることなく必ずエクスポートを行うようになります．}
:quiet|エクスポートの結果の表を表示しなくなります．これによりサイトの編集者にシンプルな使い勝手を提供できます．

*対象
対象は指定しません．

*例
 &export(directory=domains/test, label=テスト環境へエクスポート, force);
 &export(directory=domains/real, label=本番環境へエクスポート, quiet);
*/
?>
