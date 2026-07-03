/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2020 Andy Green <andy@warmcat.com>
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

#if !defined(__LWS_PRIVATE_LIB_MISC_LWSAC_H__)
#define __LWS_PRIVATE_LIB_MISC_LWSAC_H__

#if !defined(LWS_PLAT_OPTEE)
#include <sys/stat.h>
#endif

/* under page size of 4096 to allow overhead */
#define LWSAC_CHUNK_SIZE 4000

/*
 * the chunk list members all point back to the head themselves so the list
 * can be detached from the formal head and free itself when its reference
 * count reaches zero.
 */

/*
 * One of these per chunk
 */

struct lwsac {
	struct lwsac *next;
	struct lwsac *head; /* pointer back to the first chunk */
	size_t alloc_size; /* alloc size of the whole chunk */
	size_t ofs; /* next writeable position inside chunk */
};

/*
 * One of these per lwsac, at start of first chunk
 */

struct lwsac_head {
	struct lwsac *curr;
	size_t total_alloc_size;
	int refcount;
	int total_blocks;
	char detached; /* if our refcount gets to zero, free the chunk list */
};

#if !defined(LWS_PLAT_OPTEE)
struct cached_file_info {
	struct stat s;
	time_t last_confirm;
};
#endif
#endif
