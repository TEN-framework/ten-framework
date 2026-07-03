/*
 * Private register map for ILI9341
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

#if !defined(__LWS_ILI9341_H__)
#define __LWS_ILI9341_H__

enum {

	ILI9341_NOP						= 0x00,
	ILI9341_SWRESET						= 0x01,
	ILI9341_RDDID						= 0x04,
	ILI9341_RDDST						= 0x09,

	ILI9341_SLPIN						= 0x10,
	ILI9341_SLPOUT						= 0x11,
	ILI9341_PTLON						= 0x12,
	ILI9341_NORON						= 0x13,

	ILI9341_RDMODE						= 0x0a,
	ILI9341_RDMADCTL					= 0x0b,
	ILI9341_RDPIXFMT					= 0x0c,
	ILI9341_RDIMGFMT					= 0x0d,
	ILI9341_RDSELFDIAG					= 0x0f,

	ILI9341_INVOFF						= 0x20,
	ILI9341_INVON						= 0x21,
	ILI9341_GAMMASET					= 0x26,
	ILI9341_DISPOFF						= 0x28,
	ILI9341_DISPON						= 0x29,
	ILI9341_CASET						= 0x2a,
	ILI9341_PASET						= 0x2b,
	ILI9341_RAMWR						= 0x2c,
	ILI9341_RAMRD						= 0x2e,

	ILI9341_PTLAR						= 0x30,
	ILI9341_VSCRDEF						= 0x33,
	ILI9341_MADCTL						= 0x36,
	ILI9341_VSCRSADD					= 0x37,
	ILI9341_PIXFMT						= 0x3a,

	ILI9341_FRMCTR1						= 0xb1,
	ILI9341_FRMCTR2						= 0xb2,
	ILI9341_FRMCTR3						= 0xb3,
	ILI9341_INVCTR						= 0xb4,
	ILI9341_DFUNCTR						= 0xb6,

	ILI9341_PWCTR1						= 0xc0,
	ILI9341_PWCTR2						= 0xc1,
	ILI9341_PWCTR3						= 0xc2,
	ILI9341_PWCTR4						= 0xc3,
	ILI9341_PWCTR5						= 0xc4,
	ILI9341_VMCTR1						= 0xc5,
	ILI9341_VMCTR2						= 0xc7,
	ILI9341_FACPUMPRAT					= 0xcb,
	ILI9341_FACPWCTRB					= 0xcf,

	ILI9341_RDID1						= 0xda,
	ILI9341_RDID2						= 0xdb,
	ILI9341_RDID3						= 0xdc,
	ILI9341_RDID4						= 0xdd,

	ILI9341_GMCTRP1						= 0xe0,
	ILI9341_GMCTRN1						= 0xe1,
	ILI9341_FACPWCTRA					= 0xe8,
	ILI9341_FACPWCTR1					= 0xea,
	ILI9341_FACDRTIMCTRA					= 0xed,

	ILI9341_FACSETGAMMACRV					= 0xf2,
	ILI9341_FACDRTIMCTR					= 0xf7,
};

#endif

