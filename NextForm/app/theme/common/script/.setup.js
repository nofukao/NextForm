(function() {
<?php
$localized_tones = array();
foreach(tone_get_all() as $tone) {
    $localized_tones[tone_get_name($tone)] = $tone['colors'];
}
echo('var tones = ' . json_encode($localized_tones) . ';');
?>

    function loadToneSetup() {
	var colorInputs = $$('[name="const_THEME_CUSTOM_COLOR_BACKGROUND"]');
	if(colorInputs.length <= 0)
	    return;
	var dl = colorInputs[0].parentNode.parentNode;
	var p = new Element('p');
	dl.parentNode.appendChild(p);
	var link = new Element('a', {'href': ''}).update(l('Load preset tone colors'));
	p.appendChild(link);
	link.observe('click', function(event) {
	    event.stop();
	    link.hide();

	    var select = new Element('select');
	    p.appendChild(select);
	    var option = new Element('option').update('');
	    select.appendChild(option);
	    for(var toneName in tones) {
		var option = new Element('option').update(toneName);
		select.appendChild(option);
	    }
	    var button = new Element('button', {'type' : 'button'}).update(l('Load'));
	    p.appendChild(button);
	    button.observe('click', function(event) {
		var toneName = select.getValue();
		var colors = tones[toneName];
		for(var constName in colors) {
		    var formName = 'const_' + constName.replace(/^THEME_COLOR_/, 'THEME_CUSTOM_COLOR_');
		    $$('input[name="' + formName + '"]').each(function(input) {
			input.color.fromString(colors[constName]);
		    });
		}
	    });
	});
    }

    function setupMain() {
	loadToneSetup();
    }

    Event.observe(window, 'load', setupMain);
})();

LANGUAGE['ja']['Load preset tone colors'] = '標準の色調を読み込む';
