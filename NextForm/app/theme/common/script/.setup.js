(function() {
<?php
/*
 * 選べる色調を識別子で引けるようにして渡す。組み込み (app/tone/) と
 * そのサイトが保存したもの (storage/tone/) の両方が入る。
 */
$tones = array();
foreach(tone_get_all() as $id => $tone) {
    $tones[$id] = array('name' => tone_get_name($tone), 'colors' => $tone['colors']);
}
echo('var tones = ' . json_encode($tones) . ';');
echo("\nvar currentTone = " . json_encode(THEME_TONE) . ';');
?>

    /* 色調の色を「個別に色を設定」の入力欄に流し込む */
    function fillToneColors(toneId) {
	if(!tones[toneId])
	    return;
	var colors = tones[toneId].colors;
	for(var constName in colors) {
	    var formName = 'const_' + constName.replace(/^THEME_COLOR_/, 'THEME_CUSTOM_COLOR_');
	    $$('input[name="' + formName + '"]').each(function(input) {
		input.color.fromString(colors[constName]);
	    });
	}
    }

    function loadToneSetup() {
	var colorInputs = $$('[name="const_THEME_CUSTOM_COLOR_BACKGROUND"]');
	if(colorInputs.length <= 0)
	    return;
	var dl = colorInputs[0].parentNode.parentNode;
	var p = new Element('p');
	dl.parentNode.appendChild(p);
	var link = new Element('a', {'href': ''}).update(l('Load tone colors'));
	p.appendChild(link);
	link.observe('click', function(event) {
	    event.stop();
	    link.hide();

	    var select = new Element('select');
	    p.appendChild(select);
	    var option = new Element('option', {'value': ''}).update('');
	    select.appendChild(option);
	    for(var toneId in tones) {
		var option = new Element('option', {'value': toneId}).update(tones[toneId].name);
		select.appendChild(option);
	    }
	    var button = new Element('button', {'type' : 'button'}).update(l('Load'));
	    p.appendChild(button);
	    button.observe('click', function(event) {
		fillToneColors(select.getValue());
	    });
	});
    }

    /*
     * 「個別に色を設定」に切り替えたら、それまで選んでいた色調の色を入れる。
     *
     * 保存されている色調が既に custom のときは何もしない。入力欄にある色が
     * そのサイトで実際に使われている色なので、上書きしてはいけない。
     * (サーバ側でも同じ規則で初期値を出している。ここは適用しなくても
     *  すぐ見えるようにするための上乗せ)
     */
    function customToneSetup() {
	var selects = $$('select[name="const_THEME_TONE"]');
	if(selects.length <= 0 || currentTone === 'custom')
	    return;
	var select = selects[0];
	var previous = select.getValue();
	select.observe('change', function() {
	    if(select.getValue() === 'custom')
		fillToneColors(previous);
	    else
		previous = select.getValue();
	});
    }

    function setupMain() {
	loadToneSetup();
	customToneSetup();
    }

    Event.observe(window, 'load', setupMain);
})();

LANGUAGE['ja']['Load tone colors'] = '色調を読み込む';
