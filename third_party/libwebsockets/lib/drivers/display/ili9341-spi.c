/*
 * lws abstract display implementation for ili9341 on spi
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
#include <drivers/devices/display/ili9341.h>


static uint8_t ili9341_320x240_init[] = {
	/*
	 * This provides 70Hz 320x240 at RGB565, we assume im[3:0] is 1110
	 * which is 4-bit SPI
	 */

	 3, ILI9341_FACPWCTRB,	    0x00, 0x83, 0x30,
	 4, ILI9341_FACDRTIMCTRA,   0x64, 0x03, 0x12, 0x81,
	 3, ILI9341_FACPWCTRA,	    0x85, 0x01, 0x79,
	 5, ILI9341_FACPUMPRAT,	    0x39, 0x2c, 0x00, 0x34, 0x02,
	 1, ILI9341_FACDRTIMCTR,    0x20,
	 2, ILI9341_FACPWCTR1,	    0x00, 0x00,

	 1, ILI9341_PWCTR1,	    0x26,
	 1, ILI9341_PWCTR2,	    0x11,
	 2, ILI9341_VMCTR1,	    0x35, 0x3e,
	 1, ILI9341_VMCTR2,	    0xbe,
	 1, ILI9341_MADCTL,	    0x28,
	 1, ILI9341_VSCRSADD,	    0x00,
	 1, ILI9341_PIXFMT,	    0x55,
	 2, ILI9341_FRMCTR1,	    0x00, 0x1b,
	 1, ILI9341_FACSETGAMMACRV, 0x00,
	 1, ILI9341_GAMMASET,	    0x01,
	15, ILI9341_GMCTRP1,	    0x0f, 0x31, 0x2b, 0x0c, 0x0e, 0x08, 0x4e,
				    0xf1, 0x37, 0x07, 0x10, 0x03, 0x0e, 0x09,
				    0x00,
	15, ILI9341_GMCTRN1,	    0x00, 0x0e, 0x14, 0x03, 0x11, 0x07, 0x31,
				    0xc1, 0x48, 0x08, 0x0f, 0x0c, 0x31, 0x36,
				    0x0f,
	4, ILI9341_DFUNCTR,	    0x0a, 0x82, 0x27, 0x00,
};

int
lws_display_ili9341_spi_init(const struct lws_display *disp)
{
	const lws_display_ili9341_t *ili = (const lws_display_ili9341_t *)disp;
	lws_spi_desc_t desc;
	size_t pos = 0;
	uint8_t u[8];

	lwsl_user("%s\n", __func__);

	/* hardware nRESET */

	if (ili->gpio) {
		ili->gpio->mode(ili->reset_gpio, LWSGGPIO_FL_WRITE |
					         LWSGGPIO_FL_PULLUP);
		ili->gpio->set(ili->reset_gpio, 0);

		lws_msleep(1);
		ili->gpio->set(ili->reset_gpio, 1);
		lws_msleep(1);
	}

	/*
	 * We cut the init table up into transactions... atm we just go with
	 * the fact that bb spi is synchronous, using async / dma we can't use
	 * a single desc on the stack like this
	 */

	memset(&desc, 0, sizeof(desc));
	desc.count_cmd = 1;

	while (pos < LWS_ARRAY_SIZE(ili9341_320x240_init)) {
		desc.count_write = ili9341_320x240_init[pos++];
		desc.src = &ili9341_320x240_init[pos++];
		desc.data = &ili9341_320x240_init[pos];
		pos += desc.count_write;

		ili->spi->queue(ili->spi, &desc);
	}

	u[0] = ILI9341_SLPOUT;
	desc.src = &u[0];
	desc.count_write = 0;
	ili->spi->queue(ili->spi, &desc);

	lws_msleep(5);

	u[0] = ILI9341_DISPON;
	ili->spi->queue(ili->spi, &desc);

	return 0;
}

/* backlight handled by PWM */

int
lws_display_ili9341_spi_brightness(const struct lws_display *disp, uint8_t b)
{
	return 0;
}

int
lws_display_ili9341_spi_blit(const struct lws_display *disp, const uint8_t *src,
			     lws_display_scalar x, lws_display_scalar y,
			     lws_display_scalar w, lws_display_scalar h)
{
	const lws_display_ili9341_t *ili = (const lws_display_ili9341_t *)disp;
	lws_spi_desc_t desc;
	uint8_t u[5];

	memset(&desc, 0, sizeof(desc));
	desc.count_cmd = 1;
	desc.src = &u[0];
	desc.count_write = 0;

	/*
	 * Blit a line at a time
	 */

	while (h--) {

		u[0] = ILI9341_CASET;
		desc.data = &u[1];
		u[1] = x;
		u[2] = x;
		u[3] = w >> 8;
		u[4] = w & 0xff;
		desc.count_write = 4;
		ili->spi->queue(ili->spi, &desc);

		u[0] = ILI9341_PASET;
		u[1] = y >> 8;
		u[2] = y & 0xff;
		u[3] = (y + 1) >> 8;
		u[4] = (y + 1) & 0xff;
		desc.count_write = 4;
		ili->spi->queue(ili->spi, &desc);

		u[0] = ILI9341_RAMWR;
		desc.data = src;
		desc.count_write = w * 2;
		ili->spi->queue(ili->spi, &desc);
		src += w * 2;
		y++;
	}

	return 0;
}

int
lws_display_ili9341_spi_power(const struct lws_display *disp, int state)
{

	const lws_display_ili9341_t *ili = (const lws_display_ili9341_t *)disp;
	lws_spi_desc_t desc;
	uint8_t u[1];

	memset(&desc, 0, sizeof(desc));
	desc.count_cmd = 1;
	desc.data = desc.src = &u[0];
	u[0] = state ? ILI9341_SLPOUT : ILI9341_SLPIN;
	ili->spi->queue(ili->spi, &desc);

	/* we're not going to do anything useful for 5ms after this */

	return 0;
}
