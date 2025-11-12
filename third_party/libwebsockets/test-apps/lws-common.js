/*
 * This section around grayOut came from here:
 * http://www.codingforums.com/archive/index.php/t-151720.html
 * Assumed public domain
 *
 * Init like this in your main html script, this also reapplies the gray
 *
 *    lws_gray_out(true,{'zindex':'499'});
 *
 * To remove the gray
 *
 *    lws_gray_out(false);
 *
 */

function gsize(ptype)
{
	var h = document.compatMode === "CSS1Compat" &&
		!window.opera ?
			document.documentElement.clientHeight :
						document.body.clientHeight;
	var w = document.compatMode === "CSS1Compat" &&
		!window.opera ? 
			document.documentElement.clientWidth :
						document.body.clientWidth;
	var pageWidth, pageHeight, t;

	if (document.body && 
		    (document.body.scrollWidth || document.body.scrollHeight)) {
		t = document.body.scrollWidth;
		pageWidth = (w > t) ? ("" + w + "px") : ("" + (t) + "px");
		t = document.body.scrollHeight;
		pageHeight = (h > t) ? ("" + h + "px") : ("" + (t) + "px");
	} else if (document.body.offsetWidth) {
		t = document.body.offsetWidth;
		pageWidth = (w > t) ? ("" + w + "px") : ("" + (t) + "px");
		t = document.body.offsetHeight;
		pageHeight =(h > t) ? ("" + h + "px") : ("" + (t) + "px");
	} else {
		pageWidth = "100%";
		pageHeight = "100%";
	}
	return (ptype === 1) ? pageWidth : pageHeight;
}

function addEvent( obj, type, fn ) {
	if ( obj.attachEvent ) {
		obj["e" + type + fn] = fn;
		obj[type+fn] = function() { obj["e" + type + fn]( window.event );};
		obj.attachEvent("on" + type, obj[type + fn]);
	} else
		obj.addEventListener(type, fn, false);
}

function removeEvent( obj, type, fn ) {
	if ( obj.detachEvent ) {
		obj.detachEvent("on" + type, obj[type + fn]);
		obj[type + fn] = null;
	} else
		obj.removeEventListener(type, fn, false);
}

function lws_gray_out(vis, _options) {

	var options = _options || {};
	var zindex = options.zindex || 50;
	var opacity = options.opacity || 70;
	var opaque = (opacity / 100);
	var bgcolor = options.bgcolor || "#000000";
	var dark = document.getElementById("darkenScreenObject");

	if (!dark) {
		var tbody = document.getElementsByTagName("body")[0];
		var tnode = document.createElement("div");
		tnode.style.position = "absolute";
		tnode.style.top = "0px";
		tnode.style.left = "0px";
		tnode.style.overflow = "hidden";
		tnode.style.display ="none";
		tnode.id = "darkenScreenObject";
		tbody.appendChild(tnode);
		dark = document.getElementById("darkenScreenObject");
	}
	if (vis) {
		dark.style.opacity = opaque;
		dark.style.MozOpacity = opaque;
		// dark.style.filter ='alpha(opacity='+opacity+')';
		dark.style.zIndex = zindex;
		dark.style.backgroundColor = bgcolor;
		dark.style.width = gsize(1);
		dark.style.height = gsize(0);
		dark.style.display = "block";
		addEvent(window, "resize",
			function() {
				dark.style.height = gsize(0);
				dark.style.width = gsize(1);
			}
		);
	} else {
		dark.style.display = "none";
		removeEvent(window, "resize",
			function() {
				dark.style.height = gsize(0);
				dark.style.width = gsize(1);
			}
		);
	}
}

/*
 * end of grayOut related stuff
 */

function new_ws(urlpath, protocol)
{
	return new WebSocket(urlpath, protocol);
}
 
function lws_san(s)
{
	if (s.search("<") !== -1)
		return "invalid string";
	
	return s;
}
