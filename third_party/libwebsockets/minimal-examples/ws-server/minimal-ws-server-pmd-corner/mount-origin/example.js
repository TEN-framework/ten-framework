
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

var ws = new Array();

function conn(n)
{
	ws[n] = new_ws(get_appropriate_ws_url("/" + (n + 1)), "lws-minimal");
	ws[n].n = n;
	try {
		ws[n].onopen = function() {
			document.getElementById("r").disabled = 0;
			document.getElementById("status").textContent =
				document.getElementById("status").textContent + " " +
				"ws open "+ ws[n].extensions;
		};
	
		ws[n].onmessage = function got_packet(msg) {
			if (typeof msg.data !== "string") {
				//console.log(msg.data);
				document.getElementById("r").value =
					document.getElementById("r").value +
					ws[n].n + " " + "blob uncompressed length " +
						msg.data.size  + "\n";
			} else
				document.getElementById("r").value =
					document.getElementById("r").value + msg.data + "\n";
			document.getElementById("r").scrollTop =
				document.getElementById("r").scrollHeight;
		};
	
		ws[n].onclose = function(){
			document.getElementById("r").disabled = 1;
			document.getElementById("status").textContent = "ws closed";
		};
	} catch(exception) {
		alert("<p>Error " + exception);  
	}
}

window.addEventListener("load", function() {
	
	var n;

	/*
	 * we make 5 individual connections.  Because if we don't, by default pmd
	 * will reuse its dictionary to make subsequent tests very short. 
	 */
	
	for (n = 0; n < 5; n++)
		conn(n);
	
	console.log("load");
		
}, false);

