/*
 * Generic Settings storage
 *
 * Copyright (C) 2020 Andy Green <andy@warmcat.com>
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
 *
 * This is like an abstract class for non-volatile storage, whether in a file-
 * system or flash-backed blocks, etc.  Named blobs of variable size are stored
 * in nonvolatile media of some sort.  Typically, these are JSON objects under
 * a naming scheme like, eg, "network".
 *
 * There's a platform-specific storage identifier opaque_plat provided when the
 * storage object is instantiated, this describes eg the storage device or
 * partition in instantiation-specific terms.
 *
 * Blobs have a further "filename" associated with them.
 */

#define LSOOPEN_FLAG_WRITEABLE				(1 << 0)

struct lws_settings_ops;

typedef struct {
	void						*handle_plat;
	const struct lws_settings_ops			*so;
	uint8_t						refcount;
	void						*opaque_plat;
} lws_settings_instance_t;

typedef struct lws_settings_ops {
	int (*get)(lws_settings_instance_t *si, const char *name,
		   uint8_t *dest, size_t *max_actual);
	/**< if dest is NULL, max_actual is set to the actual length without
	 * copying anything out */
	int (*set)(lws_settings_instance_t *si, const char *name,
		   const uint8_t *src, size_t len);
} lws_settings_ops_t;

/**
 * lws_settings_plat_get() - read a named blob from a settings instance
 *
 * \param si: the settings instance
 * \param name: the name of the setting blob in the instance
 * \param dest: NULL, or the buffer to copy the setting blob info
 * \param max_actual: point to size of dest, or zero; actual blob size on exit
 *
 * If the named blob doesn't exist in the si, or can't read, returns nonzero.
 * Otherwise, returns 0 and sets *max_actual to the true blob size.  If dest is
 * non-NULL, as much of the blob as will fit in the amount specified by
 * *max_actual on entry is copied to dest.
 */
LWS_VISIBLE LWS_EXTERN int
lws_settings_plat_get(lws_settings_instance_t *si, const char *name,
		      uint8_t *dest, size_t *max_actual);

/**
 * lws_settings_plat_get() - read a named blob from a settings instance
 *
 * \param si: the settings instance
 * \param name: the name of the setting blob in the instance
 * \param src: blob to copy to settings instance
 * \param len: length of blob to copy
 *
 * Creates or replaces a settings blob of the given name made up of the \p len
 * bytes of data from \p src.
 */
LWS_VISIBLE LWS_EXTERN int
lws_settings_plat_set(lws_settings_instance_t *si, const char *name,
		      const uint8_t *src, size_t len);

/**
 * lws_settings_plat_printf() - read a named blob from a settings instance
 *
 * \param si: the settings instance
 * \param name: the name of the setting blob in the instance
 * \param format: printf-style format string
 *
 * Creates or replaces a settings blob of the given name from the printf-style
 * format string and arguments provided.  There's no specific limit to the size,
 * the size is computed and then a temp heap buffer used.
 */
LWS_VISIBLE LWS_EXTERN int
lws_settings_plat_printf(lws_settings_instance_t *si, const char *name,
		         const char *format, ...) LWS_FORMAT(3);

#define lws_settings_ops_plat \
	.get		= lws_settings_plat_get, \
	.set		= lws_settings_plat_set,

LWS_VISIBLE LWS_EXTERN lws_settings_instance_t *
lws_settings_init(const lws_settings_ops_t *so, void *opaque_plat);

LWS_VISIBLE LWS_EXTERN void
lws_settings_deinit(lws_settings_instance_t **si);
