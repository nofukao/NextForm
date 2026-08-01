var COOKIE_NAME_PREFIX = 'toratorawiki';
var LANG = 'en';

var viewport;
var mainPagename = null;
var messagesElement = null;
var messagesElementOffsetTop;
var lastMessage = null;
var editingForm = null;
var editingFormIsLoading = false;
var editingFormIsChange = false;

var anchorLastUpdateUrl = false;

var redirectTimer = null;

var wikiHelperInfo = null;
var wikiHelperCompletionWindow = null;
var wikiHelperCompletionSelectIndex = -1;
var wikiHelperCompletionIsForce = false;
var wikiHelperCompletionEnableTemporary = false;

var userAgent = window.navigator.userAgent.toLowerCase();
var isIOSDevice = userAgent.indexOf('ipad') != -1 || userAgent.indexOf('iphone') != -1;
var isMobileDevice = isIOSDevice || userAgent.indexOf('android') != -1;

var config = {
    'completionEnable': {
	'type': 'boolean',
	'default': 'true',
	'description': 'Enable wiki notation completion'
    },
    'editDobuleClick': {
	'type': 'boolean',
	'default': 'true',
	'description': 'Edit by double click'
    },
    'editAltClick': {
	'type': 'boolean',
	'default': 'true',
	'description': 'Edit by Alt+click'
    },
    'editControlClick': {
	'type': 'boolean',
	'default': 'false',
	'description': 'Edit by Control+click'
    }
};

function main() {
    var mainSections = $$('.main [data-pagename]');
    if(mainSections.length > 0)
	mainPagename = mainSections[0].getAttribute('data-pagename');
    if(!mainPagename) {
	location.href.match(/\?([^&]*)/);
	var firstField = RegExp.$1;
	if(firstField.indexOf('=') == -1)
	    mainPagename = decodeURI(firstField);
    }
    
    viewport = document.viewport.getDimensions();
    if(typeof viewport.width == 'undefined') {
	if(window.innerWidth) {
	    viewport.width = window.innerWidth;
	    viewport.height = window.innerHeight;
	} else {
	    viewport = null;
	}
    }

    var html = $$('html[lang]');
    if(html.length == 1)
	LANG = html[0].getAttribute('lang');

    configSetup();
    messagesSetup();
    optionalSetup();
    scrollSetup();
    summarySetup();
    wikiEditSetup();
    wikiHelperSetup();
    tagEditSetup();
    urlCopySetup();
    revertWarningSetup();
    listAddSetup();
    templateInputSetup();
    formsHelperSetup();
    calendarHelperSetup();
    tableSortSetup();
    checkboxLabelSetup();
    anchorSetup();
    anchorUpdate();
    searchSetup();
    logoutSetup();
    redirectSetup();

    if(typeof prettyPrint == 'function')
	prettyPrint();
}

Element.prototype.observeEditAction = function(f) {
    var element = this;
    var editDobuleClick = configGet('editDobuleClick') == 'true';

    if(editDobuleClick) {
	if(typeof this.ontouchstart != 'undefined') {
	    this.observe('touchstart', function(event) {
		if(event.touches.length < 2) {
		    var isMatch = false;

		    if(element.doubleClickPosition) {
			var dx = (element.doubleClickPosition.pageX - event.touches[0].pageX);
			var dy = (element.doubleClickPosition.pageY - event.touches[0].pageY);
			var distance = dx * dx + dy * dy;
			var thr;
			if(viewport)
			    thr = viewport.width * 0.05;
			else
			    thr = 30;
			if(distance < thr * thr) {
			    isMatch = true;
			}
		    }
		    if(isMatch) {
			event.stop();
			//console.log('dbl!' + ((new Date)/1000));
			if(element.timer)
			    clearTimeout(element.timer);
			f(event);
			element.doubleClickPosition = false;
		    } else {
			//element.doubleClickPosition = event.touches[0];
			setTimeout(function() {
			    element.doubleClickPosition = event.touches[0];
			}, 10);
			setTimeout(function() {
			    element.doubleClickPosition = false;
			}, 200);
		    }
		} else {
		    element.doubleClickPosition = false;
		}
		return false;
	    });
	}
	this.observe('dblclick', function(event){
	    if(element.timer)
		clearTimeout(element.timer);
	    f(event);
	});
    }

    if(configGet('editAltClick') == 'true') {
	this.observe('click', function(event){
	    if(event.altKey)
		f(event);
	});
    }

    if(configGet('editControlClick') == 'true') {
	this.observe('click', function(event){
	    if(event.ctrlKey)
		f(event);
	});
    }
}

function messagesSetup() {
    var messagesElements = $$('ul.messages');
    if(messagesElements.length <= 0)
	return;
    messagesElement = messagesElements[0];
    $A(messagesElement.childNodes).each(messagesAddCloseButton);

    messagesElement.adjust = function() {
	if(messagesElement.getHeight() > messagesElement.parentNode.getHeight() * 0.8) {
	    messagesElement.style.top = '0px';
	} else {
	    messagesElement.style.top =
		Math.max(0, document.body.cumulativeScrollOffset().top - messagesElementOffsetTop) + 'px';
	}
    };

    if(messagesElement.hasChildNodes())
	messagesAdjustStart();
}

function messagesAdjustStart() {
    messagesElement.adjust();
    Element.observe(window, 'scroll', function(event) {
	messagesElement.adjust();
    });
}

function messagesPositionUpdate() {
    messagesElementOffsetTop = messagesElement.cumulativeOffset().top;
}

function messagesAddCloseButton(element) {
    var close = document.createElement('button');
    close.setAttribute('type', 'button');
    close.innerHTML = '&times;';
    element.insertBefore(close, element.firstChild);
    close.observe('click', function(event) { 
	element.hide();
	lastMessage = null;
    });
}

function messagesAdd(level, msg) {
    if(!messagesElement)
	return;
    if(lastMessage == msg)
	return;
    if(!messagesElement.hasChildNodes())
	messagesAdjustStart();
    var li = document.createElement('li');
    li.addClassName(level);
    li.innerHTML = msg;
    messagesAddCloseButton(li);
    messagesElement.appendChild(li);
    lastMessage = msg;
}

function scrollSetup() {
    var scrollPage = decodeURIComponent(cookieGet('scrollpage'));
    var scrollLeft = cookieGet('scrollleft');
    var scrollTop = cookieGet('scrolltop');
    if(scrollLeft || scrollTop) {
	if(scrollPage == mainPagename)
	    $(window).scroll(scrollLeft, scrollTop);
	cookieSet('scrollpage', '');
	cookieSet('scrollleft', '');
	cookieSet('scrolltop', '');
    }

    $$('section.page form').each(function(form) {
	form.observe('submit', function(event) {
	    scrollSet();
	});
    });

    $$('table.calendar a').each(function(element) {
	element.observe('click', function(event) {
	    scrollSet();
	});
    });
}
    
function scrollSet() {
    var offset = document.body.cumulativeScrollOffset();
    cookieSet('scrollpage', encodeURIComponent(mainPagename));
    cookieSet('scrollleft', offset.left);
    cookieSet('scrolltop', offset.top);
}

function wikiEditSetup() {
    $$('section.page').each(function(element) {
	var isWikiPage = element.down('[data-twp]');
	if(isWikiPage) {
	    element.setAttribute('data-twp', 0);
	    element.setAttribute('data-twl', -1);
	}
	element.wikiEditObserve();
    });

    var pageSectionElement = $$('.main > section.page')[0];
    if(pageSectionElement) {
	$$('.main > h1').each(function(element) {
	    element.observeEditAction(pageSectionElement.wikiEditStartHandler.bind(pageSectionElement));
	});
    }
    
    $$('[data-twp][data-twl]').each(function(element) {
	if(element.match('section.list li, tr') || element.match('section.section > h1'))
	    return;
	element.wikiEditObserve();
    });

    $$('table').each(function(element) {
	if(!element.down('td[rowspan], td[colspan]')) {
	    element.select('tr[data-twp][data-twl]').each(function(element) {
		element.wikiEditObserve();
	    });
	}
    });

    $$('[data-ref-pagename]').each(function(element) {
	element.wikiRefPageObserve();
    });

    $(document).observe('keydown', function(event) {
	if(event.keyCode == 27)
	    wikiEditReset();
    }); 

    if(location.href.match(/action=edit\b/) && !cookieGet('partial_edit_info')) {
	var typeElement = $$('dl.page_info dd.type')[0];
	if(typeElement && typeElement.textValue() == 'wiki') {
	    messagesAdd('info', l('You can edit this page by double-clicking, too.'));
	    cookieSetLong('partial_edit_info');
	}
    }
}

Element.prototype.wikiEditObserve = function() {
    this.observeEditAction(this.wikiEditStartHandler.bind(this));
}

Element.prototype.wikiEditStartHandler = function(event, isForceText) {
    if(editingFormIsLoading) {
	event.stop();
	return false;
    }
    if(editingFormIsChange) {
	event.stop();
	messagesAdd('error', l('You are editing other part of this page.'));
	return false;
    }
    if(editingForm && this.hasAttribute('data-save')) {
	return false;
    }
    if(this.avoidEdit) {
	return true;
    }
    event.stop();
    editingFormIsLoading = true;

    var pageElement;
    if(this.hasAttribute('data-pagename'))
	pageElement = this;
    else
	pageElement = this.up('[data-pagename]');
    this.wikiPagename = pageElement.getAttribute('data-pagename');
    this.wikiTicket = pageElement.getAttribute('data-ticket');
    this.wikiPosition = this.getAttribute('data-twp');
    this.wikiLength = this.getAttribute('data-twl');
    this.wikiOptions = this.getAttribute('data-options');

    if(this.wikiPosition == null || !this.wikiPosition == null) {
	var url = '?' + urlPathEncode(this.wikiPagename) + '&' + 
	    $H({'action': 'edit'}).toQueryString();
	editingFormIsLoading = false;
	location.href = url;
	return false;
    }

    var isIncludePartialEdit = this.select('form.text_edit.partial').length != 0;
    wikiEditReset();
    if(this.match('section.list') && !isForceText && !isIncludePartialEdit) {
	this.wikiEditMakeList();
    } else if(this.wikiOptions != null){
	this.wikiEditMakeSelector();
    } else {
	this.wikiEditMakeTextarea();
    }

    return false;
}

Element.prototype.wikiEditMakeTextarea = function(request) {
    var hiddens = {
	'page': mainPagename,
	'pagename': this.wikiPagename,
	'option': 'replace',
	'action': 'write',
	'redirect': location.href,
	'ticket': this.wikiTicket,
	'position': this.wikiPosition,
	'length': this.wikiLength};

    var isContainer = ['TD', 'TH', 'TR', 'DT', 'DD'].indexOf(this.tagName) != -1;
    var isSimple = isContainer && ['TD', 'TH', 'TR'].indexOf(this.tagName) != -1;
    var endsLf;

    var form = wikiEditCreateTextForm(hiddens, function(event) {
	if(endsLf)
	    form.textarea.value += '\n';
	scrollSet();
	editingForm.submit();
    }, isSimple);
    form.addClassName('partial');
    form.textarea.disable();

    if(this.tagName == 'LI') {
	this.setAttribute('data-save', this.innerHTML);
	while(this.hasChildNodes() && this.firstChild.tagName != 'UL' && this.firstChild.tagName != 'OL') {
	    this.removeChild(this.firstChild);
	}
	this.insertBefore(form, this.firstChild);
    } else if(this.tagName == 'TR') {
	var tr = $(document.createElement('tr'));
	var td = $(document.createElement('td'));
	td.setAttribute('colspan', this.cells.length);
	this.parentNode.insertBefore(tr, this);
	tr.appendChild(td);
	td.appendChild(form);
	form.removeElement = tr;
	form.showElement = this;
	form.addClassName('row');
	this.hide();
    } else if(isContainer) {
	this.setAttribute('data-save', this.innerHTML);
	this.clearChildren();
	this.appendChild(form);
    } else {
	this.parentNode.insertBefore(form, this);
	this.hide();
	form.showElement = this;
    }
    editingForm = form;
    form.textarea.fitHeight();

    if(isIOSDevice) { // need .focus() in onclick handler
	form.textarea.enable();
	form.textarea.focus();
    }

    var url = '?' + urlPathEncode(this.wikiPagename) + '&' + 
	$H({'option': 'partial',
	    'ticket': this.wikiTicket,
	    'position': this.wikiPosition,
	    'length': this.wikiLength}).toQueryString();
    var ajax = new Ajax.Request(url, {
	method: 'get',
	onSuccess: function(request) {
	    var response = wikiEditResponseGet(request);
	    editingFormIsLoading = false;
	    if(response === false) {
		return;
	    }
	    endsLf = response.source.match(/\n$/);
	    if(endsLf)
		response.source = response.source.replace(/\n$/, '');
	    if(isContainer)
		form.textarea.value = response.source.replace(/\s+$/g, '');
	    else
		form.textarea.value = response.source;
	    form.textarea.originalValue = form.textarea.value;
	    form.textarea.enable();
	    form.textarea.focus();
	    form.textarea.fitHeight();
	}
    });
}

Element.prototype.wikiEditMakeList = function() {
    editingFormIsLoading = false;

    var originalListSection = this;
    if(originalListSection.initializers)
	originalListSection.initializers.each(function(f){f();});
    var editListSection = originalListSection.cloneNode(true);
    originalListSection.hide();
    originalListSection.parentNode.insertBefore(editListSection, originalListSection);
    editingForm = editListSection;
    editingForm.showElement = originalListSection;
    editListSection.addClassName('list_edit');

    var selectMode = 'item';

    editListSection.select('li.listadd').each(function(li) { li.hide(); });
    editListSection.select('li.optional').each(function(li) { li.optionalRemove(); });

    editListSection.select('li[data-twp][data-twl]').each(function(li) {
	if(li.up('section.list') != editListSection)
	    return;
	li.select('a').each(function(a) {
	    a.observe('click', function(event) {
		event.throughClick(a.parentNode);
	    });
	});
	li.observe('click', function(event) {
	    if(event.altKey || event.ctrlKey)
		return;
	    if(selectMode == 'item') {
		li.toggleClassName('selected');
	    } else {
		var nextClassName = null;
		if(li.hasClassName('selected_bottom'))
		    nextClassName = 'selected_top';
		else
		    nextClassName = 'selected_bottom';
		editListSection.clearListItemSelection();
		if(nextClassName)
		    li.addClassName(nextClassName);
	    }
	    submit.updateForm();
	    event.stop();
	});
	var twp = li.getAttribute('data-twp');
	var twl = li.getAttribute('data-twl');
	var targetLi = originalListSection.down('[data-twp="' + twp + '"][data-twl="' + twl + '"]');
	li.observeEditAction(targetLi.wikiEditStartHandler.bind(targetLi));
    });

    var hiddens = {
	'page': mainPagename,
	'pagename': this.wikiPagename,
	'option': 'listedit',
	'action': 'write',
	'redirect': location.href,
	'ticket': this.wikiTicket
    };

    var listEditSubmit = function(event, mode, value) {
	var targets = [];
	editListSection.select('li.selected').each(function(li) {
	    var p, l;
	    if(mode == 'strike') {
		p = li.getAttribute('data-twlp');
		l = li.getAttribute('data-twll');
	    } else {
		p = li.getAttribute('data-twp');
		l = li.getAttribute('data-twl');
	    }
	    if(p && l)
		targets.push(p + ':' + l);
	});

	hiddens.mode = mode;
	hiddens.value = value;
	hiddens.targets = targets.join(',');
	var form = createForm(hiddens);
	form.addClassName('partial');
	form.style.display = 'none';
	form.setAttribute('method', 'POST');
	form.setAttribute('action', '?' + urlPathEncode(mainPagename));
	editListSection.appendChild(form);
	scrollSet();
	form.submit();
    };

    var insertFormList = editListSection.select('>ul, >ol').last();
    var insertLi = new Element('li');
    insertFormList.appendChild(insertLi);
    var form = createForm(hiddens);
    insertLi.appendChild(form);
    var insertInput = new Element('input', {'type': 'text', 'name': 'value', 'required':'required'});
    insertInput.wikiHelperCompletionSetup();

    var replyNote = new Element('small').update(l('Reply to selected item'));
    form.appendChild(replyNote);
    form.appendChild(insertInput);

    var submit = new Element('input', {'type': 'submit', 'name': 'after', 'accesskey': 's'});
    form.appendChild(submit);

    var modeButton = new Element('button', {'type': 'button'}).update(l('Select a location'));
    form.appendChild(modeButton);
    modeButton.observe('click', function(event) {
	if(selectMode == 'item') {
	    var selectedLi = editListSection.down('li.selected');
	    editListSection.clearListItemSelection();
	    if(selectedLi)
		selectedLi.addClassName('selected_bottom');
	    selectMode = 'position';
	    modeButton.update(l('Abort'));
	} else {
	    editListSection.clearListItemSelection();
	    selectMode = 'item';
	    modeButton.update(l('Select a location'));
	}
	submit.updateForm();
	event.stop();
    });
    
    form.observe('submit', function(event) {
	event.stop();
	var topLi = editListSection.down('li.selected_top');
	var bottomLi = editListSection.down('li.selected_bottom');
	var p, l;
	if(topLi) {
	    p = topLi.getAttribute('data-twp');
	    l = topLi.getAttribute('data-twl')
	    hiddens.mode = 'insert_before';
	} else if(bottomLi) {
	    targetLi = bottomLi;
	    p = targetLi.getAttribute('data-twp');
	    l = targetLi.getAttribute('data-twl')
	    var lastChild = bottomLi;
	    var lastPosition = parseInt(p) + parseInt(l);
	    bottomLi.select('[data-twp][data-twl]').each(function(child) {
		var pos = parseInt(child.getAttribute('data-twp')) +
		    parseInt(child.getAttribute('data-twl'));
		if(lastPosition < pos) {
		    lastChild = child;
		    lastPosition = pos;
		}
	    });
	    l = lastPosition - parseInt(p);
	    hiddens.mode = 'insert_after';
	} else {
	    var parentLis = editListSection.select('li.selected');
	    if(parentLis.length == 1) {
		var parentLi = parentLis[0];
		hiddens['parent_twp'] = parentLi.getAttribute('data-twp');
		hiddens['parent_twl'] = parentLi.getAttribute('data-twl');
		parentLi.select('section.list li').each(function(li) {
		    li.removeAttribute('data-twp');
		});
		var lastLis = parentLi.select('li[data-twp][data-twl]');
		if(lastLis.length >= 1) {
		    var lastLi = lastLis.last();
		    p = lastLi.getAttribute('data-twp');
		    l = lastLi.getAttribute('data-twl');
		} else {
		    p = parentLi.getAttribute('data-twp');
		    l = parentLi.getAttribute('data-twl');
		}
		hiddens['mode'] = 'insert_after';
	    } else {
		p = editListSection.getAttribute('data-twp');
		l = editListSection.getAttribute('data-twl');
		hiddens.mode = 'insert_after';
	    }
	}

	hiddens.value = insertInput.value;
	hiddens.targets = p + ':' + l;
	var form = createForm(hiddens);
	form.addClassName('partial');
	form.style.display = 'none';
	form.setAttribute('method', 'POST');
	form.setAttribute('action', '?' + urlPathEncode(mainPagename));
	editListSection.appendChild(form);
	scrollSet();
	form.submit();
    });

    var p = new Element('p');
    editListSection.appendChild(p);
    p.update(l('Selected items will be') + ' ');

    var ul = new Element('ul');
    ul.addClassName('buttons');
    var li;
    p.appendChild(ul);

    var button = new Element('button');
    button.setAttribute('type', 'button');
    li = new Element('li');
    ul.appendChild(li);
    li.appendChild(button);
    button.update(l('Striked'));
    button.observe('click', function(event) { listEditSubmit(event, 'strike', ''); });

    var button = new Element('button');
    button.setAttribute('type', 'button');
    li = new Element('li');
    ul.appendChild(li);
    li.appendChild(button);
    button.update(l('Deleted'));
    button.observe('click', function(event) { listEditSubmit(event, 'delete', ''); });

    var button = new Element('button');
    button.setAttribute('type', 'button');
    li = new Element('li');
    ul.appendChild(li);
    li.appendChild(button);
    button.update(l('Edited'));
    button.observe('click', function(event) {
	var target = null;
	var editLis = editListSection.select('li.selected');
	target = originalListSection;
	if(editLis.length == 1) {
	    target = originalListSection.down(
		'[data-twp="' + editLis[0].getAttribute('data-twp') + '"]' + 
		    '[data-twl="' + editLis[0].getAttribute('data-twl') + '"]');
	}
	target.wikiEditStartHandler(event, true);
    });
    
    var button = new Element('button', {'type': 'button', 'class': 'close'}).update('&times;');
    button.observe('click', wikiEditReset);
    editListSection.appendChild(button);
    
    var initialListStyleType = insertLi.style.listStyleType;

    submit.updateForm = function() {
	replyNote.hide();
	insertLi.style.listStyleType = initialListStyleType;
	if(selectMode == 'item') {
	    if(editListSection.select('li.selected').length == 1) {
		submit.setAttribute('value', l('Insert a child item'));
		replyNote.show();
		insertLi.style.listStyleType = 'none';
	    } else
		submit.setAttribute('value', l('Insert an item'));
	} else {
	    submit.setAttribute('value', l('Insert an item'));
	    insertLi.style.listStyleType = 'none';
	}
    };
    submit.updateForm();

    editListSection.observeEditAction(function(event) { originalListSection.wikiEditStartHandler(event, true); });
}

Element.prototype.wikiEditMakeSelector = function(request) {
    var hiddens = {
	'page': mainPagename,
	'pagename': this.wikiPagename,
	'option': 'replace',
	'action': 'write',
	'redirect': location.href,
	'ticket': this.wikiTicket,
	'position': this.wikiPosition,
	'length': this.wikiLength};
    var form = createForm(hiddens);
    form.addClassName('partial');
    form.setAttribute('method', 'POST');

    this.setAttribute('data-save', this.innerHTML);
    var defaultValue;
    if(this.hasAttribute('data-option-value'))
	defaultValue = this.getAttribute('data-option-value');
    else
	defaultValue = this.textValue();
    
    this.clearChildren();
    this.appendChild(form);
    var select = document.createElement('select');
    select.setAttribute('name', 'value');
    form.appendChild(select);
    var isMatchDefaultValue = false;
    var options = this.wikiOptions.split('/');
    for(var i = 0; i < options.length; i++) {
	var option = new Element('option');
	var fields = options[i].split(':');
	var show, value;
	if(fields.length == 1)
	    show = value = fields[0];
	else {
	    show = fields[0];
	    value = fields[1];
	}
	select.appendChild(option);
	option.appendChild(document.createTextNode(show));
	option.setAttribute('value', value);
	if(defaultValue == value) {
	    option.setAttribute('selected', 'selected');
	    isMatchDefaultValue = true;
	}
    }
    if(!isMatchDefaultValue) {
	select.insertBefore(new Element('option', {'value': '', 'selected': 'selected'}), select.firstChild);
    }
    select.observe('change', function(event) { scrollSet(); form.submit(); });
    
    editingForm = form;
    editingFormIsLoading = false;
}

Element.prototype.wikiRefPageObserve = function() {
    this.observeEditAction(this.wikiRefPageStartHandler.bind(this));
}

Element.prototype.wikiRefPageStartHandler = function(event) {
    event.stop();

    if(editingFormIsLoading)
	return;
    if(editingFormIsChange) {
	messagesAdd('error', l('You are editing other part of this page.'));
	return;
    }
    editingFormIsLoading = true;

    this.wikiPagename = this.getAttribute('data-ref-pagename');
    this.wikiTicket = this.getAttribute('data-ref-ticket');

    wikiEditReset();
    this.wikiRefPageMakeTextarea();
}

Element.prototype.wikiRefPageReset = function(event) {
    if(editingFormIsLoading)
	return;
    if(editingFormIsChange) {
	messagesAdd('error', l('You are editing other part of this page.'));
	return;
    }
    editingFormIsLoading = false;
    wikiEditReset();
    //$$('body')[0].stopObserving('click', this.wikiRefPageResetHandler);
}

Element.prototype.wikiRefPageMakeTextarea = function(request) {
    var body = $$('body')[0];

    var hiddens = {
	'page': mainPagename,
	'pagename': this.wikiPagename,
	'option': 'replace',
	'action': 'write',
	'redirect': location.href,
	'ticket': this.wikiTicket,
	'position': 0,
	'length': -1};

    var form = wikiEditCreateTextForm(hiddens, function(event) {
	scrollSet();
	form.submit();
    }, false);
    form.addClassName('refpage');
    form.textarea.disable();

    var h = document.createElement('h1');
    h.appendChild(document.createTextNode(this.wikiPagename));
    form.insertBefore(h, form.firstChild);

    var section = $(document.createElement('section'));
    section.addClassName('refpage');
    section.appendChild(form);
    
    //body.insertBefore(section, body.firstChild);
    body.appendChild(section);
    editingForm = section;
    form.textarea.fitHeight();

    //this.wikiRefPageResetHandler = this.wikiRefPageReset.bind(this);
    //cover.observe('click', this.wikiRefPageResetHandler);
    form.observe('click', function(event) {
	event.stop();
    });
    section.observe('click', this.wikiRefPageReset.bind(this));

    var url = '?' + urlPathEncode(this.wikiPagename) + '&' + 
	$H({'option': 'partial', 'ticket': this.wikiTicket, 'position': 0, 'length': -1}).toQueryString();
    var ajax = new Ajax.Request(url, {
	method: 'get',
	onSuccess: function(request) {
	    var response = wikiEditResponseGet(request);
	    editingFormIsLoading = false;
	    if(response === false)
		return;
	    form.textarea.value = response.source;
	    form.textarea.originalValue = form.textarea.value;
	    form.textarea.enable();
	    form.textarea.focus();
	    form.textarea.fitHeight();
	}
    });
}

Element.prototype.wikiHelperSetup = function() {
    var form = this;
    var textarea = this.down('textarea');
    var ul = form.down('ul');
    form.reset(); // avoid ignoring confrict by browser form value restore

    textarea.fixTextarea();

    var attachUl = document.createElement('ul');
    attachUl.addClassName('attaches');
    //attachUl.style.position = 'relative';
    attachUl.hide();
    form.appendChild(attachUl);

    var isTypeText = form.match('.type_text');

    var helpers = {};

    if(!isTypeText) {
	helpers['Quote'] = {
	    'label': 'Quote',
	    'description': l('Quotationize or Preformation the selection.'),
	    'click': function() {
		replaceSelectedFormInputValue(textarea, function(str) {
		    var lines = str.split("\n");
		    var isQuoted = true;
		    var isPred = true;
		    for(var i = 0; i < lines.length; i++) {
			var c = lines[i].charAt(0);
			if(c != " ") isPred = false;
			if(c != ">") isQuoted = false;
		    }
		    if(isQuoted)
			return str.replace(/^>/, ' ').replace(/\n>/g,"\n ");
		    if(isPred)
			return str.replace(/^ /, '').replace(/\n /g,"\n");
		    return '>' + str.replace(/\n/g,"\n>");
		});
	    }
	};
    }

    helpers['Replace'] = {
	'label': 'Replace',
	'description': l('Replace the word in the selection'),
	'click': function() {
	    var regex = window.prompt(l('Input regex for matching'), '');
	    if(regex == null)
		return;
	    var replacement = window.prompt(l('Input replacement (\'$1\' means backreference)'), '');
	    if(replacement == null)
		return;
	    var pattarn = new RegExp(regex,'gm');
	    var isSelect = false;
	    replaceSelectedFormInputValue(textarea, function(str) {
		if(str == '')
		    return str;
		isSelect = true;
		return str.replace(pattarn, replacement);
	    });
	    if(!isSelect)
		textarea.value = textarea.value.replace(pattarn, replacement);
	}
    };

    if(!isTypeText) {
	helpers['Include'] = {
	    'label': 'Include',
	    'description': l('Embed the attachment file'),
	    'click': function(li) {
		if(attachUl.visible()) {
		    attachUl.hide();
		    return;
		}
		
		var isPartial = form.match('.partial');
		var pageElement = form.up('[data-pagename]');
		wikiPagename = form.down('input[name="pagename"]').value;

		var url = '?' + urlPathEncode(wikiPagename) + '&' + 
		    $H({'option': 'attach', 'action': 'list'}).toQueryString();
		var ajax = new Ajax.Request(url, {
		    method: 'get',
		    onSuccess: function(request) {
			while(attachUl.hasChildNodes())
			    attachUl.removeChild(attachUl.firstChild);
			var response = wikiEditResponseGet(request);
			if(response['pages'].length <= 0) {
			    messagesAdd('notice', l('No file type sub page.'));
			    return;
			}
			for(var i = 0; i < response['pages'].length; i++) {
			    var li = document.createElement('li');
			    var attachButton = document.createElement('button');
			    var attachPagename = response['pages'][i]['relativename'];
			    attachButton.setAttribute('type', 'button');
			    attachButton.appendChild(document.createTextNode(attachPagename.replace(/^.\//, '')));
			    (function(attachPagename) {
				attachButton.observe('click', function(event) {
				    Event.element(event).addClassName('clicked');
				    replaceSelectedFormInputValue(textarea, function(str) {
					editingFormIsChange = true;
					return str + '&include([[' + attachPagename + ']]);';
				    });
				});
			    })(attachPagename);
			    li.appendChild(attachButton);
			    attachUl.appendChild(li);
			}
			if(isPartial) {
			    //attachUl.style.bottom = ul.getHeight() + 'px';
			    //attachUl.style.right = attachUl.getWidth() + 'px';
			}
			attachUl.show();
		    }
		});
	    }
	};
    }

    for(var key in helpers) {
	var helper = helpers[key];
	var li = document.createElement('li');
	li.addClassName('helper');
	var button = document.createElement('button');
	button.innerHTML = l(helper['label']);
	button.setAttribute('type', 'button');
	button.addClassName('helper');
	if(helper['description'])
	    button.setAttribute('title', helper['description']);
	li.appendChild(button);
	(function(helper, li) {
	    button.observe('click', function(event) {
		var before = textarea.value;
		helper['click'](li);
		if(before != textarea.value)
		    editingFormIsChange = true;
	    });
	})(helper, li);
	ul.appendChild(li);
    }
}

Element.prototype.wikiHelperCompletionSetup = function() {
    var completionEnable = configGet('completionEnable') == 'true';

    var textarea = this;
    var body = $$('body')[0];
    var url = '?option=helper';
    var ajax = new Ajax.Request(url, {
	method: 'get',
	onSuccess: function(request) {
	    wikiHelperInfo = request.responseText.evalJSON();
	}
    });
    var targetPagename = null;
    var pagenameGroups = ['starts', 'includes'];
    var completionDone = false;

    textarea.setAttribute('autocomplete', 'off');

    if(!wikiHelperCompletionWindow) {
	wikiHelperCompletionWindow = new Element('section');
	wikiHelperCompletionWindow.addClassName('completion_window');
	wikiHelperCompletionWindowClose();

	wikiHelperCompletionWindow.list = $(document.createElement('ul'));
	wikiHelperCompletionWindow.appendChild(wikiHelperCompletionWindow.list);

	body.appendChild(wikiHelperCompletionWindow);
    }

    var candidateNum = 0;
    var updateCompletionWindow = function() {
	if(!wikiHelperInfo)
	    return;
	var position = textarea.getCursorPosition();
	var nearString = textarea.value.slice(0, position);
	var afterChar = textarea.value.charAt(position);
	var nearStrings = {'linehead': '', 'inline': '', 'function': ''};

	if(!targetPagename) {
	    targetPagename = mainPagename;
	    var pageElement = textarea.up('[data-pagename]');
	    if(pageElement)
		targetPagename = pageElement.getAttribute('data-pagename');
	}

	if((position = nearString.lastIndexOf("\n")) != -1)
	    nearString = nearString.substring(position + 1);

	nearString = nearString.replace(/&[^a-zA-Z0-9]/g, '  ');
	nearString = nearString.replace(/'''([^']*)'''/g, '   $1   ');
	nearString = nearString.replace(/''([^']*)''/g, '  $1  ');
	nearString = nearString.replace(/""([^"]*)""/g, '  $1  ');
	nearString = nearString.replace(/%%([^%]*)%%/g, '  $1  ');
	nearString = nearString.replace(/\[\[([^\]]*)\]\]/g, '  $1  ');

	nearStrings['linehead'] = nearString;
	if((position = nearString.lastIndexOf("}")) != -1)
	    nearString = nearString.substring(position + 1);
	if((position = nearString.lastIndexOf(";")) != -1)
	    nearString = nearString.substring(position + 1);

	if((position = nearString.lastIndexOf("&")) != -1)
	    nearStrings['function'] = nearString.substring(position);
	if(nearString.match(/(['%\["]+[^'%\["]*)$/))
	    nearStrings['inline'] = RegExp.$1;

	var isLineHead = afterChar == "\n" || afterChar == "";

	var pagenamePosition = nearString.lastIndexOf('[[');
	var isPagename = false;
	var pagenameString = '';
	if(pagenamePosition != -1) {
	    pagenameString = nearString.substring(pagenamePosition).substring(2);
	    if(pagenameString.match(/^.*(:|>|>>|\|)(.*)$/))
		pagenameString = RegExp.$2;
	    isPagename = pagenameString.lastIndexOf(']]') == -1;
	}
	    
	wikiHelperCompletionWindow.list.clearChildren();
	wikiHelperCompletionWindow.style.width = 'auto';
	var index = 0;
	var selectedLi = null;

	if(isPagename) {
	    var isRelative = pagenameString.startsWith('./') || pagenameString.startsWith('../');
	    var inputPagename = pagePathToPagename(pagenameString, targetPagename);
	    var inputPagenameLower = inputPagename.toLowerCase();
	    var inputDirnameLower = inputPagenameLower.replace(/[^\/]*$/, '');
	    var inputBasenameLower = inputPagenameLower.replace(/^.*\//, '');
	    if((pagenameString.endsWith('/') || pagenameString.endsWith('/..')) && inputBasenameLower != '') {
		inputDirnameLower += inputBasenameLower + '/';
		inputBasenameLower = '';
	    }

	    var candidatePagenameGroups = {};
	    pagenameGroups.each(function(group) { candidatePagenameGroups[group] = []; });

	    for(var i = 0; i < wikiHelperInfo['pagenames'].length; i++) {
		var candidatePagename = wikiHelperInfo['pagenames'][i];
		var candidatePagenameLower = candidatePagename.toLowerCase();
		if(!candidatePagenameLower.startsWith(inputDirnameLower))
		    continue;
		var candidateBasenameLower = candidatePagenameLower.substring(inputDirnameLower.length);
		if(inputBasenameLower == '' ||
		   candidateBasenameLower.indexOf(inputBasenameLower) != -1) {
		    var candidatePathname = candidatePagename;
		    if(isRelative)
			candidatePathname = pageRelativePath(targetPagename, candidatePagename);
		    (candidateBasenameLower.startsWith(inputBasenameLower) ?
		     candidatePagenameGroups['starts'] : candidatePagenameGroups['includes'])
			.push({'pagename': candidatePagename, 'pathname': candidatePathname});
		}
	    }

	    pagenameGroups.each(function(group) {
		candidatePagenameGroups[group].each(function(candidate) {
		    if(index > 200)
			return;
		    var candidatePathname = candidate['pathname'];
		    (function(candidatePathname) {
			li = wikiHelperCompletionAppendCandidateLi(candidatePathname, candidate, index++, function() {
			    var position = textarea.getCursorPosition();
			    var afterString = textarea.value.substring(position);
			    if((position = afterString.indexOf("\n")) != -1)
				afterString = afterString.substring(0, position);
			    if((position = afterString.indexOf(",")) != -1)
				afterString = afterString.substring(0, position);
			    if((position = afterString.indexOf("[[")) != -1)
				afterString = afterString.substring(0, position);
			    var isClose = false;
			    if((position = afterString.indexOf("]]")) != -1) {
				afterString = afterString.substring(0, position);
				isClose = true;
			    } else {
				afterString = '';
			    }
			    completionDone = true;
			    replaceSelectedFormInputValue(textarea, function(str) {
				return candidatePathname + (isClose ? '' : ']]');
			    }, -pagenameString.length, afterString.length);
			});
		    })(candidatePathname);
		});
	    });
	}

	var dateCompletion = false;
	if(nearString.match(/((\d\d\d\d)([-\/年])(\d+)[-\/月]?(\d*?))$/)) {
	    var inputDate = RegExp.$1;
	    var year = parseInt(RegExp.$2);
	    var sep = RegExp.$3;
	    var month = parseInt(RegExp.$4);
	    var day = RegExp.$5;
	    dateCompletion = 1900 <= year && year <= 2100 && 1 <= month && month <= 12;
	} else if(nearString.match(/((\d+)([-\/月])(\d+))$/)) {
	    var now = new Date();
	    var inputDate = RegExp.$1;
	    var year = now.getFullYear();
	    var sep = RegExp.$3;
	    var month = parseInt(RegExp.$2);
	    var day = RegExp.$4;
	    if(sep == '月')
		sep = '年';
	    dateCompletion = 1 <= month && month <= 12;
	} else if(nearString.match(/\b(tod(ay?)?|yest(e(r(d(ay?)?)?)?)?|tom(o(r(r(ow?)?)?)?)?)$/i)) {
	    var d = new Date();
	    switch(RegExp.$1.substring(0, 3)) {
	    case 'yes': d.setDate(d.getDate() - 1); break;
	    case 'tom': d.setDate(d.getDate() + 1); break;
	    }
	    var inputDate = RegExp.$1;
	    var year = d.getFullYear();
	    var sep = '-';
	    var month = d.getMonth() + 1;
	    var day = d.getDate();
	    month = (month < 10 ? '0' : '') + month;
	    day = (day < 10 ? '0' : '') + day;
	    dateCompletion = true;
	}
	if(dateCompletion) {
	    d = new Date();
	    var todayString = d.toDateString();
	    d.setDate(d.getDate() - 1);
	    var yesterdayString = d.toDateString();
	    d.setDate(d.getDate() + 2);
	    var tomorrowString = d.toDateString();
	    for(var d = 1; d <= 31; d++) {
		var date = new Date(year, month - 1, d);
		if(date.getMonth() != month - 1)
		    break;
		var inputString, dateString;
		if(sep == '年') {
		    inputString = year + sep + month + '月' + day;
		    dateString = year + sep +  month + '月' + d + '日';
		} else {
		    var monthString = ((typeof month != 'string' && month < 10) ? '0' : '') + month;
		    inputString = year + sep + monthString + sep + day;
		    dateString = year + sep + monthString + sep + (d < 10 ? '0' : '') + d;
		}
		if(dateString.startsWith(inputString) || day == d) {
		    (function(dateString) {
			var dayOfWeek = l(DAYOFWEEKS[date.getDay()]);
			dateString = dateString + '(' + dayOfWeek + ')';
			var cString = date.toDateString();
			var dateDescription = l('Date');
			if(cString == todayString)
			    dateDescription = l('Today');
			else if(cString == yesterdayString)
			    dateDescription = l('Yesterday');
			else if(cString == tomorrowString)
			    dateDescription = l('Tomorrow');
			
			li = wikiHelperCompletionAppendCandidateLi(dateString, {'date': dateDescription},
								   index++, function() {
			    completionDone = true;
			    replaceSelectedFormInputValue(textarea, function(str) {
				return dateString;
			    }, -inputDate.length, 0);
			});
		    })(dateString);
		}
	    }
	}

	for(start in wikiHelperInfo['completion']['candidates'])  {
	    var candidate = wikiHelperInfo['completion']['candidates'][start];
	    nearString = nearStrings[candidate['type']];

	    if(candidate['type'] == 'linehead' && !isLineHead)
		continue;

	    if(candidate['type'] == 'function') {
		if(nearString.length <= 0)
		    continue;
		var re = new RegExp('^' + start + '\\b');
		if(nearString.match(re)) {
		    for(var i = 0; i < candidate['examples'].length; i++) {
			var example = candidate['examples'][i];
			if(nearString == example)
			    continue;
			(function(example, nearString) {
			    li = wikiHelperCompletionAppendCandidateLi(example, candidate, index++, function() {
				var position = textarea.getCursorPosition();
				var afterString = textarea.value.substring(position);
				if((position = afterString.indexOf("\n")) != -1)
				    afterString = afterString.substring(0, position);
				if((position = afterString.indexOf(";")) != -1)
				    afterString = afterString.substring(0, position + 1);
				if((position = afterString.indexOf("}")) != -1)
				    afterString = afterString.substring(0, position + 1);
				replaceSelectedFormInputValue(textarea, function(str) {
				    return example;
				}, -nearString.length, afterString.length);
			    });
			})(example, nearString);
		    }
		    continue;
		}
		if(!(start.startsWith(nearString) && start != nearString))
		    continue;
		(function(start, nearString) {
		    li = wikiHelperCompletionAppendCandidateLi(start, candidate, index++, function() {
			replaceSelectedFormInputValue(textarea, function(str) {
			    return str + start.substring(nearString.length);
			});
		    });
		})(start, nearString);
	    } else {
		if(wikiHelperCompletionIsForce) {
		    if(nearString.length <= 0 && candidate['type'] != 'linehead' && 
		       nearStrings['linehead'] == '' && isLineHead)
			continue;
		} else {
		    if(nearString.length <= 0)
			continue;
		}
		if(!start.startsWith(nearString))
		    continue;
		for(var i = 0; i < candidate['examples'].length; i++) {
		    var example = candidate['examples'][i];
		    if(nearString == example)
			continue;
		    (function(example, nearString) {
			li = wikiHelperCompletionAppendCandidateLi(example, candidate, index++, function() {
			    replaceSelectedFormInputValue(textarea, function(str) {
				return str + example.substring(nearString.length);
			    });
			});
		    })(example, nearString);
		}
	    }
	}

	selectedLi = wikiHelperCompletionWindow.list.down('li.selected');
	candidateNum = index;
	if(candidateNum > 0) {
	    if(index <= wikiHelperCompletionSelectIndex) {
		wikiHelperCompletionSelectIndex = index - 1;
		li.addClassName('selected');
		selectedLi = li;
	    }

	    var caretPosition = getCaretPosition(textarea);
	    var textareaOffset = textarea.cumulativeOffset();
	    var scrollOffset = body.cumulativeScrollOffset();
	    caretPosition.top += textareaOffset.top + scrollOffset.top;
	    caretPosition.left += textareaOffset.left + scrollOffset.left;

	    wikiHelperCompletionWindow.style.top = (caretPosition.top + 2) + 'px';
	    wikiHelperCompletionWindow.style.left = (caretPosition.left + 2) + 'px';

	    wikiHelperCompletionWindow.style.width = (wikiHelperCompletionWindow.getWidth() + 30) + 'px';

	    if(selectedLi) {
		var height = wikiHelperCompletionWindow.getHeight() - selectedLi.getHeight();
		if(wikiHelperCompletionWindow.scrollTop + height < selectedLi.offsetTop)
		    wikiHelperCompletionWindow.scrollTop = selectedLi.offsetTop - height;
		if(wikiHelperCompletionWindow.scrollTop > selectedLi.offsetTop)
		    wikiHelperCompletionWindow.scrollTop = selectedLi.offsetTop;
	    } else {
		wikiHelperCompletionWindow.scrollTop = 0;
	    }

	    wikiHelperCompletionWindow.show();
	} else {
	    wikiHelperCompletionWindowClose();
	}    
    };

    var completionWindowKeyFlag = false;

    var updateCompletionSelect = function(di) {
	wikiHelperCompletionSelectIndex += di;
	if(wikiHelperCompletionSelectIndex < 0)
	    wikiHelperCompletionSelectIndex = candidateNum - 1;
	if(wikiHelperCompletionSelectIndex >= candidateNum)
	    wikiHelperCompletionSelectIndex = 0;
	updateCompletionWindow();
    };

    var lastTextareaValue = textarea.value;

    textarea.observe('keydown', function(event) {
	var charCode = event.keyCode ? event.keyCode : event.charCode;
	var keyString = String.fromCharCode(charCode);
	if(event.altKey && event.shiftKey)
	    keyString = 'M-S-' + keyString;
	else if(event.altKey)
	    keyString = 'M-' + keyString;
	if(event.ctrlKey)
	    keyString = 'C-' + keyString;
	//console.log(keyString);
	//console.log(charCode);

	if((event.altKey && charCode == 191) ||
	   (event.ctrlKey && (charCode == 190 || charCode == 188)) ||
	   keyString == 'C- ') {
	    wikiHelperCompletionIsForce = true;
	    wikiHelperCompletionEnableTemporary = true;
	    updateCompletionWindow();
	}

	if(wikiHelperCompletionWindow.visible()) {

	    if((charCode == 38 || keyString == 'C-P' /*|| (charCode == 9 && event.shiftKey)*/)) {
		if(wikiHelperCompletionSelectIndex >= 0) {
		    updateCompletionSelect(-1);
		    event.stop();
		    return false;
		} else {
		    wikiHelperCompletionWindowClose();
		}
	    }
	    if(charCode == 40 || keyString == 'C-N' /*|| (charCode == 9 && !event.shiftKey)*/) {
		updateCompletionSelect(1);
		event.stop();
		return false;
	    }
	    if((charCode == 13 ||
		keyString == 'C-J' || keyString == 'C-M') &&
	       wikiHelperCompletionSelectIndex >= 0) {
		wikiHelperCompletionWindow.list.down('li.selected').insertCompletion(null);
		event.stop();
		return false;
	    }
	    if(charCode == 27 || keyString == 'C-G') {
		wikiHelperCompletionWindowClose();
		event.stop();
		return false;
	    }
	}
	return true;
    });

    textarea.observe('keyup', function(event) {
	if(completionEnable || wikiHelperCompletionEnableTemporary) {
	    if(!completionDone && lastTextareaValue != textarea.value) {
		updateCompletionWindow();
		lastTextareaValue = textarea.value;
	    }
	    completionDone = false;
	}
    });

    textarea.observe('click', function(event) {
	wikiHelperCompletionWindowClose();
    });

}

function wikiHelperCompletionAppendCandidateLi(insert, candidate, index, completionFunction) {
    var li = new Element('li');
    if(insert.charAt(0) == ' ')
	li.innerHTML += '&nbsp;';
    li.appendChild(document.createTextNode(insert));
    li.appendChild(document.createTextNode(' -- '));
    li.insertCompletion = function() {
	wikiHelperCompletionWindowClose();
	completionFunction();
	wikiHelperCompletionEnableTemporary = false;
    };
    li.observe('click', function(event) { li.insertCompletion(); });

    if(candidate['description']) {
	if(candidate['manual_path'] != '') {
	    var url = wikiHelperInfo['manual_url_base'] + candidate['manual_path'];
	    var link = new Element('a', {'href': url});
	    link.observe('click', function(event) {
		event.stop();
		window.open(url);
		return false;
	    });
	    link.appendChild(document.createTextNode(candidate['description']));
	    li.appendChild(link);
	} else {
	    li.appendChild(document.createTextNode(candidate['description']));
	}
    } else if(candidate['display']) {
	li.innerHTML += candidate['display'];
    } else if(candidate['date']) {
	li.appendChild(document.createTextNode(candidate['date']));
    } else if(candidate['pagename']) {
	var url = '?' + urlPathEncode(candidate['pagename']);
	var link = new Element('a', {'href': url});
	link.observe('click', function(event) {
	    event.stop();
	    window.open(url);
	    return false;
	});
	link.appendChild(document.createTextNode(l('Page name')));
	li.appendChild(link);
    }

    if(index == wikiHelperCompletionSelectIndex)
	li.addClassName('selected');

    wikiHelperCompletionWindow.list.appendChild(li);

    return li;
}

function wikiHelperCompletionWindowClose() {
    wikiHelperCompletionWindow.scrollTop = 0;
    wikiHelperCompletionWindow.hide();
    wikiHelperCompletionSelectIndex = -1;
    //document.body.style.overflow = '';
    wikiHelperCompletionIsForce = false;
    wikiHelperCompletionEnableTemporary = false;
}

function wikiEditReset() {
    if(editingForm != null) {
	var save = null;
	var parent = null;

	if(editingForm.parentNode)
	    save = editingForm.parentNode.getAttribute('data-save');

	if(editingForm.showElement)
	    editingForm.showElement.show();
	if(editingForm.removeElement)
	    editingForm.removeElement.remove();

	if(save) {
	    parent = editingForm.parentNode;
	    editingForm.remove();
	    parent.removeAttribute('data-save');
	    parent.innerHTML = save;
	} else {
	    editingForm.remove();
	}
	
	editingFormIsChange = false;
	editingForm = null;

	if(wikiHelperCompletionWindow) {
	    wikiHelperCompletionWindowClose();
	}
    }
}

function wikiEditCreateTextarea() {
    var textarea = document.createElement('textarea');
    textarea.isManualResize = false;
    textarea.setAttribute('name', 'value');
    textarea.fitHeight = function() {
	var originalHeight = textarea.getHeight();
	if(textarea.div) {
	    textarea.div.style.height = textarea.scrollHeight + 'px';
	} else {
	    textarea.div = $(document.createElement('div'));
	    textarea.parentNode.insertBefore(textarea.div, textarea);
	    textarea.div.hide();
	}

	var lines = textarea.value.split("\n");
	if(lines.length <= 1 && lines[0] != '') {
	    var length = encodeURIComponent(lines[0]).
		replace(/%[0-7]./g,"x").replace(/%..%..%../g,"xx").replace(/%../g,"x").length;
	    var width = Math.min(Math.max(length, 5), 80) * 8.5;
	    var textareaWidth = textarea.getWidth();
	    var article = textarea.up('article');
	    if(article)
		width = Math.min(width, article.getWidth() * 0.8);
	    if(width > textareaWidth)
		textarea.style.width = width + 'px';
	}

	textarea.style.height = '1ex';
	textarea.div.show();
	var height = textarea.scrollHeight;
	textarea.style.overflow = 'hidden';
	if(viewport) {
	    var viewHeight = viewport.height * 0.6;
	    if(viewHeight < height) {
		height = viewHeight;
		textarea.style.overflow = 'auto';
	    }
	}
	if(textarea.isManualResize)
	    textarea.oldHeight = textarea.style.height = Math.max(height, originalHeight) + 'px';
	else
	    textarea.oldHeight = textarea.style.height = height + 'px';
	textarea.div.hide();

    };
    textarea.observe('keyup', textarea.fitHeight.bind(textarea));
    textarea.observe('paste', function(event) {
	setTimeout(textarea.fitHeight.bind(textarea), 10);
    });
    textarea.observe('click', textarea.fitHeight.bind(textarea));
    textarea.fixTextarea();

    var manualResize = function(event) {
	if(textarea.oldWidth  == null || textarea.oldHeight == null){
	    textarea.oldWidth  = textarea.style.width;
	    textarea.oldHeight = textarea.style.height;
	} else if(textarea.style.width != textarea.oldWidth || textarea.style.height != textarea.oldHeight){
            textarea.oldWidth  = textarea.style.width;
            textarea.oldHeight = textarea.style.height;
	    textarea.isManualResize = true;
	}
    };
    textarea.observe('mouseup', manualResize);
    textarea.observe('mousemove', manualResize);

    return textarea;
}

function wikiEditCreateTextForm(hiddens, onsubmit, isSimple) {
    var form = createForm(hiddens);
    form.addClassName('text_edit');
    form.setAttribute('method', 'POST');
    form.setAttribute('action', '?');

    var textarea = wikiEditCreateTextarea();
    textarea.wikiHelperCompletionSetup();
    form.appendChild(textarea);
    textarea.revertWarningSetup();

    var ul = document.createElement('ul');

    var li = document.createElement('li');
    ul.appendChild(li);
    var save = document.createElement('button');
    save.innerHTML = l('Save');
    save.setAttribute('type', 'button');
    save.setAttribute('accesskey', 's');
    li.appendChild(save);
    save.observe('click', function(event) {editingFormIsChange = false; return onsubmit(event);});

    form.appendChild(ul);

    form.textarea = textarea;

    if(!isSimple)
	form.wikiHelperSetup();

    var button = new Element('button', {'type': 'button', 'class': 'close'}).update('&times;');
    button.observe('click', function(event) {wikiEditReset(); event.stop();});
    form.appendChild(button);

    return form;
}

function wikiEditResponseGet(request) {
    var response;
    try {
	response = request.responseText.evalJSON();
    } catch(e) {
	messagesAdd('error', l('Server side error.'));
	return false;
    }
    if(response.status == 'permission') {
	messagesAdd('error', l('You have no permission to edit this page.'));
    } else if(response.status == 'lock') {
	messagesAdd('error', l('This page was locked.'));
    } else if(response.status == 'ignorelock') {
	messagesAdd('notice', l('This page was locked.'));
    } else if(response.status != 'ok') {
	if(response.status == 'exists')
	    messagesAdd('error', l('Specified page does not exist.'));
	else if(response.status == 'ticket')
	    messagesAdd('error', l('This page has been changed. Please reload.'));
	else if(response.status == 'type')
	    messagesAdd('error', l('This page is not editable.'));
	else
	    messagesAdd('error', l('Unkown status code: ') + response.status);
	return false;
    }
    return response;
}

function wikiHelperSetup() {
    $$('form.text_edit').each(function(form) {
	var textarea = form.down('textarea');
	textarea.revertWarningSetup();
	if(form.match('.type_wiki'))
	    textarea.wikiHelperCompletionSetup();
	if(viewport) {
	    textarea.style.height = viewport.height * 0.7 + 'px';
	}
	form.wikiHelperSetup();

	var input = form.down('input[name="save_and_edit"]');
	input.observe('click', function(event) {
	    if(mainPagename) {
		textarea.saveCursorStatus();
		cookieSet('cursorpage', encodeURIComponent(mainPagename));
	    }
	});
	if(mainPagename) {
	    var cursorPage = decodeURIComponent(cookieGet('cursorpage'));
	    if(mainPagename == cursorPage)
		textarea.loadCursorStatus();
	    cookieSet('cursorpage', '');
	}
    });
}

function formsHelperSetup() {
    $$('input.redundant').each(function(inputElement) {
	inputElement.hide();
	var form = inputElement.up('form');
	form.getElementsBySelector('select').each(function(selectElement) {
	    selectElement.observe('change', function(event) { scrollSet(); form.submit(); });
	});
    });

    if(!isMobileDevice) {
	var offset = document.body.cumulativeScrollOffset();
	var inputs = $$('input.listadd_text');
	for(var i = 0; i < inputs.length; i++) {
	    var top = inputs[i].cumulativeOffset().top;
	    if(offset.top <= top && top <= offset.top + viewport.height) {
		inputs[i].focus();
		break;
	    }
	}
    }

    /*
    $$('input[type="text"], textarea').each(function(inputElement) {
	inputElement.setEmacsKeybinds();
    });
    */
}

function tagEditSetup() {
    var uls = $$('dl.page_tags dd ul.tags');
    if(uls.length <= 0)
	return;
    var ul = uls[0];
    var dd = ul.parentNode;
    var dl = dd.parentNode;
    var editLi = ul.down('li.edit_tag');
    if(!editLi)
	return;
    var tagEditHandler = function() {
	if(editingFormIsChange) {
	    messagesAdd('error', l('You are editing other part of this page.'));
	    return;
	}

	wikiEditReset();
	var width = ul.getWidth();
	ul.hide();
	var hiddens = {
	    'option': 'tag',
	    'action': 'write',
	    'page': mainPagename,
	    'pagename': mainPagename,
	    'redirect' : location.href
	};
	var tags = ul.getAttribute('data-tags');
	var form = createForm(hiddens);
	form.addClassName('tag_edit');
	var textInput = new Element('input', {'type': 'text', 'name': 'tag_string', 'value': tags});
	textInput.style.width = Math.max(width, 100) + 'px';
	form.appendChild(textInput);
	form.setAttribute('method', 'POST');
	form.setAttribute('action', '?' + urlPathEncode(mainPagename));
	dd.appendChild(form);
	textInput.observe('change', function(event) { editingFormIsChange = true; });
	textInput.focus();

	form.appendChild(new Element('input', {'type': 'submit', 'value': l('Change')}));

	var cancelButton = new Element('button', {'type': 'button', 'class': 'close'}).update('&times;');
	form.appendChild(cancelButton);
	cancelButton.observe('click', function(event) { wikiEditReset(); event.stop(); });

	form.observe('submit', function(event) { editingFormIsChange = false; scrollSet(); });
	form.showElement = ul;
	editingForm = form;
    };
    editLi.observe('click', function(event) {
	event.stop();
	tagEditHandler();
    });
    dl.observeEditAction(tagEditHandler);
}

function urlCopySetup() {
    var links = $$('dl.page_info a.page_url');
    if(links.length <= 0)
	return;
    var link = links[0];
    var url = link.href;
    link.update(l('Show'));
    link.observe('click', function(event) {
	var textInput = new Element('input', {'type': 'text', 'class': 'page_url'});
	textInput.value = url;
	link.parentNode.appendChild(textInput);
	link.remove();
	textInput.select(0, textInput.value.length);
	event.stop();
    });
}


function revertWarningSetup() {
    var revertWarning = function(event) {
	if(editingFormIsChange)
	    return event.returnValue = l('You have unsaved changes.');
    };
    // Event.observe can't work in chrome
    if(window.addEventListener)
        return window.addEventListener('beforeunload', revertWarning, false);
    else if(window.attachEvent)
        return window.attachEvent('onbeforeunload', revertWarning);
    return false;
}

Element.prototype.revertWarningSetup = function() {
    var textarea = this;
    var form = textarea.up('form');
    if(form.hasClassName('preview'))
	editingFormIsChange = true;
    form.observe('submit', function(event) { editingFormIsChange = false; });
    textarea.originalValue = textarea.value;
    var changeHandler = function(event) {
	editingFormIsChange = textarea.originalValue != textarea.value;
    };
    textarea.observe('change', changeHandler);
    textarea.observe('click', changeHandler);
}

function listAddSetup() {
    $$('form.listadd').each(function(form) {
	if(form.down('input[name="escape"]').value != 'true') {
	    var textInput = form.down('input[name="listadd_text"]');
	    textInput.wikiHelperCompletionSetup();
	}
	var modeHidden = form.down('input[name="mode"]');
	var isBellow = modeHidden.value == 'after';
	var formParent = form.up();
	var formLength = parseInt(formParent.getAttribute('data-twl'));
	var formPosition = parseInt(formParent.getAttribute('data-twp'));
	var targetSection;
	if(isBellow) {
	    for(var i = formParent.childNodes.length - 1; i >= 0; i--) {
		var node = formParent.childNodes[i];
		if(typeof node.tagName == 'undefined' &&
		   node.nodeValue.replace(/(^\s+)|(\s+$)/,'') == '')
		    continue;
		if(node != form)
		    return;
	    }
	    targetSection = formParent.next();
	    if(!targetSection || !targetSection.match('section.list'))
		return;
	    var targetPosition = parseInt(targetSection.getAttribute('data-twp'));
	    if(targetPosition != formPosition + formLength)
		return;
	    targetSection.select('ol').each(function(ol) {
		ol.setAttribute('reversed', 'reversed');
	    });
	} else {
	    if(formParent.firstChild != form)
		return;
	    targetSection = formParent.previous();
	    if(!targetSection || !targetSection.match('section.list'))
		return;
	    var targetLength = parseInt(targetSection.getAttribute('data-twl'));
	    var targetPosition = parseInt(targetSection.getAttribute('data-twp'));
	    if(targetLength + targetPosition != formPosition)
		return;
	}
	targetSection.initializers = [];
	targetSection.listAddSetup(isBellow, form);
    });
}

Element.prototype.listAddSetup = function(isBellow, form) {
    if(!form.hasClassName('nest'))
	return;
    var targetSection = this;
    if(!targetSection.down('li[data-twp][data-twl]'))
	return;
    var isOrdered = form.down('input[name="ordered"]').value == 'true';
    var input = form.down('input[name="listadd_text"]');
    var isComment = form.down('input[name="listadd_name"]');
    var defaultPlaceholder = input.getAttribute('placeholder');
    form.setAttribute('title', isComment ?
		      l('You can click a comment to reply.') : l('You can click a parent item.'));
    var title = isComment ?
	l('Click to reply') : l('Click to insert as a child');
    input.setAttribute('placeholder', defaultPlaceholder);

    var formLi = new Element('li');
    formLi.addClassName('listadd');
    form.remove();
    formLi.appendChild(form);

    var initialList;
    if(isBellow) {
	initialList = targetSection.select('> ul, > ol').first();
	initialList.insert({'top': formLi});
    } else {
	initialList = targetSection.select('> ul, > ol').last();
	initialList.appendChild(formLi);
    }
    var initialLi = new Element('li');
    initialList.insertBefore(initialLi, formLi);

    var cancelButton = new Element('button').update(l('Back'));
    initialLi.hide();
    initialLi.appendChild(cancelButton);
    
    targetSection.select('li[data-twp][data-twl]').each(function(li) {
	if(li.up('section.list') != targetSection)
	    return;
	li.setAttribute('title', title);
	li.observe('click', function(event) {
	    if(event.altKey || event.ctrlKey ||
	       targetSection.down('form.text_edit') || targetSection.down('.list_edit'))
		return;
	    var e = event.findElement('a');
	    if(typeof e != 'undefined' && e != document)
		return;
	    var e = event.findElement('form');
	    if(typeof e != 'undefined' && e != document)
		return;
	    event.stop();
	    input.setAttribute('placeholder',
			       isComment ? l('Reply content') :
			       l('Content of child item')
			      );
	    formLi.remove();
	    form.avoidEdit = true;
	    var childList = li.down('> ul, > ol')
	    if(childList) {
		if(isBellow)
		    childList.insert({'top': formLi});
		else
		    childList.appendChild(formLi);
	    } else
		li.appendChild(new Element(isOrdered ? 'ol' : 'ul')).appendChild(formLi);
	    initialLi.show();

	    if(!window.getSelection || window.getSelection().isCollapsed)
		input.focus();
	});
    });

    var initializeListAdd = function(event) {
	input.setAttribute('placeholder', defaultPlaceholder);
	formLi.remove();
	if(isBellow)
	    initialList.insert({'top': formLi});
	else
	    initialList.appendChild(formLi);
	initialLi.hide();
	form.avoidEdit = false;
    };
    targetSection.initializers.push(initializeListAdd);
    
    cancelButton.observe('click', function(event) {
	initializeListAdd();
	event.stop();
    });

    form.observe('submit', function(event) {
	if(initialLi.visible()) {
	    var li = formLi.parentNode.parentNode;
	    form.addHiddens(
		{'parent_twp': li.getAttribute('data-twp'),
		 'parent_twl': li.getAttribute('data-twl'),
		 'parent_text': li.getListTextWithoutChildren()});
	}
    });
}

Element.prototype.clearListItemSelection = function() {
    this.select('li').each(function(li) {
	li.removeClassName('selected');
	li.removeClassName('selected_top');
	li.removeClassName('selected_bottom');
    });
}

Element.prototype.getListTextWithoutChildren = function() {
    var text = '';
    for(var i = 0; i < this.childNodes.length; i++) {
	childNode = this.childNodes[i];
	if(typeof childNode.tagName == 'undefined')
	    text += childNode.nodeValue;
	else {
	    if(childNode.tagName == 'UL' || childNode.tagName == 'OL')
		break;
	    text += childNode.textValue();
	}
    }
    return text;
}

function templateInputSetup() {
    $$('input[type="text"].templateinput, textarea.templateinput').each(function(input) {
	console.log(input);
	if(!input.match('.escape')) {
	    input.wikiHelperCompletionSetup();
	}
    });
}

function optionalSetup() {
    $$('.optional').each(function(element) {
	element.optionalToggle('hide');
    });
}

Element.prototype.optionalToggle = function(mode) {
    var action;
    if(typeof this.optionalStatus == 'undefined')
	this.optionalStatus = 'show';
    if(mode == 'show' ||
       (mode == 'toggle' && this.optionalStatus == 'hide')) {
	action = function(e){e.show();};
	this.optionalStatus = 'show';
    } else {
	action = function(e){e.hide();};
	this.optionalStatus = 'hide';
    }
    var targetIndex = 1;
    if(this.tagName == 'LI') {
	for(var i = 0; i < this.childNodes.length; i++) {
	    if(typeof (this.childNodes[i].tagName) != 'undefined' &&
	       ['UL', 'OL'].indexOf(this.childNodes[i].tagName) != -1) {
		break;
	    }
	}
	targetIndex = i;
    }
    for(var i = targetIndex; i < this.childNodes.length; i++) {
	action(this.childNodes[i]);
    }
    
    if(!this.toggleButton) {
	var button = new Element('button', {'type': 'button', 'class': 'optional'});
	this.toggleButton = button;
	var e = this;
	if(this.tagName == 'LI') {
	    this.insertBefore(this.toggleButton, this.childNodes[targetIndex]);
	    //this.insertBefore(this.toggleButton, this.firstChild);
	    this.toggleButton.observe('click', function(event) { e.optionalToggle('toggle'); });
	    this.observe('click', function(event) {
		if(event.findElement('*') == e)
		    e.optionalToggle('toggle');
	    });
	}
	else {
	    this.firstChild.appendChild(this.toggleButton);
	    //this.firstChild.insertBefore(this.toggleButton, this.firstChild.firstChild);
	    this.firstChild.observe('click', function(event) { e.optionalToggle('toggle'); });
	}
    }
    if(this.optionalStatus == 'hide')
	this.toggleButton.update('+');
    else
	this.toggleButton.update('-');
}

Element.prototype.optionalRemove = function(mode) {
    this.optionalToggle('show');
    this.select('button.optional').each(function(li){ li.remove(); });
    this.stopObserving('click');
}


function calendarHelperSetup() {
    $$('table.calendar').each(function(calendar) {
	var dds = [];
	var span = calendar.down('span.day_detail');
	var tbody = calendar.down('tbody');
	calendar.select('td.exists').each(function(td) {
	    var dd = td.down('dd');
	    dds.push(dd);
	    var handlerOn = function(event) {
		if(span.getStyle('display') != 'none')
		    return;
		if(event.altKey || event.ctrlKey)
		    return;
		
		dds.each(function(hdd) {hdd.hide(); });
		var width = tbody.getWidth() * 0.75;
		if(!dd.down('h1.pages')) {
		    var h = dd.insertBefore(new Element('h1', {'class': 'pages'}), dd.firstChild);
		    var link = h.appendChild(new Element('a', {'href': td.down('dl > dt > a').getAttribute('href')}));
		    link.appendChild(document.createTextNode(td.getAttribute('data-ref-pagename')));
		    var link = h.appendChild(new Element('button', {'class': 'button close'}).update('&times;'));
		}
		dd.addClassName('popup');
		dd.style.display = 'block';
		dd.style.position = 'absolute';
		dd.style.marginLeft = '-' + (width / 2 + 16) + 'px';
		dd.style.marginTop = '12px';
		dd.style.left = '50%';
		dd.style.width = width + 'px';
		dd.style.padding = '16px';
		dd.style.backgroundColor = td.getStyle('background-color');
		dd.style.border = 'solid 1px';
		if(dd.style.backgroundColor == 'transparent' || dd.style.backgroundColor == 'rgba(0, 0, 0, 0)')
		    dd.style.backgroundColor = td.up('body').getStyle('background-color');
	    };
	    var handlerOff = function(event) {
		if(span.getStyle('display') != 'none')
		    return;
		if(event.altKey || event.ctrlKey)
		    return;
		dd.hide();
	    };
	    var handlerToggle = function(event) {
		if(dd.getStyle('display') == 'none')
		    return handlerOn(event);
		else
		    return handlerOff(event);
	    }
	    
	    var handler = function(event) {
		if(span.getStyle('display') != 'none')
		    return;
		if(event.altKey || event.ctrlKey)
		    return;
		
		if(dd.getStyle('display') == 'none') {
		    dds.each(function(hdd) {hdd.hide(); });
		    var width = tbody.getWidth() * 0.75;
		    if(!dd.down('h1.pages')) {
			var h = dd.insertBefore(new Element('h1', {'class': 'pages'}), dd.firstChild);
			var link = h.appendChild(new Element('a', {'href': td.down('dl > dt > a').getAttribute('href')}));
			link.appendChild(document.createTextNode(td.getAttribute('data-ref-pagename')));
			var link = h.appendChild(new Element('button', {'class': 'button close'}).update('&times;'));
		    }
		    dd.addClassName('popup');
		    dd.style.display = 'block';
		    dd.style.position = 'absolute';
		    dd.style.marginLeft = '-' + (width / 2 + 16) + 'px';
		    dd.style.marginTop = '12px';
		    dd.style.left = '50%';
		    dd.style.width = width + 'px';
		    dd.style.padding = '16px';
		    dd.style.backgroundColor = td.getStyle('background-color');
		    dd.style.border = 'solid 1px';
		    if(dd.style.backgroundColor == 'transparent' || dd.style.backgroundColor == 'rgba(0, 0, 0, 0)')
			dd.style.backgroundColor = td.up('body').getStyle('background-color');
		} else {
		    dd.hide();
		}
	    };
	    if(typeof this.ontouchstart == 'undefined') {
		var motionHandler = function (f)  {
		    return function(mevent) {
			if(mevent.findElement('td') != document)
			    f(mevent);
		    };
		};
		td.observe('mouseover', motionHandler(handlerOn));
		td.observe('mouseout', motionHandler(handlerOff));
	    }
	    td.observe('click', handlerToggle);
	});
    });
}

function tableSortSetup() {
    $$('table').each(function(tableElement) {
	if(!tableElement.match('.nosort'))
	    tableElement.tableSortSetup();
    });
}

Element.prototype.tableSortSetup = function() {
    var thead = this.down('thead');
    var tbody = this.down('tbody');
    var tfoot = this.down('tfoot');

    if(!thead)
	return;

    var isRowSpan = false;
    var rows = null;
    var isPrepare = false;

    var sortPrepare = function() {
	if(isPrepare)
	    return;
	isPrepare = true;
	isRowSpan = tbody.select('[rowspan]').length > 0;
	if(isRowSpan)
	    return;
	rows = tbody.select('tr');
	var i = 0;
	rows.each(function(tr) {
	    var cells = tr.select('td, th');
	    compareCells = [];
	    var spanCount = 0;
	    for(var j = 0; j < cells.length; j++) {
		var colspan = cells[j].getAttribute('colspan');
		var value = cells[j].textValue();
		if(colspan) {
		    while(colspan-- > 0) {
			compareCells.push(value);
		    }
		} else {
		    compareCells.push(value);
		}
	    }
	    tr.compareCells = compareCells;
	    tr.defaultIndex = i++;
	    //console.log(tr.compareCells);
	});
    };
    
    var headCells = null;
    thead.select('tr').each(function(tr) {
	var cells = tr.select('td, th');
	if(headCells == null || cells.length > headCells.length) {
	    headCells = cells;
	}
    });

    var sortCells = [];

    //var headkey = '';
    //headCells.each(function(cell) { headkey += cell.textValue()});
    var headkey = 'sort_' + headCells.map(function(cell) {
	var text = cell.textValue();
	var key = '';
	if(text) {
	    for(var l = 0; l < text.length; l++)
		key += text.charCodeAt(l);
	}
	return key;
    }).join(',');

    var sortUpdate = function() {
	//console.log(sortCells);
	var i = 1;
	headCells.each(function(cell) {
	    var span = cell.down('span.sort_order');
	    if(!span) {
		span = new Element('span', {'class': 'sort_order'});
		cell.appendChild(span);
	    }
	    var index = sortCells.indexOf(i);
	    if(index == -1)
		index = sortCells.indexOf(-i);
	    if(index == -1) {
		span.innerHTML = '';
	    } else if(sortCells[index] > 0) {
		span.innerHTML = '&#9650;' + (sortCells.length == 1 ? '' : (index + 1));
	    } else {
		span.innerHTML = '&#9660;' + (sortCells.length == 1 ? '' : (index + 1));
	    }
	    i++;
	});
	var numberRreplaceRegex = new RegExp('[\s ,]+', 'g');
	var numberRegex = new RegExp('^[0-9\.]+$');

	rows.sort(function(a, b) {
	    for(var c = 0; c < sortCells.length; c++) {
		var offset = sortCells[c];
		var direction = 1;
		if(offset < 0) {
		    direction = -direction;
		    offset = -offset;
		}
		offset--;
		
		var aVal = '';
		var bVal = '';
		if(a.compareCells[offset])
		    aVal = a.compareCells[offset];
		if(b.compareCells[offset])
		    bVal = b.compareCells[offset];
		var aNum = aVal.replace(numberRreplaceRegex, '');
		var bNum = bVal.replace(numberRreplaceRegex, '');
		//console.log('rep ' + aNum + ':' + bNum);
		if(aNum.match(numberRegex) && bNum.match(numberRegex)) {
		    aNum = parseFloat(aNum);
		    bNum = parseFloat(bNum);
		    if(!isNaN(aNum) && !isNaN(bNum)) {
			aVal = aNum;
			bVal = bNum;
		    }
		}
		//console.log('compare ' + aVal + ':' + bVal);
		if(aVal < bVal) return -direction;
		else if(aVal > bVal) return direction;
	    }
	    return a.defaultIndex - b.defaultIndex;
	});

	tbody.select('tr').each(function(tr) { tr.remove(); });
	rows.each(function(tr) { tbody.appendChild(tr); });
    };

    var i = 1;
    headCells.each(function(cell) {
	(function(i) {
	    cell.observe('click', function(event) {
		if(event.altKey || event.ctrlKey  || cell.down('form'))
		    return false;
		if (cell.timer) clearTimeout(cell.timer);
		cell.timer = setTimeout(function() {
		    sortPrepare();
		    if(isRowSpan) {
			messagesAdd('notice', l('Can\'t sort rowspaned table.'));
			return;
		    }
		    var index = sortCells.indexOf(i);
		    if(index == -1) {
			index = sortCells.indexOf(-i);
			if(index == -1) {
			    sortCells.push(i);
			} else {
			    sortCells.splice(index, 1);
			}
		    } else {
			sortCells[index] = -i;
		    }
		    sortUpdate();
		    cookieSetLong(headkey, sortCells.join(','));
		}, 250);
		return true;
	    });
	})(i);
	i++;
    });

    var cookieSort = cookieGet(headkey);
    if(cookieSort) {
	sortCells = cookieSort.split(',').map(function(str) { return parseInt(str); });
	sortPrepare();
	sortUpdate();
    }
}

function summarySetup() {
    var ul = $$('ul.page_tools')[0];
    if(!ul)
	return;
    var li = ul.down('li.summary');
    if(!li)
	return;
    var link = li.down('a');
    if(!link)
	return;
    
    var metaViewport = document.querySelector("meta[name=viewport]");
    var originalViewportContent = null
    if(metaViewport)
	originalViewportContent = metaViewport.getAttribute('content');

    var summaryToggle = function() {
	link.summarySection.toggle();
	if(!link.summarySection.visible()) {
	    /* TODO: can't work */
	    /* metaViewport.setAttribute('content', originalViewportContent); */
	}
    };

    //li.style.position = 'relative';
    link.observe('click', function(event) {
	event.stop();
	if(!link.summarySection) {
	    if(metaViewport)
		metaViewport.setAttribute('content', originalViewportContent + ',user-scalable=no');

	    summarySection = new Element('section', {'class': 'summary_window'});

	    var closeButton = new Element('button', {'class': 'close'}).update('&times;');
	    summarySection.appendChild(closeButton);

	    var h = new Element('h1');
	    h.appendChild(document.createTextNode(l('Summary')));
	    summarySection.appendChild(h);

	    closeButton.observe('click', summaryToggle);
	    h.observe('click', function(event) { $(window).scroll(0, 0); });
	    
	    link.summarySection = summarySection;
	    ul.parentNode.parentNode.insertBefore(summarySection, ul.parentNode.nextSibling);

	    //summarySection.observe('click', function(event) { summarySection.toggle();});
	    new Ajax.Updater(
		summarySection,
		'?' + urlPathEncode(mainPagename) + '&option=summary&action=only',
		{ method: 'get',
		  insertion: 'bottom',
		  onComplete: function(request) {
		      summarySection.select('a').each(function(a) {
			  a.observe('click', function(event) {
			      setTimeout(anchorUpdateForce, 10);
			      return true;
			  });
		      });
		  }}
	    );

	} else {
	    summaryToggle();
	}
	
	return false;
    });
}

function checkboxLabelSetup() {
    $$('label input[type="checkbox"]').each(function(element) {
	element.setupParentCheckboxLabel();
    });
}

Element.prototype.updateCheckboxLabel = function() {
    if(this.checked)
	this.parentCheckboxLabel.addClassName('checked');
    else
	this.parentCheckboxLabel.removeClassName('checked');
    this.parentCheckboxLabel.removeClassName('focus');
}

Element.prototype.setupParentCheckboxLabel = function() {
    var label = this.up('label');
    if(label) {
	this.parentCheckboxLabel = label;
	this.parentCheckboxLabel.addClassName('checkbox');
	this.updateCheckboxLabel();
	this.observe('change', this.updateCheckboxLabel.bind(this));
	this.observe('focus', function(event) { label.addClassName('focus'); });
	this.observe('blur', function(event) { label.removeClassName('focus'); });
    }
}

function anchorSetup() {
    $$('body')[0].observe('click', function(event) {
	setTimeout(anchorUpdate, 10);
    });
}

function anchorUpdateForce() {
    anchorLastUpdateUrl = false;
    anchorUpdate();
}

function anchorUpdate() {
    if(anchorLastUpdateUrl == location.href)
	return;

    anchorLastUpdateUrl = location.href;
    if(location.href.match(/\#([^&]*)/)) {
	var id = decodeURI(RegExp.$1);
	var anchorElement = $(id).down('h1');
	if(anchorElement) {
	    anchorElement.addClassName('anchor');
	    setTimeout(function() { anchorElement.removeClassName('anchor'); }, 500);
	    setTimeout(function() { anchorElement.addClassName('anchor'); }, 1000);
	    setTimeout(function() { anchorElement.removeClassName('anchor'); }, 1500);
	    var section = anchorElement.up('section.optional');
	    while(section) {
		section.optionalToggle('show');
		section = section.up('section.optional');
	    }
	    location.href = location.href;
	}
    }
}

function searchSetup() {
    $$('form.search').each(function(form) {
	var a = form.down('input[type="search"], input[type="text"]');
	a.observe('keydown', function(event) {
	    form.isShiftKeyDown = event.shiftKey;
	});

	form.observe('submit', function(event) {
	    if(form.isShiftKeyDown) {
		var hidden = document.createElement('input');
		hidden.setAttribute('type', 'hidden');
		hidden.setAttribute('name', 'top');
		hidden.setAttribute('value', 'true');
		form.appendChild(hidden);
	    }
	});
    });
}

function configSetup() {
    var section = $$('section.config')[0];
    if(!section)
	return;
    var localSection = new Element('section', {'class': 'section local_config'});
    var h = document.createElement('h1');
    h.appendChild(document.createTextNode(l('Editing preferences (saved in the browser)')));
    localSection.appendChild(h);
    section.appendChild(localSection);
        
    var ul = $(document.createElement('ul'));
    localSection.appendChild(ul);
    for(key in config) {
	var li = document.createElement('li');
	var value = configGet(key);
	if(config[key]['type'] == 'boolean') {
	    var label = document.createElement('label');
	    var input = document.createElement('input');
	    input.setAttribute('type', 'checkbox');
	    input.checked = (value == 'true');
	    label.appendChild(input);
	    label.appendChild(document.createTextNode(l(config[key]['description'])));
	    li.appendChild(label);
	    ul.appendChild(li);
	    (function(input, key) {
		input.observe('change', function(event) {
		    configSet(key, input.checked);
		});
	    })(input, key);
	}
    }
}

function logoutSetup() {
    $$('a.logout').each(function(element) {
	element.observe('click', function(event) {
	    event.stop();
	    var result = null;
	    try { result = document.execCommand("ClearAuthenticationCache"); } catch(e){}
	    if(!result) {
		var xhr = window.XMLHttpRequest ? new window.XMLHttpRequest() : ( window.ActiveXObject ? new ActiveXObject("Microsoft.XMLHTTP") : null);
		if(xhr) {
		    xhr.open('HEAD', '?option=logout', false, "logout", (new Date()).getTime().toString());
		    xhr.send('');
		}
	    }
            window.location = '?option=logout';
	});
    });
}

function redirectSetup() {
    var nav = $$('nav.redirect');
    if(nav.length == 1) {
	nav = nav[0];
	var button = new Element('button', {'class': 'redirect_cancel'}).update(l('Cancel'));
	nav.appendChild(button);
	button.observe('click', function(event){
	    button.remove();
	    clearTimeout(redirectTimer);
	});
    } else {
	cookieSet('redirect-count', '');
    }
}
function redirectStart(url, timeout) {
    var count = cookieGet('redirect-count');
    count = parseInt(count);
    if(isNaN(count))
	count = 0;
    if(count >= 3) {
	cookieSet('redirect-count', '');
    } else {
	cookieSet('redirect-count', count + 1);
	redirectTimer = setTimeout(function() { window.location = url; }, timeout);
    }
}

function configGet(key) {
    var value = cookieGet('config-' + key);
    if(value)
	return value;
    return config[key]['default'];
}

function configSet(key, value) {
    cookieSet('config-' + key, value);
}

function cookieSet(name, value) {
    document.cookie = COOKIE_NAME_PREFIX + '_' + name + '=' + value + ';';
}

function cookieSetLong(name, value) {
    document.cookie = COOKIE_NAME_PREFIX + '_' + name + '=' + value + '; expires=' + 
	new Date(2050, 1).toUTCString();
}

function cookieGet(name) {
    var matches = document.cookie.match(new RegExp(COOKIE_NAME_PREFIX + '_' + name + '=([^;]+)'));
    if(matches)
	return matches[1];
    return null;
}

function createForm(hiddens) {
    var form = document.createElement('form');
    form.addHiddens(hiddens);
    return form;
}

Element.prototype.addHiddens = function(hiddens) {
    for(var key in hiddens) {
	if(hiddens[key] != null) {
	    var hidden = document.createElement('input');
	    hidden.setAttribute('type', 'hidden');
	    hidden.setAttribute('name', key);
	    hidden.setAttribute('value', hiddens[key]);
	    this.appendChild(hidden);
	}
    }
}

function replaceSelectedFormInputValue(inputElement, replaceFunction, startOffset, endOffset) {
    if(!inputElement || !replaceFunction)
	return;

    if(typeof document.selection != 'undefined') {
	inputElement.focus(inputElement.caretPos);
	inputElement.caretPos = document.selection.createRange().duplicate();
	if(startOffset)
	    inputElement.caretPos.moveStart('character', startOffset);
	if(endOffset)
	    inputElement.caretPos.moveEnd('character', endOffset);
	inputElement.caretPos.text = replaceFunction(inputElement.caretPos.text);
	inputElement.caretPos.select();
    } else {
	var startPosition = inputElement.selectionStart;
	var endPosition = inputElement.selectionEnd;
	if(startOffset)
	    startPosition += startOffset;
	if(endOffset)
	    endPosition += endOffset;

	var value = inputElement.value;
	var selectionValue = value.slice(startPosition, endPosition);
	inputElement.value = value.slice(0, startPosition) + 
	    replaceFunction(selectionValue) + value.slice(endPosition);
	var cursorOffset = endPosition + inputElement.value.length - value.length;

	if(startPosition == endPosition || startOffset || endOffset)
	    inputElement.setSelectionRange(cursorOffset,cursorOffset);
	else
	    inputElement.setSelectionRange(startPosition,cursorOffset);
	
	inputElement.focus();
    }
}

function getCaretPosition(textarea) {
    var dummyTextarea, dummySpan;
    if(!textarea.dummyTextarea) {
	dummyTextarea = document.createElement("pre");
	//textarea.parentNode.appendChild(dummyTextarea);
	textarea.parentNode.insertBefore(dummyTextarea, textarea);
	var style = textarea.currentStyle || document.defaultView.getComputedStyle(textarea, '');
	dummyTextarea.style.cssText = textarea.style.cssText;

	dummySpan = document.createElement("span");

	dummyTextarea.style.position = 'absolute';
	dummyTextarea.style.top = '0px';
	dummyTextarea.style.left = '0px';
	dummyTextarea.style.visibility = 'hidden';

	dummyTextarea.addText = function(text) {
	    var lines = text.split("\n");
	};
	
	textarea.dummyTextarea = dummyTextarea;
	textarea.dummySpan = dummySpan;
    } else {
	dummyTextarea = textarea.dummyTextarea;
	dummySpan = textarea.dummySpan;
    }

    var style = textarea.currentStyle || document.defaultView.getComputedStyle(textarea, '');

    dummyTextarea.style.height = style.height;
    dummyTextarea.style.width = style.width;
    dummyTextarea.style.paddingTop = style.paddingTop;
    dummyTextarea.style.paddingLeft = style.paddingLeft;
    dummyTextarea.style.paddingRight = style.paddingRight;
    dummyTextarea.style.paddingBottom = style.paddingBottom;
    dummyTextarea.style.fontSize = style.fontSize;
    dummyTextarea.style.fontStyle = style.fontStyle;
    dummyTextarea.style.fontFamily = style.fontFamily;
    dummyTextarea.style.fontWeight = style.fontWeight;
    dummyTextarea.style.fontVariant = style.fontVariant;
    dummyTextarea.style.lineHeight = style.lineHeight;
    dummyTextarea.style.wordSpacing = style.wordSpacing;
    dummyTextarea.style.letterSpacing = style.letterSpacing;
    dummyTextarea.style.borderBottomWidth = style.borderBottomWidth;
    dummyTextarea.style.borderLeftWidth = style.borderLeftWidth;
    dummyTextarea.style.borderRightWidth = style.borderRightWidth;
    dummyTextarea.style.borderTopWidth = style.borderTopWidth;
    dummyTextarea.style.borderStyle = style.borderStyle;
    dummyTextarea.style.textDecoration = style.textDecoration;
    /*
    dummyTextarea.style.cssText = style.cssText;
    dummyTextarea.style.position = 'absolute';
    dummyTextarea.style.top = '0px';
    dummyTextarea.style.left = '0px';
    dummyTextarea.style.height = style.height;
    dummyTextarea.style.width = style.width;
    dummyTextarea.style.visibility = 'hidden';
    */

    dummyTextarea.style.overflow = 'auto';
    dummyTextarea.style.whiteSpace = 'pre-wrap';
    dummyTextarea.style.wordWrap = 'break-word';
    dummyTextarea.scrollTop = textarea.scrollTop;
    
    var charPosition = Math.max(textarea.selectionStart, textarea.selectionEnd);

    while(dummyTextarea.hasChildNodes())
	dummyTextarea.removeChild(dummyTextarea.firstChild);

    var text1 = textarea.value.substring(0, charPosition);
    var text2 = textarea.value.substring(charPosition) + "\n";

    dummyTextarea.appendChild(document.createTextNode(text1));
    dummyTextarea.appendChild(dummySpan);
    dummyTextarea.appendChild(document.createTextNode(text2));

    var dummyTextareaOffset = dummyTextarea.cumulativeOffset();
    var spanOffset = dummySpan.cumulativeOffset();
    var scrollOffset = dummyTextarea.cumulativeScrollOffset();

    return {
	left: spanOffset.left - dummyTextareaOffset.left - scrollOffset.left + dummySpan.getWidth(),
	top: spanOffset.top - dummyTextareaOffset.top - scrollOffset.top + dummySpan.getHeight()};
}

Element.prototype.getCursorPosition = function() {
    return this.selectionEnd || 0;
}

Element.prototype.saveCursorStatus = function() {
    cookieSet('cursorstatus', [
	this.selectionStart || 0,
	this.selectionEnd || 0,
	this.scrollTop || 0,
	this.scrollLeft || 0
    ].join(':'));
}

Element.prototype.loadCursorStatus = function() {
    var statusString = cookieGet('cursorstatus');
    if(statusString) {
	var status = statusString.split(':');//.map(function(a){return parseInt(a);});
	if(status.length == 4) {
	    var textarea = this;
	    setTimeout(function() {
		textarea.scrollTop = status[2];
		textarea.scrollLeft = status[3];
		textarea.setSelectionRange(status[0], status[1]);
		textarea.focus();
	    }, 100); // for firefox random scroll problem
	}
    }
}

Element.prototype.clearChildren = function() {
    while(this.hasChildNodes()) {
	this.removeChild(this.firstChild);
    }
}

Element.prototype.textValue = function() {
    return this.textContent ? this.textContent : this.innerText;
}

Event.prototype.throughClick = function(target) {
    var clickEvent = document.createEvent("MouseEvent");
    clickEvent.initMouseEvent(this.type, true, true, window, 0, 
                              this.screenX, this.screenY, this.clientX, this.clientY, 
                              this.ctrlKey, this.altKey, this.shiftKey, this.metaKey, 
                              0, null);
    this.stop();
    target.dispatchEvent(clickEvent);
}

Element.prototype.fixTextarea = function() {
    if(/*@cc_on!@*/false) {
	this.style.fontFamily = "'ＭＳ ゴシック', 'MS Gothic', 'Osaka－等幅', Osaka-mono";
    }
}

/*
var emacsMark = {'start': 0, 'end': 0};
var emacsKillRing = '';
Element.prototype.setEmacsKeybinds = function() {
    this.observe('keydown', function(event) {
	var charCode = event.keyCode ? event.keyCode : event.charCode;
	console.log(charCode);
	var element = this;
	var getMarkPosition = function() {
	    var pos1 = element.getCursorPosition();
	    var pos2;
	    if(pos1 > emacsMark['start'])
		pos2 = emacsMark['start'];
	    else
		pos2 = element.value.length - emacsMark['end'];
	    if(pos1 < pos2)
		return [pos1, pos2];
	    else
		return [pos2, pos1];
	};
	
	if(event.ctrlKey && (charCode == 32 || charCode == 64)) { // C-Space or C-@
	    emacsMark['start'] = this.getCursorPosition();
	    emacsMark['end'] = this.value.length - this.getCursorPosition();
	    event.stop();
	}

	if(event.altKey && charCode == 87) { // M-w
	    var position = getMarkPosition();
	    cookieSet('emacskillring', encodeURIComponent(this.value.slice(position[0], position[1])));
	    event.stop();
	}

	if(event.ctrlKey && charCode == 87) { // C-w
	    var position = getMarkPosition();
	    cookieSet('emacskillring', encodeURIComponent(this.value.slice(position[0], position[1])));
	    this.value = this.value.slice(0, position[0]) + this.value.slice(position[1]);
	    this.setSelectionRange(position[0], position[0]);
	    event.stop();
	}

	if(event.ctrlKey && charCode == 75) { // C-k
	    var pos = this.getCursorPosition();
	    var lfPos;
	    if((lfPos = this.value.slice(pos).indexOf("\n")) != -1)
		lfPos += pos;
	    else
		lfPos = this.value.length - 1;
	    if(pos == lfPos)
		lfPos++;
	    cookieSet('emacskillring', encodeURIComponent((this.value.slice(pos, lfPos))));
	    this.value = this.value.slice(0, pos) + this.value.slice(lfPos);
	    this.setSelectionRange(pos, pos);
	    event.stop();
	}

	if(event.ctrlKey && charCode == 89) { // C-y
	    replaceSelectedFormInputValue(this, function(str) {
		return decodeURIComponent(cookieGet('emacskillring'));
	    });
	    event.stop();
	}
    });
}
*/

function pagePathToPagename(path, basename) {
    if(path.startsWith('./') || path.startsWith('../') || path == '.') {
	path = basename + '/' + path;
    } 
    var names = path.split('/');
    var resultNames = [];
    for(var i = 0; i < names.length; i++) {
	var name = names[i];
	if(name == '.' || name == '') {
	} else if(name == '..') {
	    resultNames.pop();
	} else {
	    resultNames.push(name);
	}
    }
    return resultNames.join('/');
}

function pageRelativePath(from, to) {
    from = from.replace(/\/+$/, '');
    to = to.replace(/\/+$/, '');
    var froms = from.split('/');
    var tos = to.split('/');
    var fn = froms.length;
    var tn = tos.length;
    var path = [];
    
    for(i = 0; i < fn && i < tn && froms[i] == tos[i]; i++);
    var sameIndex = i;
    for(i = 0; i < fn - sameIndex; i++)
	path.push('..');
    if(sameIndex < tn && path.length == 0)
	path.push('.');
    for(i = sameIndex; i < tn; i++)
	path.push(tos[i]);

    if(fn <= 1 && path.length >= 1 && path[0] == '..') {
	path.shift();
    }

    if(path.length == 0)
	path = '.';
    else
	path = path.join('/');
    return path;
}

function urlPathEncode(pagename) {
    var names = pagename.split(/\//);
    names = names.map(encodeURIComponent);
    return names.join('/');
}

var DAYOFWEEKS = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];

var LANGUAGE = {
    'ja' : {
	'Sun': '日',
	'Mon': '月',
	'Tue': '火',
	'Wed': '水',
	'Thu': '木',
	'Fri': '金',
	'Sat': '土',
	'Date': '日付', 
	'Today': '今日', 
	'Yesterday': '昨日', 
	'Tomorrow': '明日', 
	'Save': '保存',
	'Cancel': 'キャンセル',
	'Deleted': '削除',
	'Striked': '打消',
	'Edited': '編集',
	'Replace': '置換',
	'Quote': '引用',
	'Include': '埋込',
	'Summary': '目次',
	'Tools': 'ツール',
	'Page name': 'ページ名',
	'Insert an item': '項目を追加',
	'Insert a child item': '子項目を追加',
	'Browse': '参照',
        'Preferences': '個人設定',
	'Before': '前に',
	'Change': '変更',
	'Show' : '表示',
	'Load' : '読み込む',
	'Selected items will be': '選択した項目を',
	'Quotationize or Preformation the selection.': '選択範囲を引用文または整形済みにする',
	'Replace the word in the selection': '選択範囲の文字を置換',
	'Embed the attachment file': '添付ファイルを埋め込む',
	'No file type sub page.': '添付ファイルはありません．',
	'Input regex for matching': '置換する対象(正規表現)',
	'Input replacement (\'$1\' means backreference)': '置換する文字列($1で後方参照可能)',
	'Specified page does not exist.': '指定されたページが存在しません．',
	'You have no permission to edit this page.': 'このページを編集する権限がありません．',
	'This page was locked.': 'このページはロックされています．',
	'This page has been changed. Please reload.': 'このページは他から編集されています．リロードして下さい',
	'This page is not editable.': '編集できるページではありません．',
	'Unkown status code: ': '不明なステータス: ',
	'You are editing other part of this page.': '他の部分を編集中です．',
	'You have unsaved changes.' : '保存されていない変更箇所があります．',
	'Can\'t sort rowspaned table.': '縦に連結された表はソートできません．',
	'You can edit this page by double-clicking, too.': '編集したい部分をダブルクリックしても編集が開始できます．',

	'You can click a comment to reply.': '親コメントをクリックで返信になります．',
	'You can click a parent item.': '親項目をクリックで選択できます．',
	'Click to reply': 'クリックで返信',
	'Click to insert as a child': 'クリックで子項目を追加',
	'Reply content': '返信内容',
	'Content of child item': '子項目の内容',
	'Reply to selected comment' : '選択したコメントに返信',
	'Reply to selected item' : '選択した項目の子として追加',
	'Select a location': '位置指定',
	'Abort': 'やめる',
	'Back': '戻す',

	'The passwords do not match.': 'パスワードが一致しません．',

	'Editing preferences (saved in the browser)': '編集設定 (ブラウザに保存されます)',
	'Enable wiki notation completion': 'Wiki記法の補完機能を使用する',
	'Edit by double click': 'ダブルクリックで編集',
	'Edit by Alt+click': 'Alt+クリックで編集',
	'Edit by Control+click': 'Control+クリックで編集',

	'Site menu': 'サイトメニュー',
	'Page menu': 'ページメニュー'
    }
};

function l(key) {
    if((typeof LANGUAGE[LANG]) === 'undefined' || (typeof LANGUAGE[LANG][key]) === 'undefined')
	return key;
    return LANGUAGE[LANG][key];
}

Element.observe(window, 'load', function() {
    messagesPositionUpdate();
    if(messagesElement.adjust)
	messagesElement.adjust();
});
Event.observe(document, 'dom:loaded', main);
