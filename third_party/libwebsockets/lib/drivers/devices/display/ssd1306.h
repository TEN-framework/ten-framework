/*
 * Private register map for SSD1306
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
 *
 */

#if !defined(__LWS_SSD1306_H__)
#define __LWS_SSD1306_H__

enum {
	SSD1306_SETLOWCOLUMN		= 0x00,
	SSD1306_SETHIGHCOLUMN		= 0x10,

	SSD1306_MEMORYMODE		= 0x20,
	SSD1306_COLUMNADDR		= 0x21,
	SSD1306_PAGEADDR		= 0x22,
	SSD1306_DEACTIVATE_SCROLL	= 0x2e,

	SSD1306_SETSTARTLINE		= 0x40,

	SSD1306_SETCONTRAST		= 0x81,
	SSD1306_CHARGEPUMP		= 0x8d,

	SSD1306_SEGREMAP		= 0xa0,
	SSD1306_SETSEGMENTREMAP		= 0xa1,
	SSD1306_DISPLAYALLON_RESUME	= 0xa4,
	SSD1306_DISPLAYALLON		= 0xa5,
	SSD1306_NORMALDISPLAY		= 0xa6,
	SSD1306_INVERTDISPLAY		= 0xa7,
	SSD1306_SETMULTIPLEX		= 0xa8,
	SSD1306_DISPLAYOFF	 	= 0xae,
	SSD1306_DISPLAYON		= 0xaf,

	SSD1306_COMSCANINC		= 0xc0,
	SSD1306_COMSCANDEC		= 0xc8,

	SSD1306_SETDISPLAYOFFSET	= 0xd3,
	SSD1306_SETDISPLAYCLOCKDIV	= 0xd5,
	SSD1306_SETPRECHARGE		= 0xd9,
	SSD1306_SETCOMPINS		= 0xda,
	SSD1306_SETVCOMDESELECT		= 0xdb,

	SSD1306_NOP			= 0xe3,
};

#endif

