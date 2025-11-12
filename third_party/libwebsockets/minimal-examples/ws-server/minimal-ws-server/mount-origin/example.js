
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
			document.getElementById("m").disabled = 0;
			document.getElementById("b").disabled = 0;
		};
	
		ws.onmessage =function got_packet(msg) {
			document.getElementById("r").value =
				document.getElementById("r").value + msg.data + "\n";
			document.getElementById("r").scrollTop =
				document.getElementById("r").scrollHeight;
		};
	
		ws.onclose = function(){
			document.getElementById("m").disabled = 1;
			document.getElementById("b").disabled = 1;
		};
	} catch(exception) {
		alert("<p>Error " + exception);  
	}
	
	function sendmsg()
	{
		ws.send(document.getElementById("m").value);
		document.getElementById("m").value = "";
	}
	
	document.getElementById("b").addEventListener("click", sendmsg);
	
}, false);

