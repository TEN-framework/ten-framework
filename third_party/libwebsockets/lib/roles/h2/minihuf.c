/*
 * minilex.c
 *
 * High efficiency lexical state parser
 *
 * Copyright (C)2011-2014 Andy Green <andy@warmcat.com>
 *
 * Licensed under MIT
 *
 * Usage: gcc minihuf.c -o minihuf && ./minihuf > huftable.h
 *
 * Run it twice to test parsing on the generated table on stderr
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LWS_ARRAY_SIZE(n) (sizeof(n) / sizeof(n[0]))

struct huf {
	unsigned int code;
	unsigned char len;
};

static struct huf huf_literal[] = {
	/* 0x00 */ { 0x1ff8, 13 },
	/* 0x01 */ { 0x7fffd8, 23 },
	/* 0x02 */ { 0xfffffe2, 28 },
	/* 0x03 */ { 0xfffffe3, 28 },
	/* 0x04 */ { 0xfffffe4, 28 },
	/* 0x05 */ { 0xfffffe5, 28 },
	/* 0x06 */ { 0xfffffe6, 28 },
	/* 0x07 */ { 0xfffffe7, 28 },
	/* 0x08 */ { 0xfffffe8, 28 },
	/* 0x09 */ { 0xffffea, 24 },
	/* 0x0a */ { 0x3ffffffc, 30 },
	/* 0x0b */ { 0xfffffe9, 28 },

	/* 0x0c */ { 0xfffffea, 28 },
	/* 0x0d */ { 0x3ffffffd, 30 },
	/* 0x0e */ { 0xfffffeb, 28 },
	/* 0x0f */ { 0xfffffec, 28 },
	/* 0x10 */ { 0xfffffed, 28 },
	/* 0x11 */ { 0xfffffee, 28 },
	/* 0x12 */ { 0xfffffef, 28 },
	/* 0x13 */ { 0xffffff0, 28 },
	/* 0x14 */ { 0xffffff1, 28 },
	/* 0x15 */ { 0xffffff2, 28 },
	/* 0x16 */ { 0x3ffffffe, 30 },
	/* 0x17 */ { 0xffffff3, 28 },
	/* 0x18 */ { 0xffffff4, 28 },
	/* 0x19 */ { 0xffffff5, 28 },
	/* 0x1a */ { 0xffffff6, 28 },
	/* 0x1b */ { 0xffffff7, 28 },
	/* 0x1c */ { 0xffffff8, 28 },
	/* 0x1d */ { 0xffffff9, 28 },
	/* 0x1e */ { 0xffffffa, 28 },
	/* 0x1f */ { 0xffffffb, 28 },
	/* 0x20 */ { 0x14, 6 },
	/* 0x21 */ { 0x3f8, 10 },
	/* 0x22 */ { 0x3f9, 10 },
	/* 0x23 */ { 0xffa, 12 },
	/* 0x24 */ { 0x1ff9, 13 },
	/* 0x25 */ { 0x15, 6 },
	/* 0x26 */ { 0xf8, 8 },
	/* 0x27 */ { 0x7fa, 11 },
	/* 0x28 */ { 0x3fa, 10 },
	/* 0x29 */ { 0x3fb, 10 },
	/* 0x2a */ { 0xf9, 8 },
	/* 0x2b */ { 0x7fb, 11 },
	/* 0x2c */ { 0xfa, 8 },
	/* 0x2d */ { 0x16, 6 },
	/* 0x2e */ { 0x17, 6 },
	/* 0x2f */ { 0x18, 6 },
	/* 0x30 */ { 0x0, 5 },
	/* 0x31 */ { 0x1, 5 },
	/* 0x32 */ { 0x2, 5 },
	/* 0x33 */ { 0x19, 6 },
	/* 0x34 */ { 0x1a, 6 },
	/* 0x35 */ { 0x1b, 6 },
	/* 0x36 */ { 0x1c, 6 },
	/* 0x37 */ { 0x1d, 6 },
	/* 0x38 */ { 0x1e, 6 },
	/* 0x39 */ { 0x1f, 6 },
	/* 0x3a */ { 0x5c, 7 },
	/* 0x3b */ { 0xfb, 8 },

	/* 0x3c */ { 0x7ffc, 15 },
	/* 0x3d */ { 0x20, 6 },
	/* 0x3e */ { 0xffb, 12 },
	/* 0x3f */ { 0x3fc, 10 },
	/* 0x40 */ { 0x1ffa, 13 },
	/* 0x41 */ { 0x21, 6 },
	/* 0x42 */ { 0x5d, 7 },
	/* 0x43 */ { 0x5e, 7 },
	/* 0x44 */ { 0x5f, 7 },
	/* 0x45 */ { 0x60, 7 },
	/* 0x46 */ { 0x61, 7 },
	/* 0x47 */ { 0x62, 7 },
	/* 0x48 */ { 0x63, 7 },
	/* 0x49 */ { 0x64, 7 },
	/* 0x4a */ { 0x65, 7 },
	/* 0x4b */ { 0x66, 7 },
	/* 0x4c */ { 0x67, 7 },
	/* 0x4d */ { 0x68, 7 },
	/* 0x4e */ { 0x69, 7 },
	/* 0x4f */ { 0x6a, 7 },
	/* 0x50 */ { 0x6b, 7 },
	/* 0x51 */ { 0x6c, 7 },
	/* 0x52 */ { 0x6d, 7 },
	/* 0x53 */ { 0x6e, 7 },
	/* 0x54 */ { 0x6f, 7 },
	/* 0x55 */ { 0x70, 7 },
	/* 0x56 */ { 0x71, 7 },
	/* 0x57 */ { 0x72, 7 },
	/* 0x58 */ { 0xfc, 8 },
	/* 0x59 */ { 0x73, 7 },
	/* 0x5a */ { 0xfd, 8 },
	/* 0x5b */ { 0x1ffb, 13 },
	/* 0x5c */ { 0x7fff0, 19 },
	/* 0x5d */ { 0x1ffc, 13 },
	/* 0x5e */ { 0x3ffc, 14 },
	/* 0x5f */ { 0x22, 6 },
	/* 0x60 */ { 0x7ffd, 15 },
	/* 0x61 */ { 0x3, 5 },
	/* 0x62 */ { 0x23, 6 },
	/* 0x63 */ { 0x4, 5 },
	/* 0x64 */ { 0x24, 6 },
	/* 0x65 */ { 0x5, 5 },
	/* 0x66 */ { 0x25, 6 },
	/* 0x67 */ { 0x26, 6 },
	/* 0x68 */ { 0x27, 6 },
	/* 0x69 */ { 0x6, 5 },
	/* 0x6a */ { 0x74, 7 },
	/* 0x6b */ { 0x75, 7 },


	/* 0x6c */ { 0x28, 6 },
	/* 0x6d */ { 0x29, 6 },
	/* 0x6e */ { 0x2a, 6 },
	/* 0x6f */ { 0x7, 5 },
	/* 0x70 */ { 0x2b, 6 },
	/* 0x71 */ { 0x76, 7 },
	/* 0x72 */ { 0x2c, 6 },
	/* 0x73 */ { 0x8, 5 },
	/* 0x74 */ { 0x9, 5 },
	/* 0x75 */ { 0x2d, 6 },
	/* 0x76 */ { 0x77, 7 },
	/* 0x77 */ { 0x78, 7 },
	/* 0x78 */ { 0x79, 7 },
	/* 0x79 */ { 0x7a, 7 },
	/* 0x7a */ { 0x7b, 7 },
	/* 0x7b */ { 0x7ffe, 15 },
	/* 0x7c */ { 0x7fc, 11 },
	/* 0x7d */ { 0x3ffd, 14 },
	/* 0x7e */ { 0x1ffd, 13 },
	/* 0x7f */ { 0xffffffc, 28 },
	/* 0x80 */ { 0xfffe6, 20 },
	/* 0x81 */ { 0x3fffd2, 22 },
	/* 0x82 */ { 0xfffe7, 20 },
	/* 0x83 */ { 0xfffe8, 20 },
	/* 0x84 */ { 0x3fffd3, 22 },
	/* 0x85 */ { 0x3fffd4, 22 },
	/* 0x86 */ { 0x3fffd5, 22 },
	/* 0x87 */ { 0x7fffd9, 23 },
	/* 0x88 */ { 0x3fffd6, 22 },
	/* 0x89 */ { 0x7fffda, 23 },
	/* 0x8a */ { 0x7fffdb, 23 },
	/* 0x8b */ { 0x7fffdc, 23 },
	/* 0x8c */ { 0x7fffdd, 23 },
	/* 0x8d */ { 0x7fffde, 23 },
	/* 0x8e */ { 0xffffeb, 24 },
	/* 0x8f */ { 0x7fffdf, 23 },
	/* 0x90 */ { 0xffffec, 24 },
	/* 0x91 */ { 0xffffed, 24 },
	/* 0x92 */ { 0x3fffd7, 22 },
	/* 0x93 */ { 0x7fffe0, 23 },
	/* 0x94 */ { 0xffffee, 24 },
	/* 0x95 */ { 0x7fffe1, 23 },
	/* 0x96 */ { 0x7fffe2, 23 },
	/* 0x97 */ { 0x7fffe3, 23 },
	/* 0x98 */ { 0x7fffe4, 23 },
	/* 0x99 */ { 0x1fffdc, 21 },
	/* 0x9a */ { 0x3fffd8, 22 },
	/* 0x9b */ { 0x7fffe5, 23 },

	/* 0x9c */ { 0x3fffd9, 22 },
	/* 0x9d */ { 0x7fffe6, 23 },
	/* 0x9e */ { 0x7fffe7, 23 },
	/* 0x9f */ { 0xffffef, 24 },
	/* 0xa0 */ { 0x3fffda, 22 },
	/* 0xa1 */ { 0x1fffdd, 21 },
	/* 0xa2 */ { 0xfffe9, 20 },
	/* 0xa3 */ { 0x3fffdb, 22 },
	/* 0xa4 */ { 0x3fffdc, 22 },
	/* 0xa5 */ { 0x7fffe8, 23 },
	/* 0xa6 */ { 0x7fffe9, 23 },
	/* 0xa7 */ { 0x1fffde, 21 },
	/* 0xa8 */ { 0x7fffea, 23 },
	/* 0xa9 */ { 0x3fffdd, 22 },
	/* 0xaa */ { 0x3fffde, 22 },
	/* 0xab */ { 0xfffff0, 24 },
	/* 0xac */ { 0x1fffdf, 21 },
	/* 0xad */ { 0x3fffdf, 22 },
	/* 0xae */ { 0x7fffeb, 23 },
	/* 0xaf */ { 0x7fffec, 23 },
	/* 0xb0 */ { 0x1fffe0, 21 },
	/* 0xb1 */ { 0x1fffe1, 21 },
	/* 0xb2 */ { 0x3fffe0, 22 },
	/* 0xb3 */ { 0x1fffe2, 21 },
	/* 0xb4 */ { 0x7fffed, 23 },
	/* 0xb5 */ { 0x3fffe1, 22 },
	/* 0xb6 */ { 0x7fffee, 23 },
	/* 0xb7 */ { 0x7fffef, 23 },
	/* 0xb8 */ { 0xfffea, 20 },
	/* 0xb9 */ { 0x3fffe2, 22 },
	/* 0xba */ { 0x3fffe3, 22 },
	/* 0xbb */ { 0x3fffe4, 22 },
	/* 0xbc */ { 0x7ffff0, 23 },
	/* 0xbd */ { 0x3fffe5, 22 },
	/* 0xbe */ { 0x3fffe6, 22 },
	/* 0xbf */ { 0x7ffff1, 23 },
	/* 0xc0 */ { 0x3ffffe0, 26 },
	/* 0xc1 */ { 0x3ffffe1, 26 },
	/* 0xc2 */ { 0xfffeb, 20 },
	/* 0xc3 */ { 0x7fff1, 19 },
	/* 0xc4 */ { 0x3fffe7, 22 },
	/* 0xc5 */ { 0x7ffff2, 23 },
	/* 0xc6 */ { 0x3fffe8, 22 },
	/* 0xc7 */ { 0x1ffffec, 25 },
	/* 0xc8 */ { 0x3ffffe2, 26 },
	/* 0xc9 */ { 0x3ffffe3, 26 },
	/* 0xca */ { 0x3ffffe4, 26 },
	/* 0xcb */ { 0x7ffffde, 27 },

	/* 0xcc */ { 0x7ffffdf, 27 },
	/* 0xcd */ { 0x3ffffe5, 26 },
	/* 0xce */ { 0xfffff1, 24 },
	/* 0xcf */ { 0x1ffffed, 25 },
	/* 0xd0 */ { 0x7fff2, 19 },
	/* 0xd1 */ { 0x1fffe3, 21 },
	/* 0xd2 */ { 0x3ffffe6, 26 },
	/* 0xd3 */ { 0x7ffffe0, 27 },
	/* 0xd4 */ { 0x7ffffe1, 27 },
	/* 0xd5 */ { 0x3ffffe7, 26 },
	/* 0xd6 */ { 0x7ffffe2, 27 },
	/* 0xd7 */ { 0xfffff2, 24 },
	/* 0xd8 */ { 0x1fffe4, 21 },
	/* 0xd9 */ { 0x1fffe5, 21 },
	/* 0xda */ { 0x3ffffe8, 26 },
	/* 0xdb */ { 0x3ffffe9, 26 },
	/* 0xdc */ { 0xffffffd, 28 },
	/* 0xdd */ { 0x7ffffe3, 27 },
	/* 0xde */ { 0x7ffffe4, 27 },
	/* 0xdf */ { 0x7ffffe5, 27 },
	/* 0xe0 */ { 0xfffec, 20 },
	/* 0xe1 */ { 0xfffff3, 24 },
	/* 0xe2 */ { 0xfffed, 20 },
	/* 0xe3 */ { 0x1fffe6, 21 },
	/* 0xe4 */ { 0x3fffe9, 22 },
	/* 0xe5 */ { 0x1fffe7, 21 },
	/* 0xe6 */ { 0x1fffe8, 21 },
	/* 0xe7 */ { 0x7ffff3, 23 },
	/* 0xe8 */ { 0x3fffea, 22 },
	/* 0xe9 */ { 0x3fffeb, 22 },
	/* 0xea */ { 0x1ffffee, 25 },
	/* 0xeb */ { 0x1ffffef, 25 },
	/* 0xec */ { 0xfffff4, 24 },
	/* 0xed */ { 0xfffff5, 24 },
	/* 0xee */ { 0x3ffffea, 26 },
	/* 0xef */ { 0x7ffff4, 23 },
	/* 0xf0 */ { 0x3ffffeb, 26 },
	/* 0xf1 */ { 0x7ffffe6, 27 },
	/* 0xf2 */ { 0x3ffffec, 26 },
	/* 0xf3 */ { 0x3ffffed, 26 },
	/* 0xf4 */ { 0x7ffffe7, 27 },
	/* 0xf5 */ { 0x7ffffe8, 27 },
	/* 0xf6 */ { 0x7ffffe9, 27 },
	/* 0xf7 */ { 0x7ffffea, 27 },
	/* 0xf8 */ { 0x7ffffeb, 27 },
	/* 0xf9 */ { 0xffffffe, 28 },
	/* 0xfa */ { 0x7ffffec, 27 },
	/* 0xfb */ { 0x7ffffed, 27 },

	/* 0xfc */ { 0x7ffffee, 27 },
	/* 0xfd */ { 0x7ffffef, 27 },
	/* 0xfe */ { 0x7fffff0, 27 },
	/* 0xff */ { 0x3ffffee, 26 },
	/* 0x100 */ { 0x3fffffff, 30 },
};

int code_bit(int idx, int bit)
{
	if (bit < huf_literal[idx].len)
		return !!(huf_literal[idx].code & (1 << (huf_literal[idx].len - 1 - bit)));

	return -1;
}

#include "huftable.h"

#define PARALLEL 2

struct state {
	int terminal;
	int state[PARALLEL];
	int bytepos;

	int real_pos;
};

struct state state[2000];
unsigned char terms[2000];
int next = 1;

int lextable_decode(int pos, char c)
{
	int q = pos + !!c;

	if (lextable_terms[q >> 3] & (1 << (q & 7))) /* terminal */
		return lextable[q] | 0x8000;

	return pos + (lextable[q] << 1);
}

int main(void)
{
	int n = 0;
	int m = 0;
	int prev;
	char c;
	int walk;
	int saw;
	int y;
	int j;
	int pos = 0;
	int biggest = 0;
	int fails = 0;

	m = 0;
	while (m < LWS_ARRAY_SIZE(state)) {
		for (j = 0; j < PARALLEL; j++) {
			state[m].state[j] = 0xffff;
			state[m].terminal = 0;
		}
		m++;
	}

	while (n < LWS_ARRAY_SIZE(huf_literal)) {

		m = 0;
		walk = 0;
		prev = 0;

		while (m < huf_literal[n].len) {

			saw = 0;
			if (state[walk].state[code_bit(n, m)] != 0xffff) {
				/* exists -- go forward */
				walk = state[walk].state[code_bit(n, m)];
				goto again;
			}

			/* something we didn't see before */

			state[walk].state[code_bit(n, m)] = next;
			walk = next++;
again:
			m++;
		}

		state[walk].terminal = n++;
		state[walk].state[0] = 0; /* terminal marker */
	}

	walk = 0;
	for (n = 0; n < next; n++) {
		state[n].bytepos = walk;
		walk += (2 * 2);
	}

	/* compute everyone's position first */

	pos = 0;
	walk = 0;
	for (n = 0; n < next; n++) {

		state[n].real_pos = pos;

		if (state[n].state[0]) /* nonterminal */
			pos += 2;

		walk ++;
	}

	fprintf(stdout, "static unsigned char lextable[] = {\n");

#define TERMINAL_MASK 0x8000

	walk = 0;
	pos = 0;

	for (n = 0; n < next; n++) {
		for (m = 0; m < 2; m++) {
			saw = state[n].state[m];

			if (saw == 0) { // c is a terminal then
				m = 2;
				continue;
			}
			if (!m)
				fprintf(stdout, "/* pos %04x: %3d */ ",
							  state[n].real_pos, n);
			else
				fprintf(stdout, "                    ");

			if (saw == 0xffff) {
				fprintf(stdout,
				  "   0xff, 0xff, /* 0 = fail */\n                    ");
				pos ++; /* fail */
				fails++;
				continue;
			}

			if (state[saw].state[0] == 0) { /* points to terminal */
				fprintf(stdout, "    /* terminal %d */ 0x%02X,\n",
					state[saw].terminal,
					state[saw].terminal & 0xff);
				terms[(state[n].real_pos + m) >> 3] |=
					1 << ((state[n].real_pos + m) & 7);
				pos++;
				walk++;
				continue;
			}

			j = (state[saw].real_pos - q) >> 1;

			if (j > biggest)
				biggest = j;

			if (j > 0xffff) {
				fprintf(stderr,
				  "Jump > 64K bytes ahead (%d to %d)\n",
					state[n].real_pos, state[saw].real_pos);
				return 1;
			}

			fprintf(stdout, "   /* %d */ 0x%02X  "
				"/* (to 0x%04X state %3d) */,\n",
				m,
				j & 0xff,
				state[saw].real_pos, saw);
			pos++;

			walk++;
		}
	}

	fprintf(stdout, "/* total size %d bytes, biggest jump %d/256, fails=%d */\n};\n"
			"\n static unsigned char lextable_terms[] = {\n",
	 		pos, biggest, fails);

	for (n = 0; n < (walk + 7) / 8; n++) {
		if (!(n & 7))
			fprintf(stdout, "\n\t");
		fprintf(stdout, "0x%02x, ", terms[n]);
	}
	fprintf(stdout, "\n};\n");

	/*
	 * Try to parse every legal input string
	 */

	for (n = 0; n < LWS_ARRAY_SIZE(huf_literal); n++) {
		walk = 0;
		m = 0;
		y = -1;

		fprintf(stderr, "  trying %d\n", n);

		while (m < huf_literal[n].len) {
			prev = walk;
			walk = lextable_decode(walk, code_bit(n, m));

			if (walk == 0xffff) {
				fprintf(stderr, "failed\n");
				return 3;
			}

			if (walk & 0x8000) {
				y = walk & 0x7fff;
				if (y == 0 && m == 29) {
					y |= 0x100;
					fprintf(stdout,
						"\n/* state that points to "
						"0x100 for disambiguation with "
						"0x0 */\n"
						"#define HUFTABLE_0x100_PREV "
						"%d\n", prev);
				}
				break;
			}
			m++;
		}

		if (y != n) {
			fprintf(stderr, "decode failed %d got %d (0x%x)\n", n, y, y);
			return 4;
		}
	}

	fprintf(stderr, "All decode OK\n");

	return 0;
}
