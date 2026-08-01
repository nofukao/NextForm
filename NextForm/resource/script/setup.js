(function() {
    function formSetup(form) {
	form.select('[data-setup-need-name][data-setup-need-name]').each(function(e) {
	    var name = 'const_' + e.getAttribute('data-setup-need-name');
	    var value = e.getAttribute('data-setup-need-value');
	    var option = e.getAttribute('data-setup-need-option');
	    var input = form.down('[name="' + name + '"]');
	    var update = function(event) {
		var r = value == input.getValue();
		if(option && option == 'not')
		    r = !r;
		if(r)
		    e.show();
		else
		    e.hide();
	    };
	    update(null);
	    input.observe('change', update);
	});
	
	form.select('label.file_upload').each(function(e) {
	    var fileInput = e.down('input[type="file"]');
	    var deleteLabel = e.next('label.file_upload_delete');
	    var currentImage = e.next('span.current_image');
	    var childInputs = [];
	    var deleteCheckbox = null;
	    if(deleteLabel) {
		deleteCheckbox = deleteLabel.down('input[type="checkbox"]');
	    }
	    var childInputsHide = function() { childInputs.each(function(s){s.hide()}); };
	    var childInputsShow = function() { childInputs.each(function(s){s.show()}); };

	    e.parentNode.select('.child').each(function(child) {
		childInputs.push(child);
	    });

	    var update = function(event) {
		if(fileInput.files && fileInput.files.length > 0) {
		    if(deleteLabel) deleteLabel.hide();
		    if(currentImage) currentImage.hide();
		    childInputsShow();
		} else if(deleteCheckbox && deleteCheckbox.getValue() == 'on') {
		    if(currentImage) currentImage.hide();
		    childInputsHide();
		} else if(!currentImage) {
		    childInputsHide();
		} else {
		    if(deleteLabel) deleteLabel.show();
		    if(currentImage) currentImage.show();
		    childInputsShow();
		}
	    };

	    fileInput.observe('change', update);
	    if(deleteCheckbox)
		deleteCheckbox.observe('change', update);
	    update();
	});
    }

    function setupMain() {
	$$('form.setup').each(formSetup);
    }

    Event.observe(window, 'load', setupMain);
})();
