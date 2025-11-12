/*
 * lws abstract display implementation for ssd1306 on i2c
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
#include <drivers/devices/display/ssd1306.h>


static uint8_t ssd1306_128x64_init[] = {
	SSD1306_DISPLAYOFF,
	SSD1306_SETDISPLAYCLOCKDIV,		0xf0,
	SSD1306_SETMULTIPLEX,			64 - 1,
	SSD1306_SETDISPLAYOFFSET,		0,
	SSD1306_CHARGEPUMP,			0x14,
	SSD1306_MEMORYMODE,			0,
	SSD1306_SEGREMAP | (0 << 0),
	SSD1306_COMSCANDEC,
	SSD1306_SETCOMPINS,			(1 << 4) | 0x02,
	SSD1306_SETCONTRAST,			0, /* start at lowest */
	SSD1306_SETPRECHARGE,			(0xf << 4) | (1 << 0),
	SSD1306_SETVCOMDESELECT,		(4 << 4),
	SSD1306_DEACTIVATE_SCROLL,
	SSD1306_DISPLAYALLON_RESUME,
	SSD1306_NORMALDISPLAY,
	SSD1306_DISPLAYON
};

int
lws_display_ssd1306_i2c_init(const struct lws_display *disp)
{
	const lws_display_ssd1306_t *si = (const lws_display_ssd1306_t *)disp;

	si->i2c->init(si->i2c);

	if (si->gpio) {
		si->gpio->mode(si->reset_gpio, LWSGGPIO_FL_WRITE |
					       LWSGGPIO_FL_PULLUP);
		si->gpio->set(si->reset_gpio, 0);
		lws_msleep(1);
		si->gpio->set(si->reset_gpio, 1);
		lws_msleep(1);
	}

	if (lws_i2c_command_list(si->i2c, si->i2c7_address,
				 ssd1306_128x64_init,
				 LWS_ARRAY_SIZE(ssd1306_128x64_init))) {
		lwsl_err("%s: fail\n", __func__);
		return 1;
	}

	return 0;
}

int
lws_display_ssd1306_i2c_contrast(const struct lws_display *disp, uint8_t b)
{
	const lws_display_ssd1306_t *si = (const lws_display_ssd1306_t *)disp;
	uint8_t ba[2];

	ba[0] = SSD1306_SETCONTRAST;
	ba[1] = b;

	return lws_i2c_command_list(si->i2c, si->i2c7_address,
				    ba, LWS_ARRAY_SIZE(ba));
}

int
lws_display_ssd1306_i2c_blit(const struct lws_display *disp, const uint8_t *src,
			     lws_display_scalar x, lws_display_scalar y,
			     lws_display_scalar w, lws_display_scalar h)
{
	const lws_display_ssd1306_t *si = (const lws_display_ssd1306_t *)disp;
	uint8_t ba[6];
	int n, m;

	/*
	 * The display is arranged in 128x8 bands, with one byte containing
	 * the 8 vertical pixels of the band.
	 */

	if (h < 8)
		h = 8;

	ba[0] = SSD1306_COLUMNADDR;
	ba[1] = x;
	ba[2] = x + w - 1;
	ba[3] = SSD1306_PAGEADDR;
	ba[4] = y / 8;
	ba[5] = ba[4] + (h / 8) - 1;

	if (lws_i2c_command_list(si->i2c, si->i2c7_address,
				 ba, LWS_ARRAY_SIZE(ba))) {
		lwsl_err("%s: fail\n", __func__);
		return 1;
	}

        for (n = 0; n < (w * h) / 8;) {
                lws_bb_i2c_start(si->i2c);
                lws_bb_i2c_write(si->i2c, si->i2c7_address << 1);
                lws_bb_i2c_write(si->i2c, SSD1306_SETSTARTLINE | y);

                for (m = 0; m < w; m++)
                        lws_bb_i2c_write(si->i2c, src[n++]);

                lws_bb_i2c_stop(si->i2c);
                y += 8;
        }

	return 0;
}

int
lws_display_ssd1306_i2c_power(const struct lws_display *disp, int state)
{
	const lws_display_ssd1306_t *si = (const lws_display_ssd1306_t *)disp;

	if (!state)
		return lws_i2c_command(si->i2c, si->i2c7_address,
				       SSD1306_DISPLAYOFF | !!state);

	return lws_display_ssd1306_i2c_init(disp);
}
