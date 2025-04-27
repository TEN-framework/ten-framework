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
 *
 * This is included from libwebsockets.h if LWS_PLAT_FREERTOS
 */

typedef int lws_sockfd_type;
typedef int lws_filefd_type;

#if defined(LWS_AMAZON_RTOS)
#include <FreeRTOS.h>
#include <event_groups.h>
#include <string.h>
#include "timers.h"
#include <lwip/sockets.h>

/*
 * Later lwip (at least 2.1.12) already defines these in its own headers
 * protected by the same test as used here... if POLLIN / POLLOUT already exist
 * then assume no need to declare those and struct pollfd.
 *
 * Older lwip needs these declarations done here.
 */

#if !defined(POLLIN) && !defined(POLLOUT)

struct pollfd {
	lws_sockfd_type fd; /**< fd related to */
	short events; /**< which POLL... events to respond to */
	short revents; /**< which POLL... events occurred */
};
#define POLLIN		0x0001
#define POLLPRI		0x0002
#define POLLOUT		0x0004
#define POLLERR		0x0008
#define POLLHUP		0x0010
#define POLLNVAL	0x0020

#endif

#else /* LWS_AMAZON_RTOS */
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
//#include "esp_event_loop.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "esp_spi_flash.h"
#include "freertos/timers.h"

#if defined(LWS_ESP_PLATFORM)
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#if defined(LWS_WITH_DRIVERS)
#include "libwebsockets/lws-gpio.h"
extern const lws_gpio_ops_t lws_gpio_plat;
#endif
#endif

#endif /* LWS_AMAZON_RTOS */

#if !defined(CONFIG_FREERTOS_HZ)
#define CONFIG_FREERTOS_HZ 100
#endif
