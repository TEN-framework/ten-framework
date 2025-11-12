(function () {
function check_file()
{
	var f = document.getElementById("file").files[0];
	var max_len = 100000;
	var dis = 0;
	
	if (f) {
		if (f.size >= max_len) {
			dis = 1;
			document.getElementById("file_info").innerHTML =
				"<span style=\"color:red;font-weight:bold\">File larger than " +
							max_len+"</span>";
		} else
			document.getElementById("file_info").innerHTML =
				"File length "+f.size;
	} else
		dis = 1;
	
	document.getElementById("upload").disabled = dis;
}

/*
 * We display untrusted stuff in html context... reject anything
 * that has HTML stuff in it
 */

function san(s)
{
	if (s.search("<") !== -1)
		return "invalid string";
	
	return s;
}

/* BrowserDetect came from http://www.quirksmode.org/js/detect.html */

var BrowserDetect = {
	init: function () {
		this.browser = this.searchString(this.dataBrowser) ||
						"An unknown browser";
		this.version = this.searchVersion(navigator.userAgent)
			|| this.searchVersion(navigator.appVersion)
			|| "an unknown version";
		this.OS = this.searchString(this.dataOS) || "an unknown OS";
	},
	searchString: function (data) {
		for (var i=0;i<data.length;i++)	{
			var dataString = data[i].string;
			var dataProp = data[i].prop;
			this.versionSearchString = data[i].versionSearch || data[i].identity;
			if (dataString) {
				if (dataString.indexOf(data[i].subString) !== -1)
					return data[i].identity;
			}
			else if (dataProp)
				return data[i].identity;
		}
	},
	searchVersion: function (dataString) {
		var index = dataString.indexOf(this.versionSearchString);
		if (index === -1) return 0;
		return parseFloat(dataString.substring(index +
										this.versionSearchString.length + 1));
	},
	dataBrowser: [
		{
			string: navigator.userAgent,
			subString: "Chrome",
			identity: "Chrome"
		},
		{ 	string: navigator.userAgent,
			subString: "OmniWeb",
			versionSearch: "OmniWeb/",
			identity: "OmniWeb"
		},
		{
			string: navigator.vendor,
			subString: "Apple",
			identity: "Safari",
			versionSearch: "Version"
		},
		{
			prop: window.opera,
			identity: "Opera",
			versionSearch: "Version"
		},
		{
			string: navigator.vendor,
			subString: "iCab",
			identity: "iCab"
		},
		{
			string: navigator.vendor,
			subString: "KDE",
			identity: "Konqueror"
		},
		{
			string: navigator.userAgent,
			subString: "Firefox",
			identity: "Firefox"
		},
		{
			string: navigator.vendor,
			subString: "Camino",
			identity: "Camino"
		},
		{		// for newer Netscapes (6+)
			string: navigator.userAgent,
			subString: "Netscape",
			identity: "Netscape"
		},
		{
			string: navigator.userAgent,
			subString: "MSIE",
			identity: "Explorer",
			versionSearch: "MSIE"
		},
		{
			string: navigator.userAgent,
			subString: "Gecko",
			identity: "Mozilla",
			versionSearch: "rv"
		},
		{ 		// for older Netscapes (4-)
			string: navigator.userAgent,
			subString: "Mozilla",
			identity: "Netscape",
			versionSearch: "Mozilla"
		}
	],
	dataOS : [
		{
			string: navigator.platform,
			subString: "Win",
			identity: "Windows"
		},
		{
			string: navigator.platform,
			subString: "Mac",
			identity: "Mac"
		},
		{
			   string: navigator.userAgent,
			   subString: "iPhone",
			   identity: "iPhone/iPod"
	    },
		{
			string: navigator.platform,
			subString: "Linux",
			identity: "Linux"
		}
	]

};

var pos = 0;

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

var params = {};

if (location.search) {
    var parts = location.search.substring(1).split("&");

    for (var i = 0; i < parts.length; i++) {
        var nv = parts[i].split("=");
        if (!nv[0]) continue;
        params[nv[0]] = nv[1] || true;
    }
}

var socket_di;

var mirror_name = "";
if (params.mirror)
	mirror_name = params.mirror;

	console.log(mirror_name);

function ws_open_dumb_increment()
{
	socket_di = new_ws(get_appropriate_ws_url(""), "dumb-increment-protocol");

	try {
		socket_di.onopen = function() {
			document.getElementById("wsdi_statustd").style.backgroundColor =
																"#40ff40";
			document.getElementById("wsdi_status").innerHTML =
				" <b>websocket connection opened</b><br>" +
				san(socket_di.extensions);
		};

		socket_di.onmessage =function got_packet(msg) {
			document.getElementById("number").textContent = msg.data + "\n";
		};

		socket_di.onclose = function(){
			document.getElementById("wsdi_statustd").style.backgroundColor =
																"#ff4040";
			document.getElementById("wsdi_status").textContent =
									" websocket connection CLOSED ";
		};
	} catch(exception) {
		alert("<p>Error" + exception);  
	}
}
	
	var socket_status, jso, s;
	
function ws_open_status()
{	
	
	socket_status = new_ws(get_appropriate_ws_url(""), "lws-status");

	try {
		socket_status.onopen = function() {
			document.getElementById("s_statustd").style.backgroundColor =
																"#40ff40";
			document.getElementById("s_status").innerHTML =
				" <b>websocket connection opened</b><br>" +
				san(socket_status.extensions);
		};

		socket_status.onmessage =function got_packet(msg) {
			var s;
			
			console.log(msg.data);
			
			jso = JSON.parse(msg.data);
			
			if (jso.wss_over_h2 === "1")
				document.getElementById("wstransport").innerHTML =
										"<img src=\"./wss-over-h2.png\">";
			
			document.getElementById("servinfo").innerHTML = 
				"<table><tr><td class=l>Build info</td><td>"+
					san(jso.version) + "</td></tr>" +
					"<tr><td class=l>Server info</td><td>" +
					san(jso.hostname) + "</td></tr>" +
					"</table>";
			s="<table>";
			var n;
			for (n = 0; n < jso.conns.length; n++) {
				var d = new Date(parseInt(jso.conns[n].time, 10) * 1000);
				
				s = s + "<tr><td class=l>client " + (n + 1) +
				"</td><td><b>" + san(jso.conns[n].peer) +
				"</b><br>" + san(d.toString()) +
				"<br>" + san(jso.conns[n].ua) +
				"</td></tr>";
			}
			s = s + "</table>";
			
			document.getElementById("conninfo").innerHTML = s;
		};

		socket_status.onclose = function(){
			document.getElementById("s_statustd").style.backgroundColor =
																	"#ff4040";
			document.getElementById("s_status").textContent =
								" websocket connection CLOSED ";
		};
	} catch(exception) {
		alert("<p>Error" + exception);  
	}
}

function reset() {
	socket_di.send("reset\n");
}


function junk() {
	for(var word = ""; word.length < 9000; word += "a"){}
	socket_di.send(word);
}

function on_pmd() {
	socket_status.send("{ \"RequestType\":\"DDoS\", \"blob\":\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAFAAAABQCAYAAACOEfKtAAAJbElEQVR4Xu2af4xUVxXHv+e9O0OhIEibgrRlF8rOG6CWKiumrUlTo6byj6mmNFta2DdQsRJJrP5j+0cnjSVp1BoSU1yzO28LEeL6h7GJNhpJNa2CBQ1gd9k3QPlRBU2LLV0W2p173zFvZvbtzOybmbd7J9DEd/+befece97nfe+95/4gxEWLAGlZx8aIAWqKIAYYA9QkoGkeKzAGqElA0zxWYAxQk4CmeazAGKAmAU3zWIExQE0CmuaxAmOAmgQ0zWMFxgA1CWiaxwqMAWoS0DRvrQJ3ujeb17Hrx6SuzLwV32x7VzO+1pj3HJprJue8BcBQCSONRzr+2RrHaO2OtHDc1wDcUwqOj0s7nWpVoDp+hOMeA5Au+zggbesuHX+Vti1VoOh3j4BxRxEf8xmVSbe3KlAdP2bOPUmEpaXviiGZsVbq+IsBxgCnpp9YgVPjNan2RwagmXMfUpnUAEAc9k6Rx8AHB0zzy3euU5nUXk02RXMzl+9Ssw8PYN06FeYvOkAmM5dfpzLWL6LGFXkSMR13jIAEmKXMpBM6AIXjeih+BVLqTEcSWfJ/T71ks4bZ9vAYgU1/epC2ZegAFLnhAogEM0tV5x1r/UcGKBw3UJ1sTyVwH8laZ5EU2DO4WCTFmXFbOTbnemxZdHnq9AAMvDVTjF4ObKVS7di8IvA97jOSArNsiLZ8oGBpW5HYRKrkB1IFcGwkiS2dhWkB7B1qE6Z5uiUAew7NEsk5o4EveEtgLw98TwngwIApRlcFoogBVnzdSArUAWg6+T4QS3U69Xi98eiqK7A4vnX1+YsldWbPJmSzk8fJa6FAP67FXTv9sVLZqU1k9rvHibGsmKQzBlTGeih0gqgcA69CFxaO+zKA+4uxEF6W3dbaSXFdA4Bmzt1DhK5SLHycTMd9n4A5pT9oSNqp0GXO1VagcFx/cphZhnZF2tasjwJAkRs+CqJPFvEBIzHA8a8ScQyMAdbbTGgJwAY7FS3LA3edXCyUbJoHitzwKIhK3Zb5ssykr5/Uha9BHigc9whQ3nHyu7DIDe8A0TY/OI/xtJexngnP5ocLRCRAkLLbas1KhOGps6lE2MxvOO6TBvBsMS7Pe8rbtHz7pLhavRLpdwtgCDCkzIS/o9Gff8pg/n55EtlRSqT7h+8B0xhs62AYvHJlf524QZ3duzs0pfCT7aj7gQNsmqPHH1V2qr9+ewBePHYXIICNHfsb1TNzw7Y6e343svdNWh35dpHyQL9iKUV5VGVSu+qt94tx7DrZCa8wA93pP0deiTR80fLDyACjOGthncgAp9FmqwG+CsbnynHkpW1Z04ip5Sai3x0CY3kpU+P9sjt9d6saaSlA9JybJWaMXARDyRFjLrZ1fNiqQLX8DAwmxai4CCAhzblzsWFhsH7W8lvM8eOiRSAGqIUvVqAmvhhgDFCbgKaDqY+B2ayBBV2fQJJnwzQlSL6HjcsvhMaRHUwG/2dXjjWMtXdwPjBjPryCARKXIcQ7sJd8EOn9fFuTbkCBCDOMUZw69596SXWVv4HB2bicXAClBARdwqm95+stEurFER1gz6GESM45DGBFmLNJW+C5E7cKUmfH68pkoh3rl046r8CL7s3CQ+hdFQY2KNvaXS94o8/\" }");
	socket_status.send("{ \"RequestType\":\"SendImage\", \"RequestID\":\"283463389\", \"toType\":\"toUser\", \"toID\":\"1036\", \"fileType\":\"image/jpeg\", \"blob\":\"data:image/jpeg;base64,/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAMCAgICAgMCAgIDAwMDBAYEBAQEBAgGBgUGCQgKCgkICQkKDA8MCgsOCwkJDRENDg8QEBEQCgwSExIQEw8QEBD/2wBDAQMDAwQDBAgEBAgQCwkLEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBD/wAARCAGqAoADASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwD9U6KKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigArlviX43Pw78H3fipdHl1RreSGJLSKQRtK0kqxgBiDjlvSuprhvjNpmoav4INnpljPdznU9Ok8qCMu2xbuJmbA5wFBJPYA0AL4r+K2j+G/hxH8Rra0k1G3ureOezto3CPOXXcFBwcHbk9O1bmg+L9H12wW7juEimSygvrmAtlreOVCy7jjngN+VeOal4L8Vz3Wv8Ag99DvG0Hw5b317pEixMUupLpS0UUf94w5ZMD1FWdLfW/CFxqcF34S126l1vwxpttZfZdPllU3EUMitFIyqREcuOXIHXninYD0y8+KPgCwNkt34ntU/tCGO4tyA7BopPuOSBhQc8bsVYXxtpEc+tHULm2tbPRVheW5afIKyAkEjHy9OOTnNeEHQNX0bw/pTQaH4l0/W38NabbNCNFkvbS/kjgUeTMqrmF1bKncVx1rZ1Lwz4tuV1bVL3wpdSJDf8Ah+/urCGBmFzFAxaeOIf8tNv90ZzgDvRYD2fw34w8NeLoppvDurRXqwNslChlZD1GVYAj8q0rq9tLIRtd3CRCVxGhc4BY9Bn3rlfBl/Z61reqazp/g660q3kjhiW8u7SS0muyoOR5UiqwVc4DEc89q2fFQSTRprVtFk1VrjESWyjhmPQs38AHXdkYx60gNivPb74uwWXhPW/EX/CPzyXmjal/ZQ05Zh5k85ZFjCtjjcZFxxXXeGdO1LSdDtNP1bUPtt1CmJJvXngZ74HGcDOM4FeYXfhPXm+Nq28elXDeG7yaHXbi68s+SLmGORRGW6bi7RNjr8hoA6XU/i3p1pYeHr+w043a+INPk1NMziMQwIiNuY7T/fA/OtS8+J3gnSntLbWtftbO6u4IpxESzhA4BG5gMKPc4ryPSvBHipIvGdheaDei20HT7jRtB/ct/pMLzSSK0Yx8w8t4k47oaj8XaLqVglzJpmjeIbfV7rRLSAWw0WS/sdUdIiBFJsX9yQSVJZhgHPanYD1B/jD4PsfEWseHNdvo9Nm0q7itUeRiwn8yGOQN8q/IP3m3k9R1ra8ceLoPBfgzVfGTWpvYdMs3vPKSQL5qqucBsHGfXFebDwpq15YfE681LwvIuoaotuLdRbl/NK2EAKxHHzgSBxx3Bre+IukavqHwA1TRbPTbq41GXw75C2scTNM0vkAbAgGS2eMYzmgDt9c1xNF0CfXXiRlhjWQo8mwckDlsHHX0rMf4leCYNUg0O78QW0OoT+WogO4gO+Nql8bQTkYBIrjfFvjJfFvgjUfDeleE/Fq3s1qgQXHh68gQlXTI3PGBnr3rjdc0rWtM1K8Ph3QdeXVbq6tJW0250iS50/UGUIPN89V2wEYOSzfKVziiwHsPhL4k+GfGer6zoujXLvcaLcfZ5gyMA3yg7gSOmTj8K6qvPfh9aT6V418b2d3ot5bG81NL22uDZusE0Jt4l+WXGwkMrZUHI9K9CpAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUVma34m8OeGvsf/CQ67YaZ/aNyllafa7hIvtFw/3Yk3EbnPZRyaANOiisbQfGfhHxTc39l4a8T6XqtxpUohvorO7jme1kOcLIFJKHg8HHQ0AbNFZ2u+ItA8MWa6j4j1qx0u1eVIFmvLhYUMjnCoGYgbieg6mi08R+H7/VrnQbLW7G41KzjSa4s47hWmhjb7rOgOVB7EjmgDRoorO1/wAQ6D4V0uXXPE2tWOladAVEt3ezrDChZgq5diAMkgDnqRQBo0UgORkUtABRVbUtT07RrCfVNWvreys7VDJPcTyCOONB1ZmPAHuaNO1LT9XsYNT0q9gvLO5QSQzwSB45EPRlYcEe4oAs0VT1fWNJ8P6Zcazrup2un2FonmT3V1KsUUS+rMxAA9zU9tc297bRXlnPHPBOiyxSxsGV0YZDAjggggg0AS0UUmRnGeaAFooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooqrqGpWumQ+dcsQCcKBySaaTk7ITairstUVz3/AAmdh/z63H/jv+NH/CZ2H/Prcf8Ajv8AjW31ar/KZfWKfc6Giue/4TOw/wCfW4/8d/xo/wCEzsP+fW4/8d/xo+rVf5Q+sU+50NFc9/wmdh/z63H/AI7/AI0f8JnYf8+tx/47/jR9Wq/yh9Yp9zoaK57/AITOw/59bj/x3/GtDTNcstVLJBuWRRkow5xUyoVIK8kVGtCTsmaNFFFZGgUUUUAFFFFABXin7YfhS78TfAbxBeaUCup+HVj16ykUZaOS1cSkr7lVYfjXtdVtRsLXVLC50y+hWW3u4nhljboyMMEH8DQBxNz8UNOb4IH4tQyrHaz6AurREH7peEMg/wC+mAr52/Zu8OXnwh+KfhZtT3xn4seHLi9u1JO3+0Y5DcA/UxMwA9qxNG1PUX+COl/s0vcNJrFt46m8ITg9Xs7aYzE/TyvK9sGvaf2ptPTwl4J8LfEvS4WB+HGuWGolYx8xsg4imT6eU7E/SgBn7Qif8Jp8Ufhb8KU/eQTanJ4h1KP0gtgBG3/fbH8q6zw7rXw8j+M/jO3stIltPENlpdrNq2oSSnypbfLbAATgbcEk4rmPh40fj39pfx146jdZ7DwtYWnhrT5lOV8woJ7gA/70gH4Vm6DDpFx+0b8XLfX7mO30yXwzZpeTSSCNY4T5gdix4UAZ57UAbEH7S2r67YyeJfAvwY8TeIfDCFzHq0UkcX2iNTgyRRN8zrwcHIzXK/tQ+P8Aw78Sf2QNf8X+Grh3sriWzVllXZJDIt5EHjcdmB6irvg3wv8AtE/BnwxZ6B4BPhj4ieEbGEDSkkujZXv2Xqi7yDG+FxghuRiuE+Mvivwf4x/Y78djw34Tl8MXVtq8EOs6TKuJLe+a7hMhPJB3cEEcGgD2LU/2htUjt5tY8J/CHxL4j8O2u7fq9u0cccqL96SFG+aROCQeMjpXo/gHx34f+JPhSx8Y+GLhpbC/Qsu9drowOGRh2YHgitXTNNsdM0q10mxtkitLaBIIoVGFWNVwFA9McV4l+xt8vw28QQrwkPjDV0Reyr5q8D86AO2/aF/5Ij40/wCwRN/KpPgF/wAkX8G/9giD/wBBqP8AaF/5Ij40/wCwRN/KpPgF/wAkX8G/9giD/wBBoAn+Nt74T074UeJb7xzpcupaDDZFr+0ico8se5flBBBHOO9Y3iH4w+Gfh/4c8Jabo3h/UtV1HxDZxLoWh2QDTyRLCG5ZjhVVcZY1D+1X/wAm8eOv+wWf/Q1rktd+Hep+Lrb4aeLPh5440vRvHnhbQIns7W+Hmw3NrNbxrKskaneFPHzAH9aAOn8P/H+ZvF2neC/iJ8OtY8GX2tFk0uW6lSe3unUZKCRcbWx2IryTxZ8WPF1p+1VodxD8JvFM/wBj0TULaOxjeLddruT9+g3Y2jHfnmuq174jeNPDus+HbL9o/wCEumHSm1SKHT/EWk3v2m3t71uI2eMhZI89Bwa1vEP/ACeL4Q/7FbUv/Qo6APcrOeS6tIbmW3e3eWNXaKT70ZIztOO46VNRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABXKeNSfNtRnjax/UV1dcp41/wBda/7rfzFdOD/jI58V/CZzVeear8Q9V0r4tWHg6e3g/sa8tlDT4PmR3D58vJ6YYgL9WFeh15/4t+HOo+IdY1fVrW/t4JLnTYobB23boLuKQSRyHA+6GVTxzxX0eF9lzNVdrf0/keFiPacqdPe5Qb4sXMPxG1fRbiGIeH9Js2Zp1UmWSdCBJjnGFZtv1U11Ws/ELw1oKeZqc8sajS5NX3bMj7OhQHv97Mi8Vx8fwf1MWcEMmq2rTto89reTYbMt7LIZXlAx90uSfXmn/wDCAePdSujfaxceHo3h8PzaNDAvmTxu7PEdz7lX5SI8EDkZ4zXVKGFk009Fo/Pz+ZzxniIp3Wr/AK/A2n+JNneQwG3t77S5jfQWzx39n8zrIpK7drYwcdcnGOlWrP4m+H77V49LtrXUmimuTaRX/wBm/wBFeYdUD5znIxnGM965DTvhN4lW4M00+n6fafbbW5XT4Lya4iTyg+91aRQQW3gbeny1veGPCXjXw+bTQFvdK/sKyvJLlZlLm6ljZy4jKFdo+ZuWDHipqU8Mk+V/j/V/QcJ4htcyO/rV8LkjWoAD1D5/75NZVavhf/kNwfR//QTXk1v4cvRnp0f4kfU7uiiivBPZCiiigAooooAKKKKAPEbP9mu3tf2kLv47HX1ayngLx6P5JxFetFHG9wGzjJEYzxnnrXqHjzwlZePfBWueC9RIW31uwnsJGIztEiFdw9xnNb1FAHl/7O/wZl+B/wAPh4Tv9eGt6ncXk99f6j5Rj+0SyOSCQSTwu1eT2p0HwVtp/iH428Wa3qEV7pfjPSItIn0/yirLGoYPl88hgxHAFenUUAeC+G/hh+0R8OdFh8D+CPiV4fvvD9kgttNm1nS3lvLK3UYSPcjqsm0YA3L0Apus/suyan8E/EXw0HjBpNc8VX8Wp6prdzb7vNuFmRziNSMLhNoGeM175RQAijaoX0GK8/8Agp8L7j4TeG9T0G51iPUm1DW73VhIkJjCLOwITBJyRjr3r0GigDm/iP4Tl8d+BNc8HQ3q2b6vZvarOybxGW7lQRn868h8L/CT9qDwj4e0/wAM6R8dPC62WmQLbwB/C+5gijjJMnNfQVFAHhmsfCL46eNfBviXwX8QPi1oWo2euaebSE2ugfZzBIWB3kh8sMDGOK0PGHwM128bwh4o8B+MF0Pxh4Q09NNivHtvNtr238tUkimiJ5VtoI5yp6GvY6KAPDL/AOEXxa+Juo6Snxj8ZaIdB0i9i1AaVolg8Qu54jmNpZJHZtoPO1cVr/FL4ReMvEPxB8PfFD4c+MLLRNa0O1uLGSO+sftMFzBNjIIDKQRtGCDXrlFAENmt0lpCl9KklwsaiV0XarPjkgdhntU1FFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFZ+r6RDq0IR2KOnKOO1aFFVGTg+aO4pRUlZnKf8IVN/wBBBP8Av2f8aP8AhCpv+ggn/fs/411dFb/XK3cw+q0uxyn/AAhU3/QQT/v2f8aP+EKm/wCggn/fs/411dFH1yt3D6rS7HKf8IVN/wBBBP8Av2f8aP8AhCpv+ggn/fs/411dFH1yt3D6rS7HKf8ACFTf9BBP+/Z/xrT0bw7FpUhuHl82XGAcYArYoqZ4mrNcrZUcPTg7pBRRRWBsFFFFABRRRQAVFcPcRqDbwLK2eQX24H5GpaKAKX2jVP8AoGx/+BH/ANjR9o1T/oGx/wDgR/8AY1dooApfaNU/6Bsf/gR/9jR9o1T/AKBsf/gR/wDY1dooApfaNU/6Bsf/AIEf/Y0faNU/6Bsf/gR/9jV2igCl9o1T/oGx/wDgR/8AY0faNU/6Bsf/AIEf/Y1dooApfaNU/wCgbH/4Ef8A2NH2jVP+gbH/AOBH/wBjV2igCl9o1T/oGx/+BH/2NH2jVP8AoGx/+BH/ANjV2igCl9o1T/oGx/8AgR/9jR9o1T/oGx/+BH/2NXaKAKX2jVP+gbH/AOBH/wBjR9o1T/oGx/8AgR/9jV2igCl9o1T/AKBsf/gR/wDY0faNU/6Bsf8A4Ef/AGNXaKAKX2jVP+gbH/4Ef/Y0faNU/wCgbH/4Ef8A2NXaKAKX2jVP+gbH/wCBH/2NH2jVP+gbH/4Ef/Y1dooApfaNU/6Bsf8A4Ef/AGNH2jVP+gbH/wCBH/2NXaKAKX2jVP8AoGx/+BH/ANjR9o1T/oGx/wDgR/8AY1dooApfaNU/6Bsf/gR/9jR9o1T/AKBsf/gR/wDY1dooApfaNU/6Bsf/AIEf/Y0faNU/6Bsf/gR/9jV2igCl9o1T/oGx/wDgR/8AY0faNU/6Bsf/AIEf/Y1dooApfaNU/wCgbH/4Ef8A2NH2jVP+gbH/AOBH/wBjV2igCl9o1T/oGx/+BH/2NH2jVP8AoGx/+BH/ANjV2igCl9o1T/oGx/8AgR/9jR9o1T/oGx/+BH/2NXaKAKX2jVP+gbH/AOBH/wBjR9o1T/oGx/8AgR/9jV2igCl9o1T/AKBsf/gR/wDY0faNU/6Bsf8A4Ef/AGNXaKAKX2jVP+gbH/4Ef/Y0faNU/wCgbH/4Ef8A2NXaKAKX2jVP+gbH/wCBH/2NH2jVP+gbH/4Ef/Y1dooApfaNU/6Bsf8A4Ef/AGNH2jVP+gbH/wCBH/2NXaKAKX2jVP8AoGx/+BH/ANjR9o1T/oGx/wDgR/8AY1dooApfaNU/6Bsf/gR/9jR9o1T/AKBsf/gR/wDY1dooApfaNU/6Bsf/AIEf/Y0faNU/6Bsf/gR/9jV2igCl9o1T/oGx/wDgR/8AY0faNU/6Bsf/AIEf/Y1dooApfaNU/wCgbH/4Ef8A2NH2jVP+gbH/AOBH/wBjV2igCl9o1T/oGx/+BH/2NH2jVP8AoGx/+BH/ANjV2igCl9o1T/oGx/8AgR/9jR9o1T/oGx/+BH/2NXaKAKX2jVP+gbH/AOBH/wBjR9o1T/oGx/8AgR/9jV2igCl9o1T/AKBsf/gR/wDY0faNU/6Bsf8A4Ef/AGNXaKAKX2jVP+gbH/4Ef/Y0faNU/wCgbH/4Ef8A2NXaKAKX2jVP+gbH/wCBH/2NH2jVP+gbH/4Ef/Y1dooApfaNU/6Bsf8A4Ef/AGNOSfUWdRJp8aqSMsJ84HrjFW6KACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACioZry0t/8AX3Mcf+8wFUZvEekxcfaN5/2VJosBqUVz03jC3GRBau3+8QKozeLb9/8AVRRxj86fKwOvorg5tc1Sb7124HoOKjtdUvba4WcXDtgjILZBFPlA9AoooqQCiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACori5gtYzLcSqijualrkPFsztfpAWO1EBA9zTSuBqzeK9NjyIxJIfYYFUpvGMh/497NR/vtn+Vc3RV8qA1pvE+rS/dlWMf7Kj+tUZtRv7j/XXcrD0LnFV6KdkAUUUUAFFFFABQOtFA60AelDpS0g6UtZAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFcb4r/5Cv/bJf612Vcb4r/5Cv/bJf61UdwMaiiirAKKKVVZztVSSewFACUVch0fU5/8AV2UuD3K4H61eh8KanJ/rDFF/vNn+VF0Bi0V08Pg5BzPeE+yrV2HwxpUX3o3kP+01LmQHF0DrXb3ui6b9ilCWqIVQkEdQRXEDrQncD0odKWkHSlrMAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigArjfFf8AyFf+2S/1rsq43xX/AMhX/tkv9aqO4FfSNGl1UuVkEaJwWIzzW7D4RsU5lmlkPpnApvg//jzn/wCuv9BW/Q27gZ8Og6TD92zRv9/5v51djhihG2KJUHooxT6KkAooooAKKKKAIbz/AI9Jv+ubfyrzoda9FvP+PSb/AK5t/KvOh1q4gelDpS0g6UtQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAVxviv8A5Cv/AGyX+tdlXG+K/wDkK/8AbJf61UdwNPwf/wAec3/XX+grfrA8H/8AHnN/11/oK36T3AKKKKQBRRRQAUUUUAQ3n/HpN/1zb+VedDrXot5/x6Tf9c2/lXnQ61cQPSh0paQdKWoAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAK43xX/yFf+2S/wBa7KuN8V/8hX/tkv8AWqjuBp+D/wDjzm/66/0Fb9YHg/8A485v+uv9BW/Se4BRRRSAKKKKACiiigCG8/49Jv8Arm38q86HWvRbz/j0m/65t/KvOh1q4gelDpS0g6UtQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAVxviv/AJCv/bJf612Vcb4r/wCQr/2yX+tVHcDT8H/8ec3/AF1/oK36wPB//HnN/wBdf6Ct+k9wCiiikAUUVDc3dtZp5lzMsY7ZPX6UATUViy+K9MQ4QSye4XH86j/4S+w/59p/yH+NOzA2Lw4tJs/882/lXnQ610GqeKBdW7W9pCyBxhmbGce1c+OtXFWA9KHSlpB0pazAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiioLy9t7GEz3MgVR09SfQUAT0Vy9z4vkJItLYAdi55/Kqh8Vaqe8Q+iVXKwOzri/FDq+qttOdqKp+vNNfxNqrqV81Fz3C81mO7yOXdizMcknvTirAdV4P/AOPOb/rr/QVv1geD/wDjzm/66/0Fb9S9wCiiikBXv7tLG1kuX/gHA9TXBXd3PeztPO5Zj09h6Cuq8WFhpqgdDIM/rXH1cUAUUUVQBQOtFS2sTT3MUKjJdwv5mgD0UdKWkpayAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACuH8QX73l+67j5cR2qP5mu4rzzUI2ivZ43HIc/zqo7gV6KKKsAooooA6zweP8AQpj/ANNf6Ct+snwzbNb6WjMMGUl/w7fpWtWb3AKKKKQFXU7IX9lJbHgsMqfQ1wU8EttK0MyFWU4INej1UvtLstQXFzECezDgiqTsB5/RXUyeDoScxXjKPQrmmr4OT+K+P4J/9eq5kBzFdH4Y0h/MGo3CFQv+rB7n1rSs/DWnWrB2VpWHQv0/KtUAAYAwBUuXYBaKKKkAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKK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RGJ2jIDLzkMM9wQCPpXM6V8IfDuk614c1uC7vGm8N2ktrAjsu2YuCDJIMcuNzYIx9413VFAHBn4VfY2a48PeMNY0m6a+vr1pYhFIr/apmldGjkRkIDMdpIyB3qfwl8K9E8IajZ6rY3t3LPa2dzaMZSuJTPKkjyMABhtyDgYGCeK7WigDC8N+EbHwzoM3h+0uZ5YZp7qcvJjcDPK8jDgAYBcgewFc6vwc0NbW1tU1fVI/sel3WlRSRSiOQJPKspkDKAQ6soxjj1Brv6KAPPf+FQwX76jdeJvFWp6xe39gNNW5ljgiaGAMWG0RIqk7jnLA10Wg+F73RZ7SWfxVqmoJa2IsvJn8tY5CGBEpVFA3gDbxgYzxmugooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigD//Z\"}");
	socket_status.send("{ \"RequestType\":\"SendImage\", \"RequestID\":\"788346414\", \"toType\":\"toUser\", \"toID\":\"1036\", \"fileType\":\"image/jpeg\", \"blob\":\"data:image/jpeg;base64,/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAMCAgICAgMCAgIDAwMDBAYEBAQEBAgGBgUGCQgKCgkICQkKDA8MCgsOCwkJDRENDg8QEBEQCgwSExIQEw8QEBD/2wBDAQMDAwQDBAgEBAgQCwkLEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBD/wAARCAHgAoADASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwD9U6KKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAopNy/wB4fnRuX+8PzoAWik3L/eH50bl/vD86AFopNy/3h+dG5f7w/OgBaKTcv94fnRuX+8PzoAWik3L/AHh+dG5f7w/OgBaKTcv94fnRuX+8PzoAWik3L/eH50bl/vD86AFopNy/3h+dG5f7w/OgBaKTcv8AeH50bl/vD86AFopNy/3h+dG5f7w/OgBaKTcv94fnRuX+8PzoAWik3L/eH50bl/vD86AFopNy/wB4fnRuX+8PzoAWik3L/eH50bl/vD86AFopNy/3h+dG5f7w/OgBaKTcv94fnRuX+8PzoAWik3L/AHh+dG5f7w/OgBaKTcv94fnRuX+8PzoAWik3L/eH50bl/vD86AFopNy/3h+dG5f7w/OgBaKTcv8AeH50bl/vD86AFopNy/3h+dG5f7w/OgBaKTcv94fnRuX+8PzoAWik3L/eH50bl/vD86AFopNy/wB4fnRuX+8PzoAWik3L/eH50bl/vD86AFopNy/3h+dG5f7w/OgBaKTcv94fnRuX+8PzoAWik3L/AHh+dG5f7w/OgBaKTcv94fnRuX+8PzoAWik3L/eH50bl/vD86AFopNy/3h+dG5f7w/OgBaKTcv8AeH50bl/vD86AFopNy/3h+dG5f7w/OgBaKTcv94fnRuX+8PzoAWik3L/eH50bl/vD86AFopNy/wB4fnRuX+8PzoAWik3L/eH50bl/vD86AFopNy/3h+dG5f7w/OgBaKTcv94fnRuX+8PzoAxst6mky3940UmRQA7J9TRk+ppKKAAFh/EaCWP8RoooAXJ9TRk+ppKKADL/AN40Zf8AvGkwKMCgA3N/eNG5v7xoI70Ad6AF3Mf4iPxpMsP4j+dHSlxmgBCWH8R/OlBb+8fzpCM96OgoAVWb+8fzoLN6n86OBTSc0AOy3940Zb+8aTg0YFACkv8A3jRlv7xpNvvSYx3oAXLn+I0Ev/eNAwOc0YBNABubHU/nSbm/vH86VaMc5oATc394/nTt3+0aTb70baAAl8/eP50uW/vGk20baADc3940B27k0babQA/Lf3jRlv7xpMAdTRgH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iiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKAP/9k=\"}");
}

var socket_ot;

function ot_open() {
	socket_ot = new_ws(get_appropriate_ws_url(""), "dumb-increment-protocol");

	console.log("ot_open");

	try {
		socket_ot.onopen = function() {
			document.getElementById("ot_statustd").style.backgroundColor =
																"#40ff40";
			document.getElementById("ot_status").innerHTML =
					" <b>websocket connection opened</b><br>" +
					san(socket_di.extensions);
			document.getElementById("ot_open_btn").disabled = true;
			document.getElementById("ot_close_btn").disabled = false;
			document.getElementById("ot_req_close_btn").disabled = false;
			console.log("ot_open.onopen");
		};

		socket_ot.onclose = function(e){
			document.getElementById("ot_statustd").style.backgroundColor =
																"#ff4040";
			document.getElementById("ot_status").textContent =
						" websocket connection CLOSED, code: " + e.code +
						", reason: " + e.reason;
			document.getElementById("ot_open_btn").disabled = false;
			document.getElementById("ot_close_btn").disabled = true;
			document.getElementById("ot_req_close_btn").disabled = true;
		};
	} catch(exception) {
		alert("<p>Error" + exception);  
	}
}

/* browser will close the ws in a controlled way */
function ot_close() {
	socket_ot.close(3000, "Bye!");
}

/* we ask the server to close the ws in a controlled way */
function ot_req_close() {
	socket_ot.send("closeme\n");
}

var socket_lm;
var pending = "";

function lm_timer_handler(ev) {
	socket_lm.send(pending);
	pending="";
}

/* lws-mirror protocol */

var down = 0;
var no_last = 1;
var last_x = 0, last_y = 0;
var ctx;
var color = "#000000";
var lm_timer;

function ev_mousemove (ev) {
	var x, y;

	if (ev.offsetX) {
		x = ev.offsetX;
		y = ev.offsetY;
	} else {
		x = ev.layerX - offsetX;
		y = ev.layerY - offsetY;

	}

	if (!down)
		return;
	if (no_last) {
		no_last = 0;
		last_x = x;
		last_y = y;
		return;
	}
	pending = pending + "d " + color + " " + last_x + " " + last_y +
			" " + x + " " + y + ";";
			
	if (pending.length > 400) {
		socket_lm.send(pending);
		clearTimeout(lm_timer);
		pending = "";
	} else
		lm_timer = setTimeout(lm_timer_handler, 1);

	last_x = x;
	last_y = y;
}

function ev_mousedown (ev) {
	down = 1;
}

function ev_mouseup(ev) {
	down = 0;
	no_last = 1;
}


function ws_open_mirror()
{	
	socket_lm = new_ws(get_appropriate_ws_url("?mirror=" + mirror_name),
			"lws-mirror-protocol");
	try {
		socket_lm.onopen = function() {
			document.getElementById("wslm_statustd").style.backgroundColor =
																	"#40ff40";
			document.getElementById("wslm_status").innerHTML =
								" <b>websocket connection opened</b><br>" +
								san(socket_lm.extensions);
			lws_gray_out(false);
		};

		socket_lm.onmessage =function got_packet(msg) {
			j = msg.data.split(";");
			var f = 0;
			while (f < j.length - 1) {
				i = j[f].split(" ");
				if (i[0] === "d") {
					ctx.strokeStyle = i[1];
					ctx.beginPath();
					ctx.moveTo(+(i[2]), +(i[3]));
					ctx.lineTo(+(i[4]), +(i[5]));
					ctx.stroke();
				}
				if (i[0] === "c") {
					ctx.strokeStyle = i[1];
					ctx.beginPath();
					ctx.arc(+(i[2]), +(i[3]), +(i[4]), 0, Math.PI*2, true); 
					ctx.stroke();
				}

				f++;
			}
		};

		socket_lm.onclose = function(){
			document.getElementById("wslm_statustd").style.backgroundColor =
																	"#ff4040";
			document.getElementById("wslm_status").textContent =
											" websocket connection CLOSED ";
			lws_gray_out(true,{"zindex":"499"});
		};
	} catch(exception) {
		alert("<p>Error" + exception);  
	}

	var canvas = document.createElement("canvas");
	canvas.height = 300;
	canvas.width = 480;
	ctx = canvas.getContext("2d");

	document.getElementById("wslm_drawing").appendChild(canvas);

	canvas.addEventListener("mousemove", ev_mousemove, false);
	canvas.addEventListener("mousedown", ev_mousedown, false);
	canvas.addEventListener("mouseup", ev_mouseup, false);

	offsetX = offsetY = 0;
	element = canvas;
      if (element.offsetParent) {
        do {
          offsetX += element.offsetLeft;
          offsetY += element.offsetTop;
          element = element.offsetParent;
        } while (element);
      }
}

function update_color() {
	color = document.getElementById("color").value;
}

/* stuff that has to be delayed until all the page assets are loaded */

window.addEventListener("load", function() {
	
	lws_gray_out(true,{"zindex":"499"});

	document.getElementById("file").onchange = check_file;
	document.getElementById("offset").onclick = reset;
	document.getElementById("junk").onclick = junk;
	document.getElementById("color").onclick = update_color;
	document.getElementById("ot_open_btn").onclick = ot_open;
	document.getElementById("ot_close_btn").onclick = ot_close;
	document.getElementById("ot_req_close_btn").onclick = ot_req_close;
	document.getElementById("pmd").onclick = on_pmd;

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
	   
	   console.log("transport protocol " + transport_protocol);
	   
	   if (transport_protocol === "h2")
		   document.getElementById("transport").innerHTML =
			   								"<img src=\"./http2.png\">";

	   BrowserDetect.init();

	   document.getElementById("brow").textContent = " " +
	   		BrowserDetect.browser + " " + BrowserDetect.version + " " +
	   		BrowserDetect.OS +" ";

	   document.getElementById("number").textContent =
		   get_appropriate_ws_url(mirror_name);
	   
	   /* create the ws connections back to the server */
	   
	   ws_open_dumb_increment();
	   ws_open_status();
	   ws_open_mirror();

}, false);

}());
