(function() {
    function passwordMain() {
	$('submit_button').observe('click', function(event) {
	    var form = $('password_form');
	    var username = form.down('input[name="user"]').value;
	    if(typeof AUTH_DIGEST_REALM != 'string' || AUTH_DIGEST_REALM == '' ||
	       !username || username == '') {
		messagesAdd('error', l('Server side error.'));
		return;
	    }
	    
	    var password = $('password').value;
	    if(password != $('password_confirm').value || password == '') {
		messagesAdd('error', l('The passwords do not match.'));
		return;
	    }

	    var digest = MD5_hexhash(username + ':' + AUTH_DIGEST_REALM + ':' + password);
	    var hidden = new Element('input', {type: 'hidden', name:'digest', value: digest});
	    form.appendChild(hidden);
	    
	    form.submit();
	});
    }
    
    Event.observe(window, 'load', passwordMain);
})();
