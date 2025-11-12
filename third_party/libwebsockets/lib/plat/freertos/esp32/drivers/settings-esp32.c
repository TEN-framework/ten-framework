/*
 * esp32 / esp-idf NV settings shim
 *
 * Copyright (C) 2019 - 2020 Andy Green <andy@warmcat.com>
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

#include <private-lib-core.h>

#include <nvs_flash.h>

int
lws_settings_plat_get(lws_settings_instance_t *si, const char *name,
		      uint8_t *dest, size_t *max_actual)
{
	int n;

	n = nvs_flash_init_partition((const char *)si->opaque_plat);

	lwsl_notice("%s: init partition %d\n", __func__, n);
	if (n == ESP_ERR_NOT_FOUND)
		return 1;

	if (nvs_open_from_partition((const char *)si->opaque_plat,
				    "_lws_settings", NVS_READONLY,
				    (nvs_handle_t *)&si->handle_plat))
		return 1;

	n = nvs_get_blob((nvs_handle_t)si->handle_plat,
			 name, dest, max_actual);

	nvs_close((nvs_handle_t)si->handle_plat);

	return !!n;
}

int
lws_settings_plat_set(lws_settings_instance_t *si, const char *name,
		      const uint8_t *src, size_t len)
{
	int n = nvs_flash_init_partition((const char *)si->opaque_plat);

	lwsl_notice("%s: init partition %d\n", __func__, n);
	if (n == ESP_ERR_NOT_FOUND)
		return 1;

	if (nvs_open_from_partition((const char *)si->opaque_plat,
				    "_lws_settings", NVS_READWRITE,
				    (nvs_handle_t *)&si->handle_plat))
		return 1;

	n = nvs_set_blob((nvs_handle_t)si->handle_plat, name, src, len);

	nvs_commit((nvs_handle_t)si->handle_plat);
	nvs_close((nvs_handle_t)si->handle_plat);

	return 0;
}
