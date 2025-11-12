/* lws-fts.js - JS supporting lws fulltext search
 *
 * Copyright (C) 2010 - 2019 Andy Green <andy@warmcat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */


(function() {
	
	var last_ac = "";
	
	function san(s)
	{
		s.replace(/</g, "!");
		s.replace(/%/g, "!");
		
		return s;
	}
	
	function lws_fts_choose()
	{
		var xhr = new XMLHttpRequest();
		var sr = document.getElementById("searchresults");
		var ac = document.getElementById("acomplete");
		var inp = document.getElementById("lws_fts");

		xhr.onopen = function(e) {
			xhr.setRequestHeader("cache-control", "max-age=0");
		};

		xhr.onload = function(e) {	
			var jj, n, m, s = "", lic = 0;
			var sr = document.getElementById("searchresults");
			var inp = document.getElementById("lws_fts");
			sr.style.width = (parseInt(sr.parentNode.offsetWidth, 10) - 88) + "px";
			sr.style.opacity = "1";
			inp.blur();
			
			// console.log(xhr.responseText);
			jj = JSON.parse(xhr.responseText);
			
			if (jj.fp) {
				lic = jj.fp.length;						
				for (n = 0; n < lic; n++) {
					
					s += "<div class='filepath'>" + jj.fp[n].path + "</div>";
					
					s += "<table>";
					for (m = 0; m < jj.fp[n].hits.length; m++)
						s += "<tr><td class='r'>" + jj.fp[n].hits[m].l +
							 "</td><td>" + jj.fp[n].hits[m].s +
							 "</td></tr>";
					
					s += "</table>";
	
				}
			}
			
			sr.innerHTML = s;
		};
		
		inp.blur();
		ac.style.opacity = "0";
		sr.style.innerHTML = "";
		xhr.open("GET", "../fts/r/" + document.getElementById("lws_fts").value);
		xhr.send();
	}
	
	function lws_fts_ac_select(e)
	{
		var t = e.target;

		while (t) {
			if (t.getAttribute && t.getAttribute("string")) {
				document.getElementById("lws_fts").value =
						t.getAttribute("string");

				lws_fts_choose();
			}

			t = t.parentNode;
		}
	}
	
	function lws_fts_search_input()
	{
		var ac = document.getElementById("acomplete"),
		    sb = document.getElementById("lws_fts");
		
		if (last_ac === sb.value)
			return;
		
		last_ac = sb.value;
		
		ac.style.width = (parseInt(sb.offsetWidth, 10) - 2) + "px";
		ac.style.opacity = "1";
		
		/* detect loss of focus for popup menu */
		sb.addEventListener("focusout", function(e) {
				ac.style.opacity = "0";
		});
		
		
		var xhr = new XMLHttpRequest();

		xhr.onopen = function(e) {
			xhr.setRequestHeader("cache-control", "max-age=0");
		};
		xhr.onload = function(e) {
			var jj, n, s = "", lic = 0;
			var inp = document.getElementById("lws_fts");
			var ac = document.getElementById("acomplete");
			
			// console.log(xhr.responseText);
			jj = JSON.parse(xhr.responseText);
			
			switch(parseInt(jj.indexed, 10)) {
			case 0: /* there is no index */
				break;

			case 1: /* yay there is an index */
			
					if (jj.ac) {
						lic = jj.ac.length;
						s += "<ul id='menu-ul'>";
						for (n = 0; n < lic; n++) {

							if (jj.ac[n] && parseInt(jj.ac[n].matches, 10))
								s += "<li id='mi_ac" + n + "' string='" +
									san(jj.ac[n].ac) + 
									"'><table><tr><td>" + san(jj.ac[n].ac) +
									"</td><td class='r'>" +
									parseInt(jj.ac[n].matches, 10) +
									"</td></tr></table></li>";
						}
						
						s += "</ul>";

					 if (!lic) {
						//s = "<img class='noentry'>";
						inp.className = "nonviable";
						ac.style.opacity = "0";
					 } else {
						 inp.className = "viable";
						 ac.style.opacity = "1";
					 }
					}

				break;
				
			default:
				
				/* an index is being built... */
				
				s = "<table><tr><td><img class='spinner'></td><td>" +
					"<table><tr><td>Indexing</td></tr><tr><td>" +
					"<div id='bar1' class='bar1'>" +
					"<div id='bar2' class='bar2'>" +
					jj.index_done + "&nbsp;/&nbsp;" + jj.index_files +
					"</div></div></td></tr></table>" +
					"</td></tr></table>";
			
				setTimeout(lws_fts_search_input, 300);
			
				break;
			}
			
			ac.innerHTML = s;
			
			for (n = 0; n < lic; n++)
				if (document.getElementById("mi_ac" + n))
					document.getElementById("mi_ac" + n).
						addEventListener("click", lws_fts_ac_select);
			if (jj.index_files) {
				document.getElementById("bar2").style.width =
					((150 * jj.index_done) / (jj.index_files + 1)) + "px";
			}
		};
		
		xhr.open("GET", "../fts/a/" + document.getElementById("lws_fts").value);
		xhr.send();
	}

	document.addEventListener("DOMContentLoaded", function() {
		var inp = document.getElementById("lws_fts");

		inp.addEventListener("input", lws_fts_search_input, false);

		inp.addEventListener("keydown",
				function(e) {
			var inp = document.getElementById("lws_fts");
			var sr = document.getElementById("searchresults");
			var ac = document.getElementById("acomplete");
			if (e.key === "Enter" && inp.className === "viable") {
				lws_fts_choose();
				sr.focus();
				ac.style.opacity = "0";
			}
		}, false);

	}, false);
	
}());
