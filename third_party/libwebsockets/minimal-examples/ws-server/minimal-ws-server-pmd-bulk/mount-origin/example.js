
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

	var ws = new_ws(get_appropriate_ws_url(""), "lws-minimal-pmd-bulk");
	try {
		ws.onopen = function() {
			document.getElementById("r").disabled = 0;
			document.getElementById("status").textContent = "ws open "+ ws.extensions;
		};
	
		ws.onmessage = function got_packet(msg) {
			console.log("Received ws message len " + msg.data.size);
			document.getElementById("r").value =
				document.getElementById("r").value + "\nReceived: " + msg.data.size + " bytes\n";
			document.getElementById("r").scrollTop =
				document.getElementById("r").scrollHeight;
	
			/* echo it back */
			ws.send(msg.data);
		};
	
		ws.onclose = function(){
			document.getElementById("r").disabled = 1;
			document.getElementById("status").textContent = "ws closed";
		};
	} catch(exception) {
		alert("<p>Error " + exception);  
	}

}, false);

