var head = 0, tail = 0, ring = new Array();

function get_appropriate_ws_url(extra_url)
{
	var pcol;
	var u = document.URL;

	/*
	 * We open the websocket encrypted if this page came on an
	 * https:// url itself, otherwise unencrypted
	 */

	if (u.substring(0, 5) === "https") {
		pcol = "wss://";
		u = u.substr(8);
	} else {
		pcol = "ws://";
		if (u.substring(0, 4) === "http")
			u = u.substr(7);
	}

	u = u.split("/");

	/* + "/xxx" bit is for IE10 workaround */

	return pcol + u[0] + "/" + extra_url;
}

function new_ws(urlpath, protocol)
{
	return new WebSocket(urlpath, protocol);
}

document.addEventListener("DOMContentLoaded", function() {

	var ws = new_ws(get_appropriate_ws_url(""), "lws-minimal");
	try {
		ws.onopen = function() {
			document.getElementById("r").disabled = 0;
		};
	
		ws.onmessage =function got_packet(msg) {
			var n, s = "";
	
			ring[head] = msg.data + "\n";
			head = (head + 1) % 50;
			if (tail === head)
				tail = (tail + 1) % 50;
	
			n = tail;
			do {
				s = s + ring[n];
				n = (n + 1) % 50;
			} while (n !== head);
	
			document.getElementById("r").value = s; 
			document.getElementById("r").scrollTop =
				document.getElementById("r").scrollHeight;
		};
	
		ws.onclose = function(){
			document.getElementById("r").disabled = 1;
		};
	} catch(exception) {
		alert("<p>Error " + exception);  
	}

}, false);
