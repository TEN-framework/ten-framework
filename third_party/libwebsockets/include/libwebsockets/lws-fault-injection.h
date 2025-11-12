/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2021 Andy Green <andy@warmcat.com>
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
 *
 * Fault injection api if built with LWS_WITH_SYS_FAULT_INJECTION
 */

typedef struct lws_xos {
	uint64_t s[4];
} lws_xos_t;

/**
 * lws_xos_init() - seed xoshiro256 PRNG
 *
 * \param xos: the prng state object to initialize
 * \param seed: the 64-bit seed
 *
 * Initialize PRNG \xos with the starting state represented by \p seed
 */
LWS_VISIBLE LWS_EXTERN void
lws_xos_init(struct lws_xos *xos, uint64_t seed);

/**
 * lws_xos() - get next xoshiro256 PRNG result and update state
 *
 * \param xos: the PRNG state to use
 *
 * Returns next 64-bit PRNG result.  These are cheap to get,
 * quite a white noise sequence, and completely deterministic
 * according to the seed it was initialized with.
 */
LWS_VISIBLE LWS_EXTERN uint64_t LWS_WARN_UNUSED_RESULT
lws_xos(struct lws_xos *xos);

/**
 * lws_xos_percent() - return 1 a given percent of the time on average
 *
 * \param xos: the PRNG state to use
 * \param percent: chance in 100 of returning 1
 *
 * Returns 1 if next random % 100 is < \p percent, such that
 * 100 always returns 1, 0 never returns 1, and the chance linearly scales
 * inbetween
 */
LWS_VISIBLE LWS_EXTERN int LWS_WARN_UNUSED_RESULT
lws_xos_percent(struct lws_xos *xos, int percent);

#if defined(LWS_WITH_SYS_FAULT_INJECTION)

enum {
	LWSFI_ALWAYS,
	LWSFI_DETERMINISTIC,	/* do .count injections after .pre then stop */
	LWSFI_PROBABILISTIC,	/* .pre % chance of injection */
	LWSFI_PATTERN,		/* use .count bits in .pattern after .pre */
	LWSFI_PATTERN_ALLOC,	/* as _PATTERN, but .pattern is malloc'd */
	LWSFI_RANGE		/* pick a number between pre and count */
};

typedef struct lws_fi {
	const char		*name;
	const uint8_t		*pattern;
	uint64_t		pre;
	uint64_t		count;
	uint64_t		times;		/* start at 0, tracks usage */
	char			type;		/* LWSFI_* */
} lws_fi_t;

typedef struct lws_fi_ctx {
	lws_dll2_owner_t	fi_owner;
	struct lws_xos		xos;
	const char		*name;
} lws_fi_ctx_t;

/**
 * lws_fi() - find out if we should perform the named fault injection this time
 *
 * \param fic: fault injection tracking context
 * \param fi_name: name of fault injection
 *
 * This checks if the named fault is configured in the fi tracking context
 * provided, if it is, then it will make a decision if the named fault should
 * be applied this time, using the tracking in the named lws_fi_t.
 *
 * If the provided context has a parent, that is also checked for the named fi
 * item recursively, with the first found being used to determine if to inject
 * or not.
 *
 * If LWS_WITH_SYS_FAULT_INJECTION is not defined, then this always return 0.
 */
LWS_VISIBLE LWS_EXTERN int
lws_fi(const lws_fi_ctx_t *fic, const char *fi_name);

/**
 * lws_fi_range() - get a random number from a range
 *
 * \param fic: fault injection tracking context
 * \param fi_name: name of fault injection
 * \param result: points to uint64_t to be set to the result
 *
 * This lets you get a random number from an externally-set range, set using a
 * fault injection syntax like "myfault(123..456)".  That will cause us to
 * return a number between those two inclusive, from the seeded PRNG.
 *
 * This is useful when you used lws_fi() with its own fault name to decide
 * whether to inject the fault, and then the code to cause the fault needs
 * additional constrained pseudo-random fuzzing for, eg, delays before issuing
 * the fault.
 *
 * Returns 0 if \p *result is set, else nonzero for failure.
 */
LWS_VISIBLE LWS_EXTERN int
lws_fi_range(const lws_fi_ctx_t *fic, const char *name, uint64_t *result);

/**
 * lws_fi_add() - add an allocated copy of fault injection to a context
 *
 * \param fic: fault injection tracking context
 * \param fi: the fault injection details
 *
 * This allocates a copy of \p fi and attaches it to the fault injection context
 * \p fic.  \p fi can go out of scope after this safely.
 */
LWS_VISIBLE LWS_EXTERN int
lws_fi_add(lws_fi_ctx_t *fic, const lws_fi_t *fi);

/**
 * lws_fi_remove() - remove an allocated copy of fault injection from a context
 *
 * \param fic: fault injection tracking context
 * \param name: the fault injection name to remove
 *
 * This looks for the named fault injection and removes and destroys it from
 * the specified fault injection context
 */
LWS_VISIBLE LWS_EXTERN void
lws_fi_remove(lws_fi_ctx_t *fic, const char *name);

/**
 * lws_fi_import() - transfers all the faults from one context to another
 *
 * \param fic_dest: the fault context to receive the faults
 * \param fic_src: the fault context that will be emptied out into \p fic_dest
 *
 * This is used to initialize created object fault injection contexts from
 * the caller.
 */
LWS_VISIBLE LWS_EXTERN void
lws_fi_import(lws_fi_ctx_t *fic_dest, const lws_fi_ctx_t *fic_src);

/**
 * lws_fi_inherit_copy() - attach copies of matching fault injection objects to dest
 *
 * \param fic_dest: destination Fault Injection context
 * \param fic_src: parent fault context that may contain matching rules
 * \param scope: the name of the path match required, eg, "vh"
 * \param value: the dynamic name of our match, eg, "myvhost"
 *
 * If called with scope "vh" and value "myvhost", then matches faults starting
 * "vh=myvhost/", strips that part of the name if it matches and makes a copy
 * of the rule with the modified name attached to the destination Fault Injection
 * context.
 */
LWS_VISIBLE LWS_EXTERN void
lws_fi_inherit_copy(lws_fi_ctx_t *fic_dest, const lws_fi_ctx_t *fic_src,
		    const char *scope, const char *value);

/**
 * lws_fi_destroy() - removes all allocated fault injection entries
 *
 * \param fic: fault injection tracking context
 *
 * This walks any allocated fault injection entries in \p fic and detaches and
 * destroys them.  It doesn't try to destroc \p fic itself, since this is
 * not usually directly allocated.
 */
LWS_VISIBLE LWS_EXTERN void
lws_fi_destroy(const lws_fi_ctx_t *fic);

/**
 * lws_fi_deserialize() - adds fault in string form to Fault Injection Context
 *
 * \p fic: the fault injection context
 * \p sers: the string serializing the desired fault details
 *
 * This turns a string like "ss=captive_portal_detect/wsi/dnsfail(10%)" into
 * a fault injection struct added to the fault injection context \p fic
 *
 * You can prepare the context creation info .fic with these before creating
 * the context, and use namespace paths on those to target other objects.
 */

LWS_VISIBLE LWS_EXTERN void
lws_fi_deserialize(lws_fi_ctx_t *fic, const char *sers);

LWS_VISIBLE LWS_EXTERN int
_lws_fi_user_wsi_fi(struct lws *wsi, const char *name);
LWS_VISIBLE LWS_EXTERN int
_lws_fi_user_context_fi(struct lws_context *ctx, const char *name);

#if defined(LWS_WITH_SECURE_STREAMS)
struct lws_ss_handle;
LWS_VISIBLE LWS_EXTERN int
_lws_fi_user_ss_fi(struct lws_ss_handle *h, const char *name);
#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)
struct lws_sspc_handle;
LWS_VISIBLE LWS_EXTERN int
_lws_fi_user_sspc_fi(struct lws_sspc_handle *h, const char *name);
#endif
#endif

#define lws_fi_user_wsi_fi(_wsi, _name) _lws_fi_user_wsi_fi(_wsi, _name)
#define lws_fi_user_context_fi(_ctx, _name) _lws_fi_user_context_fi(_ctx, _name)
#define lws_fi_user_ss_fi(_h, _name) _lws_fi_user_ss_fi(_h, _name)
#define lws_fi_user_sspc_fi(_h, _name) _lws_fi_user_sspc_fi(_h, _name)

#else

/*
 * Helper so we can leave lws_fi() calls embedded in the code being tested,
 * if fault injection is not enabled then it just always says "no" at buildtime.
 */

#define lws_fi(_fi_name, _fic) (0)
#define lws_fi_destroy(_x)
#define lws_fi_inherit_copy(_a, _b, _c, _d)
#define lws_fi_deserialize(_x, _y)
#define lws_fi_user_wsi_fi(_wsi, _name) (0)
#define lws_fi_user_context_fi(_wsi, _name) (0)
#define lws_fi_user_ss_fi(_h, _name) (0)
#define lws_fi_user_sspc_fi(_h, _name) (0)

#endif
