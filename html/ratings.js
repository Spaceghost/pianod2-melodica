/* pianod web client - jQuery rating widget extension
   Copr. 2015 Perette Barella/Devious Fish
   All rights reserved.
*/

function setup_rating_widget (target) {
	function ratingFromPosition (element, click) {
		var whole = element.width();
		var portion = click.pageX - element.offset().left;
		var rating = Math.round ((portion / whole) * 10) / 2;
		assert (typeof (rating) == 'number');
		if (rating < 0.5) rating = 0.5;
		if (rating > 5) rating = 5;
		return rating;
	}
	function setHover (element, hovering) {
		// element.find ('.outline').css ('visibility', hovering ? 'visible' : 'hidden');
		element.find ('.hoverrating').css ('visibility', hovering ? 'inherit' : 'hidden');
		element.find ('.currentrating').css ('visibility', hovering ? 'hidden' : 'inherit');
	}
	$(target).html ('<span class="outline" visibility="hidden">☆☆☆☆☆</span>' +
					'<span class="hoverrating" visibility="hidden"><span class="value"></span></span>' +
					'<span class="currentrating"><span class="value"></span></span>' +
					'<span class="spacer" visibility="hidden">☆☆☆☆☆</span>')
	.addClass ('button')
	.addClass ('ratingwidget')
	.hover (function () { setHover ($(this), true); },
		 	function () { setHover ($(this), false); }
	)
	.mousemove (function (event) {
		$(this).rating (ratingFromPosition ($(this), event), true);
	});
}

jQuery.fn.extend ({
	rating: function (newvalue, preview) {
		// Get the rating value
		var input = this.find ('.hoverrating');
		assert (input.length <= 1);
		if (typeof (newvalue) == 'undefined') {
			// Widget should be created before reading
			assert (input.length === 1);
			if (input.length === 0)
				return 0.0;
			if (input.css ('visibility') == 'hidden')
				input = this.find ('.currentrating');
			var ratestr = input.attr ('class').match (/ rated([0-5][05])/);
			if (ratestr == null) {
				// User is itchy on the clicker; hoverrating not set.
				// Use existing rating for now.
				ratestr = this.find ('.currentrating').attr ('class').match (/ rated([0-5][05])/);
			}
			assert (ratestr.length == 2);
			assert (ratestr [1] !== "");
			return ratestr [1] / 10;

		}
		// If the widget isn't set up yet, set it up.
		if (input.length === 0) {
			setup_rating_widget (this);
		}

		// Set the rating
		var rating = newvalue.toString().replace ('.', '');
		if (rating.length == 1) {
			rating = rating + '0';
		}
		assert (rating.length == 2);
		var target = preview ? 'hoverrating' : 'currentrating';
		var here = this.find ('.' + target);
		assert (here.length == 1);
		here.attr ('class', target + ' rated' + rating);
		return this;
	},
	/** Add a handler to something (typically an input) so it does something
	    when enter is pressed. */
	entergize: function (action) {
		this.keydown (function (event) {
			var keypressed = event.keyCode || event.which;
			if (keypressed == 13) {
				action.call (this, event);
				return false;
			}
			return true;
		});
	},
	enterclicks: function (target) {
		this.keydown (function (event) {
			var keypressed = event.keyCode || event.which;
			if (keypressed == 13) {
				$(target).click ();
			}
		});
	}
});
