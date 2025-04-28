# lws_plugins

Lws now offers apis to manage your own user plugins with `LWS_WITH_PLUGINS_API`.
Lws uses these apis internally for protocol plugins and event loop plugins
if they're selected for build.  But they are also exported for user code to
use them how you like.

## Creating your plugin export

### Specifying your plugin export type

Lws plugins have a single exported struct with a specified header and a user
defined remainder.  The public `lws_plugin_header_t` describes the common
plugin export header, it's defined via libwebsockets.h as

```
typedef struct lws_plugin_header {
	const char *name;
	const char *_class;

	unsigned int api_magic;
	/* set to LWS_PLUGIN_API_MAGIC at plugin build time */

	/* plugin-class specific superclass data follows */
} lws_plugin_header_t;
```

The exported symbol name itself must match the plugin filename, for
example if the symbol name is `my_plugin`, then the filename of the
plugin might be `libmyapp-my_plugin.so` or similar... the matching
part is after the first `-` or `_`, up to the first `.`.  The exact
details differ by platform but these rules cover the supported
platforms.  If lws has the filename of the plugin, it can then
deduce the symbol export it should look for in the plugin.

`name` is a freeform human-readable description for the plugin.

`_class` is shared by your plugins and used to select them from other kinds
of plugin that may be in the same dir.  So choose a unique name like
`"myapp xxx plugin"` or whatever shared by all plugins of that class.

`api_magic` is set to `LWS_PLUGIN_API_MAGIC` to detect if the plugin is
incompatible with the lws plugin apis version.

So for example your plugin type wrapping the header might look like

```
typedef struct myapp_plugin {
        lws_plugin_header_t	hdr; /* must be first */

	/* optional extra data like function pointers from your plugin */
	mytype_t		mymember;
	/* ... */
} myapp_plugin_t;
```

Typically, you will put function pointers to whatever capability your plugin
class offers as the additional members.

## Building your own plugins

Plugins are built standalone, cmake is recommended but you can do what you want.

The only requirement is the single visible export of the plugin name, eg

```
const myapp_plugin_t my_plugin = {
	.hdr = {
		"my_plugin",
		"myapp xxx plugin",
		LWS_PLUGIN_API_MAGIC
	},

	.mymember	= my_plugin_init,
	/*...*/
};
```

## Bringing in plugins at runtime

Lws provides an api to import plugins into the process space and another to
remove and destroy plugins.

You can take two approaches depending on what you're doing, either bring in and
later destroy a whole class of plugins at once, and walk them via a linked-list,
or bring in and later destroy a single specific plugin from the class by filtering
on its specific export name.

See `include/libwebsockets/lws-protocols-plugins.h` for documentation.

```
LWS_VISIBLE LWS_EXTERN int
lws_plugins_init(struct lws_plugin **pplugin, const char * const *d,
		 const char *_class, const char *filter,
		 each_plugin_cb_t each, void *each_user);

LWS_VISIBLE LWS_EXTERN int
lws_plugins_destroy(struct lws_plugin **pplugin, each_plugin_cb_t each,
		    void *each_user);
```

`struct lws_plugin` is a public struct that contains the linked-list of loaded
plugins and a pointer to its exported header object, so you can walk this
after loading.

