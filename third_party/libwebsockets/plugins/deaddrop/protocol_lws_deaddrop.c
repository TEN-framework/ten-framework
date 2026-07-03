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

#if !defined (LWS_PLUGIN_STATIC)
#if !defined(LWS_DLL)
#define LWS_DLL
#endif
#if !defined(LWS_INTERNAL)
#define LWS_INTERNAL
#endif
#include <libwebsockets.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#ifdef WIN32
#include <io.h>
#endif
#include <stdio.h>
#include <errno.h>

struct dir_entry {
	lws_list_ptr next; /* sorted by mtime */
	char user[32];
	unsigned long long size;
	time_t mtime;
};
/* filename follows */

#define lp_to_dir_entry(p, _n) lws_list_ptr_container(p, struct dir_entry, _n)

struct pss_deaddrop;

struct vhd_deaddrop {
	struct lws_context *context;
	struct lws_vhost *vh;
	const struct lws_protocols *protocol;

	struct pss_deaddrop *pss_head;

	const char *upload_dir;

	struct lwsac *lwsac_head;
	struct dir_entry *dire_head;
	int filelist_version;

	unsigned long long max_size;
};

struct pss_deaddrop {
	struct lws_spa *spa;
	struct vhd_deaddrop *vhd;
	struct lws *wsi;
	char result[LWS_PRE + LWS_RECOMMENDED_MIN_HEADER_SPACE];
	char filename[256];
	char user[32];
	unsigned long long file_length;
	lws_filefd_type fd;
	int response_code;

	struct pss_deaddrop *pss_list;

	struct lwsac *lwsac_head;
	struct dir_entry *dire;
	int filelist_version;

	uint8_t completed:1;
	uint8_t sent_headers:1;
	uint8_t sent_body:1;
	uint8_t first:1;
};

static const char * const param_names[] = {
	"text",
	"send",
	"file",
	"upload",
};

enum enum_param_names {
	EPN_TEXT,
	EPN_SEND,
	EPN_FILE,
	EPN_UPLOAD,
};

static int
de_mtime_sort(lws_list_ptr a, lws_list_ptr b)
{
	struct dir_entry *p1 = lp_to_dir_entry(a, next),
			 *p2 = lp_to_dir_entry(b, next);

	return (int)(p2->mtime - p1->mtime);
}

static void
start_sending_dir(struct pss_deaddrop *pss)
{
	if (pss->vhd->lwsac_head)
		lwsac_reference(pss->vhd->lwsac_head);
	pss->lwsac_head = pss->vhd->lwsac_head;
	pss->dire = pss->vhd->dire_head;
	pss->filelist_version = pss->vhd->filelist_version;
	pss->first = 1;
}

static int
scan_upload_dir(struct vhd_deaddrop *vhd)
{
	char filepath[256], subdir[3][128], *p;
	struct lwsac *lwsac_head = NULL;
	lws_list_ptr sorted_head = NULL;
	int i, sp = 0, found = 0;
	struct dir_entry *dire;
	struct dirent *de;
	size_t initial, m;
	struct stat s;
	DIR *dir[3];

	initial = strlen(vhd->upload_dir) + 1;
	lws_strncpy(subdir[sp], vhd->upload_dir, sizeof(subdir[sp]));
	dir[sp] = opendir(vhd->upload_dir);
	if (!dir[sp]) {
		lwsl_err("%s: Unable to walk upload dir '%s'\n", __func__,
			 vhd->upload_dir);
		return -1;
	}

	do {
		de = readdir(dir[sp]);
		if (!de) {
			closedir(dir[sp]);
#if !defined(__COVERITY__)
			if (!sp)
#endif
				break;
#if !defined(__COVERITY__)
			sp--;
			continue;
#endif
		}

		p = filepath;

		for (i = 0; i <= sp; i++)
			p += lws_snprintf(p, lws_ptr_diff_size_t((filepath + sizeof(filepath)), p),
					  "%s/", subdir[i]);

		lws_snprintf(p, lws_ptr_diff_size_t((filepath + sizeof(filepath)), p), "%s",
				  de->d_name);

		/* ignore temp files */
		if (de->d_name[strlen(de->d_name) - 1] == '~')
			continue;
#if defined(__COVERITY__)
		s.st_size = 0;
		s.st_mtime = 0;
#else
		/* coverity[toctou] */
		if (stat(filepath, &s))
			continue;

		if (S_ISDIR(s.st_mode)) {
			if (!strcmp(de->d_name, ".") ||
			    !strcmp(de->d_name, ".."))
				continue;
			sp++;
			if (sp == LWS_ARRAY_SIZE(dir)) {
				lwsl_err("%s: Skipping too-deep subdir %s\n",
					 __func__, filepath);
				sp--;
				continue;
			}
			lws_strncpy(subdir[sp], de->d_name, sizeof(subdir[sp]));
			dir[sp] = opendir(filepath);
			if (!dir[sp]) {
				lwsl_err("%s: Unable to open subdir '%s'\n",
					 __func__, filepath);
				goto bail;
			}
			continue;
		}
#endif

		m = strlen(filepath + initial) + 1;
		dire = lwsac_use(&lwsac_head, sizeof(*dire) + m, 0);
		if (!dire) {
			lwsac_free(&lwsac_head);

			goto bail;
		}

		dire->next = NULL;
		dire->size = (unsigned long long)s.st_size;
		dire->mtime = s.st_mtime;
		dire->user[0] = '\0';
#if !defined(__COVERITY__)
		if (sp)
			lws_strncpy(dire->user, subdir[1], sizeof(dire->user));
#endif

		found++;

		memcpy(&dire[1], filepath + initial, m);

		lws_list_ptr_insert(&sorted_head, &dire->next, de_mtime_sort);
	} while (1);

	/* the old lwsac continues to live while someone else is consuming it */
	if (vhd->lwsac_head)
		lwsac_detach(&vhd->lwsac_head);

	/* we replace it with the fresh one */
	vhd->lwsac_head = lwsac_head;
	if (sorted_head)
		vhd->dire_head = lp_to_dir_entry(sorted_head, next);
	else
		vhd->dire_head = NULL;

	vhd->filelist_version++;

	lwsl_info("%s: found %d\n", __func__, found);

	lws_start_foreach_llp(struct pss_deaddrop **, ppss, vhd->pss_head) {
		start_sending_dir(*ppss);
		lws_callback_on_writable((*ppss)->wsi);
	} lws_end_foreach_llp(ppss, pss_list);

	return 0;

bail:
	while (sp >= 0)
		closedir(dir[sp--]);

	return -1;
}

static int
file_upload_cb(void *data, const char *name, const char *filename,
	       char *buf, int _len, enum lws_spa_fileupload_states state)
{
	struct pss_deaddrop *pss = (struct pss_deaddrop *)data;
	char filename2[256];
	size_t len = (size_t)_len;
	int n;

	(void)n;

	switch (state) {
	case LWS_UFS_OPEN:
		lws_urldecode(filename2, filename, sizeof(filename2) - 1);
		lws_filename_purify_inplace(filename2);
		if (pss->user[0]) {
			lws_filename_purify_inplace(pss->user);
			lws_snprintf(pss->filename, sizeof(pss->filename),
				     "%s/%s", pss->vhd->upload_dir, pss->user);
			if (mkdir(pss->filename
#if !defined(WIN32)
				, 0700
#endif
				) < 0)
				lwsl_debug("%s: mkdir failed\n", __func__);
			lws_snprintf(pss->filename, sizeof(pss->filename),
				     "%s/%s/%s~", pss->vhd->upload_dir,
				     pss->user, filename2);
		} else
			lws_snprintf(pss->filename, sizeof(pss->filename),
				     "%s/%s~", pss->vhd->upload_dir, filename2);
		lwsl_notice("%s: filename '%s'\n", __func__, pss->filename);

		pss->fd = (lws_filefd_type)(long long)lws_open(pss->filename,
			      O_CREAT | O_TRUNC | O_RDWR, 0600);
		if (pss->fd == LWS_INVALID_FILE) {
			pss->response_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
			lwsl_err("%s: unable to open %s (errno %d)\n", __func__,
					pss->filename, errno);
			return -1;
		}
		break;

	case LWS_UFS_FINAL_CONTENT:
	case LWS_UFS_CONTENT:
		if (len) {
			pss->file_length += (unsigned int)len;

			/* if the file length is too big, drop it */
			if (pss->file_length > pss->vhd->max_size) {
				pss->response_code =
					HTTP_STATUS_REQ_ENTITY_TOO_LARGE;
				close((int)(lws_intptr_t)pss->fd);
				pss->fd = LWS_INVALID_FILE;
				unlink(pss->filename);

				return -1;
			}

			if (pss->fd != LWS_INVALID_FILE) {
				n = (int)write((int)(lws_intptr_t)pss->fd, buf, (unsigned int)len);
				lwsl_debug("%s: write %d says %d\n", __func__,
					   (int)len, n);
				lws_set_timeout(pss->wsi, PENDING_TIMEOUT_HTTP_CONTENT, 30);
			}
		}
		if (state == LWS_UFS_CONTENT)
			break;

		if (pss->fd != LWS_INVALID_FILE)
			close((int)(lws_intptr_t)pss->fd);

		/* the temp filename without the ~ */
		lws_strncpy(filename2, pss->filename, sizeof(filename2));
		filename2[strlen(filename2) - 1] = '\0';
		if (rename(pss->filename, filename2) < 0)
			lwsl_err("%s: unable to rename\n", __func__);

		pss->fd = LWS_INVALID_FILE;
		pss->response_code = HTTP_STATUS_OK;
		scan_upload_dir(pss->vhd);

		break;
	case LWS_UFS_CLOSE:
		break;
	}

	return 0;
}

/*
 * returns length in bytes
 */

static int
format_result(struct pss_deaddrop *pss)
{
	unsigned char *p, *start, *end;

	p = (unsigned char *)pss->result + LWS_PRE;
	start = p;
	end = p + sizeof(pss->result) - LWS_PRE - 1;

	p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p),
			"<!DOCTYPE html><html lang=\"en\"><head>"
			"<meta charset=utf-8 http-equiv=\"Content-Language\" "
			"content=\"en\"/>"
			"</head>");
	p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p), "</body></html>");

	return (int)lws_ptr_diff(p, start);
}

static int
callback_deaddrop(struct lws *wsi, enum lws_callback_reasons reason,
		  void *user, void *in, size_t len)
{
	struct vhd_deaddrop *vhd = (struct vhd_deaddrop *)
				lws_protocol_vh_priv_get(lws_get_vhost(wsi),
							 lws_get_protocol(wsi));
	struct pss_deaddrop *pss = (struct pss_deaddrop *)user;
	uint8_t buf[LWS_PRE + LWS_RECOMMENDED_MIN_HEADER_SPACE],
		*start = &buf[LWS_PRE], *p = start,
		*end = &buf[sizeof(buf) - 1];
	char fname[256], *wp;
	const char *cp;
	int n, m, was;

	switch (reason) {

	case LWS_CALLBACK_PROTOCOL_INIT: /* per vhost */
		lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
					    lws_get_protocol(wsi),
					    sizeof(struct vhd_deaddrop));

		vhd = (struct vhd_deaddrop *)
			lws_protocol_vh_priv_get(lws_get_vhost(wsi),
						 lws_get_protocol(wsi));
		if (!vhd)
			return 0;

		vhd->context = lws_get_context(wsi);
		vhd->vh = lws_get_vhost(wsi);
		vhd->protocol = lws_get_protocol(wsi);
		vhd->max_size = 20 * 1024 * 1024; /* default without pvo */

		if (!lws_pvo_get_str(in, "max-size", &cp))
			vhd->max_size = (unsigned long long)atoll(cp);
		if (lws_pvo_get_str(in, "upload-dir", &vhd->upload_dir)) {
			lwsl_warn("%s: requires 'upload-dir' pvo\n", __func__);
			return 0;
		}

		scan_upload_dir(vhd);

		lwsl_notice("  deaddrop: vh %s, upload dir %s, max size %llu\n",
			    lws_get_vhost_name(vhd->vh), vhd->upload_dir,
			    vhd->max_size);
		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		if (vhd)
			lwsac_free(&vhd->lwsac_head);
		break;

	/* WS-related */

	case LWS_CALLBACK_ESTABLISHED:
		pss->vhd = vhd;
		pss->wsi = wsi;
		/* add ourselves to the list of live pss held in the vhd */
		pss->pss_list = vhd->pss_head;
		vhd->pss_head = pss;

		m = lws_hdr_copy(wsi, pss->user, sizeof(pss->user),
				 WSI_TOKEN_HTTP_AUTHORIZATION);
		if (m > 0)
			lwsl_info("%s: basic auth user: %s\n",
				  __func__, pss->user);
		else
			pss->user[0] = '\0';

		start_sending_dir(pss);
		lws_callback_on_writable(wsi);
		return 0;

	case LWS_CALLBACK_CLOSED:
		if (pss->lwsac_head)
			lwsac_unreference(&pss->lwsac_head);
		/* remove our closing pss from the list of live pss */
		lws_start_foreach_llp(struct pss_deaddrop **,
				      ppss, vhd->pss_head) {
			if (*ppss == pss) {
				*ppss = pss->pss_list;
				break;
			}
		} lws_end_foreach_llp(ppss, pss_list);
		return 0;

	case LWS_CALLBACK_RECEIVE:
		/* we get this kind of thing {"del":"agreen/no-entry.svg"} */
		if (!pss || len < 10)
			break;

		if (strncmp((const char *)in, "{\"del\":\"", 8))
			break;

		cp = strchr((const char *)in, '/');
		if (cp) {
			n = (int)(((void *)cp - in)) - 8;

			if ((int)strlen(pss->user) != n ||
			    memcmp(pss->user, ((const char *)in) + 8, (unsigned int)n)) {
				lwsl_notice("%s: del: auth mismatch "
					    " '%s' '%s' (%d)\n",
					    __func__, pss->user,
					    ((const char *)in) + 8, n);
				break;
			}
		}

		lws_strncpy(fname, ((const char *)in) + 8, sizeof(fname));
		lws_filename_purify_inplace(fname);
		wp = strchr((const char *)fname, '\"');
		if (wp)
			*wp = '\0';

		lws_snprintf((char *)buf, sizeof(buf), "%s/%s", vhd->upload_dir,
			     fname);

		lwsl_notice("%s: del: path %s\n", __func__, (const char *)buf);

		if (unlink((const char *)buf) < 0)
			lwsl_err("%s: unlink %s failed\n", __func__,
					(const char *)buf);

		scan_upload_dir(vhd);
		break;

	case LWS_CALLBACK_SERVER_WRITEABLE:
		if (pss->lwsac_head && !pss->dire)
			return 0;

		was = 0;
		if (pss->first) {
			p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p),
					  "{\"max_size\":%llu, \"files\": [",
					  vhd->max_size);
			was = 1;
		}

		m = 5;
		while (m-- && pss->dire) {
			p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p),
					  "%c{\"name\":\"%s\", "
					  "\"size\":%llu,"
					  "\"mtime\":%llu,"
					  "\"yours\":%d}",
					  pss->first ? ' ' : ',',
					  (const char *)&pss->dire[1],
					  pss->dire->size,
					  (unsigned long long)pss->dire->mtime,
					  !strcmp(pss->user, pss->dire->user) &&
						  pss->user[0]);
			pss->first = 0;
			pss->dire = lp_to_dir_entry(pss->dire->next, next);
		}

		if (!pss->dire) {
			p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p),
					  "]}");
			if (pss->lwsac_head) {
				lwsac_unreference(&pss->lwsac_head);
				pss->lwsac_head = NULL;
			}
		}

		n = lws_write(wsi, start, lws_ptr_diff_size_t(p, start),
				(enum lws_write_protocol)lws_write_ws_flags(LWS_WRITE_TEXT, was,
						 !pss->dire));
		if (n < 0) {
			lwsl_notice("%s: ws write failed\n", __func__);
			return 1;
		}
		if (pss->dire) {
			lws_callback_on_writable(wsi);

			return 0;
		}

		/* ie, we finished */

		if (pss->filelist_version != pss->vhd->filelist_version) {
			lwsl_info("%s: restart send\n", __func__);
			/* what we just sent is already out of date */
			start_sending_dir(pss);
			lws_callback_on_writable(wsi);
		}

		return 0;

	/* POST-related */

	case LWS_CALLBACK_HTTP_BODY:

		/* create the POST argument parser if not already existing */
		if (!pss->spa) {
			pss->vhd = vhd;
			pss->wsi = wsi;
			pss->spa = lws_spa_create(wsi, param_names,
						  LWS_ARRAY_SIZE(param_names),
						  1024, file_upload_cb, pss);
			if (!pss->spa)
				return -1;

			pss->filename[0] = '\0';
			pss->file_length = 0;
			/* catchall */
			pss->response_code = HTTP_STATUS_SERVICE_UNAVAILABLE;

			m = lws_hdr_copy(wsi, pss->user, sizeof(pss->user),
					 WSI_TOKEN_HTTP_AUTHORIZATION);
			if (m > 0)
				lwsl_info("basic auth user: %s\n", pss->user);
			else
				pss->user[0] = '\0';
		}

		/* let it parse the POST data */
		if (lws_spa_process(pss->spa, in, (int)len)) {
			lwsl_notice("spa saw a problem\n");
			/* some problem happened */
			lws_spa_finalize(pss->spa);

			pss->completed = 1;
			lws_callback_on_writable(wsi);
		}
		break;

	case LWS_CALLBACK_HTTP_BODY_COMPLETION:
		/* call to inform no more payload data coming */
		lws_spa_finalize(pss->spa);

		pss->completed = 1;
		lws_callback_on_writable(wsi);
		break;

	case LWS_CALLBACK_HTTP_WRITEABLE:
		if (!pss->completed)
			break;

		p = (unsigned char *)pss->result + LWS_PRE;
		start = p;
		end = p + sizeof(pss->result) - LWS_PRE - 1;

		if (!pss->sent_headers) {
			n = format_result(pss);

			if (lws_add_http_header_status(wsi,
					(unsigned int)pss->response_code,
						       &p, end))
				goto bail;

			if (lws_add_http_header_by_token(wsi,
					WSI_TOKEN_HTTP_CONTENT_TYPE,
					(unsigned char *)"text/html", 9,
					&p, end))
				goto bail;
			if (lws_add_http_header_content_length(wsi, (lws_filepos_t)n, &p, end))
				goto bail;
			if (lws_finalize_http_header(wsi, &p, end))
				goto bail;

			/* first send the headers ... */
			n = lws_write(wsi, start, lws_ptr_diff_size_t(p, start),
				      LWS_WRITE_HTTP_HEADERS |
				      LWS_WRITE_H2_STREAM_END);
			if (n < 0)
				goto bail;

			pss->sent_headers = 1;
			lws_callback_on_writable(wsi);
			break;
		}

		if (!pss->sent_body) {
			n = format_result(pss);
			n = lws_write(wsi, (unsigned char *)start, (unsigned int)n,
				      LWS_WRITE_HTTP_FINAL);

			pss->sent_body = 1;
			if (n < 0) {
				lwsl_err("%s: writing body failed\n", __func__);
				return 1;
			}
			goto try_to_reuse;
		}
		break;

	case LWS_CALLBACK_HTTP_DROP_PROTOCOL:
		/* called when our wsi user_space is going to be destroyed */
		if (pss->spa) {
			lws_spa_destroy(pss->spa);
			pss->spa = NULL;
		}
		break;

	default:
		break;
	}

	return 0;

bail:

	return 1;

try_to_reuse:
	if (lws_http_transaction_completed(wsi))
		return -1;

	return 0;
}

#define LWS_PLUGIN_PROTOCOL_DEADDROP \
	{ \
		"lws-deaddrop", \
		callback_deaddrop, \
		sizeof(struct pss_deaddrop), \
		1024, \
		0, NULL, 0 \
	}

#if !defined (LWS_PLUGIN_STATIC)

LWS_VISIBLE const struct lws_protocols deaddrop_protocols[] = {
	LWS_PLUGIN_PROTOCOL_DEADDROP
};

LWS_VISIBLE const lws_plugin_protocol_t deaddrop = {
	.hdr = {
		"deaddrop",
		"lws_protocol_plugin",
		LWS_BUILD_HASH,
		LWS_PLUGIN_API_MAGIC
	},

	.protocols = deaddrop_protocols,
	.count_protocols = LWS_ARRAY_SIZE(deaddrop_protocols),
	.extensions = NULL,
	.count_extensions = 0,
};

#endif
