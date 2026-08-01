<?php
$SETUP_CONSTANTS['THEME_LAYOUT'] = array(
    'category' => 'theme',
    'description' => 'Layout',
    'default' => 'full',
    'type' => 'select',
    'options' => array(
	'liquid' => 'full width',
	'solid' => 'fixed width',
	));

$SETUP_CONSTANTS['THEME_SIDE_PAGE_POSITION'] = array(
    'category' => 'theme',
    'description' => 'Side page position',
    'default' => 'left',
    'type' => 'select',
    'options' => array(
	'left' => 'left',
	'right' => 'right',
	));

$SETUP_CONSTANTS['THEME_FONT_SIZE'] = array(
    'category' => 'theme',
    'description' => 'Font size',
    'default' => '14',
    'type' => 'select',
    'options' => array(
	'12' => '12px',
	'14' => '14px',
	'16' => '16px',
	'18' => '18px',
	'20' => '20px',
	));

$SETUP_CONSTANTS['THEME_IMAGE_LOGO'] = array(
    'category' => 'theme',
    'description' => 'Site logo image',
    'type' => 'theme_file_image',
    'default' => THEME_URI . '/' . THEME . '/image/logo.png',
    'allowempty' => true,
    'extensions' => array('jpg', 'jpeg', 'gif', 'png', 'svg', 'ico'),
    'path' => 'logo.');

$SETUP_CONSTANTS['THEME_IMAGE_ICON'] = array(
    'category' => 'theme',
    'description' => 'Site favorite icon',
    'type' => 'theme_file_image',
    'default' => THEME_URI . '/' . THEME . '/image/favicon.ico',
    'allowempty' => true,
    'extensions' => array('jpg', 'jpeg', 'gif', 'png', 'svg', 'ico'),
    'path' => 'favicon.',
    'note' => '.ico format recommended');

$SETUP_CONSTANTS['THEME_TONE'] = array(
    'category' => 'theme',
    'description' => 'Tone',
    'default' => 'beige/green',
    'type' => 'select',
    'options' => array(
	'beige/green' => 'beige/green',
	'yellow/pink' => 'yellow/pink',
	'white/blue' => 'white/blue',
	'black/blue' => 'black/blue',
	'navy/yellow' => 'navy/yellow',
	'gray/orange' => 'gray/orange',
	'custom' => 'custom color',
	));

$SETUP_CONSTANTS['THEME_COLOR_MAIN_CUSTOM'] = array(
    'category' => 'theme',
    'need_name' => 'THEME_TONE',
    'need_value' => 'custom',
    'need_option' => 'not',
    'description' => 'Head color',
    'default' => '',
    'allowempty' => true,
    'type' => 'color',
    'note' => 'blank means tone default');

global $THEME_CUSTOM_COLORS;
$THEME_CUSTOM_COLORS = array(
    'THEME_CUSTOM_COLOR_MAIN_DEFAULT' =>           '#ecc274',
    'THEME_CUSTOM_COLOR_BACKGROUND' =>        	   '#fbf6ea',
    'THEME_CUSTOM_COLOR_OUTSIDE' =>        	   '#b3ada2',
    'THEME_CUSTOM_COLOR_HEADER_TEXT' =>            '#2a2f32',
    'THEME_CUSTOM_COLOR_TEXT' =>              	   '#2a2f32',
    'THEME_CUSTOM_COLOR_LINK' =>              	   '#6ea157',
    'THEME_CUSTOM_COLOR_LINK_VISITED' =>      	   '#4b6e3b',
    'THEME_CUSTOM_COLOR_EXTERNAL_LINK' =>          '#6ea157',
    'THEME_CUSTOM_COLOR_EXTERNAL_LINK_VISITED' =>  '#4b6e3b',
    'THEME_CUSTOM_COLOR_EDIT' =>              	   '#fae9c3',
    'THEME_CUSTOM_COLOR_PAGE_INFO' =>        	   '#ecc274',
    'THEME_CUSTOM_COLOR_HIGHLIGHT' =>        	   '#ecc274',
    'THEME_CUSTOM_COLOR_HIGHLIGHT_TEXT' =>         '#fa4d4d',
    'THEME_CUSTOM_COLOR_HEAD' =>              	   '#ddfa6b',
    'THEME_CUSTOM_COLOR_PRE' =>               	   '#f0ebdf',
    'THEME_CUSTOM_COLOR_CODE' =>              	   '#f0ebdf',
    'THEME_CUSTOM_COLOR_ERROR' =>             	   '#fa98d5',
    'THEME_CUSTOM_COLOR_NOTICE' =>            	   '#a5f980',
    'THEME_CUSTOM_COLOR_INFO' =>              	   '#b1fae3',
    'THEME_CUSTOM_COLOR_COMPARE_DELETE' =>    	   '#efb7ac',
    'THEME_CUSTOM_COLOR_COMPARE_ADD' =>       	   '#c8efac',
    'THEME_CUSTOM_COLOR_TEXTINPUT' =>              '#2a2f32',
    'THEME_CUSTOM_COLOR_TEXTINPUT_BACKGROUND' =>   '#ffffff',
    'THEME_CUSTOM_COLOR_ACTION_BACKGROUND' =>      '#2a2f32',
    'THEME_CUSTOM_COLOR_ACTION_TEXT' =>            '#ffffff',
    );
foreach($THEME_CUSTOM_COLORS as $custom_color_name => $custom_color_value) {
    $SETUP_CONSTANTS[$custom_color_name] = array(
	'category' => 'theme',
	'group' => 'custom color',
	'need_name' => 'THEME_TONE',
	'need_value' => 'custom',
	'description' => strtolower(strtr(substr($custom_color_name, 19), '_', ' ')) . ' color',
	'default' => $custom_color_value,
	'type' => 'color');
}

$SETUP_CONSTANTS['THEME_HEADER_HEIGHT'] = array(
    'category' => 'theme',
    'group' => 'Header parts',
    'description' => 'Height',
    'type' => 'integer',
    'default' => '52',
    'round' => array(52, 512),
    'allowempty' => true,
    'note' => 'pixels');

$SETUP_CONSTANTS['THEME_HEADER_SITE_NAME_ENABLE'] = array(
    'category' => 'theme',
    'group' => 'Header parts',
    'description' => 'Site name',
    'type' => 'select',
    'default' => 'true',
    'options' => array('true' => 'Show', 'false' => 'Hide'));

$SETUP_CONSTANTS['THEME_SEARCH_TOOL_ENABLE'] = array(
    'category' => 'theme',
    'group' => 'Header parts',
    'description' => 'Search form',
    'type' => 'select',
    'default' => 'true',
    'options' => array('true' => 'Show', 'false' => 'Hide'));

$SETUP_CONSTANTS['THEME_SITE_MENU_SIZE'] = array(
    'category' => 'theme',
    'group' => 'Header parts',
    'description' => 'Site menu size',
    'type' => 'select',
    'default' => 'normal',
    'options' => array('normal' => 'Normal', 'small' => 'Small'));

$THEME_BASIC_BACKGROUND_CONSTANTS = array(
    'THEME_IMAGE_BACKGROUND_HEADER' => array('background-header.', 'header image'),
    'THEME_IMAGE_BACKGROUND_BODY' => array('background-body.', 'body background image'),
    'THEME_IMAGE_BACKGROUND_OUTSIDE' => array('background-outside.', 'outside background image'),
    );
foreach($THEME_BASIC_BACKGROUND_CONSTANTS as $background_const_name => $background_const) {
    $SETUP_CONSTANTS[$background_const_name] = array(
	'category' => 'theme',
	'description' => $background_const[1],
	'type' => 'theme_file_image',
	'default' => '',
	'allowempty' => true,
	'extensions' => array('jpg', 'jpeg', 'gif', 'png', 'svg', 'ico'),
	'path' => $background_const[0]);
    $SETUP_CONSTANTS[$background_const_name . '_REPEAT'] = array(
	'category' => 'theme',
	'childof' => $background_const_name,
	'type' => 'select',
	'default' => 'no-repeat',
	'options' => array(
	    'no-repeat' => 'no repeat',
	    'repeat' => 'repeat',
	    'repeat-x' => 'repeat horizontally',
	    'repeat-y' => 'repeat vertically',
	    'cover' => 'cover',
	    'contain' => 'contain',
	    'contain-repeat' => 'contain and repeat',
	    )
	);
    $SETUP_CONSTANTS[$background_const_name . '_POSITION'] = array(
	'category' => 'theme',
	'childof' => $background_const_name,
	'type' => 'select',
	'default' => 'left top',
	'options' => array(
	    'left top' => 'left top',
	    'left center' => 'left center',
	    'left bottom' => 'left bottom',
	    'center top' => 'center top',
	    'center center' => 'center center',
	    'center bottom' => 'center bottom',
	    'right top' => 'right top',
	    'right center' => 'right center',
	    'right bottom' => 'right bottom',
	    )
	);
}

$SETUP_CONSTANTS['THEME_IMAGE_BACKGROUND_OUTSIDE']['need_name'] = 'THEME_LAYOUT';
$SETUP_CONSTANTS['THEME_IMAGE_BACKGROUND_OUTSIDE']['need_value'] = 'solid';
$SETUP_CONSTANTS['THEME_IMAGE_BACKGROUND_OUTSIDE_ATTACHMENT'] = array(
    'category' => 'theme',
    'childof' => 'THEME_IMAGE_BACKGROUND_OUTSIDE',
    'type' => 'select',
    'default' => 'scroll',
    'options' => array(
	'scroll' => 'scroll',
	'fixed' => 'fixed',
	)
    );

$LANGUAGE['ja']['Layout']                     	= 'レイアウト';
$LANGUAGE['ja']['full width']                 	= '画面全体';
$LANGUAGE['ja']['fixed width']                	= '固定幅';
$LANGUAGE['ja']['Side page position']         	= 'サイドページの位置';
$LANGUAGE['ja']['left']                       	= '左';
$LANGUAGE['ja']['right']                      	= '右';
$LANGUAGE['ja']['Font size']                  	= '文字の大きさ';
$LANGUAGE['ja']['Site logo image']    	      	= 'サイトのロゴ画像';
$LANGUAGE['ja']['Site favorite icon'] 	      	= 'サイトのお気に入りアイコン';
$LANGUAGE['ja']['Tone']                       	= '色調';
$LANGUAGE['ja']['beige/green']         	      	= 'ベージュ/グリーン';
$LANGUAGE['ja']['yellow/pink']         	      	= 'イエロー/ピンク';
$LANGUAGE['ja']['white/blue']         	      	= 'ホワイト/ブルー';
$LANGUAGE['ja']['black/blue']                 	= 'ブラック/ブルー';
$LANGUAGE['ja']['navy/yellow']                	= 'ネイビー/イエロー';
$LANGUAGE['ja']['gray/orange']                	= 'グレー/オレンジ';
$LANGUAGE['ja']['custom color']               	= '個別に色を設定';
$LANGUAGE['ja']['Head color']         	      	= '見出しの色';
$LANGUAGE['ja']['.ico format recommended']    	= '.ico形式推奨';
$LANGUAGE['ja']['Header parts']               	= '見出しの構成';
$LANGUAGE['ja']['Search form']                	= '検索フォーム';
$LANGUAGE['ja']['Site menu size']             	= 'サイトメニューの大きさ';
$LANGUAGE['ja']['Normal']                     	= '通常';
$LANGUAGE['ja']['Small']                      	= '小さい';
$LANGUAGE['ja']['Hide']                       	= '非表示';
$LANGUAGE['ja']['blank means tone default']   	= '空欄は標準の色になります';
$LANGUAGE['ja']['main default color']         	= '見出しの色';
$LANGUAGE['ja']['header text color']          	= '見出しの文字の色';
$LANGUAGE['ja']['background color']           	= '背景色';
$LANGUAGE['ja']['outside color']              	= '外側の色';
$LANGUAGE['ja']['text color']                 	= '文字色';
$LANGUAGE['ja']['link color']                 	= '未訪問リンクの色';
$LANGUAGE['ja']['link visited color']         	= '訪問済みリンクの色';
$LANGUAGE['ja']['external link color']        	= '未訪問外部リンクの色';
$LANGUAGE['ja']['external link visited color']	= '訪問済み外部リンクの色';
$LANGUAGE['ja']['edit color']                 	= '編集時の色';
$LANGUAGE['ja']['page info color']            	= 'ページ情報の色';
$LANGUAGE['ja']['highlight color']            	= '強調された領域の色';
$LANGUAGE['ja']['highlight text color']       	= '強調された文字の色';
$LANGUAGE['ja']['head color']                 	= '表の見出しの色';
$LANGUAGE['ja']['pre color']                  	= '整形済みテキストの色';
$LANGUAGE['ja']['code color']                 	= 'コードの色';
$LANGUAGE['ja']['error color']                	= 'エラーの色';
$LANGUAGE['ja']['notice color']               	= '注意の色';
$LANGUAGE['ja']['info color']                 	= 'お知らせの色';
$LANGUAGE['ja']['compare delete color']       	= '比較時に削除の色';
$LANGUAGE['ja']['compare add color']          	= '比較時に追加の色';
$LANGUAGE['ja']['textinput color']            	= '入力時の文字色';
$LANGUAGE['ja']['textinput background color'] 	= '入力時の背景色';
$LANGUAGE['ja']['Height']                     	= '高さ';
$LANGUAGE['ja']['pixels']                     	= 'ピクセル';
$LANGUAGE['ja']['header image']               	= '見出しの画像';
$LANGUAGE['ja']['body background image']      	= '本文の背景画像';
$LANGUAGE['ja']['outside background image']   	= '外側の背景画像';
$LANGUAGE['ja']['action background color']    	= 'ボタン等の背景色';
$LANGUAGE['ja']['action text color']          	= 'ボタン等の文字色';
$LANGUAGE['ja']['no repeat']                  	= '繰り返さない';
$LANGUAGE['ja']['repeat']                     	= '繰り返す';
$LANGUAGE['ja']['repeat horizontally']        	= '水平に繰り返し';
$LANGUAGE['ja']['repeat vertically']          	= '垂直に繰り返し';
$LANGUAGE['ja']['cover']                      	= '覆う';
$LANGUAGE['ja']['contain']                    	= '収める';
$LANGUAGE['ja']['contain and repeat']         	= '収めて繰り返す';
$LANGUAGE['ja']['scroll']                     	= 'スクロール';
$LANGUAGE['ja']['fixed']                      	= '固定';
$LANGUAGE['ja']['left top'] 	              	= '左上';
$LANGUAGE['ja']['left center']   	      	= '左中';
$LANGUAGE['ja']['left bottom']   	      	= '左下';
$LANGUAGE['ja']['center top']    	      	= '中上';
$LANGUAGE['ja']['center center'] 	      	= '中央';
$LANGUAGE['ja']['center bottom'] 	      	= '中下';
$LANGUAGE['ja']['right top']     	      	= '右上';
$LANGUAGE['ja']['right center']  	      	= '右中';
$LANGUAGE['ja']['right bottom']  	      	= '右下';
?>
