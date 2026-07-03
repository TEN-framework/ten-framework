document.addEventListener("DOMContentLoaded", function() {

	var head = 0, tail = 0, ring = new Array(), es;

	es = new EventSource("/sse/sourcename");
	try {
		es.onopen = function() {
			// console.log("EventSource opened");
			document.getElementById("r").disabled = 0;
		};

		es.onmessage = function got_packet(msg) {
			var n, s = "";

			// console.log(msg.data);
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

		/* there is no onclose() for EventSource */
	
	} catch(exception) {
		alert("<p>Error " + exception);  
	}

}, false);
