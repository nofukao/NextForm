(function() {
    function themeMain() {
	siteMenuSetup();
	pageMenuSetup();
	fileuploadButtonSetup();
    }

    var menus = [];
    function siteMenuSetup() {
	var header = $$('header.title')[0];
	var nav = $$('nav.site_menu')[0];
	if(nav.select('li').length <= 0)
	    return;
	
	var link = new Element('a', {'class': 'site_menu button', href: ''}).update(l('Site menu'));
	link.setAttribute('title', l('Site menu'));
	var menuTitle = new Element('h1').update(l('Site menu'));
	nav.insert({top: menuTitle});
	header.insertBefore(link, header.down('ul.search_tools'));
	nav.hide();
	menus.push(nav);
	link.observe('click', function(event) {
	    event.stop();
	    visibilityToggle(nav, 'block');
	    menus.each(function(e){nav != e && e.hide();});
	});
	document.observe('click', function(event) { nav.hide(); });
    }

    function pageMenuSetup() {
	var header = $$('article.main > h1')[0];
	var nav = $$('nav.page_menu')[0];

	var items = nav.select('li');
	var needShow = false;
	for(var i = 0; i < items.length; i++) {
	    if(!items[i].hasClassName('disabled'))
		needShow = true;
	}
	if(!needShow)
	    return;
	
	var link = new Element('a', {class: 'page_menu', href: ''}).update(l('Page menu'));
	header.appendChild(link);
	//header.insertBefore(link, header.firstChild);
	//header.parentNode.insertBefore(link, header.nextSibling);
	nav.hide();
	menus.push(nav);
	var toggleAction = function(event) {
	    event.stop();
	    visibilityToggle(nav, 'block');
	    menus.each(function(e){nav != e && e.hide();});
	    if(nav.visible()) {
		link.addClassName('open');
	    } else {
		link.removeClassName('open');
	    }
	    messagesPositionUpdate();
	}

	header.observe('click', function(event) { event.element() == header && toggleAction(event); });
	link.observe('click', toggleAction);
	document.observe('click', function(event) { nav.hide(); link.removeClassName('open'); });
    }
    
    function visibilityToggle(element, displayType) {
	if(element.visible())
	    element.hide();
	else
	    element.setStyle({display: displayType});
    }

    function fileuploadButtonSetup() {
	$$('input[type="file"]').each(function(element) {
	    var label = element.up('label.file_upload');
	    
	    if(label) {
		element.hide();
		var button = document.createElement('button');
		button.setAttribute('type', 'button');
		button.update(l('Browse'));
		button.observe('click', function(event) {
		    event.throughClick(label);
		});
		label.appendChild(button);
		var span = document.createElement('span');
		span.addClassName('selected_files');
		span.hide();
		label.appendChild(span);
		element.observe('change', function(event) {
		    if(element.files) {
			var name = [];
			for(var i = 0; i < element.files.length; i++) {
			    name.push(element.files[i].name);
			}
		    }
		    span.clearChildren();
		    if(name.length > 0) {
			span.appendChild(document.createTextNode(name.join(' ')));
			span.show();
		    } else
			span.hide();
		});
	    }
	});
    }

    //Event.observe(document, 'dom:loaded', themeMain);
    Event.observe(window, 'load', themeMain);
})();
