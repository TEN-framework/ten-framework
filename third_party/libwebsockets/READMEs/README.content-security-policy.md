## Using Content Security Policy (CSP)

### What is it?

Modern browsers have recently implemented a new feature providing
a sort of "selinux for your web page".  If the server sends some
new headers describing the security policy for the content, then
the browser strictly enforces it.

### Why would we want to do that?

Scripting on webpages is pretty universal, sometimes the scripts
come from third parties, and sometimes attackers find a way to
inject scripts into the DOM, eg, through scripts in content.

CSP lets the origin server define what is legitimate for the page it
served and everything else is denied.

The CSP for warmcat.com and libwebsockets.org looks like this,
I removed a handful of approved image sources like travis
status etc for clarity...

```
"content-security-policy": "default-src 'none'; img-src 'self' data:; script-src 'self'; font-src 'self'; style-src 'self'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none';",
"x-content-type-options": "nosniff",
"x-xss-protection": "1; mode=block",
"x-frame-options": "deny",
"referrer-policy": "no-referrer"
```

The result of this is the browser won't let the site content be iframed, and it
will reject any inline styles or inline scripts.  Fonts, css, ajax, ws and
images are only allowed to come from 'self', ie, the server that served the
page.  You may inject your script, or deceptive styles: it won't run or be shown.

Because inline scripts are banned, the usual methods for XSS are dead;
the attacker can't even load js from another server.  So these rules
provide a very significant increase in client security.

### Implications of strict CSP

Halfhearted CSP isn't worth much.  The only useful approach is to start
with `default-src 'none'` which disables everything, and then allow the
minimum needed for the pages to operate.

"Minimum needed for the pages to operate" doesn't mean defeat the protections
necessary so everything in the HTML can stay the same... it means adapt the
pages to want the minimum and then enable the minimum.

The main point is segregation of styles and script away from the content, in
files referenced in the document `<head>` section, along these lines:

```
<head>
 <meta charset=utf-8 http-equiv="Content-Language" content="en"/>
 <link rel="stylesheet" type="text/css" href="test.css"/>
 <script type='text/javascript' src="/lws-common.js"></script>
 <script type='text/javascript' src='test.js'></script>
 <title>Minimal Websocket test app</title>
</head>
```

#### Inline styles must die

All styling must go in one or more `.css` file(s) best served by the same
server... while you can approve other sources in the CSP if you have to,
unless you control that server as well, you are allowing whoever gains
access to that server access to your users.

Inline styles are no longer allowed (eg, "style='font-size:120%'" in the
HTML)... they must be replaced by reference to one or more CSS class, which
in this case includes "font-size:120%".  This has always been the best
practice anyway, and your pages will be cleaner and more maintainable.

#### Inline scripts must die

Inline scripts need to be placed in a `.js` file and loaded in the page head
section, again it should only be from the server that provided the page.

Then, any kind of inline script, yours or injected or whatever, will be
completely rejected by the browser.

#### onXXX must be replaced by eventListener

Inline `onclick()` etc are kinds of inline scripting and are banned.

Modern browsers have offered a different system called ["EventListener" for
a while](https://developer.mozilla.org/en-US/docs/Web/API/EventListener)
which allows binding of events to DOM elements in JS.

A bunch of different named events are possible to listen on, commonly the
`.js` file will ask for one or both of

```
window.addEventListener("load", function() {
...
}, false);

document.addEventListener("DOMContentLoaded", function() {
...
}, false);
```

These give the JS a way to trigger when either everything on the page has
been "loaded" or the DOM has been populated from the initial HTML.  These
can set up other event listeners on the DOM objects and aftwards the
events will drive what happens on the page from user interaction and / or
timers etc.

If you have `onclick` in your HTML today, you would replace it with an id
for the HTML element, then eg in the DOMContentLoaded event listener,
apply 

```
   document.getElementById("my-id").addEventListener("click", function() {
   ...
   }, false);
```

ie the .js file becomes the only place with the "business logic" of the
elements mentioned in the HTML, applied at runtime.

#### Do you really need external sources?

Do your scripts and fonts really need to come from external sources?
If your caching policy is liberal, they are not actually that expensive
to serve once and then the user is using his local copy for the next
days.

Some external sources are marked as anti-privacy in modern browsers, meaning
they track your users, in turn meaning if your site refers to them, you
will lose your green padlock in the browser.  If the content license allows
it, hosting them on "self", ie, the same server that provided the HTML,
will remove that problem.

Bringing in scripts from external sources is actually quite scary from the
security perspective.  If someone hacks the `ajax.googleapis.com` site to serve
a hostile, modified jquery, half the Internet will instantly
become malicious.  However if you serve it yourself, unless your server
was specifically targeted you know it will continue to serve what you
expect.

Since these scripts are usually sent with cache control headers for local
caching duration of 1 year, the cost of serving them yourself under the same
conditions is small but your susceptibility to attack is reduced to only taking
care of your own server.  And there is a privacy benefit that google is not
informed of your users' IPs and activities on your site.

