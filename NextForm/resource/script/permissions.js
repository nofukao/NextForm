LANGUAGE['ja']['Page name'] = 'ページ名';
LANGUAGE['ja']['Permission'] = '権限';
LANGUAGE['ja']['Action'] = '操作';
LANGUAGE['ja']['Settings for each page'] = 'ページごとに設定';

(function() {
    var tableElement = null;
    function permissionsUpdate() {
	if(!tableElement) {
	    tableElement = new Element('table');
	    var form = $$('form.permissions')[0];
	    if(!form)
		return;
	    form.insertBefore(tableElement, form.firstChild);
	    var thead = new Element('thead');
	    tableElement.appendChild(thead);

	    var tr = thead.appendChild(new Element('tr'));
	    tr.appendChild(new Element('td').update(l('Page name')));
	    tr.appendChild(new Element('td').update(l('Permission')));
	    tr.appendChild(new Element('td').update(l('Action')));
	    tableElement.appendChild(new Element('tbody'));
	}
	var tbody = tableElement.firstChild.nextSibling;
	tbody.clearChildren();
	for(var i = 0; i < targetUserPermissions.length; i++) {
	    var permission = targetUserPermissions[i];
	    var tr = tbody.appendChild(new Element('tr', {'data-index': i}));
	    var td = tr.appendChild(new Element('td'));
	    var input = td.appendChild(new Element('input', {'type': 'text', 'value': permission[0]}));
	    var td = tr.appendChild(new Element('td'));
	    var select = td.appendChild(new Element('select'));
	    for(var j = 0; j < permissionOptions.length; j++) {
		var option = select.appendChild(
		    new Element('option', {'value': permissionOptions[j][0]}).update(permissionOptions[j][1]));
		if(permissionOptions[j][0] == permission[1])
		    option.setAttribute('selected', 'selected');
	    }
	    var td = tr.appendChild(new Element('td'));
	    var upButton = td.appendChild(new Element('button', {'type': 'button'}).update('&#9650;'));
	    var downButton = td.appendChild(new Element('button', {'type': 'button'}).update('&#9660;'));
	    var addButton = td.appendChild(new Element('button', {'type': 'button'}).update('+'));
	    var deleteButton = td.appendChild(new Element('button', {'type': 'button'}).update('-'));
	    if(i == 0)
		upButton.disable();
	    if(i == targetUserPermissions.length - 1)
		downButton.disable();
	    if(targetUserPermissions.length <= 1)
		deleteButton.disable();
	    upButton.observe('click', permissionsUp);
	    downButton.observe('click', permissionsDown);
	    addButton.observe('click', permissionsAdd);
	    deleteButton.observe('click', permissionsDelete);
	}
    }

    function permissionsChangeUpdate() {
	tableElement.select('tr[data-index]').each(function(tr) {
	    var index = parseInt(tr.getAttribute('data-index'));
	    var pattern = tr.down('input[type="text"]').value;
	    var permission = tr.down('select').value;
	    targetUserPermissions[index] = [pattern, permission];
	});
    }

    function permissionsAdd(event) {
	permissionsChangeUpdate();
	var tr = event.findElement('tr[data-index]');
	var index = parseInt(tr.getAttribute('data-index'));
	targetUserPermissions.splice(index, 0, targetUserPermissions[index]);
	permissionsUpdate();
    }

    function permissionsDelete(event) {
	permissionsChangeUpdate();
	var tr = event.findElement('tr[data-index]');
	var index = parseInt(tr.getAttribute('data-index'));
	targetUserPermissions.splice(index, 1);
	permissionsUpdate();
    }

    function permissionsUp(event) {
	permissionsChangeUpdate();
	var tr = event.findElement('tr[data-index]');
	var index = parseInt(tr.getAttribute('data-index'));
	if(index <= 0)
	    return;
	var move = targetUserPermissions[index];
	targetUserPermissions.splice(index, 1);
	targetUserPermissions.splice(index - 1, 0, move);
	permissionsUpdate();
    }

    function permissionsDown(event) {
	permissionsChangeUpdate();
	var tr = event.findElement('tr[data-index]');
	var index = parseInt(tr.getAttribute('data-index'));
	if(index >= targetUserPermissions.length - 1)
	    return;
	var move = targetUserPermissions[index];
	targetUserPermissions.splice(index, 1);
	targetUserPermissions.splice(index + 1, 0, move);
	permissionsUpdate();

    }
    
    function permissionsAdminUser() {
	$$('table select[data-url]').each(function(select) {
	    var option = select.appendChild(new Element('option', {'value': 'page'}));
	    option.update(l('Settings for each page'));
	    select.observe('change', function(event) {
		if(select.value == 'page') {
		    var url = select.getAttribute('data-url');
		    window.location = url;
		}
	    });
	});
	$$('form').each(function(form) {form.reset()});
    }

    function permissionsMain() {

	if(typeof permissionsIsAdminUser != 'undefined' && permissionsIsAdminUser) {
	    permissionsAdminUser();
	    return;
	}

	permissionsUpdate();
	$('submit_button').observe('click', function(event) {
	    permissionsChangeUpdate();
	    var form = $('permissions_form');
	    var json = Object.toJSON(targetUserPermissions);
	    form.addHiddens({'permissions': json});
	    form.submit();
	});
    }
    
    Event.observe(window, 'load', permissionsMain);
})();
