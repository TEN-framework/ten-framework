document.addEventListener("DOMContentLoaded", function() {

	var transport_protocol = "";
	
	if ( performance && performance.timing.nextHopProtocol ) {
	    transport_protocol = performance.timing.nextHopProtocol;
	} else if ( window.chrome && window.chrome.loadTimes ) {
	    transport_protocol = window.chrome.loadTimes().connectionInfo;
	} else {
	
	  var p = performance.getEntriesByType("resource");
	  for (var i=0; i < p.length; i++) {
		var value = "nextHopProtocol" in p[i];
		  if (value)
		    transport_protocol = p[i].nextHopProtocol;
	    }
	   }
	   
	   if (transport_protocol === "h2")
	   	document.getElementById("transport").innerHTML = "<img src=\"/http2.png\">";
}, false);
