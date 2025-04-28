#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
/*
 * The bitmap mapping for the framebuffer is from top left to right, a strip of 8 vertical
 * bits from each byte, so there is a block of 128 x 8 px on a stride of 128 bytes.
 *
 * The 8 bits from the first byte in the fb are the leftmost vertical strip of 8, then the
 * next byte is the 8 pixels one to the right, until the 127th byte if the vertical strip
 * of 8 on the rhs.
 *
 * +----------------------------------+
 * |0                                 |
 * |1                                 |
 * |2                                 |
 * |3                                 |
 * |4                                 |
 * |5                                 |
 * |6                                 |
 * |7                                 |
 *
 * In this way the fb is more like (8 x vertical (128 x 8))
 *
 */


static const uint8_t scan[] = {

#include "pic.h"
};

/*
 * input byte 0 is like ABCDEFGH, one bit per horizontal pixel for one line
 * on an hstride of 16 bytes
 *
 * output byte 0 =  b0 = byte 0 b0, b1 = byte16 b0, b2 = byte24 b0 etc
 *
 * px(0,0) --> byte0 b0
 * px(0,1) --> byte0 b1
 */

int
main(void)
{
	const uint8_t *p = scan;
	uint8_t r[1024];
	int x, y, t = 0;

	memset(&r, 0, sizeof(r));

	while (t < 1024) {

		for (x = 0; x < 128; x++) {
			for (y = 0; y < 8; y++) {
				if (p[t + (16 * y) + (x / 8)] & (1 << (7 - (x & 7))))
					r[t + x] |= 1 << y;
			}
		}

		t += 128;
	}

	for (x = 0; x < 1024; x++) {
		printf("0x%02X, ", r[x]);
		if ((x & 0xf) == 0xf)
			printf("\n");
	}
}

