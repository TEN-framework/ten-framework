/*
 * Copyright (C) 2017 National Institute of Advanced Industrial Science
 *                    and Technology (AIST)
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of AIST nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <libwebsockets.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "romfs.h"
#if defined(LWS_WITH_ESP32)
#include "esp_spi_flash.h"
#endif

#define RFS_STRING_MAX 96

static u32_be_t cache[(RFS_STRING_MAX + 32) / 4];
static romfs_inode_t ci = (romfs_inode_t)cache;
static romfs_t cr = (romfs_t)cache;

static void
set_cache(romfs_inode_t inode, size_t len)
{
#if defined(LWS_WITH_ESP32)
	spi_flash_read((uint32_t)inode, cache, len);
#endif
}

static uint32_t
untohl(const u32_be_t be)
{
	return ((be >> 24) & 0xff) |
	       ((be >> 16) & 0xff) << 8 |
	       ((be >> 8) & 0xff) << 16 |
	       (be & 0xff) << 24;
}
static romfs_inode_t
romfs_lookup(romfs_t romfs, romfs_inode_t start, const char *path);

static int
plus_padding(const uint8_t *s)
{
	int n;
       
	set_cache((romfs_inode_t)s, RFS_STRING_MAX);
	n = strlen((const char *)cache);

	if (!(n & 15))
		n += 0x10;

	return (n + 15) & ~15;
}

static romfs_inode_t
skip_and_pad(romfs_inode_t ri)
{
	const uint8_t *p = ((const uint8_t *)ri) + sizeof(*ri);

	return (romfs_inode_t)(p + plus_padding(p));
}

size_t
romfs_mount_check(romfs_t romfs)
{
	set_cache((romfs_inode_t)romfs, sizeof(*romfs));

	if (cr->magic1 != 0x6d6f722d ||
	    cr->magic2 != 0x2d736631)
		return 0;

	return untohl(cr->size);
}

static romfs_inode_t
romfs_symlink(romfs_t romfs, romfs_inode_t level, romfs_inode_t i)
{
	const char *p = (const char *)skip_and_pad(i);

	if (*p == '/') {
		level = skip_and_pad((romfs_inode_t)romfs);
		p++;
	}

	return romfs_lookup(romfs, level, p);
}

static romfs_inode_t
dir_link(romfs_t romfs, romfs_inode_t i)
{
	set_cache(i, sizeof(*i));
	return (romfs_inode_t)((const uint8_t *)romfs +
						untohl(ci->dir_start));
}

static romfs_inode_t
romfs_lookup(romfs_t romfs, romfs_inode_t start, const char *path)
{
	romfs_inode_t level, i = start, i_in;
	const char *p, *cp;
	uint32_t next_be;

	if (start == (romfs_inode_t)romfs)
		i = skip_and_pad((romfs_inode_t)romfs);
	level = i;
	while (i != (romfs_inode_t)romfs) {
		const char *n = ((const char *)i) + sizeof(*i);

		p = path;
		i_in = i;

		set_cache(i, sizeof(*i));
		next_be = ci->next;

		cp = (const char *)cache;
		set_cache((romfs_inode_t)n, RFS_STRING_MAX);

		while (*p && *p != '/' && *cp && *p == *cp &&
		       (p - path) < RFS_STRING_MAX) {
			p++;
			n++;
			cp++;
		}

		while (*p == '/' && p[1] == '/')
			p++;

		if (!*cp && (!*p || *p == '/') &&
		    (untohl(next_be) & 7) == RFST_HARDLINK) {
			set_cache(i, sizeof(*i));
			return (romfs_inode_t)
			       ((const uint8_t *)romfs +
			        (untohl(ci->dir_start) & ~15));
		}

		if (!*p && !*cp) {
			set_cache(i, sizeof(*i));
			if ((untohl(ci->next) & 7) == RFST_SYMLINK) {
				i = romfs_symlink(romfs, level, i);
				continue;
			}
			return i;
		}

		if (!*p && *cp == '/')
			return NULL;

		while (*p == '/' && p[1] == '/')
			p++;

		if (*p == '/' && !*cp) {
			set_cache(i, sizeof(*i));
			switch (untohl(ci->next) & 7) {
			case RFST_SYMLINK:
				i = romfs_symlink(romfs, level, i);
				if (!i)
					return NULL;
				i = dir_link(romfs, i);
				while (*path != '/' && *path)
					path++;
				if (!*path)
					return NULL;
				path++;
				continue;
			case RFST_DIR:
				path = p + 1;
				i = dir_link(romfs, i);
				break;
			default:
				path = p + 1;
				i = skip_and_pad(i);
				break;
			}
			level = i;
			continue;
		}

		set_cache(i, sizeof(*i));
		if (!(untohl(ci->next) & ~15))
			return NULL;

		i = (romfs_inode_t)((const uint8_t *)romfs +
				    (untohl(ci->next) & ~15));
		if (i == i_in)
			return NULL;
	}

	return NULL;
}

const void *
romfs_get_info(romfs_t romfs, const char *path, size_t *len, size_t *csum)
{
	romfs_inode_t i;
       
	if (*path == '/')
		path++;

	i = romfs_lookup(romfs, (romfs_inode_t)romfs, path);

	if (!i)
		return NULL;

	set_cache(i, sizeof(*i));
	*len = untohl(ci->size);
	if (csum)
		*csum = untohl(ci->checksum);

	return (void *)skip_and_pad(i);
}
