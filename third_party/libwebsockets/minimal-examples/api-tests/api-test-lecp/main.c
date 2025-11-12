/*
 * lws-api-test-lecp
 *
 * Written in 2010-2021 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * unit tests for lecp
 */

#include <libwebsockets.h>

#if defined(LWS_WITH_CBOR_FLOAT)
#include <math.h>
#endif

#define VERBOSE

#if defined(VERBOSE)
static const char * const reason_names[] = {
	"LECPCB_CONSTRUCTED",
	"LECPCB_DESTRUCTED",
	"LECPCB_START",
	"LECPCB_COMPLETE",
	"LECPCB_FAILED",
	"LECPCB_PAIR_NAME",
	"LECPCB_VAL_TRUE",
	"LECPCB_VAL_FALSE",
	"LECPCB_VAL_NULL",
	"LECPCB_VAL_NUM_INT",
	"LECPCB_VAL_RESERVED", /* float in lejp */
	"LECPCB_VAL_STR_START",
	"LECPCB_VAL_STR_CHUNK",
	"LECPCB_VAL_STR_END",
	"LECPCB_ARRAY_START",
	"LECPCB_ARRAY_END",
	"LECPCB_OBJECT_START",
	"LECPCB_OBJECT_END",
	"LECPCB_TAG_START",
	"LECPCB_TAG_END",
	"LECPCB_VAL_NUM_UINT",
	"LECPCB_VAL_UNDEFINED",
	"LECPCB_VAL_FLOAT16",
	"LECPCB_VAL_FLOAT32",
	"LECPCB_VAL_FLOAT64",
	"LECPCB_VAL_SIMPLE",
	"LECPCB_VAL_BLOB_START",
	"LECPCB_VAL_BLOB_CHUNK",
	"LECPCB_VAL_BLOB_END",
	"LECPCB_ARRAY_ITEM_START",
	"LECPCB_ARRAY_ITEM_END",
	"LECPCB_LITERAL_CBOR"
};
#endif

/*
 * Based on the official CBOR test vectors from here
 *
 * https://github.com/cbor/test-vectors/blob/master/appendix_a.json
 */

static const uint8_t
	test1[]		= { 0x00 },
	test2[]		= { 0x01 },
	test3[]		= { 0x0a },
	test4[]		= { 0x17 },
	test5[]		= { 0x18, 0x18 },
	test6[]		= { 0x18, 0x19 },
	test7[]		= { 0x18, 0x64 },
	test8[]		= { 0x19, 0x03, 0xe8 },
	test9[]		= { 0x1a, 0x00, 0x0f, 0x42, 0x40 },
	test10[]	= { 0x1b, 0x00, 0x00, 0x00,
			    0xe8, 0xd4, 0xa5, 0x10, 0x00 },
	test11[]	= { 0x1b, 0xff, 0xff, 0xff, 0xff,
			    0xff, 0xff, 0xff, 0xff },
	test12[]	= { 0xc2, 0x49, 0x01, 0x00, 0x00,
			    0x00, 0x00, 0x00, 0x00, 0x00,
			    0x00 },
	test13[]	= { 0x3b, 0xff, 0xff, 0xff, 0xff,
			    0xff, 0xff, 0xff, 0xff },
	test14[]	= { 0xc3, 0x49, 0x01, 0x00, 0x00,
			    0x00, 0x00, 0x00, 0x00, 0x00,
			    0x00 },
	test15[]	= { 0x20 },
	test16[]	= { 0x29 },
	test17[]	= { 0x38, 0x63 },
	test18[]	= { 0x39, 0x03, 0xe7 },
	test19[]	= { 0xf9, 0x00, 0x00 },
	test20[]	= { 0xf9, 0x80, 0x00 },
	test21[]	= { 0xf9, 0x3c, 0x00 },
	test22[]	= { 0xfb, 0x3f, 0xf1, 0x99, 0x99,
			    0x99, 0x99, 0x99, 0x9a },
	test23[]	= { 0xf9, 0x3e, 0x00 },
	test24[]	= { 0xf9, 0x7b, 0xff },
	test25[]	= { 0xfa, 0x47, 0xc3, 0x50, 0x00 },
	test26[]	= { 0xfa, 0x7f, 0x7f, 0xff, 0xff },
	test27[]	= { 0xfb, 0x7e, 0x37, 0xe4, 0x3c,
			    0x88, 0x00, 0x75, 0x9c },
	test28[]	= { 0xf9, 0x00, 0x01 },
	test29[]	= { 0xf9, 0x04, 0x00 },
	test30[]	= { 0xf9, 0xc4, 0x00 },
	test31[]	= { 0xfb, 0xc0, 0x10, 0x66, 0x66,
			    0x66, 0x66, 0x66, 0x66 },
	test32[]	= { 0xf9, 0x7c, 0x00 },
	test33[]	= { 0xf9, 0x7e, 0x00 },
	test34[]	= { 0xf9, 0xfc, 0x00 },
	test35[]	= { 0xfa, 0x7f, 0x80, 0x00, 0x00 },
	test36[]	= { 0xfa, 0x7f, 0xc0, 0x00, 0x00 },
	test37[]	= { 0xfa, 0xff, 0x80, 0x00, 0x00 },
	test38[]	= { 0xfb, 0x7f, 0xf0, 0x00, 0x00,
			    0x00, 0x00, 0x00, 0x00 },
	test39[]	= { 0xfb, 0x7f, 0xf8, 0x00, 0x00,
			    0x00, 0x00, 0x00, 0x00 },
	test40[]	= { 0xfb, 0xff, 0xf0, 0x00, 0x00,
			    0x00, 0x00, 0x00, 0x00 },
	test41[]	= { 0xf4 },
	test42[]	= { 0xf5 },
	test43[]	= { 0xf6 },
	test44[]	= { 0xf7 },
	test45[]	= { 0xf0 },
	test46[]	= { 0xf8, 0x18 },
	test47[]	= { 0xf8, 0xff },
	test48[]	= { 0xc0, 0x74, 0x32, 0x30, 0x31,
			    0x33, 0x2d, 0x30, 0x33, 0x2d,
			    0x32, 0x31, 0x54, 0x32, 0x30,
			    0x3a, 0x30, 0x34, 0x3a, 0x30,
			    0x30, 0x5a },
	test49[]	= { 0xc1, 0x1a, 0x51, 0x4b, 0x67,
			    0xb0 },
	test50[]	= { 0xc1, 0xfb, 0x41, 0xd4, 0x52,
			    0xd9, 0xec, 0x20, 0x00, 0x00 },
	test51[]	= { 0xd7, 0x44, 0x01, 0x02, 0x03,
			    0x04 },
	test52[]	= { 0xd8, 0x18, 0x45, 0x64, 0x49,
			    0x45, 0x54, 0x46 },
	test53[]	= { 0xd8, 0x20, 0x76, 0x68, 0x74,
			    0x74, 0x70, 0x3a, 0x2f, 0x2f,
			    0x77, 0x77, 0x77, 0x2e, 0x65,
			    0x78, 0x61, 0x6d, 0x70, 0x6c,
			    0x65, 0x2e, 0x63, 0x6f, 0x6d },
	test54[]	= { 0x40 },
	test55[]	= { 0x44, 0x01, 0x02, 0x03, 0x04 },
	test56[]	= { 0x60 },
	test57[]	= { 0x61, 0x61 },
	test58[]	= { 0x64, 0x49, 0x45, 0x54, 0x46 },
	test59[]	= { 0x62, 0x22, 0x5c },
	test60[]	= { 0x62, 0xc3, 0xbc },
	test61[]	= { 0x63, 0xe6, 0xb0, 0xb4 },
	test62[]	= { 0x64, 0xf0, 0x90, 0x85, 0x91 },
	test63[]	= { 0x80 },
	test64[]	= { 0x83, 0x01, 0x02, 0x03 },
	test65[]	= { 0x83, 0x01, 0x82, 0x02, 0x03,
			    0x82, 0x04, 0x05 },
	test66[]	= { 0x98, 0x19, 0x01, 0x02, 0x03,
			    0x04, 0x05, 0x06, 0x07, 0x08,
			    0x09, 0x0a, 0x0b, 0x0c, 0x0d,
			    0x0e, 0x0f, 0x10, 0x11, 0x12,
			    0x13, 0x14, 0x15, 0x16, 0x17,
			    0x18, 0x18, 0x18, 0x19 },
	test67[]	= { 0xa0 },
	test68[]	= { 0xa2, 0x01, 0x02, 0x03, 0x04 },
	test69[]	= { 0xa2, 0x61, 0x61, 0x01, 0x61,
			    0x62, 0x82, 0x02, 0x03 },
	test70[]	= { 0x82, 0x61, 0x61, 0xa1, 0x61,
			    0x62, 0x61, 0x63 },
	test71[]	= { 0xa5, 0x61, 0x61, 0x61, 0x41,
			    0x61, 0x62, 0x61, 0x42, 0x61,
			    0x63, 0x61, 0x43, 0x61, 0x64,
			    0x61, 0x44, 0x61, 0x65, 0x61,
			    0x45 },
	test72[]	= { 0x5f, 0x42, 0x01, 0x02, 0x43,
			    0x03, 0x04, 0x05, 0xff },
	test73[]	= { 0x7f, 0x65, 0x73, 0x74, 0x72,
			    0x65, 0x61, 0x64, 0x6d, 0x69,
			    0x6e, 0x67, 0xff },
	test74[]	= { 0x9f, 0xff },
	test75[]	= { 0x9f, 0x01, 0x82, 0x02, 0x03,
			    0x9f, 0x04, 0x05, 0xff, 0xff },
	test76[]	= { 0x9f, 0x01, 0x82, 0x02, 0x03,
			    0x82, 0x04, 0x05, 0xff },
	test77[]	= { 0x83, 0x01, 0x82, 0x02, 0x03,
			    0x9f, 0x04, 0x05, 0xff },
	test78[]	= { 0x83, 0x01, 0x9f, 0x02, 0x03,
			    0xff, 0x82, 0x04, 0x05 },
	test79[]	= { 0x9f, 0x01, 0x02, 0x03, 0x04,
			    0x05, 0x06, 0x07, 0x08, 0x09,
			    0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
			    0x0f, 0x10, 0x11, 0x12, 0x13,
			    0x14, 0x15, 0x16, 0x17, 0x18,
			    0x18, 0x18, 0x19, 0xff },
	test80[]	= { 0xbf, 0x61, 0x61, 0x01, 0x61,
			    0x62, 0x9f, 0x02, 0x03, 0xff,
			    0xff },
	test81[]	= { 0x82, 0x61, 0x61, 0xbf, 0x61,
			    0x62, 0x61, 0x63, 0xff },
	test82[]	= { 0xbf, 0x63, 0x46, 0x75, 0x6e,
			    0xf5, 0x63, 0x41, 0x6d, 0x74,
			    0x21, 0xff },

	/* some random COSE examples
	 *
	 * COSE hmac-01 test vector
	 */

	test83[]	= { 0xD8, 0x61, 0x85, 0x43, 0xA1,
			    0x01, 0x05, 0xA0, 0x54, 0x54,
			    0x68, 0x69, 0x73, 0x20, 0x69,
			    0x73, 0x20, 0x74, 0x68, 0x65,
			    0x20, 0x63, 0x6F, 0x6E, 0x74,
			    0x65, 0x6E, 0x74, 0x2E, 0x58,
			    0x20, 0x2B, 0xDC, 0xC8, 0x9F,
			    0x05, 0x82, 0x16, 0xB8, 0xA2,
			    0x08, 0xDD, 0xC6, 0xD8, 0xB5,
			    0x4A, 0xA9, 0x1F, 0x48, 0xBD,
			    0x63, 0x48, 0x49, 0x86, 0x56,
			    0x51, 0x05, 0xC9, 0xAD, 0x5A,
			    0x66, 0x82, 0xF6, 0x81, 0x83,
			    0x40, 0xA2, 0x01, 0x25, 0x04,
			    0x4A, 0x6F, 0x75, 0x72, 0x2D,
			    0x73, 0x65, 0x63, 0x72, 0x65,
			    0x74, 0x40 },
	 /*
	 * COSE hmac-02 test vector
	 */
	test84[]	= { 0xD8, 0x61, 0x85, 0x43, 0xA1,
			    0x01, 0x06, 0xA0, 0x54, 0x54,
			    0x68, 0x69, 0x73, 0x20, 0x69,
			    0x73, 0x20, 0x74, 0x68, 0x65,
			    0x20, 0x63, 0x6F, 0x6E, 0x74,
			    0x65, 0x6E, 0x74, 0x2E, 0x58,
			    0x30, 0xB3, 0x09, 0x7F, 0x70,
			    0x00, 0x9A, 0x11, 0x50, 0x74,
			    0x09, 0x59, 0x8A, 0x83, 0xE1,
			    0x5B, 0xBB, 0xBF, 0x19, 0x82,
			    0xDC, 0xE2, 0x8E, 0x5A, 0xB6,
			    0xD5, 0xA6, 0xAF, 0xF6, 0x89,
			    0x7B, 0xD2, 0x4B, 0xB8, 0xB7,
			    0x47, 0x96, 0x22, 0xC9, 0x40,
			    0x1B, 0x24, 0x09, 0x0D, 0x45,
			    0x82, 0x06, 0xD5, 0x87, 0x81,
			    0x83, 0x40, 0xA2, 0x01, 0x25,
			    0x04, 0x46, 0x73, 0x65, 0x63,
			    0x2D, 0x34, 0x38, 0x40 },
	test85[]	= { 0xD8, 0x61, 0x85, 0x43, 0xA1,
			    0x01, 0x07, 0xA0, 0x54, 0x54,
			    0x68, 0x69, 0x73, 0x20, 0x69,
			    0x73, 0x20, 0x74, 0x68, 0x65,
			    0x20, 0x63, 0x6F, 0x6E, 0x74,
			    0x65, 0x6E, 0x74, 0x2E, 0x58,
			    0x40, 0xCD, 0x28, 0xA6, 0xB3,
			    0xCF, 0xBB, 0xBF, 0x21, 0x48,
			    0x51, 0xB9, 0x06, 0xE0, 0x50,
			    0x05, 0x6C, 0xB4, 0x38, 0xA8,
			    0xB8, 0x89, 0x05, 0xB8, 0xB7,
			    0x46, 0x19, 0x77, 0x02, 0x27,
			    0x11, 0xA9, 0xD8, 0xAC, 0x5D,
			    0xBC, 0x54, 0xE2, 0x9A, 0x56,
			    0xD9, 0x26, 0x04, 0x6B, 0x40,
			    0xFC, 0x26, 0x07, 0xC2, 0x5B,
			    0x34, 0x44, 0x54, 0xAA, 0x5F,
			    0x68, 0xDE, 0x09, 0xA3, 0xE5,
			    0x25, 0xD3, 0x86, 0x5A, 0x05,
			    0x81, 0x83, 0x40, 0xA2, 0x01,
			    0x25, 0x04, 0x46, 0x73, 0x65,
			    0x63, 0x2D, 0x36, 0x34, 0x40 },
	test86[]	= { 0xD8, 0x61, 0x85, 0x43, 0xA1,
			    0x01, 0x05, 0xA0, 0x54, 0x54,
			    0x68, 0x69, 0x73, 0x20, 0x69,
			    0x73, 0x20, 0x74, 0x68, 0x65,
			    0x20, 0x63, 0x6F, 0x6E, 0x74,
			    0x65, 0x6E, 0x74, 0x2E, 0x58,
			    0x20, 0x2B, 0xDC, 0xC8, 0x9F,
			    0x05, 0x82, 0x16, 0xB8, 0xA2,
			    0x08, 0xDD, 0xC6, 0xD8, 0xB5,
			    0x4A, 0xA9, 0x1F, 0x48, 0xBD,
			    0x63, 0x48, 0x49, 0x86, 0x56,
			    0x51, 0x05, 0xC9, 0xAD, 0x5A,
			    0x66, 0x82, 0xF7, 0x81, 0x83,
			    0x40, 0xA2, 0x01, 0x25, 0x04,
			    0x4A, 0x6F, 0x75, 0x72, 0x2D,
			    0x73, 0x65, 0x63, 0x72, 0x65,
			    0x74, 0x40 },
	test87[]	= { 0xD8, 0x61, 0x85, 0x43, 0xA1,
			    0x01, 0x04, 0xA0, 0x54, 0x54,
			    0x68, 0x69, 0x73, 0x20, 0x69,
			    0x73, 0x20, 0x74, 0x68, 0x65,
			    0x20, 0x63, 0x6F, 0x6E, 0x74,
			    0x65, 0x6E, 0x74, 0x2E, 0x48,
			    0x6F, 0x35, 0xCA, 0xB7, 0x79,
			    0xF7, 0x78, 0x33, 0x81, 0x83,
			    0x40, 0xA2, 0x01, 0x25, 0x04,
			    0x4A, 0x6F, 0x75, 0x72, 0x2D,
			    0x73, 0x65, 0x63, 0x72, 0x65,
			    0x74, 0x40

			    /* COSE HMAX Enc 01 vector */

	}, test88[]	= { 0xD1, 0x84, 0x43, 0xA1, 0x01,
			    0x05, 0xA0, 0x54, 0x54, 0x68,
			    0x69, 0x73, 0x20, 0x69, 0x73,
			    0x20, 0x74, 0x68, 0x65, 0x20,
			    0x63, 0x6F, 0x6E, 0x74, 0x65,
			    0x6E, 0x74, 0x2E, 0x58, 0x20,
			    0xA1, 0xA8, 0x48, 0xD3, 0x47,
			    0x1F, 0x9D, 0x61, 0xEE, 0x49,
			    0x01, 0x8D, 0x24, 0x4C, 0x82,
			    0x47, 0x72, 0xF2, 0x23, 0xAD,
			    0x4F, 0x93, 0x52, 0x93, 0xF1,
			    0x78, 0x9F, 0xC3, 0xA0, 0x8D,
			    0x8C, 0x58
	}, test89[]	= { 0xD1, 0x84, 0x43, 0xA1, 0x01,
			    0x06, 0xA0, 0x54, 0x54, 0x68,
			    0x69, 0x73, 0x20, 0x69, 0x73,
			    0x20, 0x74, 0x68, 0x65, 0x20,
			    0x63, 0x6F, 0x6E, 0x74, 0x65,
			    0x6E, 0x74, 0x2E, 0x58, 0x30,
			    0x99, 0x8D, 0x26, 0xC6, 0x45,
			    0x9A, 0xAE, 0xEC, 0xF4, 0x4E,
			    0xD2, 0x0C, 0xE0, 0x0C, 0x8C,
			    0xCE, 0xDF, 0x0A, 0x1F, 0x3D,
			    0x22, 0xA9, 0x2F, 0xC0, 0x5D,
			    0xB0, 0x8C, 0x5A, 0xEB, 0x1C,
			    0xB5, 0x94, 0xCA, 0xAF, 0x5A,
			    0x5C, 0x5E, 0x2E, 0x9D, 0x01,
			    0xCC, 0xE7, 0xE7, 0x7A, 0x93,
			    0xAA, 0x8C, 0x62
	}, test90[]	= { 0xD1, 0x84, 0x43, 0xA1, 0x01,
			    0x07, 0xA0, 0x54, 0x54, 0x68,
			    0x69, 0x73, 0x20, 0x69, 0x73,
			    0x20, 0x74, 0x68, 0x65, 0x20,
			    0x63, 0x6F, 0x6E, 0x74, 0x65,
			    0x6E, 0x74, 0x2E, 0x58, 0x40,
			    0x4A, 0x55, 0x5B, 0xF9, 0x71,
			    0xF7, 0xC1, 0x89, 0x1D, 0x9D,
			    0xDF, 0x30, 0x4A, 0x1A, 0x13,
			    0x2E, 0x2D, 0x6F, 0x81, 0x74,
			    0x49, 0x47, 0x4D, 0x81, 0x3E,
			    0x6D, 0x04, 0xD6, 0x59, 0x62,
			    0xBE, 0xD8, 0xBB, 0xA7, 0x0C,
			    0x17, 0xE1, 0xF5, 0x30, 0x8F,
			    0xA3, 0x99, 0x62, 0x95, 0x9A,
			    0x4B, 0x9B, 0x8D, 0x7D, 0xA8,
			    0xE6, 0xD8, 0x49, 0xB2, 0x09,
			    0xDC, 0xD3, 0xE9, 0x8C, 0xC0,
			    0xF1, 0x1E, 0xDD, 0xF2

	}, test91[]	= { 0xD1, 0x84, 0x43, 0xA1, 0x01,
			    0x05, 0xA0, 0x54, 0x54, 0x68,
			    0x69, 0x73, 0x20, 0x69, 0x73,
			    0x20, 0x74, 0x68, 0x65, 0x20,
			    0x63, 0x6F, 0x6E, 0x74, 0x65,
			    0x6E, 0x74, 0x2E, 0x58, 0x20,
			    0xA1, 0xA8, 0x48, 0xD3, 0x47,
			    0x1F, 0x9D, 0x61, 0xEE, 0x49,
			    0x01, 0x8D, 0x24, 0x4C, 0x82,
			    0x47, 0x72, 0xF2, 0x23, 0xAD,
			    0x4F, 0x93, 0x52, 0x93, 0xF1,
			    0x78, 0x9F, 0xC3, 0xA0, 0x8D,
			    0x8C, 0x59

	}, test92[]	= { 0xD1, 0x84, 0x43, 0xA1, 0x01,
			    0x04, 0xA0, 0x54, 0x54, 0x68,
			    0x69, 0x73, 0x20, 0x69, 0x73,
			    0x20, 0x74, 0x68, 0x65, 0x20,
			    0x63, 0x6F, 0x6E, 0x74, 0x65,
			    0x6E, 0x74, 0x2E, 0x48, 0x11,
			    0xF9, 0xE3, 0x57, 0x97, 0x5F,
			    0xB8, 0x49

			 /*
			 * COSE countersign encrypt-01
			 */

	}, test93[]	= {
			    0xd0, 0x83, 0x43, 0xa1, 0x01,
			    0x01, 0xa2, 0x05, 0x4c, 0x02,
			    0xd1, 0xf7, 0xe6, 0xf2, 0x6c,
			    0x43, 0xd4, 0x86, 0x8d, 0x87,
			    0xce, 0x07, 0x83, 0x43, 0xa1,
			    0x01, 0x27, 0xa1, 0x04, 0x42,
			    0x31, 0x31, 0x58, 0x40, 0xe1,
			    0x04, 0x39, 0x15, 0x4c, 0xc7,
			    0x5c, 0x7a, 0x3a, 0x53, 0x91,
			    0x49, 0x1f, 0x88, 0x65, 0x1e,
			    0x02, 0x92, 0xfd, 0x0f, 0xe0,
			    0xe0, 0x2c, 0xf7, 0x40, 0x54,
			    0x7e, 0xaf, 0x66, 0x77, 0xb4,
			    0xa4, 0x04, 0x0b, 0x8e, 0xca,
			    0x16, 0xdb, 0x59, 0x28, 0x81,
			    0x26, 0x2f, 0x77, 0xb1, 0x4c,
			    0x1a, 0x08, 0x6c, 0x02, 0x26,
			    0x8b, 0x17, 0x17, 0x1c, 0xa1,
			    0x6b, 0xe4, 0xb8, 0x59, 0x5f,
			    0x8c, 0x0a, 0x08, 0x58, 0x24,
			    0x60, 0x97, 0x3a, 0x94, 0xbb,
			    0x28, 0x98, 0x00, 0x9e, 0xe5,
			    0x2e, 0xcf, 0xd9, 0xab, 0x1d,
			    0xd2, 0x58, 0x67, 0x37, 0x4b,
			    0x16, 0x2e, 0x2c, 0x03, 0x56,
			    0x8b, 0x41, 0xf5, 0x7c, 0x3c,
			    0xc1, 0x6f, 0x91, 0x66, 0x25,
			    0x0a
			 /*
			  * COSE countersign encrypt-02
			  */
		}, test94[]	= {
			0xd0, 0x83, 0x43, 0xa1, 0x01,
			0x01, 0xa2, 0x05, 0x4c, 0x02,
			0xd1, 0xf7, 0xe6, 0xf2, 0x6c,
			0x43, 0xd4, 0x86, 0x8d, 0x87,
			0xce, 0x07, 0x82, 0x83, 0x43,
			0xa1, 0x01, 0x27, 0xa1, 0x04,
			0x42, 0x31, 0x31, 0x58, 0x40,
			0xe1, 0x04, 0x39, 0x15, 0x4c,
			0xc7, 0x5c, 0x7a, 0x3a, 0x53,
			0x91, 0x49, 0x1f, 0x88, 0x65,
			0x1e, 0x02, 0x92, 0xfd, 0x0f,
			0xe0, 0xe0, 0x2c, 0xf7, 0x40,
			0x54, 0x7e, 0xaf, 0x66, 0x77,
			0xb4, 0xa4, 0x04, 0x0b, 0x8e,
			0xca, 0x16, 0xdb, 0x59, 0x28,
			0x81, 0x26, 0x2f, 0x77, 0xb1,
			0x4c, 0x1a, 0x08, 0x6c, 0x02,
			0x26, 0x8b, 0x17, 0x17, 0x1c,
			0xa1, 0x6b, 0xe4, 0xb8, 0x59,
			0x5f, 0x8c, 0x0a, 0x08, 0x83,
			0x43, 0xa1, 0x01, 0x26, 0xa1,
			0x04, 0x42, 0x31, 0x31, 0x58,
			0x40, 0xfc, 0xa9, 0x8e, 0xca,
			0xc8, 0x0b, 0x5f, 0xeb, 0x3a,
			0xc7, 0xc1, 0x08, 0xb2, 0xb7,
			0x91, 0x10, 0xde, 0x88, 0x86,
			0x7b, 0xc0, 0x42, 0x6f, 0xc8,
			0x3c, 0x53, 0xcc, 0xd6, 0x78,
			0x96, 0x94, 0xed, 0xc5, 0xfe,
			0xe3, 0xc4, 0x0d, 0xe8, 0xe7,
			0xb4, 0x4f, 0xe8, 0xaa, 0xd3,
			0x67, 0xe0, 0x95, 0xc8, 0xfc,
			0x31, 0xb7, 0x9e, 0xe6, 0x66,
			0xdf, 0x9c, 0xf9, 0x09, 0x06,
			0xeb, 0x43, 0x75, 0x6c, 0x73,
			0x58, 0x24, 0x60, 0x97, 0x3a,
			0x94, 0xbb, 0x28, 0x98, 0x00,
			0x9e, 0xe5, 0x2e, 0xcf, 0xd9,
			0xab, 0x1d, 0xd2, 0x58, 0x67,
			0x37, 0x4b, 0x16, 0x2e, 0x2c,
			0x03, 0x56, 0x8b, 0x41, 0xf5,
			0x7c, 0x3c, 0xc1, 0x6f, 0x91,
			0x66, 0x25, 0x0a

			 /*
			  * COSE countersign enveloped-01
			  */
	}, test95[]	= {
			0xd8, 0x60, 0x84, 0x43, 0xa1,
			0x01, 0x01, 0xa2, 0x05, 0x4c,
			0x02, 0xd1, 0xf7, 0xe6, 0xf2,
			0x6c, 0x43, 0xd4, 0x86, 0x8d,
			0x87, 0xce, 0x07, 0x83, 0x43,
			0xa1, 0x01, 0x27, 0xa1, 0x04,
			0x42, 0x31, 0x31, 0x58, 0x40,
			0x9a, 0x8e, 0xed, 0xe3, 0xb3,
			0xcb, 0x83, 0x7b, 0xa0, 0x0d,
			0xf0, 0x8f, 0xa2, 0x1b, 0x12,
			0x8b, 0x2d, 0x6d, 0x91, 0x62,
			0xa4, 0x29, 0x0a, 0x58, 0x2d,
			0x9f, 0x19, 0xbd, 0x0f, 0xb5,
			0x02, 0xf0, 0xf9, 0x2b, 0x9b,
			0xf4, 0x53, 0xa4, 0x05, 0x40,
			0x1f, 0x8b, 0x70, 0x55, 0xef,
			0x4e, 0x95, 0x8d, 0xf7, 0xf4,
			0xfb, 0xd7, 0xcf, 0xb4, 0xa0,
			0xc9, 0x71, 0x60, 0xf9, 0x47,
			0x2b, 0x0a, 0xa1, 0x04, 0x58,
			0x24, 0x60, 0x97, 0x3a, 0x94,
			0xbb, 0x28, 0x98, 0x00, 0x9e,
			0xe5, 0x2e, 0xcf, 0xd9, 0xab,
			0x1d, 0xd2, 0x58, 0x67, 0x37,
			0x4b, 0x35, 0x81, 0xf2, 0xc8,
			0x00, 0x39, 0x82, 0x63, 0x50,
			0xb9, 0x7a, 0xe2, 0x30, 0x0e,
			0x42, 0xfc, 0x81, 0x83, 0x40,
			0xa2, 0x01, 0x25, 0x04, 0x4a,
			0x6f, 0x75, 0x72, 0x2d, 0x73,
			0x65, 0x63, 0x72, 0x65, 0x74,
			0x40
	}, test96[]	= {
			0xd8, 0x60, 0x84, 0x43, 0xa1,
			0x01, 0x01, 0xa2, 0x05, 0x4c,
			0x02, 0xd1, 0xf7, 0xe6, 0xf2,
			0x6c, 0x43, 0xd4, 0x86, 0x8d,
			0x87, 0xce, 0x07, 0x82, 0x83,
			0x43, 0xa1, 0x01, 0x27, 0xa1,
			0x04, 0x42, 0x31, 0x31, 0x58,
			0x40, 0x9a, 0x8e, 0xed, 0xe3,
			0xb3, 0xcb, 0x83, 0x7b, 0xa0,
			0x0d, 0xf0, 0x8f, 0xa2, 0x1b,
			0x12, 0x8b, 0x2d, 0x6d, 0x91,
			0x62, 0xa4, 0x29, 0x0a, 0x58,
			0x2d, 0x9f, 0x19, 0xbd, 0x0f,
			0xb5, 0x02, 0xf0, 0xf9, 0x2b,
			0x9b, 0xf4, 0x53, 0xa4, 0x05,
			0x40, 0x1f, 0x8b, 0x70, 0x55,
			0xef, 0x4e, 0x95, 0x8d, 0xf7,
			0xf4, 0xfb, 0xd7, 0xcf, 0xb4,
			0xa0, 0xc9, 0x71, 0x60, 0xf9,
			0x47, 0x2b, 0x0a, 0xa1, 0x04,
			0x83, 0x43, 0xa1, 0x01, 0x26,
			0xa1, 0x04, 0x42, 0x31, 0x31,
			0x58, 0x40, 0x24, 0x27, 0xcb,
			0x37, 0x56, 0x85, 0x0f, 0xbb,
			0x79, 0x05, 0x18, 0x07, 0xc8,
			0xb2, 0x3d, 0x2e, 0x6d, 0x16,
			0xa3, 0x22, 0x4f, 0x99, 0x01,
			0xb4, 0x73, 0x99, 0xcf, 0xc7,
			0xe3, 0xfa, 0xc4, 0xcc, 0x62,
			0x1d, 0xbb, 0xeb, 0x02, 0x02,
			0xa6, 0xd8, 0xbb, 0x25, 0x69,
			0x5c, 0x9d, 0xcc, 0x9c, 0x47,
			0x49, 0x20, 0xff, 0x57, 0x60,
			0x6d, 0x76, 0x4d, 0xea, 0x19,
			0x2f, 0xc8, 0x67, 0x41, 0x16,
			0xf2, 0x58, 0x24, 0x60, 0x97,
			0x3a, 0x94, 0xbb, 0x28, 0x98,
			0x00, 0x9e, 0xe5, 0x2e, 0xcf,
			0xd9, 0xab, 0x1d, 0xd2, 0x58,
			0x67, 0x37, 0x4b, 0x35, 0x81,
			0xf2, 0xc8, 0x00, 0x39, 0x82,
			0x63, 0x50, 0xb9, 0x7a, 0xe2,
			0x30, 0x0e, 0x42, 0xfc, 0x81,
			0x83, 0x40, 0xa2, 0x01, 0x25,
			0x04, 0x4a, 0x6f, 0x75, 0x72,
			0x2d, 0x73, 0x65, 0x63, 0x72,
			0x65, 0x74, 0x40

	}, test97[]	= {
			0xd8, 0x60, 0x84, 0x43, 0xa1,
			0x01, 0x01, 0xa1, 0x05, 0x4c,
			0x02, 0xd1, 0xf7, 0xe6, 0xf2,
			0x6c, 0x43, 0xd4, 0x86, 0x8d,
			0x87, 0xce, 0x58, 0x24, 0x60,
			0x97, 0x3a, 0x94, 0xbb, 0x28,
			0x98, 0x00, 0x9e, 0xe5, 0x2e,
			0xcf, 0xd9, 0xab, 0x1d, 0xd2,
			0x58, 0x67, 0x37, 0x4b, 0x35,
			0x81, 0xf2, 0xc8, 0x00, 0x39,
			0x82, 0x63, 0x50, 0xb9, 0x7a,
			0xe2, 0x30, 0x0e, 0x42, 0xfc,
			0x81, 0x83, 0x40, 0xa3, 0x01,
			0x25, 0x04, 0x4a, 0x6f, 0x75,
			0x72, 0x2d, 0x73, 0x65, 0x63,
			0x72, 0x65, 0x74, 0x07, 0x83,
			0x43, 0xa1, 0x01, 0x27, 0xa1,
			0x04, 0x42, 0x31, 0x31, 0x58,
			0x40, 0xcc, 0xb1, 0xf3, 0xfe,
			0xdf, 0xce, 0xa7, 0x2b, 0x9c,
			0x86, 0x79, 0x63, 0xe2, 0x52,
			0xb6, 0x65, 0x8a, 0xd0, 0x7f,
			0x3f, 0x5f, 0x15, 0xa3, 0x26,
			0xa3, 0xf5, 0x72, 0x54, 0xcc,
			0xb8, 0xd4, 0x8d, 0x60, 0x02,
			0x1d, 0x2f, 0x1f, 0x8a, 0x80,
			0x3b, 0x84, 0x4b, 0x78, 0x72,
			0x16, 0x6c, 0x6d, 0x45, 0x90,
			0x25, 0xd2, 0x1c, 0x8c, 0x84,
			0x62, 0xa2, 0x44, 0xba, 0x19,
			0x60, 0x4e, 0xc4, 0xd5, 0x0b,
			0x40
	}, test98[]	= {
			0xd8, 0x61, 0x85, 0x43, 0xa1,
			0x01, 0x05, 0xa1, 0x07, 0x83,
			0x43, 0xa1, 0x01, 0x27, 0xa1,
			0x04, 0x42, 0x31, 0x31, 0x58,
			0x40, 0xb4, 0x92, 0x4b, 0x18,
			0xeb, 0x4e, 0x04, 0x73, 0x13,
			0xc7, 0x07, 0xb0, 0xed, 0xa4,
			0xab, 0x84, 0x43, 0x45, 0xf2,
			0xc4, 0x49, 0x87, 0xd6, 0xf9,
			0xeb, 0xcc, 0x77, 0x7e, 0xfd,
			0x40, 0x78, 0xcc, 0x0f, 0x4c,
			0x10, 0x8d, 0xef, 0x95, 0x9f,
			0x78, 0xf1, 0xed, 0xb2, 0x76,
			0x54, 0x25, 0x78, 0x5f, 0xcd,
			0x17, 0xd5, 0x12, 0xbe, 0x31,
			0xee, 0xb6, 0x6b, 0xef, 0xf1,
			0xe8, 0xfc, 0x27, 0x47, 0x07,
			0x54, 0x54, 0x68, 0x69, 0x73,
			0x20, 0x69, 0x73, 0x20, 0x74,
			0x68, 0x65, 0x20, 0x63, 0x6f,
			0x6e, 0x74, 0x65, 0x6e, 0x74,
			0x2e, 0x58, 0x20, 0x2b, 0xdc,
			0xc8, 0x9f, 0x05, 0x82, 0x16,
			0xb8, 0xa2, 0x08, 0xdd, 0xc6,
			0xd8, 0xb5, 0x4a, 0xa9, 0x1f,
			0x48, 0xbd, 0x63, 0x48, 0x49,
			0x86, 0x56, 0x51, 0x05, 0xc9,
			0xad, 0x5a, 0x66, 0x82, 0xf6,
			0x81, 0x83, 0x40, 0xa2, 0x01,
			0x25, 0x04, 0x4a, 0x6f, 0x75,
			0x72, 0x2d, 0x73, 0x65, 0x63,
			0x72, 0x65, 0x74, 0x40
	}, test99[]	= {
			0xd8, 0x61, 0x85, 0x43, 0xa1,
			0x01, 0x05, 0xa1, 0x07, 0x82,
			0x83, 0x43, 0xa1, 0x01, 0x27,
			0xa1, 0x04, 0x42, 0x31, 0x31,
			0x58, 0x40, 0xb4, 0x92, 0x4b,
			0x18, 0xeb, 0x4e, 0x04, 0x73,
			0x13, 0xc7, 0x07, 0xb0, 0xed,
			0xa4, 0xab, 0x84, 0x43, 0x45,
			0xf2, 0xc4, 0x49, 0x87, 0xd6,
			0xf9, 0xeb, 0xcc, 0x77, 0x7e,
			0xfd, 0x40, 0x78, 0xcc, 0x0f,
			0x4c, 0x10, 0x8d, 0xef, 0x95,
			0x9f, 0x78, 0xf1, 0xed, 0xb2,
			0x76, 0x54, 0x25, 0x78, 0x5f,
			0xcd, 0x17, 0xd5, 0x12, 0xbe,
			0x31, 0xee, 0xb6, 0x6b, 0xef,
			0xf1, 0xe8, 0xfc, 0x27, 0x47,
			0x07, 0x83, 0x43, 0xa1, 0x01,
			0x26, 0xa1, 0x04, 0x42, 0x31,
			0x31, 0x58, 0x40, 0x6a, 0xcd,
			0x94, 0xd3, 0xcc, 0xf7, 0x1d,
			0x19, 0x2e, 0x85, 0x28, 0x36,
			0x0b, 0xa7, 0xe3, 0x46, 0xda,
			0xc4, 0x64, 0xe9, 0xed, 0xca,
			0x4c, 0xfe, 0xb6, 0xce, 0xb6,
			0xbd, 0xe7, 0xba, 0xec, 0x9f,
			0xf2, 0x6c, 0xa6, 0xbd, 0xf7,
			0x3d, 0x0b, 0xe4, 0x1e, 0x36,
			0x12, 0x9d, 0xcf, 0xf7, 0x51,
			0xdd, 0x2b, 0x5a, 0xd5, 0xce,
			0x11, 0x6e, 0x8a, 0x96, 0x3a,
			0x27, 0x38, 0xa2, 0x99, 0x47,
			0x7a, 0x68, 0x54, 0x54, 0x68,
			0x69, 0x73, 0x20, 0x69, 0x73,
			0x20, 0x74, 0x68, 0x65, 0x20,
			0x63, 0x6f, 0x6e, 0x74, 0x65,
			0x6e, 0x74, 0x2e, 0x58, 0x20,
			0x2b, 0xdc, 0xc8, 0x9f, 0x05,
			0x82, 0x16, 0xb8, 0xa2, 0x08,
			0xdd, 0xc6, 0xd8, 0xb5, 0x4a,
			0xa9, 0x1f, 0x48, 0xbd, 0x63,
			0x48, 0x49, 0x86, 0x56, 0x51,
			0x05, 0xc9, 0xad, 0x5a, 0x66,
			0x82, 0xf6, 0x81, 0x83, 0x40,
			0xa2, 0x01, 0x25, 0x04, 0x4a,
			0x6f, 0x75, 0x72, 0x2d, 0x73,
			0x65, 0x63, 0x72, 0x65, 0x74,
			0x40
	}, test100[]	= {
			0xd1, 0x84, 0x43, 0xa1, 0x01,
			0x05, 0xa1, 0x07, 0x83, 0x43,
			0xa1, 0x01, 0x27, 0xa1, 0x04,
			0x42, 0x31, 0x31, 0x58, 0x40,
			0xb4, 0x92, 0x4b, 0x18, 0xeb,
			0x4e, 0x04, 0x73, 0x13, 0xc7,
			0x07, 0xb0, 0xed, 0xa4, 0xab,
			0x84, 0x43, 0x45, 0xf2, 0xc4,
			0x49, 0x87, 0xd6, 0xf9, 0xeb,
			0xcc, 0x77, 0x7e, 0xfd, 0x40,
			0x78, 0xcc, 0x0f, 0x4c, 0x10,
			0x8d, 0xef, 0x95, 0x9f, 0x78,
			0xf1, 0xed, 0xb2, 0x76, 0x54,
			0x25, 0x78, 0x5f, 0xcd, 0x17,
			0xd5, 0x12, 0xbe, 0x31, 0xee,
			0xb6, 0x6b, 0xef, 0xf1, 0xe8,
			0xfc, 0x27, 0x47, 0x07, 0x54,
			0x54, 0x68, 0x69, 0x73, 0x20,
			0x69, 0x73, 0x20, 0x74, 0x68,
			0x65, 0x20, 0x63, 0x6f, 0x6e,
			0x74, 0x65, 0x6e, 0x74, 0x2e,
			0x58, 0x20, 0xa1, 0xa8, 0x48,
			0xd3, 0x47, 0x1f, 0x9d, 0x61,
			0xee, 0x49, 0x01, 0x8d, 0x24,
			0x4c, 0x82, 0x47, 0x72, 0xf2,
			0x23, 0xad, 0x4f, 0x93, 0x52,
			0x93, 0xf1, 0x78, 0x9f, 0xc3,
			0xa0, 0x8d, 0x8c, 0x58
	}, test101[]	= { /* mac-02 */
			0xd8, 0x61, 0x85, 0x43, 0xa1,
			0x01, 0x05, 0xa1, 0x07, 0x82,
			0x83, 0x43, 0xa1, 0x01, 0x27,
			0xa1, 0x04, 0x42, 0x31, 0x31,
			0x58, 0x40, 0xb4, 0x92, 0x4b,
			0x18, 0xeb, 0x4e, 0x04, 0x73,
			0x13, 0xc7, 0x07, 0xb0, 0xed,
			0xa4, 0xab, 0x84, 0x43, 0x45,
			0xf2, 0xc4, 0x49, 0x87, 0xd6,
			0xf9, 0xeb, 0xcc, 0x77, 0x7e,
			0xfd, 0x40, 0x78, 0xcc, 0x0f,
			0x4c, 0x10, 0x8d, 0xef, 0x95,
			0x9f, 0x78, 0xf1, 0xed, 0xb2,
			0x76, 0x54, 0x25, 0x78, 0x5f,
			0xcd, 0x17, 0xd5, 0x12, 0xbe,
			0x31, 0xee, 0xb6, 0x6b, 0xef,
			0xf1, 0xe8, 0xfc, 0x27, 0x47,
			0x07, 0x83, 0x43, 0xa1, 0x01,
			0x26, 0xa1, 0x04, 0x42, 0x31,
			0x31, 0x58, 0x40, 0x6a, 0xcd,
			0x94, 0xd3, 0xcc, 0xf7, 0x1d,
			0x19, 0x2e, 0x85, 0x28, 0x36,
			0x0b, 0xa7, 0xe3, 0x46, 0xda,
			0xc4, 0x64, 0xe9, 0xed, 0xca,
			0x4c, 0xfe, 0xb6, 0xce, 0xb6,
			0xbd, 0xe7, 0xba, 0xec, 0x9f,
			0xf2, 0x6c, 0xa6, 0xbd, 0xf7,
			0x3d, 0x0b, 0xe4, 0x1e, 0x36,
			0x12, 0x9d, 0xcf, 0xf7, 0x51,
			0xdd, 0x2b, 0x5a, 0xd5, 0xce,
			0x11, 0x6e, 0x8a, 0x96, 0x3a,
			0x27, 0x38, 0xa2, 0x99, 0x47,
			0x7a, 0x68, 0x54, 0x54, 0x68,
			0x69, 0x73, 0x20, 0x69, 0x73,
			0x20, 0x74, 0x68, 0x65, 0x20,
			0x63, 0x6f, 0x6e, 0x74, 0x65,
			0x6e, 0x74, 0x2e, 0x58, 0x20,
			0x2b, 0xdc, 0xc8, 0x9f, 0x05,
			0x82, 0x16, 0xb8, 0xa2, 0x08,
			0xdd, 0xc6, 0xd8, 0xb5, 0x4a,
			0xa9, 0x1f, 0x48, 0xbd, 0x63,
			0x48, 0x49, 0x86, 0x56, 0x51,
			0x05, 0xc9, 0xad, 0x5a, 0x66,
			0x82, 0xf6, 0x81, 0x83, 0x40,
			0xa2, 0x01, 0x25, 0x04, 0x4a,
			0x6f, 0x75, 0x72, 0x2d, 0x73,
			0x65, 0x63, 0x72, 0x65, 0x74,
			0x40
	}, test102[] = { /* mac0-01 */
			0xd1, 0x84, 0x43, 0xa1, 0x01,
			0x05, 0xa1, 0x07, 0x83, 0x43,
			0xa1, 0x01, 0x27, 0xa1, 0x04,
			0x42, 0x31, 0x31, 0x58, 0x40,
			0xb4, 0x92, 0x4b, 0x18, 0xeb,
			0x4e, 0x04, 0x73, 0x13, 0xc7,
			0x07, 0xb0, 0xed, 0xa4, 0xab,
			0x84, 0x43, 0x45, 0xf2, 0xc4,
			0x49, 0x87, 0xd6, 0xf9, 0xeb,
			0xcc, 0x77, 0x7e, 0xfd, 0x40,
			0x78, 0xcc, 0x0f, 0x4c, 0x10,
			0x8d, 0xef, 0x95, 0x9f, 0x78,
			0xf1, 0xed, 0xb2, 0x76, 0x54,
			0x25, 0x78, 0x5f, 0xcd, 0x17,
			0xd5, 0x12, 0xbe, 0x31, 0xee,
			0xb6, 0x6b, 0xef, 0xf1, 0xe8,
			0xfc, 0x27, 0x47, 0x07, 0x54,
			0x54, 0x68, 0x69, 0x73, 0x20,
			0x69, 0x73, 0x20, 0x74, 0x68,
			0x65, 0x20, 0x63, 0x6f, 0x6e,
			0x74, 0x65, 0x6e, 0x74, 0x2e,
			0x58, 0x20, 0xa1, 0xa8, 0x48,
			0xd3, 0x47, 0x1f, 0x9d, 0x61,
			0xee, 0x49, 0x01, 0x8d, 0x24,
			0x4c, 0x82, 0x47, 0x72, 0xf2,
			0x23, 0xad, 0x4f, 0x93, 0x52,
			0x93, 0xf1, 0x78, 0x9f, 0xc3,
			0xa0, 0x8d, 0x8c, 0x58
	}, test103[] = { /* mac0-02 */
			0xd1, 0x84, 0x43, 0xa1, 0x01,
			0x05, 0xa1, 0x07, 0x82, 0x83,
			0x43, 0xa1, 0x01, 0x27, 0xa1,
			0x04, 0x42, 0x31, 0x31, 0x58,
			0x40, 0xb4, 0x92, 0x4b, 0x18,
			0xeb, 0x4e, 0x04, 0x73, 0x13,
			0xc7, 0x07, 0xb0, 0xed, 0xa4,
			0xab, 0x84, 0x43, 0x45, 0xf2,
			0xc4, 0x49, 0x87, 0xd6, 0xf9,
			0xeb, 0xcc, 0x77, 0x7e, 0xfd,
			0x40, 0x78, 0xcc, 0x0f, 0x4c,
			0x10, 0x8d, 0xef, 0x95, 0x9f,
			0x78, 0xf1, 0xed, 0xb2, 0x76,
			0x54, 0x25, 0x78, 0x5f, 0xcd,
			0x17, 0xd5, 0x12, 0xbe, 0x31,
			0xee, 0xb6, 0x6b, 0xef, 0xf1,
			0xe8, 0xfc, 0x27, 0x47, 0x07,
			0x83, 0x43, 0xa1, 0x01, 0x26,
			0xa1, 0x04, 0x42, 0x31, 0x31,
			0x58, 0x40, 0x6a, 0xcd, 0x94,
			0xd3, 0xcc, 0xf7, 0x1d, 0x19,
			0x2e, 0x85, 0x28, 0x36, 0x0b,
			0xa7, 0xe3, 0x46, 0xda, 0xc4,
			0x64, 0xe9, 0xed, 0xca, 0x4c,
			0xfe, 0xb6, 0xce, 0xb6, 0xbd,
			0xe7, 0xba, 0xec, 0x9f, 0xf2,
			0x6c, 0xa6, 0xbd, 0xf7, 0x3d,
			0x0b, 0xe4, 0x1e, 0x36, 0x12,
			0x9d, 0xcf, 0xf7, 0x51, 0xdd,
			0x2b, 0x5a, 0xd5, 0xce, 0x11,
			0x6e, 0x8a, 0x96, 0x3a, 0x27,
			0x38, 0xa2, 0x99, 0x47, 0x7a,
			0x68, 0x54, 0x54, 0x68, 0x69,
			0x73, 0x20, 0x69, 0x73, 0x20,
			0x74, 0x68, 0x65, 0x20, 0x63,
			0x6f, 0x6e, 0x74, 0x65, 0x6e,
			0x74, 0x2e, 0x58, 0x20, 0xa1,
			0xa8, 0x48, 0xd3, 0x47, 0x1f,
			0x9d, 0x61, 0xee, 0x49, 0x01,
			0x8d, 0x24, 0x4c, 0x82, 0x47,
			0x72, 0xf2, 0x23, 0xad, 0x4f,
			0x93, 0x52, 0x93, 0xf1, 0x78,
			0x9f, 0xc3, 0xa0, 0x8d, 0x8c,
			0x58
	}, test104[] = { /* signed-01 */
			0xd8, 0x62, 0x84, 0x43, 0xa1,
			0x03, 0x00, 0xa0, 0x54, 0x54,
			0x68, 0x69, 0x73, 0x20, 0x69,
			0x73, 0x20, 0x74, 0x68, 0x65,
			0x20, 0x63, 0x6f, 0x6e, 0x74,
			0x65, 0x6e, 0x74, 0x2e, 0x81,
			0x83, 0x43, 0xa1, 0x01, 0x27,
			0xa2, 0x07, 0x83, 0x43, 0xa1,
			0x01, 0x27, 0xa1, 0x04, 0x42,
			0x31, 0x31, 0x58, 0x40, 0x8e,
			0x1b, 0xe2, 0xf9, 0x45, 0x3d,
			0x26, 0x48, 0x12, 0xe5, 0x90,
			0x49, 0x91, 0x32, 0xbe, 0xf3,
			0xfb, 0xf9, 0xee, 0x9d, 0xb2,
			0x7c, 0x2c, 0x16, 0x87, 0x88,
			0xe3, 0xb7, 0xeb, 0xe5, 0x06,
			0xc0, 0x4f, 0xd3, 0xd1, 0x9f,
			0xaa, 0x9f, 0x51, 0x23, 0x2a,
			0xf5, 0xc9, 0x59, 0xe4, 0xef,
			0x47, 0x92, 0x88, 0x34, 0x64,
			0x7f, 0x56, 0xdf, 0xbe, 0x93,
			0x91, 0x12, 0x88, 0x4d, 0x08,
			0xef, 0x25, 0x05, 0x04, 0x42,
			0x31, 0x31, 0x58, 0x40, 0x77,
			0xf3, 0xea, 0xcd, 0x11, 0x85,
			0x2c, 0x4b, 0xf9, 0xcb, 0x1d,
			0x72, 0xfa, 0xbe, 0x6b, 0x26,
			0xfb, 0xa1, 0xd7, 0x60, 0x92,
			0xb2, 0xb5, 0xb7, 0xec, 0x83,
			0xb8, 0x35, 0x57, 0x65, 0x22,
			0x64, 0xe6, 0x96, 0x90, 0xdb,
			0xc1, 0x17, 0x2d, 0xdc, 0x0b,
			0xf8, 0x84, 0x11, 0xc0, 0xd2,
			0x5a, 0x50, 0x7f, 0xdb, 0x24,
			0x7a, 0x20, 0xc4, 0x0d, 0x5e,
			0x24, 0x5f, 0xab, 0xd3, 0xfc,
			0x9e, 0xc1, 0x06
	}, test105[] = { /* signed-02 */
			0xd8, 0x62, 0x84, 0x43, 0xa1,
			0x03, 0x00, 0xa0, 0x54, 0x54,
			0x68, 0x69, 0x73, 0x20, 0x69,
			0x73, 0x20, 0x74, 0x68, 0x65,
			0x20, 0x63, 0x6f, 0x6e, 0x74,
			0x65, 0x6e, 0x74, 0x2e, 0x81,
			0x83, 0x43, 0xa1, 0x01, 0x27,
			0xa2, 0x07, 0x82, 0x83, 0x43,
			0xa1, 0x01, 0x27, 0xa1, 0x04,
			0x42, 0x31, 0x31, 0x58, 0x40,
			0x8e, 0x1b, 0xe2, 0xf9, 0x45,
			0x3d, 0x26, 0x48, 0x12, 0xe5,
			0x90, 0x49, 0x91, 0x32, 0xbe,
			0xf3, 0xfb, 0xf9, 0xee, 0x9d,
			0xb2, 0x7c, 0x2c, 0x16, 0x87,
			0x88, 0xe3, 0xb7, 0xeb, 0xe5,
			0x06, 0xc0, 0x4f, 0xd3, 0xd1,
			0x9f, 0xaa, 0x9f, 0x51, 0x23,
			0x2a, 0xf5, 0xc9, 0x59, 0xe4,
			0xef, 0x47, 0x92, 0x88, 0x34,
			0x64, 0x7f, 0x56, 0xdf, 0xbe,
			0x93, 0x91, 0x12, 0x88, 0x4d,
			0x08, 0xef, 0x25, 0x05, 0x83,
			0x43, 0xa1, 0x01, 0x26, 0xa1,
			0x04, 0x42, 0x31, 0x31, 0x58,
			0x40, 0xaf, 0x04, 0x9b, 0x80,
			0xd5, 0x2c, 0x36, 0x69, 0xb2,
			0x99, 0x70, 0xc1, 0x33, 0x54,
			0x37, 0x54, 0xf9, 0xcc, 0x60,
			0x8c, 0xe4, 0x11, 0x23, 0xae,
			0x1c, 0x82, 0x7e, 0x36, 0xb3,
			0x8c, 0xb8, 0x25, 0x98, 0x7f,
			0x01, 0xf2, 0x2b, 0xb8, 0xab,
			0x13, 0xe9, 0xc6, 0x62, 0x26,
			0xee, 0x23, 0x17, 0x8f, 0xfa,
			0x00, 0xa4, 0xfc, 0x22, 0x05,
			0x93, 0xb6, 0xe5, 0xac, 0x38,
			0x96, 0x00, 0x71, 0xc9, 0xc8,
			0x04, 0x42, 0x31, 0x31, 0x58,
			0x40, 0x77, 0xf3, 0xea, 0xcd,
			0x11, 0x85, 0x2c, 0x4b, 0xf9,
			0xcb, 0x1d, 0x72, 0xfa, 0xbe,
			0x6b, 0x26, 0xfb, 0xa1, 0xd7,
			0x60, 0x92, 0xb2, 0xb5, 0xb7,
			0xec, 0x83, 0xb8, 0x35, 0x57,
			0x65, 0x22, 0x64, 0xe6, 0x96,
			0x90, 0xdb, 0xc1, 0x17, 0x2d,
			0xdc, 0x0b, 0xf8, 0x84, 0x11,
			0xc0, 0xd2, 0x5a, 0x50, 0x7f,
			0xdb, 0x24, 0x7a, 0x20, 0xc4,
			0x0d, 0x5e, 0x24, 0x5f, 0xab,
			0xd3, 0xfc, 0x9e, 0xc1, 0x06
	}, test106[] = { /* signed-03 */
			0xd8, 0x62, 0x84, 0x43, 0xa1,
			0x03, 0x00, 0xa1, 0x07, 0x83,
			0x43, 0xa1, 0x01, 0x27, 0xa1,
			0x04, 0x42, 0x31, 0x31, 0x58,
			0x40, 0xb7, 0xca, 0xcb, 0xa2,
			0x85, 0xc4, 0xcd, 0x3e, 0xd2,
			0xf0, 0x14, 0x6f, 0x41, 0x98,
			0x86, 0x14, 0x4c, 0xa6, 0x38,
			0xd0, 0x87, 0xde, 0x12, 0x3d,
			0x40, 0x01, 0x67, 0x30, 0x8a,
			0xce, 0xab, 0xc4, 0xb5, 0xe5,
			0xc6, 0xa4, 0x0c, 0x0d, 0xe0,
			0xb7, 0x11, 0x67, 0xa3, 0x91,
			0x75, 0xea, 0x56, 0xc1, 0xfe,
			0x96, 0xc8, 0x9e, 0x5e, 0x7d,
			0x30, 0xda, 0xf2, 0x43, 0x8a,
			0x45, 0x61, 0x59, 0xa2, 0x0a,
			0x54, 0x54, 0x68, 0x69, 0x73,
			0x20, 0x69, 0x73, 0x20, 0x74,
			0x68, 0x65, 0x20, 0x63, 0x6f,
			0x6e, 0x74, 0x65, 0x6e, 0x74,
			0x2e, 0x81, 0x83, 0x43, 0xa1,
			0x01, 0x27, 0xa1, 0x04, 0x42,
			0x31, 0x31, 0x58, 0x40, 0x77,
			0xf3, 0xea, 0xcd, 0x11, 0x85,
			0x2c, 0x4b, 0xf9, 0xcb, 0x1d,
			0x72, 0xfa, 0xbe, 0x6b, 0x26,
			0xfb, 0xa1, 0xd7, 0x60, 0x92,
			0xb2, 0xb5, 0xb7, 0xec, 0x83,
			0xb8, 0x35, 0x57, 0x65, 0x22,
			0x64, 0xe6, 0x96, 0x90, 0xdb,
			0xc1, 0x17, 0x2d, 0xdc, 0x0b,
			0xf8, 0x84, 0x11, 0xc0, 0xd2,
			0x5a, 0x50, 0x7f, 0xdb, 0x24,
			0x7a, 0x20, 0xc4, 0x0d, 0x5e,
			0x24, 0x5f, 0xab, 0xd3, 0xfc,
			0x9e, 0xc1, 0x06
	}, test107[] = { /* signed1-01 */
			0xd2, 0x84, 0x45, 0xa2, 0x01,
			0x27, 0x03, 0x00, 0xa2, 0x07,
			0x83, 0x43, 0xa1, 0x01, 0x27,
			0xa1, 0x04, 0x42, 0x31, 0x31,
			0x58, 0x40, 0x6d, 0xae, 0xd1,
			0x58, 0xaf, 0xe4, 0x03, 0x2e,
			0x8d, 0xd4, 0x77, 0xd3, 0xd2,
			0xb7, 0xf6, 0x67, 0xe7, 0x95,
			0x7a, 0xa8, 0x30, 0x2b, 0xb5,
			0xe5, 0x68, 0xb4, 0xdc, 0xbc,
			0xce, 0x3c, 0xf0, 0xed, 0x5a,
			0x90, 0xf8, 0x31, 0x35, 0x1c,
			0x85, 0xd6, 0x15, 0x5a, 0x42,
			0xa1, 0x7c, 0xa1, 0xf2, 0x5f,
			0x50, 0x1c, 0xc1, 0x3f, 0x67,
			0x10, 0x8a, 0xe5, 0x3b, 0xda,
			0x92, 0xdb, 0x88, 0x27, 0x2e,
			0x00, 0x04, 0x42, 0x31, 0x31,
			0x54, 0x54, 0x68, 0x69, 0x73,
			0x20, 0x69, 0x73, 0x20, 0x74,
			0x68, 0x65, 0x20, 0x63, 0x6f,
			0x6e, 0x74, 0x65, 0x6e, 0x74,
			0x2e, 0x58, 0x40, 0x71, 0x42,
			0xfd, 0x2f, 0xf9, 0x6d, 0x56,
			0xdb, 0x85, 0xbe, 0xe9, 0x05,
			0xa7, 0x6b, 0xa1, 0xd0, 0xb7,
			0x32, 0x1a, 0x95, 0xc8, 0xc4,
			0xd3, 0x60, 0x7c, 0x57, 0x81,
			0x93, 0x2b, 0x7a, 0xfb, 0x87,
			0x11, 0x49, 0x7d, 0xfa, 0x75,
			0x1b, 0xf4, 0x0b, 0x58, 0xb3,
			0xbc, 0xc3, 0x23, 0x00, 0xb1,
			0x48, 0x7f, 0x3d, 0xb3, 0x40,
			0x85, 0xee, 0xf0, 0x13, 0xbf,
			0x08, 0xf4, 0xa4, 0x4d, 0x6f,
			0xef, 0x0d
	}, test108[] = { /* signed1-02 */
			0xd2, 0x84, 0x45, 0xa2, 0x01,
			0x27, 0x03, 0x00, 0xa2, 0x07,
			0x82, 0x83, 0x43, 0xa1, 0x01,
			0x27, 0xa1, 0x04, 0x42, 0x31,
			0x31, 0x58, 0x40, 0x6d, 0xae,
			0xd1, 0x58, 0xaf, 0xe4, 0x03,
			0x2e, 0x8d, 0xd4, 0x77, 0xd3,
			0xd2, 0xb7, 0xf6, 0x67, 0xe7,
			0x95, 0x7a, 0xa8, 0x30, 0x2b,
			0xb5, 0xe5, 0x68, 0xb4, 0xdc,
			0xbc, 0xce, 0x3c, 0xf0, 0xed,
			0x5a, 0x90, 0xf8, 0x31, 0x35,
			0x1c, 0x85, 0xd6, 0x15, 0x5a,
			0x42, 0xa1, 0x7c, 0xa1, 0xf2,
			0x5f, 0x50, 0x1c, 0xc1, 0x3f,
			0x67, 0x10, 0x8a, 0xe5, 0x3b,
			0xda, 0x92, 0xdb, 0x88, 0x27,
			0x2e, 0x00, 0x83, 0x43, 0xa1,
			0x01, 0x26, 0xa1, 0x04, 0x42,
			0x31, 0x31, 0x58, 0x40, 0x93,
			0x48, 0x7d, 0x09, 0x25, 0x6a,
			0x3e, 0xf4, 0x96, 0x37, 0x19,
			0xba, 0x5c, 0xf1, 0x01, 0xac,
			0xe2, 0xfc, 0x13, 0xd6, 0x31,
			0x4b, 0x49, 0x58, 0x21, 0x71,
			0xff, 0xa4, 0xa1, 0x31, 0x4d,
			0xc9, 0x3e, 0x4a, 0x4a, 0xdf,
			0xa4, 0x2a, 0x79, 0xe3, 0x1b,
			0x35, 0xd7, 0x30, 0x43, 0x58,
			0x58, 0x5b, 0x41, 0x79, 0x96,
			0x78, 0xce, 0x00, 0xca, 0x47,
			0xc3, 0xe0, 0x23, 0x86, 0x39,
			0x23, 0xf8, 0xc8, 0x04, 0x42,
			0x31, 0x31, 0x54, 0x54, 0x68,
			0x69, 0x73, 0x20, 0x69, 0x73,
			0x20, 0x74, 0x68, 0x65, 0x20,
			0x63, 0x6f, 0x6e, 0x74, 0x65,
			0x6e, 0x74, 0x2e, 0x58, 0x40,
			0x71, 0x42, 0xfd, 0x2f, 0xf9,
			0x6d, 0x56, 0xdb, 0x85, 0xbe,
			0xe9, 0x05, 0xa7, 0x6b, 0xa1,
			0xd0, 0xb7, 0x32, 0x1a, 0x95,
			0xc8, 0xc4, 0xd3, 0x60, 0x7c,
			0x57, 0x81, 0x93, 0x2b, 0x7a,
			0xfb, 0x87, 0x11, 0x49, 0x7d,
			0xfa, 0x75, 0x1b, 0xf4, 0x0b,
			0x58, 0xb3, 0xbc, 0xc3, 0x23,
			0x00, 0xb1, 0x48, 0x7f, 0x3d,
			0xb3, 0x40, 0x85, 0xee, 0xf0,
			0x13, 0xbf, 0x08, 0xf4, 0xa4,
			0x4d, 0x6f, 0xef, 0x0d
	};
;

struct seq {
	char			reason;
	struct lecp_item	item;
	const uint8_t		*buf;
	size_t			buf_len;
};

static const uint8_t bm12[] = {
	0x01, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00
}, bm48[] = {
	0x32, 0x30, 0x31, 0x33,
	0x2D, 0x30, 0x33, 0x2D,
	0x32, 0x31, 0x54, 0x32,
	0x30, 0x3A, 0x30, 0x34,
	0x3A, 0x30, 0x30, 0x5A
}, bm51[] = {
	0x01, 0x02, 0x03, 0x04
}, bm52[] = {
	0x64, 0x49, 0x45, 0x54,
	0x46
}, bm53[] = {
	0x68, 0x74, 0x74, 0x70,
	0x3A, 0x2F, 0x2F, 0x77,
	0x77, 0x77, 0x2E, 0x65,
	0x78, 0x61, 0x6D, 0x70,
	0x6C, 0x65, 0x2E, 0x63,
	0x6F, 0x6D
}, bm57[] = {
	0x61
}, bm58[] = {
	0x49, 0x45, 0x54, 0x46
}, bm59[] = {
	0x22, 0x5C
}, bm60[] = {
	0xc3, 0xbc
}, bm61[] = {
	0xe6, 0xb0, 0xb4
}, bm62[] = {
	0xF0, 0x90, 0x85, 0x91
}, bm72a[] = {
	0x01, 0x02
}, bm72b[] = {
	0x03, 0x04, 0x05
}, bm83a[] = {
	0xa1, 0x01, 0x05
}, bm83b[] = {
	0x54, 0x68, 0x69, 0x73,
	0x20, 0x69, 0x73, 0x20,
	0x74, 0x68, 0x65, 0x20,
	0x63, 0x6F, 0x6E, 0x74,
	0x65, 0x6E, 0x74, 0x2E
}, bm83c[] = {
	0x2B, 0xDC, 0xC8, 0x9F,
	0x05, 0x82, 0x16, 0xB8,
	0xA2, 0x08, 0xDD, 0xC6,
	0xD8, 0xB5, 0x4A, 0xA9,
	0x1F, 0x48, 0xBD, 0x63,
	0x48, 0x49, 0x86, 0x56,
	0x51, 0x05, 0xC9, 0xAD,
	0x5A, 0x66, 0x82, 0xF6
}, bm83d[] = {
	0x6F, 0x75, 0x72, 0x2D,
	0x73, 0x65, 0x63, 0x72,
	0x65, 0x74
}, bm84a[] = {
	0xa1, 0x01, 0x06
}, bm84b[] = {
	0x54, 0x68, 0x69, 0x73,
	0x20, 0x69, 0x73, 0x20,
	0x74, 0x68, 0x65, 0x20,
	0x63, 0x6F, 0x6E, 0x74,
	0x65, 0x6E, 0x74, 0x2E
}, bm84c[] = {
	0xB3, 0x09, 0x7F, 0x70,
	0x00, 0x9A, 0x11, 0x50,
	0x74, 0x09, 0x59, 0x8A,
	0x83, 0xE1, 0x5B, 0xBB,
	0xBF, 0x19, 0x82, 0xDC,
	0xE2, 0x8E, 0x5A, 0xB6,
	0xD5, 0xA6, 0xAF, 0xF6,
	0x89, 0x7B, 0xD2, 0x4B,
	0xB8, 0xB7, 0x47, 0x96,
	0x22, 0xC9, 0x40, 0x1B,
	0x24, 0x09, 0x0D, 0x45,
	0x82, 0x06, 0xD5, 0x87
}, bm84d[] = {
	0x73, 0x65, 0x63, 0x2D,
	0x34, 0x38
}, bm85a[] = {
	0xa1, 0x01, 0x07
}, bm85b[] = {
	0x54, 0x68, 0x69, 0x73,
	0x20, 0x69, 0x73, 0x20,
	0x74, 0x68, 0x65, 0x20,
	0x63, 0x6F, 0x6E, 0x74,
	0x65, 0x6E, 0x74, 0x2E
}, bm85c[] = {
	0xCD, 0x28, 0xA6, 0xB3,
	0xCF, 0xBB, 0xBF, 0x21,
	0x48, 0x51, 0xB9, 0x06,
	0xE0, 0x50, 0x05, 0x6C,
	0xB4, 0x38, 0xA8, 0xB8,
	0x89, 0x05, 0xB8, 0xB7,
	0x46, 0x19, 0x77, 0x02,
	0x27, 0x11, 0xA9, 0xD8,
	0xAC, 0x5D, 0xBC, 0x54,
	0xE2, 0x9A, 0x56, 0xD9,
	0x26, 0x04, 0x6B, 0x40,
	0xFC, 0x26, 0x07, 0xC2,
	0x5B, 0x34, 0x44, 0x54,
	0xAA, 0x5F, 0x68, 0xDE,
	0x09, 0xA3, 0xE5, 0x25,
	0xD3, 0x86, 0x5A, 0x05
}, bm85d[] = {
	0x73, 0x65, 0x63, 0x2D,
	0x36, 0x34
}, bm86a[] = {
	0xa1, 0x01, 0x05
}, bm86b[] = {
	0x54, 0x68, 0x69, 0x73,
	0x20, 0x69, 0x73, 0x20,
	0x74, 0x68, 0x65, 0x20,
	0x63, 0x6F, 0x6E, 0x74,
	0x65, 0x6E, 0x74, 0x2E
}, bm86c[] = {
	0x2B, 0xDC, 0xC8, 0x9F,
	0x05, 0x82, 0x16, 0xB8,
	0xA2, 0x08, 0xDD, 0xC6,
	0xD8, 0xB5, 0x4A, 0xA9,
	0x1F, 0x48, 0xBD, 0x63,
	0x48, 0x49, 0x86, 0x56,
	0x51, 0x05, 0xC9, 0xAD,
	0x5A, 0x66, 0x82, 0xF7
}, bm86d[] = {
	0x6F, 0x75, 0x72, 0x2D,
	0x73, 0x65, 0x63, 0x72,
	0x65, 0x74
}, bm87a[] = {
	0xa1, 0x01, 0x04
}, bm87b[] = {
	0x54, 0x68, 0x69, 0x73,
	0x20, 0x69, 0x73, 0x20,
	0x74, 0x68, 0x65, 0x20,
	0x63, 0x6F, 0x6E, 0x74,
	0x65, 0x6E, 0x74, 0x2E
}, bm87c[] = {
	0x6F, 0x35, 0xCA, 0xB7,
	0x79, 0xF7, 0x78, 0x33
}, bm87d[] = {
	0x6F, 0x75, 0x72, 0x2D,
	0x73, 0x65, 0x63, 0x72,
	0x65, 0x74
}, bm88a[] = {
	0xa1, 0x01, 0x05
}, bm88b[] = {
	0x54, 0x68, 0x69, 0x73,
	0x20, 0x69, 0x73, 0x20,
	0x74, 0x68, 0x65, 0x20,
	0x63, 0x6F, 0x6E, 0x74,
	0x65, 0x6E, 0x74, 0x2E
}, bm88c[] = {
	0xA1, 0xA8, 0x48, 0xD3,
	0x47, 0x1F, 0x9D, 0x61,
	0xEE, 0x49, 0x01, 0x8D,
	0x24, 0x4C, 0x82, 0x47,
	0x72, 0xF2, 0x23, 0xAD,
	0x4F, 0x93, 0x52, 0x93,
	0xF1, 0x78, 0x9F, 0xC3,
	0xA0, 0x8D, 0x8C, 0x58
}, bm89a[] = {
	0xa1, 0x01, 0x06
}, bm89b[] = {
	0x54, 0x68, 0x69, 0x73,
	0x20, 0x69, 0x73, 0x20,
	0x74, 0x68, 0x65, 0x20,
	0x63, 0x6F, 0x6E, 0x74,
	0x65, 0x6E, 0x74, 0x2E
}, bm89c[] = {
	0x99, 0x8D, 0x26, 0xC6,
	0x45, 0x9A, 0xAE, 0xEC,
	0xF4, 0x4E, 0xD2, 0x0C,
	0xE0, 0x0C, 0x8C, 0xCE,
	0xDF, 0x0A, 0x1F, 0x3D,
	0x22, 0xA9, 0x2F, 0xC0,
	0x5D, 0xB0, 0x8C, 0x5A,
	0xEB, 0x1C, 0xB5, 0x94,
	0xCA, 0xAF, 0x5A, 0x5C,
	0x5E, 0x2E, 0x9D, 0x01,
	0xCC, 0xE7, 0xE7, 0x7A,
	0x93, 0xAA, 0x8C, 0x62
}, bm90a[] = {
	0xa1, 0x01, 0x07
}, bm90b[] = {
	0x54, 0x68, 0x69, 0x73,
	0x20, 0x69, 0x73, 0x20,
	0x74, 0x68, 0x65, 0x20,
	0x63, 0x6F, 0x6E, 0x74,
	0x65, 0x6E, 0x74, 0x2E
}, bm90c[] = {
	0x4A, 0x55, 0x5B, 0xF9,
	0x71, 0xF7, 0xC1, 0x89,
	0x1D, 0x9D, 0xDF, 0x30,
	0x4A, 0x1A, 0x13, 0x2E,
	0x2D, 0x6F, 0x81, 0x74,
	0x49, 0x47, 0x4D, 0x81,
	0x3E, 0x6D, 0x04, 0xD6,
	0x59, 0x62, 0xBE, 0xD8,
	0xBB, 0xA7, 0x0C, 0x17,
	0xE1, 0xF5, 0x30, 0x8F,
	0xA3, 0x99, 0x62, 0x95,
	0x9A, 0x4B, 0x9B, 0x8D,
	0x7D, 0xA8, 0xE6, 0xD8,
	0x49, 0xB2, 0x09, 0xDC,
	0xD3, 0xE9, 0x8C, 0xC0,
	0xF1, 0x1E, 0xDD, 0xF2
}, bm91a[] = {
	0xa1, 0x01, 0x05
}, bm91b[] = {
	0x54, 0x68, 0x69, 0x73,
	0x20, 0x69, 0x73, 0x20,
	0x74, 0x68, 0x65, 0x20,
	0x63, 0x6F, 0x6E, 0x74,
	0x65, 0x6E, 0x74, 0x2E
}, bm91c[] = {
	0xA1, 0xA8, 0x48, 0xD3,
	0x47, 0x1F, 0x9D, 0x61,
	0xEE, 0x49, 0x01, 0x8D,
	0x24, 0x4C, 0x82, 0x47,
	0x72, 0xF2, 0x23, 0xAD,
	0x4F, 0x93, 0x52, 0x93,
	0xF1, 0x78, 0x9F, 0xC3,
	0xA0, 0x8D, 0x8C, 0x59
}, bm92a[] = {
	0xa1, 0x01, 0x04
}, bm92b[] = {
	0x54, 0x68, 0x69, 0x73,
	0x20, 0x69, 0x73, 0x20,
	0x74, 0x68, 0x65, 0x20,
	0x63, 0x6F, 0x6E, 0x74,
	0x65, 0x6E, 0x74, 0x2E
}, bm92c[] = {
	0x11, 0xF9, 0xE3, 0x57,
	0x97, 0x5F, 0xB8, 0x49
}, bm93a[] = {
	0xa1, 0x01, 0x01
}, bm93b[] = {
	0x02, 0xd1, 0xf7, 0xe6, 0xf2,
	0x6c, 0x43, 0xd4, 0x86, 0x8d,
	0x87, 0xce
}, bm93c[] = {
	0xa1, 0x01, 0x27
}, bm93d[] = {
	0x31, 0x31
}, bm93e[] = {
	0xe1, 0x04, 0x39, 0x15, 0x4c,
	0xc7, 0x5c, 0x7a, 0x3a, 0x53,
	0x91, 0x49, 0x1f, 0x88, 0x65,
	0x1e, 0x02, 0x92, 0xfd, 0x0f,
	0xe0, 0xe0, 0x2c, 0xf7, 0x40,
	0x54, 0x7e, 0xaf, 0x66, 0x77,
	0xb4, 0xa4, 0x04, 0x0b, 0x8e,
	0xca, 0x16, 0xdb, 0x59, 0x28,
	0x81, 0x26, 0x2f, 0x77, 0xb1,
	0x4c, 0x1a, 0x08, 0x6c, 0x02,
	0x26, 0x8b, 0x17, 0x17, 0x1c,
	0xa1, 0x6b, 0xe4, 0xb8, 0x59,
	0x5f, 0x8c, 0x0a, 0x08
}, bm93f[] = {
	0x60, 0x97, 0x3a, 0x94, 0xbb,
	0x28, 0x98, 0x00, 0x9e, 0xe5,
	0x2e, 0xcf, 0xd9, 0xab, 0x1d,
	0xd2, 0x58, 0x67, 0x37, 0x4b,
	0x16, 0x2e, 0x2c, 0x03, 0x56,
	0x8b, 0x41, 0xf5, 0x7c, 0x3c,
	0xc1, 0x6f, 0x91, 0x66, 0x25,
	0x0a

}, bm94a[] = {
	0xa1, 0x01, 0x01
}, bm94b[] = {
	0x02, 0xd1, 0xf7, 0xe6, 0xf2,
	0x6c, 0x43, 0xd4, 0x86, 0x8d,
	0x87, 0xce
}, bm94c[] = {
	0xa1, 0x01, 0x27
}, bm94d[] = {
	0x31, 0x31
}, bm94e[] = {
	0xe1, 0x04, 0x39, 0x15, 0x4c,
	0xc7, 0x5c, 0x7a, 0x3a, 0x53,
	0x91, 0x49, 0x1f, 0x88, 0x65,
	0x1e, 0x02, 0x92, 0xfd, 0x0f,
	0xe0, 0xe0, 0x2c, 0xf7, 0x40,
	0x54, 0x7e, 0xaf, 0x66, 0x77,
	0xb4, 0xa4, 0x04, 0x0b, 0x8e,
	0xca, 0x16, 0xdb, 0x59, 0x28,
	0x81, 0x26, 0x2f, 0x77, 0xb1,
	0x4c, 0x1a, 0x08, 0x6c, 0x02,
	0x26, 0x8b, 0x17, 0x17, 0x1c,
	0xa1, 0x6b, 0xe4, 0xb8, 0x59,
	0x5f, 0x8c, 0x0a, 0x08
}, bm94f[] = {
	0xa1, 0x01, 0x26
}, bm94g[] = {
	0x31, 0x31
}, bm94h[] = {
	0xfc, 0xa9, 0x8e, 0xca, 0xc8,
	0x0b, 0x5f, 0xeb, 0x3a, 0xc7,
	0xc1, 0x08, 0xb2, 0xb7, 0x91,
	0x10, 0xde, 0x88, 0x86, 0x7b,
	0xc0, 0x42, 0x6f, 0xc8, 0x3c,
	0x53, 0xcc, 0xd6, 0x78, 0x96,
	0x94, 0xed, 0xc5, 0xfe, 0xe3,
	0xc4, 0x0d, 0xe8, 0xe7, 0xb4,
	0x4f, 0xe8, 0xaa, 0xd3, 0x67,
	0xe0, 0x95, 0xc8, 0xfc, 0x31,
	0xb7, 0x9e, 0xe6, 0x66, 0xdf,
	0x9c, 0xf9, 0x09, 0x06, 0xeb,
	0x43, 0x75, 0x6c, 0x73
}, bm94i[] = {
	0x60, 0x97, 0x3a, 0x94, 0xbb,
	0x28, 0x98, 0x00, 0x9e, 0xe5,
	0x2e, 0xcf, 0xd9, 0xab, 0x1d,
	0xd2, 0x58, 0x67, 0x37, 0x4b,
	0x16, 0x2e, 0x2c, 0x03, 0x56,
	0x8b, 0x41, 0xf5, 0x7c, 0x3c,
	0xc1, 0x6f, 0x91, 0x66, 0x25,
	0x0a

}, bm95a[] = {
	0xa1, 0x01, 0x01
}, bm95b[] = {
	0x02, 0xd1, 0xf7, 0xe6, 0xf2,
	0x6c, 0x43, 0xd4, 0x86, 0x8d,
	0x87, 0xce
}, bm95c[] = {
	0xa1, 0x01, 0x27
}, bm95d[] = {
	0x31, 0x31
}, bm95e[] = {
	0x9a, 0x8e, 0xed, 0xe3, 0xb3,
	0xcb, 0x83, 0x7b, 0xa0, 0x0d,
	0xf0, 0x8f, 0xa2, 0x1b, 0x12,
	0x8b, 0x2d, 0x6d, 0x91, 0x62,
	0xa4, 0x29, 0x0a, 0x58, 0x2d,
	0x9f, 0x19, 0xbd, 0x0f, 0xb5,
	0x02, 0xf0, 0xf9, 0x2b, 0x9b,
	0xf4, 0x53, 0xa4, 0x05, 0x40,
	0x1f, 0x8b, 0x70, 0x55, 0xef,
	0x4e, 0x95, 0x8d, 0xf7, 0xf4,
	0xfb, 0xd7, 0xcf, 0xb4, 0xa0,
	0xc9, 0x71, 0x60, 0xf9, 0x47,
	0x2b, 0x0a, 0xa1, 0x04
}, bm95f[] = {
	0x60, 0x97, 0x3a, 0x94, 0xbb,
	0x28, 0x98, 0x00, 0x9e, 0xe5,
	0x2e, 0xcf, 0xd9, 0xab, 0x1d,
	0xd2, 0x58, 0x67, 0x37, 0x4b,
	0x35, 0x81, 0xf2, 0xc8, 0x00,
	0x39, 0x82, 0x63, 0x50, 0xb9,
	0x7a, 0xe2, 0x30, 0x0E, 0x42,
	0xFC
}, bm95g[] = {
	0x6f, 0x75, 0x72, 0x2d, 0x73,
	0x65, 0x63, 0x72, 0x65, 0x74

}, bm96a[] = {
	0xa1, 0x01, 0x01
}, bm96b[] = {
	0x02, 0xd1, 0xf7, 0xe6, 0xf2,
	0x6c, 0x43, 0xd4, 0x86, 0x8d,
	0x87, 0xce
}, bm96c[] = {
	0xa1, 0x01, 0x27
}, bm96d[] = {
	0x31, 0x31
}, bm96e[] = {
	0x9a, 0x8e, 0xed, 0xe3, 0xb3,
	0xcb, 0x83, 0x7b, 0xa0, 0x0d,
	0xf0, 0x8f, 0xa2, 0x1b, 0x12,
	0x8b, 0x2d, 0x6d, 0x91, 0x62,
	0xa4, 0x29, 0x0a, 0x58, 0x2d,
	0x9f, 0x19, 0xbd, 0x0f, 0xb5,
	0x02, 0xf0, 0xf9, 0x2b, 0x9b,
	0xf4, 0x53, 0xa4, 0x05, 0x40,
	0x1f, 0x8b, 0x70, 0x55, 0xef,
	0x4e, 0x95, 0x8d, 0xf7, 0xf4,
	0xfb, 0xd7, 0xcf, 0xb4, 0xa0,
	0xc9, 0x71, 0x60, 0xf9, 0x47,
	0x2b, 0x0a, 0xa1, 0x04
}, bm96f[] = {
	0xa1, 0x01, 0x26
}, bm96g[] = {
	0x31, 0x31
}, bm96h[] = {
	0x24, 0x27, 0xcb, 0x37, 0x56,
	0x85, 0x0f, 0xbb, 0x79, 0x05,
	0x18, 0x07, 0xc8, 0xb2, 0x3d,
	0x2e, 0x6d, 0x16, 0xa3, 0x22,
	0x4f, 0x99, 0x01, 0xb4, 0x73,
	0x99, 0xcf, 0xc7, 0xe3, 0xfa,
	0xc4, 0xcc, 0x62, 0x1d, 0xbb,
	0xeb, 0x02, 0x02, 0xa6, 0xd8,
	0xbb, 0x25, 0x69, 0x5c, 0x9d,
	0xcc, 0x9c, 0x47, 0x49, 0x20,
	0xff, 0x57, 0x60, 0x6d, 0x76,
	0x4d, 0xea, 0x19, 0x2f, 0xc8,
	0x67, 0x41, 0x16, 0xf2
}, bm96i[] = {
	0x60, 0x97, 0x3a, 0x94, 0xbb,
	0x28, 0x98, 0x00, 0x9e, 0xe5,
	0x2e, 0xcf, 0xd9, 0xab, 0x1d,
	0xd2, 0x58, 0x67, 0x37, 0x4b,
	0x35, 0x81, 0xf2, 0xc8, 0x00,
	0x39, 0x82, 0x63, 0x50, 0xb9,
	0x7a, 0xe2, 0x30, 0x0e, 0x42,
	0xfc
}, bm96j[] = {
	0x6f, 0x75, 0x72, 0x2d, 0x73,
	0x65, 0x63, 0x72, 0x65, 0x74

}, bm97a[] = {
	0xa1, 0x01, 0x01
}, bm97b[] = {
	0x02, 0xd1, 0xf7, 0xe6, 0xf2,
	0x6c, 0x43, 0xd4, 0x86, 0x8d,
	0x87, 0xce
}, bm97c[] = {
	0x60, 0x97, 0x3a, 0x94, 0xbb,
	0x28, 0x98, 0x00, 0x9e, 0xe5,
	0x2e, 0xcf, 0xd9, 0xab, 0x1d,
	0xd2, 0x58, 0x67, 0x37, 0x4b,
	0x35, 0x81, 0xf2, 0xc8, 0x00,
	0x39, 0x82, 0x63, 0x50, 0xb9,
	0x7a, 0xe2, 0x30, 0x0e, 0x42,
	0xfc
}, bm97d[] = {
	0x6f, 0x75, 0x72, 0x2d, 0x73,
	0x65, 0x63, 0x72, 0x65, 0x74
}, bm97e[] = {
	0xa1, 0x01, 0x27
}, bm97f[] = {
	0x31, 0x31
}, bm97g[] = {
	0xcc, 0xb1, 0xf3, 0xfe, 0xdf,
	0xce, 0xa7, 0x2b, 0x9c, 0x86,
	0x79, 0x63, 0xe2, 0x52, 0xb6,
	0x65, 0x8a, 0xd0, 0x7f, 0x3f,
	0x5f, 0x15, 0xa3, 0x26, 0xa3,
	0xf5, 0x72, 0x54, 0xcc, 0xb8,
	0xd4, 0x8d, 0x60, 0x02, 0x1d,
	0x2f, 0x1f, 0x8a, 0x80, 0x3b,
	0x84, 0x4b, 0x78, 0x72, 0x16,
	0x6c, 0x6d, 0x45, 0x90, 0x25,
	0xd2, 0x1c, 0x8c, 0x84, 0x62,
	0xa2, 0x44, 0xba, 0x19, 0x60,
	0x4e, 0xc4, 0xd5, 0x0b

}, bm98a[] = {
	0xa1, 0x01, 0x05
}, bm98b[] = {
	0xa1, 0x01, 0x27
}, bm98c[] = {
	0x31, 0x31
}, bm98d[] = {
	0xb4, 0x92, 0x4b, 0x18, 0xeb,
	0x4e, 0x04, 0x73, 0x13, 0xc7,
	0x07, 0xb0, 0xed, 0xa4, 0xab,
	0x84, 0x43, 0x45, 0xf2, 0xc4,
	0x49, 0x87, 0xd6, 0xf9, 0xeb,
	0xcc, 0x77, 0x7e, 0xfd, 0x40,
	0x78, 0xcc, 0x0f, 0x4c, 0x10,
	0x8d, 0xef, 0x95, 0x9f, 0x78,
	0xf1, 0xed, 0xb2, 0x76, 0x54,
	0x25, 0x78, 0x5f, 0xcd, 0x17,
	0xd5, 0x12, 0xbe, 0x31, 0xee,
	0xb6, 0x6b, 0xef, 0xf1, 0xe8,
	0xfc, 0x27, 0x47, 0x07
}, bm98e[] = {
	0x54, 0x68, 0x69, 0x73, 0x20,
	0x69, 0x73, 0x20, 0x74, 0x68,
	0x65, 0x20, 0x63, 0x6f, 0x6e,
	0x74, 0x65, 0x6e, 0x74, 0x2e
}, bm98f[] = {
	0x2b, 0xdc, 0xc8, 0x9f, 0x05,
	0x82, 0x16, 0xb8, 0xa2, 0x08,
	0xdd, 0xc6, 0xd8, 0xb5, 0x4a,
	0xa9, 0x1f, 0x48, 0xbd, 0x63,
	0x48, 0x49, 0x86, 0x56, 0x51,
	0x05, 0xc9, 0xad, 0x5a, 0x66,
	0x82, 0xf6
}, bm98g[] = {
	0x6f, 0x75, 0x72, 0x2d, 0x73,
	0x65, 0x63, 0x72, 0x65, 0x74

}, bm99a[] = {
	0xa1, 0x01, 0x05
}, bm99b[] = {
	0xa1, 0x01, 0x27
}, bm99c[] = {
	0x31, 0x31
}, bm99d[] = {
	0xb4, 0x92, 0x4b, 0x18, 0xeb,
	0x4e, 0x04, 0x73, 0x13, 0xc7,
	0x07, 0xb0, 0xed, 0xa4, 0xab,
	0x84, 0x43, 0x45, 0xf2, 0xc4,
	0x49, 0x87, 0xd6, 0xf9, 0xeb,
	0xcc, 0x77, 0x7e, 0xfd, 0x40,
	0x78, 0xcc, 0x0f, 0x4c, 0x10,
	0x8d, 0xef, 0x95, 0x9f, 0x78,
	0xf1, 0xed, 0xb2, 0x76, 0x54,
	0x25, 0x78, 0x5f, 0xcd, 0x17,
	0xd5, 0x12, 0xbe, 0x31, 0xee,
	0xb6, 0x6b, 0xef, 0xf1, 0xe8,
	0xfc, 0x27, 0x47, 0x07
}, bm99e[] = {
	0xa1, 0x01, 0x26
}, bm99f[] = {
	0x31, 0x31
}, bm99g[] = {
	0x6a, 0xcd, 0x94, 0xd3, 0xcc,
	0xf7, 0x1d, 0x19, 0x2e, 0x85,
	0x28, 0x36, 0x0b, 0xa7, 0xe3,
	0x46, 0xda, 0xc4, 0x64, 0xe9,
	0xed, 0xca, 0x4c, 0xfe, 0xb6,
	0xce, 0xb6, 0xbd, 0xe7, 0xba,
	0xec, 0x9f, 0xf2, 0x6c, 0xa6,
	0xbd, 0xf7, 0x3d, 0x0b, 0xe4,
	0x1e, 0x36, 0x12, 0x9d, 0xcf,
	0xf7, 0x51, 0xdd, 0x2b, 0x5a,
	0xd5, 0xce, 0x11, 0x6e, 0x8a,
	0x96, 0x3a, 0x27, 0x38, 0xa2,
	0x99, 0x47, 0x7a, 0x68
}, bm99h[] = {
	0x54, 0x68, 0x69, 0x73, 0x20,
	0x69, 0x73, 0x20, 0x74, 0x68,
	0x65, 0x20, 0x63, 0x6f, 0x6e,
	0x74, 0x65, 0x6e, 0x74, 0x2e
}, bm99i[] = {
	0x2b, 0xdc, 0xc8, 0x9f, 0x05,
	0x82, 0x16, 0xb8, 0xa2, 0x08,
	0xdd, 0xc6, 0xd8, 0xb5, 0x4a,
	0xa9, 0x1f, 0x48, 0xbd, 0x63,
	0x48, 0x49, 0x86, 0x56, 0x51,
	0x05, 0xc9, 0xad, 0x5a, 0x66,
	0x82, 0xf6
}, bm99j[] = {
	0x6f, 0x75, 0x72, 0x2d, 0x73,
	0x65, 0x63, 0x72, 0x65, 0x74

}, bm100a[] = {
	0xa1, 0x01, 0x05
}, bm100b[] = {
	0xa1, 0x01, 0x27
}, bm100c[] = {
	0x31, 0x31
}, bm100d[] = {
	0xb4, 0x92, 0x4b, 0x18, 0xeb,
	0x4e, 0x04, 0x73, 0x13, 0xc7,
	0x07, 0xb0, 0xed, 0xa4, 0xab,
	0x84, 0x43, 0x45, 0xf2, 0xc4,
	0x49, 0x87, 0xd6, 0xf9, 0xeb,
	0xcc, 0x77, 0x7e, 0xfd, 0x40,
	0x78, 0xcc, 0x0f, 0x4c, 0x10,
	0x8d, 0xef, 0x95, 0x9f, 0x78,
	0xf1, 0xed, 0xb2, 0x76, 0x54,
	0x25, 0x78, 0x5f, 0xcd, 0x17,
	0xd5, 0x12, 0xbe, 0x31, 0xee,
	0xb6, 0x6b, 0xef, 0xf1, 0xe8,
	0xfc, 0x27, 0x47, 0x07
}, bm100e[] = {
	0x54, 0x68, 0x69, 0x73, 0x20,
	0x69, 0x73, 0x20, 0x74, 0x68,
	0x65, 0x20, 0x63, 0x6f, 0x6e,
	0x74, 0x65, 0x6e, 0x74, 0x2e
}, bm100f[] = {
	0xa1, 0xa8, 0x48, 0xd3, 0x47,
	0x1f, 0x9d, 0x61, 0xee, 0x49,
	0x01, 0x8d, 0x24, 0x4c, 0x82,
	0x47, 0x72, 0xf2, 0x23, 0xad,
	0x4f, 0x93, 0x52, 0x93, 0xf1,
	0x78, 0x9f, 0xc3, 0xa0, 0x8d,
	0x8c, 0x58


}, bm101a[] = {
	0xa1, 0x01, 0x05
}, bm101b[] = {
	0xa1, 0x01, 0x27
}, bm101c[] = {
	0x31, 0x31
}, bm101d[] = {
	0xb4, 0x92, 0x4b, 0x18, 0xeb,
	0x4e, 0x04, 0x73, 0x13, 0xc7,
	0x07, 0xb0, 0xed, 0xa4, 0xab,
	0x84, 0x43, 0x45, 0xf2, 0xc4,
	0x49, 0x87, 0xd6, 0xf9, 0xeb,
	0xcc, 0x77, 0x7e, 0xfd, 0x40,
	0x78, 0xcc, 0x0f, 0x4c, 0x10,
	0x8d, 0xef, 0x95, 0x9f, 0x78,
	0xf1, 0xed, 0xb2, 0x76, 0x54,
	0x25, 0x78, 0x5f, 0xcd, 0x17,
	0xd5, 0x12, 0xbe, 0x31, 0xee,
	0xb6, 0x6b, 0xef, 0xf1, 0xe8,
	0xfc, 0x27, 0x47, 0x07
}, bm101e[] = {
	0xa1, 0x01, 0x26
}, bm101f[] = {
	0x31, 0x31
}, bm101g[] = {
	0x6a, 0xcd, 0x94, 0xd3, 0xcc,
	0xf7, 0x1d, 0x19, 0x2e, 0x85,
	0x28, 0x36, 0x0b, 0xa7, 0xe3,
	0x46, 0xda, 0xc4, 0x64, 0xe9,
	0xed, 0xca, 0x4c, 0xfe, 0xb6,
	0xce, 0xb6, 0xbd, 0xe7, 0xba,
	0xec, 0x9f, 0xf2, 0x6c, 0xa6,
	0xbd, 0xf7, 0x3d, 0x0b, 0xe4,
	0x1e, 0x36, 0x12, 0x9d, 0xcf,
	0xf7, 0x51, 0xdd, 0x2b, 0x5a,
	0xd5, 0xce, 0x11, 0x6e, 0x8a,
	0x96, 0x3a, 0x27, 0x38, 0xa2,
	0x99, 0x47, 0x7a, 0x68
}, bm101h[] = {
	0x54, 0x68, 0x69, 0x73, 0x20,
	0x69, 0x73, 0x20, 0x74, 0x68,
	0x65, 0x20, 0x63, 0x6f, 0x6e,
	0x74, 0x65, 0x6e, 0x74, 0x2e
}, bm101i[] = {
	0x2b, 0xdc, 0xc8, 0x9f, 0x05,
	0x82, 0x16, 0xb8, 0xa2, 0x08,
	0xdd, 0xc6, 0xd8, 0xb5, 0x4a,
	0xa9, 0x1f, 0x48, 0xbd, 0x63,
	0x48, 0x49, 0x86, 0x56, 0x51,
	0x05, 0xc9, 0xad, 0x5a, 0x66,
	0x82, 0xf6
}, bm101j[] = {
	0x6f, 0x75, 0x72, 0x2d, 0x73,
	0x65, 0x63, 0x72, 0x65, 0x74

}, bm102a[] = { /* mac0-01 */
	0xa1, 0x01, 0x05
}, bm102b[] = {
	0xa1, 0x01, 0x27
}, bm102c[] = {
	0x31, 0x31
}, bm102d[] = {
	0xb4, 0x92, 0x4b, 0x18, 0xeb,
	0x4e, 0x04, 0x73, 0x13, 0xc7,
	0x07, 0xb0, 0xed, 0xa4, 0xab,
	0x84, 0x43, 0x45, 0xf2, 0xc4,
	0x49, 0x87, 0xd6, 0xf9, 0xeb,
	0xcc, 0x77, 0x7e, 0xfd, 0x40,
	0x78, 0xcc, 0x0f, 0x4c, 0x10,
	0x8d, 0xef, 0x95, 0x9f, 0x78,
	0xf1, 0xed, 0xb2, 0x76, 0x54,
	0x25, 0x78, 0x5f, 0xcd, 0x17,
	0xd5, 0x12, 0xbe, 0x31, 0xee,
	0xb6, 0x6b, 0xef, 0xf1, 0xe8,
	0xfc, 0x27, 0x47, 0x07
}, bm102e[] = {
	0x54, 0x68, 0x69, 0x73, 0x20,
	0x69, 0x73, 0x20, 0x74, 0x68,
	0x65, 0x20, 0x63, 0x6f, 0x6e,
	0x74, 0x65, 0x6e, 0x74, 0x2e
}, bm102f[] = {
	0xa1, 0xa8, 0x48, 0xd3, 0x47,
	0x1f, 0x9d, 0x61, 0xee, 0x49,
	0x01, 0x8d, 0x24, 0x4c, 0x82,
	0x47, 0x72, 0xf2, 0x23, 0xad,
	0x4f, 0x93, 0x52, 0x93, 0xf1,
	0x78, 0x9f, 0xc3, 0xa0, 0x8d,
	0x8c, 0x58

}, bm103a[] = {
	0xa1, 0x01, 0x05
}, bm103b[] = {
	0xa1, 0x01, 0x27
}, bm103c[] = {
	0x31, 0x31
}, bm103d[] = {
	0xb4, 0x92, 0x4b, 0x18, 0xeb,
	0x4e, 0x04, 0x73, 0x13, 0xc7,
	0x07, 0xb0, 0xed, 0xa4, 0xab,
	0x84, 0x43, 0x45, 0xf2, 0xc4,
	0x49, 0x87, 0xd6, 0xf9, 0xeb,
	0xcc, 0x77, 0x7e, 0xfd, 0x40,
	0x78, 0xcc, 0x0f, 0x4c, 0x10,
	0x8d, 0xef, 0x95, 0x9f, 0x78,
	0xf1, 0xed, 0xb2, 0x76, 0x54,
	0x25, 0x78, 0x5f, 0xcd, 0x17,
	0xd5, 0x12, 0xbe, 0x31, 0xee,
	0xb6, 0x6b, 0xef, 0xf1, 0xe8,
	0xfc, 0x27, 0x47, 0x07
}, bm103e[] = {
	0xa1, 0x01, 0x26
}, bm103f[] = {
	0x31, 0x31
}, bm103g[] = {
	0x6a, 0xcd, 0x94, 0xd3, 0xcc,
	0xf7, 0x1d, 0x19, 0x2e, 0x85,
	0x28, 0x36, 0x0b, 0xa7, 0xe3,
	0x46, 0xda, 0xc4, 0x64, 0xe9,
	0xed, 0xca, 0x4c, 0xfe, 0xb6,
	0xce, 0xb6, 0xbd, 0xe7, 0xba,
	0xec, 0x9f, 0xf2, 0x6c, 0xa6,
	0xbd, 0xf7, 0x3d, 0x0b, 0xe4,
	0x1e, 0x36, 0x12, 0x9d, 0xcf,
	0xf7, 0x51, 0xdd, 0x2b, 0x5a,
	0xd5, 0xce, 0x11, 0x6e, 0x8a,
	0x96, 0x3a, 0x27, 0x38, 0xa2,
	0x99, 0x47, 0x7a, 0x68
}, bm103h[] = {
	0x54, 0x68, 0x69, 0x73, 0x20,
	0x69, 0x73, 0x20, 0x74, 0x68,
	0x65, 0x20, 0x63, 0x6f, 0x6e,
	0x74, 0x65, 0x6e, 0x74, 0x2e
}, bm103i[] = {
	0xa1, 0xa8, 0x48, 0xd3, 0x47,
	0x1f, 0x9d, 0x61, 0xee, 0x49,
	0x01, 0x8d, 0x24, 0x4c, 0x82,
	0x47, 0x72, 0xf2, 0x23, 0xad,
	0x4f, 0x93, 0x52, 0x93, 0xf1,
	0x78, 0x9f, 0xc3, 0xa0, 0x8d,
	0x8c, 0x58

}, bm104a[] = {
	0xa1, 0x03, 0x00
}, bm104b[] = {
	0x54, 0x68, 0x69, 0x73, 0x20,
	0x69, 0x73, 0x20, 0x74, 0x68,
	0x65, 0x20, 0x63, 0x6f, 0x6e,
	0x74, 0x65, 0x6e, 0x74, 0x2e
}, bm104c[] = {
	0xa1, 0x01, 0x27
}, bm104d[] = {
	0xa1, 0x01, 0x27
}, bm104e[] = {
	0x31, 0x31
}, bm104f[] = {
	0x8e, 0x1b, 0xe2, 0xf9, 0x45,
	0x3d, 0x26, 0x48, 0x12, 0xe5,
	0x90, 0x49, 0x91, 0x32, 0xbe,
	0xf3, 0xfb, 0xf9, 0xee, 0x9d,
	0xb2, 0x7c, 0x2c, 0x16, 0x87,
	0x88, 0xe3, 0xb7, 0xeb, 0xe5,
	0x06, 0xc0, 0x4f, 0xd3, 0xd1,
	0x9f, 0xaa, 0x9f, 0x51, 0x23,
	0x2a, 0xf5, 0xc9, 0x59, 0xe4,
	0xef, 0x47, 0x92, 0x88, 0x34,
	0x64, 0x7f, 0x56, 0xdf, 0xbe,
	0x93, 0x91, 0x12, 0x88, 0x4d,
	0x08, 0xef, 0x25, 0x05
}, bm104g[] = {
	0x31, 0x31
}, bm104h[] = {
	0x77, 0xf3, 0xea, 0xcd, 0x11,
	0x85, 0x2c, 0x4b, 0xf9, 0xcb,
	0x1d, 0x72, 0xfa, 0xbe, 0x6b,
	0x26, 0xfb, 0xa1, 0xd7, 0x60,
	0x92, 0xb2, 0xb5, 0xb7, 0xec,
	0x83, 0xb8, 0x35, 0x57, 0x65,
	0x22, 0x64, 0xe6, 0x96, 0x90,
	0xdb, 0xc1, 0x17, 0x2d, 0xdc,
	0x0b, 0xf8, 0x84, 0x11, 0xc0,
	0xd2, 0x5a, 0x50, 0x7f, 0xdb,
	0x24, 0x7a, 0x20, 0xc4, 0x0d,
	0x5e, 0x24, 0x5f, 0xab, 0xd3,
	0xfc, 0x9e, 0xc1, 0x06

}, bm105a[] = {
	0xa1, 0x03, 0x00
}, bm105b[] = {
	0x54, 0x68, 0x69, 0x73, 0x20,
	0x69, 0x73, 0x20, 0x74, 0x68,
	0x65, 0x20, 0x63, 0x6f, 0x6e,
	0x74, 0x65, 0x6e, 0x74, 0x2e
}, bm105c[] = {
	0xa1, 0x01, 0x27
}, bm105d[] = {
	0xa1, 0x01, 0x27
}, bm105e[] = {
	0x31, 0x31
}, bm105f[] = {
		0x8e, 0x1b, 0xe2, 0xf9, 0x45,
		0x3d, 0x26, 0x48, 0x12, 0xe5,
		0x90, 0x49, 0x91, 0x32, 0xbe,
		0xf3, 0xfb, 0xf9, 0xee, 0x9d,
		0xb2, 0x7c, 0x2c, 0x16, 0x87,
		0x88, 0xe3, 0xb7, 0xeb, 0xe5,
		0x06, 0xc0, 0x4f, 0xd3, 0xd1,
		0x9f, 0xaa, 0x9f, 0x51, 0x23,
		0x2a, 0xf5, 0xc9, 0x59, 0xe4,
		0xef, 0x47, 0x92, 0x88, 0x34,
		0x64, 0x7f, 0x56, 0xdf, 0xbe,
		0x93, 0x91, 0x12, 0x88, 0x4d,
		0x08, 0xef, 0x25, 0x05
}, bm105g[] = {
	0xa1, 0x01, 0x26
}, bm105h[] = {
	0x31, 0x31
}, bm105i[] = {
		0xaf, 0x04, 0x9b, 0x80, 0xd5,
		0x2c, 0x36, 0x69, 0xb2, 0x99,
		0x70, 0xc1, 0x33, 0x54, 0x37,
		0x54, 0xf9, 0xcc, 0x60, 0x8c,
		0xe4, 0x11, 0x23, 0xae, 0x1c,
		0x82, 0x7e, 0x36, 0xb3, 0x8c,
		0xb8, 0x25, 0x98, 0x7f, 0x01,
		0xf2, 0x2b, 0xb8, 0xab, 0x13,
		0xe9, 0xc6, 0x62, 0x26, 0xee,
		0x23, 0x17, 0x8f, 0xfa, 0x00,
		0xa4, 0xfc, 0x22, 0x05, 0x93,
		0xb6, 0xe5, 0xac, 0x38, 0x96,
		0x00, 0x71, 0xc9, 0xc8
}, bm105j[] = {
	0x31, 0x31
}, bm105k[] = {
		0x77, 0xf3, 0xea, 0xcd, 0x11,
		0x85, 0x2c, 0x4b, 0xf9, 0xcb,
		0x1d, 0x72, 0xfa, 0xbe, 0x6b,
		0x26, 0xfb, 0xa1, 0xd7, 0x60,
		0x92, 0xb2, 0xb5, 0xb7, 0xec,
		0x83, 0xb8, 0x35, 0x57, 0x65,
		0x22, 0x64, 0xe6, 0x96, 0x90,
		0xdb, 0xc1, 0x17, 0x2d, 0xdc,
		0x0b, 0xf8, 0x84, 0x11, 0xc0,
		0xd2, 0x5a, 0x50, 0x7f, 0xdb,
		0x24, 0x7a, 0x20, 0xc4, 0x0d,
		0x5e, 0x24, 0x5f, 0xab, 0xd3,
		0xfc, 0x9e, 0xc1, 0x06

}, bm106a[] = {
	0xa1, 0x03, 0x00
}, bm106b[] = {
	0xa1, 0x01, 0x27
}, bm106c[] = {
	0x31, 0x31
}, bm106d[] = {
		0xb7, 0xca, 0xcb, 0xa2, 0x85,
		0xc4, 0xcd, 0x3e, 0xd2, 0xf0,
		0x14, 0x6f, 0x41, 0x98, 0x86,
		0x14, 0x4c, 0xa6, 0x38, 0xd0,
		0x87, 0xde, 0x12, 0x3d, 0x40,
		0x01, 0x67, 0x30, 0x8a, 0xce,
		0xab, 0xc4, 0xb5, 0xe5, 0xc6,
		0xa4, 0x0c, 0x0d, 0xe0, 0xb7,
		0x11, 0x67, 0xa3, 0x91, 0x75,
		0xea, 0x56, 0xc1, 0xfe, 0x96,
		0xc8, 0x9e, 0x5e, 0x7d, 0x30,
		0xda, 0xf2, 0x43, 0x8a, 0x45,
		0x61, 0x59, 0xa2, 0x0a
}, bm106e[] = {
	0x54, 0x68, 0x69, 0x73, 0x20,
	0x69, 0x73, 0x20, 0x74, 0x68,
	0x65, 0x20, 0x63, 0x6f, 0x6e,
	0x74, 0x65, 0x6e, 0x74, 0x2e
}, bm106f[] = {
	0xa1, 0x01, 0x27
}, bm106g[] = {
	0x31, 0x31
}, bm106h[] = {
		0x77, 0xf3, 0xea, 0xcd, 0x11,
		0x85, 0x2c, 0x4b, 0xf9, 0xcb,
		0x1d, 0x72, 0xfa, 0xbe, 0x6b,
		0x26, 0xfb, 0xa1, 0xd7, 0x60,
		0x92, 0xb2, 0xb5, 0xb7, 0xec,
		0x83, 0xb8, 0x35, 0x57, 0x65,
		0x22, 0x64, 0xe6, 0x96, 0x90,
		0xdb, 0xc1, 0x17, 0x2d, 0xdc,
		0x0b, 0xf8, 0x84, 0x11, 0xc0,
		0xd2, 0x5a, 0x50, 0x7f, 0xdb,
		0x24, 0x7a, 0x20, 0xc4, 0x0d,
		0x5e, 0x24, 0x5f, 0xab, 0xd3,
		0xfc, 0x9e, 0xc1, 0x06

}, bm107a[] = {
	0xa2, 0x01, 0x27, 0x03, 0x00
}, bm107b[] = {
	0xa1, 0x01, 0x27,
}, bm107c[] = {
	0x31, 0x31
}, bm107d[] = {
	0x6d, 0xae, 0xd1, 0x58, 0xaf,
	0xe4, 0x03, 0x2e, 0x8d, 0xd4,
	0x77, 0xd3, 0xd2, 0xb7, 0xf6,
	0x67, 0xe7, 0x95, 0x7a, 0xa8,
	0x30, 0x2b, 0xb5, 0xe5, 0x68,
	0xb4, 0xdc, 0xbc, 0xce, 0x3c,
	0xf0, 0xed, 0x5a, 0x90, 0xf8,
	0x31, 0x35, 0x1c, 0x85, 0xd6,
	0x15, 0x5a, 0x42, 0xa1, 0x7c,
	0xa1, 0xf2, 0x5f, 0x50, 0x1c,
	0xc1, 0x3f, 0x67, 0x10, 0x8a,
	0xe5, 0x3b, 0xda, 0x92, 0xdb,
	0x88, 0x27, 0x2e, 0x00
}, bm107e[] = {
	0x31, 0x31
}, bm107f[] = {
	0x54, 0x68, 0x69, 0x73, 0x20,
	0x69, 0x73, 0x20, 0x74, 0x68,
	0x65, 0x20, 0x63, 0x6f, 0x6e,
	0x74, 0x65, 0x6e, 0x74, 0x2e
}, bm107g[] = {
	0x71, 0x42, 0xfd, 0x2f, 0xf9,
	0x6d, 0x56, 0xdb, 0x85, 0xbe,
	0xe9, 0x05, 0xa7, 0x6b, 0xa1,
	0xd0, 0xb7, 0x32, 0x1a, 0x95,
	0xc8, 0xc4, 0xd3, 0x60, 0x7c,
	0x57, 0x81, 0x93, 0x2b, 0x7a,
	0xfb, 0x87, 0x11, 0x49, 0x7d,
	0xfa, 0x75, 0x1b, 0xf4, 0x0b,
	0x58, 0xb3, 0xbc, 0xc3, 0x23,
	0x00, 0xb1, 0x48, 0x7f, 0x3d,
	0xb3, 0x40, 0x85, 0xee, 0xf0,
	0x13, 0xbf, 0x08, 0xf4, 0xa4,
	0x4d, 0x6f, 0xef, 0x0d

}, bm108a[] = {
	0xa2, 0x01, 0x27, 0x03, 0x00
}, bm108b[] = {
	0xa1, 0x01, 0x27
}, bm108c[] = {
	0x31, 0x31
}, bm108d[] = {
	0x6d, 0xae, 0xd1, 0x58, 0xaf,
	0xe4, 0x03, 0x2e, 0x8d, 0xd4,
	0x77, 0xd3, 0xd2, 0xb7, 0xf6,
	0x67, 0xe7, 0x95, 0x7a, 0xa8,
	0x30, 0x2b, 0xb5, 0xe5, 0x68,
	0xb4, 0xdc, 0xbc, 0xce, 0x3c,
	0xf0, 0xed, 0x5a, 0x90, 0xf8,
	0x31, 0x35, 0x1c, 0x85, 0xd6,
	0x15, 0x5a, 0x42, 0xa1, 0x7c,
	0xa1, 0xf2, 0x5f, 0x50, 0x1c,
	0xc1, 0x3f, 0x67, 0x10, 0x8a,
	0xe5, 0x3b, 0xda, 0x92, 0xdb,
	0x88, 0x27, 0x2e, 0x00
}, bm108e[] = {
	0xa1, 0x01, 0x26
}, bm108f[] = {
	0x31, 0x31
}, bm108g[] = {
	0x93, 0x48, 0x7d, 0x09, 0x25,
	0x6a, 0x3e, 0xf4, 0x96, 0x37,
	0x19, 0xba, 0x5c, 0xf1, 0x01,
	0xac, 0xe2, 0xfc, 0x13, 0xd6,
	0x31, 0x4b, 0x49, 0x58, 0x21,
	0x71, 0xff, 0xa4, 0xa1, 0x31,
	0x4d, 0xc9, 0x3e, 0x4a, 0x4a,
	0xdf, 0xa4, 0x2a, 0x79, 0xe3,
	0x1b, 0x35, 0xd7, 0x30, 0x43,
	0x58, 0x58, 0x5b, 0x41, 0x79,
	0x96, 0x78, 0xce, 0x00, 0xca,
	0x47, 0xc3, 0xe0, 0x23, 0x86,
	0x39, 0x23, 0xf8, 0xc8
}, bm108h[] = {
	0x31, 0x31
}, bm108i[] = {
	0x54, 0x68, 0x69, 0x73, 0x20,
	0x69, 0x73, 0x20, 0x74, 0x68,
	0x65, 0x20, 0x63, 0x6f, 0x6e,
	0x74, 0x65, 0x6e, 0x74, 0x2e
}, bm108j[] = {
	0x71, 0x42, 0xfd, 0x2f, 0xf9,
	0x6d, 0x56, 0xdb, 0x85, 0xbe,
	0xe9, 0x05, 0xa7, 0x6b, 0xa1,
	0xd0, 0xb7, 0x32, 0x1a, 0x95,
	0xc8, 0xc4, 0xd3, 0x60, 0x7c,
	0x57, 0x81, 0x93, 0x2b, 0x7a,
	0xfb, 0x87, 0x11, 0x49, 0x7d,
	0xfa, 0x75, 0x1b, 0xf4, 0x0b,
	0x58, 0xb3, 0xbc, 0xc3, 0x23,
	0x00, 0xb1, 0x48, 0x7f, 0x3d,
	0xb3, 0x40, 0x85, 0xee, 0xf0,
	0x13, 0xbf, 0x08, 0xf4, 0xa4,
	0x4d, 0x6f, 0xef, 0x0d
};

static const struct seq
seq1[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 0 } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq2[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq3[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 10 } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq4[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 23 } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq5[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 24 } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq6[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 25 } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq7[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 100 } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq8[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1000 } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq9[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1000000 } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq10[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1000000000000 } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq11[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 18446744073709551615ull } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq12[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 0 } },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm12, .buf_len = sizeof(bm12)},
	{ .reason = LECPCB_TAG_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq13[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_NUM_INT, .item = { .u.i64 = 0ull } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq14[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 3 } },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm12, .buf_len = sizeof(bm12)},
	{ .reason = LECPCB_TAG_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq15[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_NUM_INT, .item = { .u.i64 = -1ll } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq16[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_NUM_INT, .item = { .u.i64 = -10ll } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq17[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_NUM_INT, .item = { .u.i64 = -100ll } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq18[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_NUM_INT, .item = { .u.i64 = -1000ll } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq19[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_FLOAT16, .item = { .u.hf = 0 } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq20[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_FLOAT16, .item = { .u.hf = 0x8000 } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq21[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_FLOAT16, .item = { .u.hf = 0x3c00 } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq22[] = {
	{ .reason = LECPCB_CONSTRUCTED },
#if defined(LWS_WITH_CBOR_FLOAT)
	{ .reason = LECPCB_VAL_FLOAT64, .item = { .u.d = 1.1 } },
#else
	{ .reason = LECPCB_VAL_FLOAT64, .item = { .u.u64 = 0x3ff199999999999aull } },
#endif
	{ .reason = LECPCB_DESTRUCTED },
}, seq23[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_FLOAT16, .item = { .u.hf = 0x3e00 } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq24[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_FLOAT16, .item = { .u.hf = 0x7bff } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq25[] = {
	{ .reason = LECPCB_CONSTRUCTED },
#if defined(LWS_WITH_CBOR_FLOAT)
	{ .reason = LECPCB_VAL_FLOAT32, .item = { .u.f =  100000.0  } },
#else
	{ .reason = LECPCB_VAL_FLOAT32, .item = { .u.f = 0x47c35000 } },
#endif
	{ .reason = LECPCB_DESTRUCTED },
}, seq26[] = {
	{ .reason = LECPCB_CONSTRUCTED },
#if defined(LWS_WITH_CBOR_FLOAT)
	{ .reason = LECPCB_VAL_FLOAT32, .item = { .u.f = 3.4028234663852886e+38 } },
#else
	{ .reason = LECPCB_VAL_FLOAT32, .item = { .u.f = 0x7f7fffff } },
#endif
	{ .reason = LECPCB_DESTRUCTED },
}, seq27[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_FLOAT64, .item = { .u.u64 = 0x7e37e43c8800759cull } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq28[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_FLOAT16, .item = { .u.hf = 0x0001 } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq29[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_FLOAT16, .item = { .u.hf = 0x0400 } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq30[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_FLOAT16, .item = { .u.hf = 0xc400 } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq31[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_FLOAT64, .item = { .u.u64 = 0xc010666666666666ull } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq32[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_FLOAT16, .item = { .u.hf = 0x7c00 } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq33[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_FLOAT16, .item = { .u.hf = 0x7e00 } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq34[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_FLOAT16, .item = { .u.hf = 0xfc00 } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq35[] = {
	{ .reason = LECPCB_CONSTRUCTED },
#if defined(LWS_WITH_CBOR_FLOAT)
	{ .reason = LECPCB_VAL_FLOAT32, .item = { .u.u32 = 0x7f800000 } },
#else
	{ .reason = LECPCB_VAL_FLOAT32, .item = { .u.f = 0x7f800000 } },
#endif
	{ .reason = LECPCB_DESTRUCTED },
}, seq36[] = {
	{ .reason = LECPCB_CONSTRUCTED },
#if defined(LWS_WITH_CBOR_FLOAT)
	{ .reason = LECPCB_VAL_FLOAT32, .item = { .u.f = NAN } },
#else
	{ .reason = LECPCB_VAL_FLOAT32, .item = { .u.f = 0x7fc00000 } },
#endif
	{ .reason = LECPCB_DESTRUCTED },
}, seq37[] = {
	{ .reason = LECPCB_CONSTRUCTED },
#if defined(LWS_WITH_CBOR_FLOAT)
	{ .reason = LECPCB_VAL_FLOAT32, .item = { .u.u32 = 0xff800000 } },
#else
	{ .reason = LECPCB_VAL_FLOAT32, .item = { .u.f = 0xff800000 } },
#endif
	{ .reason = LECPCB_DESTRUCTED },
}, seq38[] = {
	{ .reason = LECPCB_CONSTRUCTED },
#if defined(LWS_WITH_CBOR_FLOAT)
	{ .reason = LECPCB_VAL_FLOAT64, .item = { .u.u64 = 0x7ff0000000000000ull } },
#else
	{ .reason = LECPCB_VAL_FLOAT64, .item = { .u.u64 = 0x7ff0000000000000ull } },
#endif
	{ .reason = LECPCB_DESTRUCTED },
}, seq39[] = {
	{ .reason = LECPCB_CONSTRUCTED },
#if defined(LWS_WITH_CBOR_FLOAT)
	{ .reason = LECPCB_VAL_FLOAT64, .item = { .u.u64 = 0x7ff8000000000000ull } },
#else
	{ .reason = LECPCB_VAL_FLOAT64, .item = { .u.u64 = 0x7ff8000000000000ull } },
#endif
	{ .reason = LECPCB_DESTRUCTED },
}, seq40[] = {
	{ .reason = LECPCB_CONSTRUCTED },
#if defined(LWS_WITH_CBOR_FLOAT)
	{ .reason = LECPCB_VAL_FLOAT64, .item = { .u.u64 = 0xfff0000000000000ull } },
#else
	{ .reason = LECPCB_VAL_FLOAT64, .item = { .u.u64 = 0xfff0000000000000ull } },
#endif
	{ .reason = LECPCB_DESTRUCTED },
}, seq41[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_FALSE },
	{ .reason = LECPCB_DESTRUCTED },
}, seq42[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_TRUE },
	{ .reason = LECPCB_DESTRUCTED },
}, seq43[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_NULL },
	{ .reason = LECPCB_DESTRUCTED },
}, seq44[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_UNDEFINED },
	{ .reason = LECPCB_DESTRUCTED },
}, seq45[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_SIMPLE, .item = { .u.u64 = 16 } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq46[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_FAILED }, /* example disallowed by RFC! */
	{ .reason = LECPCB_DESTRUCTED },
}, seq47[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_SIMPLE, .item = { .u.u64 = 255 } },
	{ .reason = LECPCB_DESTRUCTED },
}, seq48[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 0 } },
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = bm48, .buf_len = sizeof(bm48)},
	{ .reason = LECPCB_TAG_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq49[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 1 } },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1363896240 } },
	{ .reason = LECPCB_TAG_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq50[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 1 } },
	{ .reason = LECPCB_VAL_FLOAT64, .item = { .u.u64 = 0x41d452d9ec200000ull } },
	{ .reason = LECPCB_TAG_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq51[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 23 } },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm51, .buf_len = sizeof(bm51)},
	{ .reason = LECPCB_TAG_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq52[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 24 } },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm52, .buf_len = sizeof(bm52)},
	{ .reason = LECPCB_TAG_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq53[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 32 } },
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = bm53, .buf_len = sizeof(bm53)},
	{ .reason = LECPCB_TAG_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq54[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm53, .buf_len = 0},
	{ .reason = LECPCB_DESTRUCTED },
}, seq55[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm51, .buf_len = sizeof(bm51)},
	{ .reason = LECPCB_DESTRUCTED },
}, seq56[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = bm53, .buf_len = 0},
	{ .reason = LECPCB_DESTRUCTED },
}, seq57[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = bm57, .buf_len = sizeof(bm57)},
	{ .reason = LECPCB_DESTRUCTED },
}, seq58[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = bm58, .buf_len = sizeof(bm58)},
	{ .reason = LECPCB_DESTRUCTED },
}, seq59[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = bm59, .buf_len = sizeof(bm59)},
	{ .reason = LECPCB_DESTRUCTED },
}, seq60[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = bm60, .buf_len = sizeof(bm60)},
	{ .reason = LECPCB_DESTRUCTED },
}, seq61[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = bm61, .buf_len = sizeof(bm61)},
	{ .reason = LECPCB_DESTRUCTED },
}, seq62[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = bm62, .buf_len = sizeof(bm62)},
	{ .reason = LECPCB_DESTRUCTED },
}, seq63[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq64[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 2 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 3 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq65[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_ARRAY_START, },

	 { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	 { .reason = LECPCB_ARRAY_ITEM_END, },

	 { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 2 } },
	   { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 3 } },
	   { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_END },
	 { .reason = LECPCB_ARRAY_ITEM_END, },

	 { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	   { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 5 } },
	   { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_END },
	 { .reason = LECPCB_ARRAY_ITEM_END, },

	{ .reason = LECPCB_ARRAY_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq66[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 2 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 3 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 5 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 6 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 7 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 8 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 9 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 10 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 11 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 12 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 13 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 14 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 15 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 16 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 17 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 18 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 19 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 20 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 21 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 22 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 23 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 24 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 25 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq67[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq68[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 2 } },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 3 } },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq69[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = (const uint8_t *)"a", .buf_len = 1},
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = (const uint8_t *)"b", .buf_len = 1},
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 2 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 3 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq70[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = (const uint8_t *)"a", .buf_len = 1},
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = (const uint8_t *)"b", .buf_len = 1},
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = (const uint8_t *)"c", .buf_len = 1},
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_DESTRUCTED },
}, seq71[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = (const uint8_t *)"a", .buf_len = 1},
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = (const uint8_t *)"A", .buf_len = 1},
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = (const uint8_t *)"b", .buf_len = 1},
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = (const uint8_t *)"B", .buf_len = 1},
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = (const uint8_t *)"c", .buf_len = 1},
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = (const uint8_t *)"C", .buf_len = 1},
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = (const uint8_t *)"d", .buf_len = 1},
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = (const uint8_t *)"D", .buf_len = 1},
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = (const uint8_t *)"e", .buf_len = 1},
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = (const uint8_t *)"E", .buf_len = 1},
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq72[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_CHUNK, .buf = bm72a, .buf_len = sizeof(bm72a)},
	{ .reason = LECPCB_VAL_BLOB_CHUNK, .buf = bm72b, .buf_len = sizeof(bm72b)},
	{ .reason = LECPCB_VAL_BLOB_END, },
	{ .reason = LECPCB_DESTRUCTED },
}, seq73[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_CHUNK, .buf = (const uint8_t *)"stream", .buf_len = 5},
	{ .reason = LECPCB_VAL_STR_CHUNK, .buf = (const uint8_t *)"ming", .buf_len = 4},
	{ .reason = LECPCB_VAL_STR_END, },
	{ .reason = LECPCB_DESTRUCTED },
}, seq74[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq75[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 2 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 3 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 5 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq76[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 2 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 3 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 5 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq77[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 2 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 3 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 5 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq78[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 2 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 3 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 5 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq79[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 2 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 3 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 5 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 6 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 7 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 8 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 9 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 10 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 11 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 12 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 13 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 14 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 15 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 16 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 17 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 18 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 19 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 20 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 21 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 22 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 23 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 24 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 25 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq80[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = (const uint8_t *)"a", .buf_len = 1},
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = (const uint8_t *)"b", .buf_len = 1},
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 2 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 3 } },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq81[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = (const uint8_t *)"a", .buf_len = 1},
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = (const uint8_t *)"b", .buf_len = 1},
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = (const uint8_t *)"c", .buf_len = 1},
	{ .reason = LECPCB_OBJECT_END, },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END },
	{ .reason = LECPCB_DESTRUCTED },
}, seq82[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = (const uint8_t *)"Fun", .buf_len = 3},
	{ .reason = LECPCB_VAL_TRUE },
	{ .reason = LECPCB_VAL_STR_START, },
	{ .reason = LECPCB_VAL_STR_END, .buf = (const uint8_t *)"Amt", .buf_len = 3},
	{ .reason = LECPCB_VAL_NUM_INT, .item = { .u.i64 = (int64_t)-2ll } },
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_DESTRUCTED },

}, seq83[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 97 } },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm83a, .buf_len = sizeof(bm83a) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm83b, .buf_len = sizeof(bm83b) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm83c, .buf_len = sizeof(bm83c) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm83a, .buf_len = 0 },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	{ .reason = LECPCB_VAL_NUM_INT, .item = { .u.i64 = -6 } },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm83d, .buf_len = sizeof(bm83d) },
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm83a, .buf_len = 0 },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },

}, seq84[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 97 } },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm84a, .buf_len = sizeof(bm84a) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm84b, .buf_len = sizeof(bm84b) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm84c, .buf_len = sizeof(bm84c) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm84a, .buf_len = 0 },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	{ .reason = LECPCB_VAL_NUM_INT, .item = { .u.i64 = -6 } },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm84d, .buf_len = sizeof(bm84d) },
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm84a, .buf_len = 0 },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },

}, seq85[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 97 } },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm85a, .buf_len = sizeof(bm85a) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm85b, .buf_len = sizeof(bm85b) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm85c, .buf_len = sizeof(bm85c) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm85a, .buf_len = 0 },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	{ .reason = LECPCB_VAL_NUM_INT, .item = { .u.i64 = -6 } },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm85d, .buf_len = sizeof(bm85d) },
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm85a, .buf_len = 0 },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },

}, seq86[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 97 } },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm86a, .buf_len = sizeof(bm86a) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm86b, .buf_len = sizeof(bm86b) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm86c, .buf_len = sizeof(bm86c) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm86a, .buf_len = 0 },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	{ .reason = LECPCB_VAL_NUM_INT, .item = { .u.i64 = -6 } },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm86d, .buf_len = sizeof(bm86d) },
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm86a, .buf_len = 0 },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },
}, seq87[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 97 } },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm87a, .buf_len = sizeof(bm87a) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm87b, .buf_len = sizeof(bm87b) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm87c, .buf_len = sizeof(bm87c) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm87a, .buf_len = 0 },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	{ .reason = LECPCB_VAL_NUM_INT, .item = { .u.i64 = -6 } },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm87d, .buf_len = sizeof(bm87d) },
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm87a, .buf_len = 0 },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },
}, seq88[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 17 } },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm88a, .buf_len = sizeof(bm88a) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm88b, .buf_len = sizeof(bm88b) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm88c, .buf_len = sizeof(bm88c) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },
}, seq89[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 17 } },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm89a, .buf_len = sizeof(bm89a) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm89b, .buf_len = sizeof(bm89b) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm89c, .buf_len = sizeof(bm89c) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },
}, seq90[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 17 } },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm90a, .buf_len = sizeof(bm90a) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm90b, .buf_len = sizeof(bm90b) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm90c, .buf_len = sizeof(bm90c) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },
}, seq91[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 17 } },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm91a, .buf_len = sizeof(bm91a) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm91b, .buf_len = sizeof(bm91b) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm91c, .buf_len = sizeof(bm91c) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },
}, seq92[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 17 } },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm92a, .buf_len = sizeof(bm92a) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm92b, .buf_len = sizeof(bm92b) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm92c, .buf_len = sizeof(bm92c) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },
}, seq93[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 16 } },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm93a, .buf_len = sizeof(bm93a) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 5 } },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm93b, .buf_len = sizeof(bm93b) },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 7 } },
	{ .reason = LECPCB_ARRAY_START, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm93c, .buf_len = sizeof(bm93c) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_OBJECT_START, },
	{ .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm93d, .buf_len = sizeof(bm93d) },
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm93e, .buf_len = sizeof(bm93e) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_OBJECT_END },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_ITEM_START, },
	{ .reason = LECPCB_VAL_BLOB_START, },
	{ .reason = LECPCB_VAL_BLOB_END, .buf = bm93f, .buf_len = sizeof(bm93f) },
	{ .reason = LECPCB_ARRAY_ITEM_END, },
	{ .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },
}, seq94[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 16 } },
	 { .reason = LECPCB_ARRAY_START, },
	 { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm94a, .buf_len = sizeof(bm94a) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_OBJECT_START, },
	   { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 5 } },
	   { .reason = LECPCB_VAL_BLOB_START, },
	   { .reason = LECPCB_VAL_BLOB_END, .buf = bm94b, .buf_len = sizeof(bm94b) },
	   { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 7 } },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_ARRAY_START, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm94c, .buf_len = sizeof(bm94c) },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_OBJECT_START, },
	      { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	      { .reason = LECPCB_VAL_BLOB_START, },
	      { .reason = LECPCB_VAL_BLOB_END, .buf = bm94d, .buf_len = sizeof(bm94d) },
	     { .reason = LECPCB_OBJECT_END },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm94e, .buf_len = sizeof(bm94e) },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_END, },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_ARRAY_START, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm94f, .buf_len = sizeof(bm94f) },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_OBJECT_START, },
	      { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	      { .reason = LECPCB_VAL_BLOB_START, },
	      { .reason = LECPCB_VAL_BLOB_END, .buf = bm94g, .buf_len = sizeof(bm94g) },
	     { .reason = LECPCB_OBJECT_END },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm94h, .buf_len = sizeof(bm94h) },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_END, },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	  { .reason = LECPCB_OBJECT_END },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm94i, .buf_len = sizeof(bm94i) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	 { .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },
}, seq95[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 96 } },
	 { .reason = LECPCB_ARRAY_START, },
	 { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm95a, .buf_len = sizeof(bm95a) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_OBJECT_START, },
	   { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 5 } },
	   { .reason = LECPCB_VAL_BLOB_START, },
	   { .reason = LECPCB_VAL_BLOB_END, .buf = bm95b, .buf_len = sizeof(bm95b) },
	   { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 7 } },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm95c, .buf_len = sizeof(bm95c) },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_OBJECT_START, },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm95d, .buf_len = sizeof(bm95d) },
	    { .reason = LECPCB_OBJECT_END },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm95e, .buf_len = sizeof(bm95e) },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	  { .reason = LECPCB_OBJECT_END },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm95f, .buf_len = sizeof(bm95f) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_ARRAY_START, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm95f, .buf_len = 0 },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_OBJECT_START, },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	     { .reason = LECPCB_VAL_NUM_INT, .item = { .u.i64 = -6 } },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm95g, .buf_len = sizeof(bm95g) },
	     { .reason = LECPCB_OBJECT_END },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm95f, .buf_len = 0 },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	   { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_END, },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	 { .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },
}, seq96[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 96 } },
	 { .reason = LECPCB_ARRAY_START, },
	 { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm96a, .buf_len = sizeof(bm96a) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_OBJECT_START, },
	   { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 5 } },
	   { .reason = LECPCB_VAL_BLOB_START, },
	   { .reason = LECPCB_VAL_BLOB_END, .buf = bm96b, .buf_len = sizeof(bm96b) },
	   { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 7 } },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_ARRAY_START, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm96c, .buf_len = sizeof(bm96c) },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_OBJECT_START, },
	      { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	      { .reason = LECPCB_VAL_BLOB_START, },
	      { .reason = LECPCB_VAL_BLOB_END, .buf = bm96d, .buf_len = sizeof(bm96d) },
	     { .reason = LECPCB_OBJECT_END },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm96e, .buf_len = sizeof(bm96e) },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_END, },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_ARRAY_START, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm96f, .buf_len = sizeof(bm96f) },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_OBJECT_START, },
	      { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	      { .reason = LECPCB_VAL_BLOB_START, },
	      { .reason = LECPCB_VAL_BLOB_END, .buf = bm96g, .buf_len = sizeof(bm96g) },
	     { .reason = LECPCB_OBJECT_END },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm96h, .buf_len = sizeof(bm96h) },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_END, },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	  { .reason = LECPCB_OBJECT_END },
	   { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm96i, .buf_len = sizeof(bm96i) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_ARRAY_START, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm96f, .buf_len = 0 },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_OBJECT_START, },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	     { .reason = LECPCB_VAL_NUM_INT, .item = { .u.i64 = -6 } },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm96j, .buf_len = sizeof(bm96j) },
	     { .reason = LECPCB_OBJECT_END },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm96f, .buf_len = 0 },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	   { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_END, },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	 { .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },
}, seq97[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 96 } },
	 { .reason = LECPCB_ARRAY_START, },
	 { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm97a, .buf_len = sizeof(bm97a) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_OBJECT_START, },
	   { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 5 } },
	   { .reason = LECPCB_VAL_BLOB_START, },
	   { .reason = LECPCB_VAL_BLOB_END, .buf = bm97b, .buf_len = sizeof(bm97b) },
	  { .reason = LECPCB_OBJECT_END },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm97c, .buf_len = sizeof(bm97c) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_ARRAY_START, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm97f, .buf_len = 0 },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_OBJECT_START, },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	     { .reason = LECPCB_VAL_NUM_INT, .item = { .u.i64 = -6 } },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm97d, .buf_len = sizeof(bm97d) },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 7 } },
	     { .reason = LECPCB_ARRAY_START, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	      { .reason = LECPCB_VAL_BLOB_START, },
	      { .reason = LECPCB_VAL_BLOB_END, .buf = bm97e, .buf_len = sizeof(bm97e) },
	      { .reason = LECPCB_ARRAY_ITEM_END, },
	      { .reason = LECPCB_ARRAY_ITEM_START, },
	      { .reason = LECPCB_OBJECT_START, },
	       { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	       { .reason = LECPCB_VAL_BLOB_START, },
	       { .reason = LECPCB_VAL_BLOB_END, .buf = bm97f, .buf_len = sizeof(bm97f) },
	      { .reason = LECPCB_OBJECT_END },
	      { .reason = LECPCB_ARRAY_ITEM_END, },
	      { .reason = LECPCB_ARRAY_ITEM_START, },
	      { .reason = LECPCB_VAL_BLOB_START, },
	      { .reason = LECPCB_VAL_BLOB_END, .buf = bm97g, .buf_len = sizeof(bm97g) },
	      { .reason = LECPCB_ARRAY_ITEM_END, },
             { .reason = LECPCB_ARRAY_END, },
	    { .reason = LECPCB_OBJECT_END },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm97e, .buf_len = 0 },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	   { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_END, },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	 { .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },
}, seq98[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 97 } },
	 { .reason = LECPCB_ARRAY_START, },
	 { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm98a, .buf_len = sizeof(bm98a) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_OBJECT_START, },
	   { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 7 } },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm98b, .buf_len = sizeof(bm98b) },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_OBJECT_START, },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm98c, .buf_len = sizeof(bm98c) },
	    { .reason = LECPCB_OBJECT_END },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm98d, .buf_len = sizeof(bm98d) },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	  { .reason = LECPCB_OBJECT_END },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm98e, .buf_len = sizeof(bm98e) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm98f, .buf_len = sizeof(bm98f) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_ARRAY_START, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm98e, .buf_len = 0 },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_OBJECT_START, },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	     { .reason = LECPCB_VAL_NUM_INT, .item = { .u.i64 = -6 } },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm98g, .buf_len = sizeof(bm98g) },
	    { .reason = LECPCB_OBJECT_END },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm98e, .buf_len = 0 },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	   { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_END, },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	 { .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },

}, seq99[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 97 } },
	 { .reason = LECPCB_ARRAY_START, },
	 { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm99a, .buf_len = sizeof(bm99a) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_OBJECT_START, },
	   { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 7 } },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_ARRAY_START, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm99b, .buf_len = sizeof(bm99b) },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_OBJECT_START, },
	      { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	      { .reason = LECPCB_VAL_BLOB_START, },
	      { .reason = LECPCB_VAL_BLOB_END, .buf = bm99c, .buf_len = sizeof(bm99c) },
	     { .reason = LECPCB_OBJECT_END },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm99d, .buf_len = sizeof(bm99d) },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_END, },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_ARRAY_START, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm99e, .buf_len = sizeof(bm99e) },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_OBJECT_START, },
	      { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	      { .reason = LECPCB_VAL_BLOB_START, },
	      { .reason = LECPCB_VAL_BLOB_END, .buf = bm99f, .buf_len = sizeof(bm99f) },
	     { .reason = LECPCB_OBJECT_END },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm99g, .buf_len = sizeof(bm99g) },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_END, },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	  { .reason = LECPCB_OBJECT_END },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm99h, .buf_len = sizeof(bm99h) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm99i, .buf_len = sizeof(bm99i) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_ARRAY_START, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm99a, .buf_len = 0 },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_OBJECT_START, },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	     { .reason = LECPCB_VAL_NUM_INT, .item = { .u.i64 = -6 } },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm99j, .buf_len = sizeof(bm99j) },
	    { .reason = LECPCB_OBJECT_END },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm98e, .buf_len = 0 },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	   { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_END, },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	 { .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },

}, seq100[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 17 } },
	 { .reason = LECPCB_ARRAY_START, },
	 { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm100a, .buf_len = sizeof(bm100a) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_OBJECT_START, },
	   { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 7 } },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm100b, .buf_len = sizeof(bm100b) },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_OBJECT_START, },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm100c, .buf_len = sizeof(bm100c) },
	    { .reason = LECPCB_OBJECT_END },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm100d, .buf_len = sizeof(bm100d) },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	  { .reason = LECPCB_OBJECT_END },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm100e, .buf_len = sizeof(bm100e) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm100f, .buf_len = sizeof(bm100f) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	 { .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },

}, seq101[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 17 } },
	 { .reason = LECPCB_ARRAY_START, },
	 { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm101a, .buf_len = sizeof(bm101a) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_OBJECT_START, },
	   { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 7 } },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_ARRAY_START, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm101b, .buf_len = sizeof(bm101b) },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_OBJECT_START, },
	      { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	      { .reason = LECPCB_VAL_BLOB_START, },
	      { .reason = LECPCB_VAL_BLOB_END, .buf = bm101c, .buf_len = sizeof(bm101c) },
	     { .reason = LECPCB_OBJECT_END },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm101d, .buf_len = sizeof(bm101d) },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_END, },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_ARRAY_START, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm101e, .buf_len = sizeof(bm101e) },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_OBJECT_START, },
	      { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	      { .reason = LECPCB_VAL_BLOB_START, },
	      { .reason = LECPCB_VAL_BLOB_END, .buf = bm101f, .buf_len = sizeof(bm101f) },
	     { .reason = LECPCB_OBJECT_END },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm101g, .buf_len = sizeof(bm101g) },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_END, },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	  { .reason = LECPCB_OBJECT_END },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm101h, .buf_len = sizeof(bm101h) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm101i, .buf_len = sizeof(bm101i) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm101j, .buf_len = 0 },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_OBJECT_START, },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 1 } },
	     { .reason = LECPCB_VAL_NUM_INT, .item = { .u.i64 = -6 } },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm101j, .buf_len = sizeof(bm101j) },
	    { .reason = LECPCB_OBJECT_END },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm101j, .buf_len = 0 },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	   { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_END, },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	 { .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },
}, seq102[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 17 } },
	 { .reason = LECPCB_ARRAY_START, },
	 { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm102a, .buf_len = sizeof(bm102a) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_OBJECT_START, },
	   { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 7 } },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm102b, .buf_len = sizeof(bm102b) },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_OBJECT_START, },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm102c, .buf_len = sizeof(bm102c) },
	    { .reason = LECPCB_OBJECT_END },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm102d, .buf_len = sizeof(bm102d) },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	  { .reason = LECPCB_OBJECT_END },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm102e, .buf_len = sizeof(bm102e) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm102f, .buf_len = sizeof(bm102f) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	 { .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },
}, seq103[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 17 } },
	 { .reason = LECPCB_ARRAY_START, },
	 { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm103a, .buf_len = sizeof(bm103a) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_OBJECT_START, },
	   { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 7 } },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm103b, .buf_len = sizeof(bm103b) },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_OBJECT_START, },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm103c, .buf_len = sizeof(bm103c) },
	    { .reason = LECPCB_OBJECT_END },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm103d, .buf_len = sizeof(bm103d) },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	   { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
		  { .reason = LECPCB_VAL_BLOB_START, },
		  { .reason = LECPCB_VAL_BLOB_END, .buf = bm103e, .buf_len = sizeof(bm103e) },
		  { .reason = LECPCB_ARRAY_ITEM_END, },
		  { .reason = LECPCB_ARRAY_ITEM_START, },
		  { .reason = LECPCB_OBJECT_START, },
		     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
			  { .reason = LECPCB_VAL_BLOB_START, },
			  { .reason = LECPCB_VAL_BLOB_END, .buf = bm103f, .buf_len = sizeof(bm103f) },

	  { .reason = LECPCB_OBJECT_END },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm103g, .buf_len = sizeof(bm103g) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	   { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	    { .reason = LECPCB_OBJECT_END },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm103h, .buf_len = sizeof(bm103h) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm103i, .buf_len = sizeof(bm103i) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	 { .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },

}, seq104[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 98 } },
	 { .reason = LECPCB_ARRAY_START, },
	 { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm104a, .buf_len = sizeof(bm104a) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_OBJECT_START, },
	  { .reason = LECPCB_OBJECT_END },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm104b, .buf_len = sizeof(bm104b) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_ARRAY_START, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm104c, .buf_len = sizeof(bm104c) },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_OBJECT_START, },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 7 } },
	     { .reason = LECPCB_ARRAY_START, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	      { .reason = LECPCB_VAL_BLOB_START, },
	      { .reason = LECPCB_VAL_BLOB_END, .buf = bm104d, .buf_len = sizeof(bm104d) },
	      { .reason = LECPCB_ARRAY_ITEM_END, },
	      { .reason = LECPCB_ARRAY_ITEM_START, },
	      { .reason = LECPCB_OBJECT_START },
	       { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	       { .reason = LECPCB_VAL_BLOB_START, },
	       { .reason = LECPCB_VAL_BLOB_END, .buf = bm104e, .buf_len = sizeof(bm104e) },
	      { .reason = LECPCB_OBJECT_END },
	      { .reason = LECPCB_ARRAY_ITEM_END, },
	      { .reason = LECPCB_ARRAY_ITEM_START, },
	      { .reason = LECPCB_VAL_BLOB_START, },
	      { .reason = LECPCB_VAL_BLOB_END, .buf = bm104f, .buf_len = sizeof(bm104f) },
	      { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_END, },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm104g, .buf_len = sizeof(bm104g) },
	    { .reason = LECPCB_OBJECT_END },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm104h, .buf_len = sizeof(bm104h) },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	   { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_END, },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	 { .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },
}, seq105[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 98 } },
	 { .reason = LECPCB_ARRAY_START, },
	 { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm105a, .buf_len = sizeof(bm105a) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_OBJECT_START, },
	  { .reason = LECPCB_OBJECT_END, },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm105b, .buf_len = sizeof(bm105b) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_ARRAY_START, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm105c, .buf_len = sizeof(bm105c) },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_OBJECT_START, },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 7 } },
	     { .reason = LECPCB_ARRAY_START, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	      { .reason = LECPCB_ARRAY_START, },
	      { .reason = LECPCB_ARRAY_ITEM_START, },
	       { .reason = LECPCB_VAL_BLOB_START, },
	       { .reason = LECPCB_VAL_BLOB_END, .buf = bm105d, .buf_len = sizeof(bm105d) },
	       { .reason = LECPCB_ARRAY_ITEM_END, },
	       { .reason = LECPCB_ARRAY_ITEM_START, },
	       { .reason = LECPCB_OBJECT_START, },
	        { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	        { .reason = LECPCB_VAL_BLOB_START, },
	        { .reason = LECPCB_VAL_BLOB_END, .buf = bm105e, .buf_len = sizeof(bm105e) },
	       { .reason = LECPCB_OBJECT_END, },
	       { .reason = LECPCB_ARRAY_ITEM_END, },
	       { .reason = LECPCB_ARRAY_ITEM_START, },
	       { .reason = LECPCB_VAL_BLOB_START, },
	       { .reason = LECPCB_VAL_BLOB_END, .buf = bm105f, .buf_len = sizeof(bm105f) },
	       { .reason = LECPCB_ARRAY_ITEM_END, },
	      { .reason = LECPCB_ARRAY_END, },
	      { .reason = LECPCB_ARRAY_ITEM_END, },
	      { .reason = LECPCB_ARRAY_ITEM_START, },
	      { .reason = LECPCB_ARRAY_START, },
	      { .reason = LECPCB_ARRAY_ITEM_START, },
	       { .reason = LECPCB_VAL_BLOB_START, },
	       { .reason = LECPCB_VAL_BLOB_END, .buf = bm105g, .buf_len = sizeof(bm105g) },
	       { .reason = LECPCB_ARRAY_ITEM_END, },
	       { .reason = LECPCB_ARRAY_ITEM_START, },
	       { .reason = LECPCB_OBJECT_START, },
	        { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	        { .reason = LECPCB_VAL_BLOB_START, },
		{ .reason = LECPCB_VAL_BLOB_END, .buf = bm105h, .buf_len = sizeof(bm105h) },
	       { .reason = LECPCB_OBJECT_END, },
	       { .reason = LECPCB_ARRAY_ITEM_END, },
	       { .reason = LECPCB_ARRAY_ITEM_START, },
	       { .reason = LECPCB_VAL_BLOB_START, },
	       { .reason = LECPCB_VAL_BLOB_END, .buf = bm105i, .buf_len = sizeof(bm105i) },
	       { .reason = LECPCB_ARRAY_ITEM_END, },
	      { .reason = LECPCB_ARRAY_END, },
	      { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_END, },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm105j, .buf_len = sizeof(bm105j) },
	    { .reason = LECPCB_OBJECT_END, },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm105k, .buf_len = sizeof(bm105k) },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	   { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_END, },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	 { .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },
}, seq106[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 98 } },
	 { .reason = LECPCB_ARRAY_START, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm106a, .buf_len = sizeof(bm106a) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_OBJECT_START, },
	   { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 7 } },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm106b, .buf_len = sizeof(bm106b) },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_OBJECT_START, },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm106c, .buf_len = sizeof(bm106c) },
	    { .reason = LECPCB_OBJECT_END },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm106d, .buf_len = sizeof(bm106d) },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	  { .reason = LECPCB_OBJECT_END },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm106e, .buf_len = sizeof(bm106e) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_ARRAY_START, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm106f, .buf_len = sizeof(bm106f) },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_OBJECT_START, },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm106g, .buf_len = sizeof(bm106g) },
	    { .reason = LECPCB_OBJECT_END },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm106h, .buf_len = sizeof(bm106h) },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	   { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_END, },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	 { .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },
}, seq107[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 17 } },
	 { .reason = LECPCB_ARRAY_START, },
	 { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm107a, .buf_len = sizeof(bm107a) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_OBJECT_START, },
	   { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 7 } },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm107b, .buf_len = sizeof(bm107b) },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_OBJECT_START, },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm107c, .buf_len = sizeof(bm107c) },
	    { .reason = LECPCB_OBJECT_END },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm107d, .buf_len = sizeof(bm107d) },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm107e, .buf_len = sizeof(bm107e) },
	    { .reason = LECPCB_OBJECT_END },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm107f, .buf_len = sizeof(bm107f) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm107g, .buf_len = sizeof(bm107g) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	 { .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },
}, seq108[] = {
	{ .reason = LECPCB_CONSTRUCTED },
	{ .reason = LECPCB_TAG_START, .item = { .u.u64 = 18 } },
	 { .reason = LECPCB_ARRAY_START, },
	 { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm108a, .buf_len = sizeof(bm108a) },
	  { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_ITEM_START, },
	  { .reason = LECPCB_OBJECT_START, },
	   { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 7 } },
	   { .reason = LECPCB_ARRAY_START, },
	   { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_ARRAY_START, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm108b, .buf_len = sizeof(bm108b) },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_OBJECT_START, },
	      { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	      { .reason = LECPCB_VAL_BLOB_START, },
	      { .reason = LECPCB_VAL_BLOB_END, .buf = bm108c, .buf_len = sizeof(bm108c) },
	     { .reason = LECPCB_OBJECT_END },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	     { .reason = LECPCB_ARRAY_ITEM_START, },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm108d, .buf_len = sizeof(bm108d) },
	     { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_END, },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_ARRAY_START, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm108e, .buf_len = sizeof(bm108e) },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_OBJECT_START, },
	     { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	     { .reason = LECPCB_VAL_BLOB_START, },
	     { .reason = LECPCB_VAL_BLOB_END, .buf = bm108f, .buf_len = sizeof(bm108f) },
	    { .reason = LECPCB_OBJECT_END },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	    { .reason = LECPCB_ARRAY_ITEM_START, },
	    { .reason = LECPCB_VAL_BLOB_START, },
	    { .reason = LECPCB_VAL_BLOB_END, .buf = bm108g, .buf_len = sizeof(bm108g) },
	    { .reason = LECPCB_ARRAY_ITEM_END, },
	   { .reason = LECPCB_ARRAY_END, },
	   { .reason = LECPCB_ARRAY_ITEM_END, },
	  { .reason = LECPCB_ARRAY_END, },
	  { .reason = LECPCB_VAL_NUM_UINT, .item = { .u.u64 = 4 } },
	  { .reason = LECPCB_VAL_BLOB_START, },
	  { .reason = LECPCB_VAL_BLOB_END, .buf = bm108h, .buf_len = sizeof(bm108h) },
	 { .reason = LECPCB_OBJECT_END },
	 { .reason = LECPCB_ARRAY_ITEM_END, },
	 { .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_BLOB_START, },
	 { .reason = LECPCB_VAL_BLOB_END, .buf = bm108i, .buf_len = sizeof(bm108i) },
	 { .reason = LECPCB_ARRAY_ITEM_END, },
	 { .reason = LECPCB_ARRAY_ITEM_START, },
	 { .reason = LECPCB_VAL_BLOB_START, },
	 { .reason = LECPCB_VAL_BLOB_END, .buf = bm108j, .buf_len = sizeof(bm108j) },
	 { .reason = LECPCB_ARRAY_ITEM_END, },
	 { .reason = LECPCB_ARRAY_END, },
	{ .reason = LECPCB_TAG_END, },
	{ .reason = LECPCB_DESTRUCTED },
};


struct cbort {
	const uint8_t		*b;
	size_t			blen;
	const struct seq	*seq;
	size_t			seq_size;
};

static const struct cbort cbor_tests[] = {
	{ .b = test1,  .blen = sizeof(test1),
			.seq = seq1,  .seq_size = LWS_ARRAY_SIZE(seq1) },
	{ .b = test2,  .blen = sizeof(test2),
			.seq = seq2,  .seq_size = LWS_ARRAY_SIZE(seq2) },
	{ .b = test3,  .blen = sizeof(test3),
			.seq = seq3,  .seq_size = LWS_ARRAY_SIZE(seq3) },
	{ .b = test4,  .blen = sizeof(test4),
			.seq = seq4,  .seq_size = LWS_ARRAY_SIZE(seq4) },
	{ .b = test5,  .blen = sizeof(test5),
			.seq = seq5,  .seq_size = LWS_ARRAY_SIZE(seq5) },
	{ .b = test6,  .blen = sizeof(test6),
			.seq = seq6,  .seq_size = LWS_ARRAY_SIZE(seq6) },
	{ .b = test7,  .blen = sizeof(test7),
			.seq = seq7,  .seq_size = LWS_ARRAY_SIZE(seq7) },
	{ .b = test8,  .blen = sizeof(test8),
			.seq = seq8,  .seq_size = LWS_ARRAY_SIZE(seq8) },
	{ .b = test9,  .blen = sizeof(test9),
			.seq = seq9,  .seq_size = LWS_ARRAY_SIZE(seq9) },
	{ .b = test10, .blen = sizeof(test10),
			.seq = seq10, .seq_size = LWS_ARRAY_SIZE(seq10) },
	{ .b = test11, .blen = sizeof(test11),
			.seq = seq11, .seq_size = LWS_ARRAY_SIZE(seq11) },
	{ .b = test12, .blen = sizeof(test12),
			.seq = seq12, .seq_size = LWS_ARRAY_SIZE(seq12) },
	{ .b = test13, .blen = sizeof(test13),
			.seq = seq13, .seq_size = LWS_ARRAY_SIZE(seq13) },
	{ .b = test14, .blen = sizeof(test14),
			.seq = seq14, .seq_size = LWS_ARRAY_SIZE(seq14) },
	{ .b = test15, .blen = sizeof(test15),
			.seq = seq15, .seq_size = LWS_ARRAY_SIZE(seq15) },
	{ .b = test16, .blen = sizeof(test16),
			.seq = seq16, .seq_size = LWS_ARRAY_SIZE(seq16) },
	{ .b = test17, .blen = sizeof(test17),
			.seq = seq17, .seq_size = LWS_ARRAY_SIZE(seq17) },
	{ .b = test18, .blen = sizeof(test18),
			.seq = seq18, .seq_size = LWS_ARRAY_SIZE(seq18) },
	{ .b = test19, .blen = sizeof(test19),
			.seq = seq19, .seq_size = LWS_ARRAY_SIZE(seq19) },
	{ .b = test20, .blen = sizeof(test20),
			.seq = seq20, .seq_size = LWS_ARRAY_SIZE(seq20) },
	{ .b = test21, .blen = sizeof(test21),
			.seq = seq21, .seq_size = LWS_ARRAY_SIZE(seq21) },
	{ .b = test22, .blen = sizeof(test22),
			.seq = seq22, .seq_size = LWS_ARRAY_SIZE(seq22) },
	{ .b = test23, .blen = sizeof(test23),
			.seq = seq23, .seq_size = LWS_ARRAY_SIZE(seq23) },
	{ .b = test24, .blen = sizeof(test24),
			.seq = seq24, .seq_size = LWS_ARRAY_SIZE(seq24) },
	{ .b = test25, .blen = sizeof(test25),
			.seq = seq25, .seq_size = LWS_ARRAY_SIZE(seq25) },
	{ .b = test26, .blen = sizeof(test26),
			.seq = seq26, .seq_size = LWS_ARRAY_SIZE(seq26) },
	{ .b = test27, .blen = sizeof(test27),
			.seq = seq27, .seq_size = LWS_ARRAY_SIZE(seq27) },
	{ .b = test28, .blen = sizeof(test28),
			.seq = seq28, .seq_size = LWS_ARRAY_SIZE(seq28) },
	{ .b = test29, .blen = sizeof(test29),
			.seq = seq29, .seq_size = LWS_ARRAY_SIZE(seq29) },
	{ .b = test30, .blen = sizeof(test30),
			.seq = seq30, .seq_size = LWS_ARRAY_SIZE(seq30) },
	{ .b = test31, .blen = sizeof(test31),
			.seq = seq31, .seq_size = LWS_ARRAY_SIZE(seq31) },
	{ .b = test32, .blen = sizeof(test32),
			.seq = seq32, .seq_size = LWS_ARRAY_SIZE(seq32) },
	{ .b = test33, .blen = sizeof(test33),
			.seq = seq33, .seq_size = LWS_ARRAY_SIZE(seq33) },
	{ .b = test34, .blen = sizeof(test34),
			.seq = seq34, .seq_size = LWS_ARRAY_SIZE(seq34) },
	{ .b = test35, .blen = sizeof(test35),
			.seq = seq35, .seq_size = LWS_ARRAY_SIZE(seq35) },
	{ .b = test36, .blen = sizeof(test36),
			.seq = seq36, .seq_size = LWS_ARRAY_SIZE(seq36) },
	{ .b = test37, .blen = sizeof(test37),
			.seq = seq37, .seq_size = LWS_ARRAY_SIZE(seq37) },
	{ .b = test38, .blen = sizeof(test38),
			.seq = seq38, .seq_size = LWS_ARRAY_SIZE(seq38) },
	{ .b = test39, .blen = sizeof(test39),
			.seq = seq39, .seq_size = LWS_ARRAY_SIZE(seq39) },
	{ .b = test40, .blen = sizeof(test40),
			.seq = seq40, .seq_size = LWS_ARRAY_SIZE(seq40) },
	{ .b = test41, .blen = sizeof(test41),
			.seq = seq41, .seq_size = LWS_ARRAY_SIZE(seq41) },
	{ .b = test42, .blen = sizeof(test42),
			.seq = seq42, .seq_size = LWS_ARRAY_SIZE(seq42) },
	{ .b = test43, .blen = sizeof(test43),
			.seq = seq43, .seq_size = LWS_ARRAY_SIZE(seq43) },
	{ .b = test44, .blen = sizeof(test44),
			.seq = seq44, .seq_size = LWS_ARRAY_SIZE(seq44) },
	{ .b = test45, .blen = sizeof(test45),
			.seq = seq45, .seq_size = LWS_ARRAY_SIZE(seq45) },
	{ .b = test46, .blen = sizeof(test46),
			.seq = seq46, .seq_size = LWS_ARRAY_SIZE(seq46) },
	{ .b = test47, .blen = sizeof(test47),
			.seq = seq47, .seq_size = LWS_ARRAY_SIZE(seq47) },
	{ .b = test48, .blen = sizeof(test48),
			.seq = seq48, .seq_size = LWS_ARRAY_SIZE(seq48) },
	{ .b = test49, .blen = sizeof(test49),
			.seq = seq49, .seq_size = LWS_ARRAY_SIZE(seq49) },
	{ .b = test50, .blen = sizeof(test50),
			.seq = seq50, .seq_size = LWS_ARRAY_SIZE(seq50) },
	{ .b = test51, .blen = sizeof(test51),
			.seq = seq51, .seq_size = LWS_ARRAY_SIZE(seq51) },
	{ .b = test52, .blen = sizeof(test52),
			.seq = seq52, .seq_size = LWS_ARRAY_SIZE(seq52) },
	{ .b = test53, .blen = sizeof(test53),
			.seq = seq53, .seq_size = LWS_ARRAY_SIZE(seq53) },
	{ .b = test54, .blen = sizeof(test54),
			.seq = seq54, .seq_size = LWS_ARRAY_SIZE(seq54) },
	{ .b = test55, .blen = sizeof(test55),
			.seq = seq55, .seq_size = LWS_ARRAY_SIZE(seq55) },
	{ .b = test56, .blen = sizeof(test56),
			.seq = seq56, .seq_size = LWS_ARRAY_SIZE(seq56) },
	{ .b = test57, .blen = sizeof(test57),
			.seq = seq57, .seq_size = LWS_ARRAY_SIZE(seq57) },
	{ .b = test58, .blen = sizeof(test58),
			.seq = seq58, .seq_size = LWS_ARRAY_SIZE(seq58) },
	{ .b = test59, .blen = sizeof(test59),
			.seq = seq59, .seq_size = LWS_ARRAY_SIZE(seq59) },
	{ .b = test60, .blen = sizeof(test60),
			.seq = seq60, .seq_size = LWS_ARRAY_SIZE(seq60) },
	{ .b = test61, .blen = sizeof(test61),
			.seq = seq61, .seq_size = LWS_ARRAY_SIZE(seq61) },
	{ .b = test62, .blen = sizeof(test62),
			.seq = seq62, .seq_size = LWS_ARRAY_SIZE(seq62) },
	{ .b = test63, .blen = sizeof(test63),
			.seq = seq63, .seq_size = LWS_ARRAY_SIZE(seq63) },
	{ .b = test64, .blen = sizeof(test64),
			.seq = seq64, .seq_size = LWS_ARRAY_SIZE(seq64) },
	{ .b = test65, .blen = sizeof(test65),
			.seq = seq65, .seq_size = LWS_ARRAY_SIZE(seq65) },
	{ .b = test66, .blen = sizeof(test66),
			.seq = seq66, .seq_size = LWS_ARRAY_SIZE(seq66) },
	{ .b = test67, .blen = sizeof(test67),
			.seq = seq67, .seq_size = LWS_ARRAY_SIZE(seq67) },
	{ .b = test68, .blen = sizeof(test68),
			.seq = seq68, .seq_size = LWS_ARRAY_SIZE(seq68) },
	{ .b = test69, .blen = sizeof(test69),
			.seq = seq69, .seq_size = LWS_ARRAY_SIZE(seq69) },
	{ .b = test70, .blen = sizeof(test70),
			.seq = seq70, .seq_size = LWS_ARRAY_SIZE(seq70) },
	{ .b = test71, .blen = sizeof(test71),
			.seq = seq71, .seq_size = LWS_ARRAY_SIZE(seq71) },
	{ .b = test72, .blen = sizeof(test72),
			.seq = seq72, .seq_size = LWS_ARRAY_SIZE(seq72) },
	{ .b = test73, .blen = sizeof(test73),
			.seq = seq73, .seq_size = LWS_ARRAY_SIZE(seq73) },
	{ .b = test74, .blen = sizeof(test74),
			.seq = seq74, .seq_size = LWS_ARRAY_SIZE(seq74) },
	{ .b = test75, .blen = sizeof(test75),
			.seq = seq75, .seq_size = LWS_ARRAY_SIZE(seq75) },
	{ .b = test76, .blen = sizeof(test76),
			.seq = seq76, .seq_size = LWS_ARRAY_SIZE(seq76) },
	{ .b = test77, .blen = sizeof(test77),
			.seq = seq77, .seq_size = LWS_ARRAY_SIZE(seq77) },
	{ .b = test78, .blen = sizeof(test78),
			.seq = seq78, .seq_size = LWS_ARRAY_SIZE(seq78) },
	{ .b = test79, .blen = sizeof(test79),
			.seq = seq79, .seq_size = LWS_ARRAY_SIZE(seq79) },
	{ .b = test80, .blen = sizeof(test80),
			.seq = seq80, .seq_size = LWS_ARRAY_SIZE(seq80) },
	{ .b = test81, .blen = sizeof(test81),
			.seq = seq81, .seq_size = LWS_ARRAY_SIZE(seq81) },
	{ .b = test82, .blen = sizeof(test82),
			.seq = seq82, .seq_size = LWS_ARRAY_SIZE(seq82) },

	/* COSE-dervied test vectors */

	{ .b = test83, .blen = sizeof(test83),
			.seq = seq83, .seq_size = LWS_ARRAY_SIZE(seq83) },
	{ .b = test84, .blen = sizeof(test84),
			.seq = seq84, .seq_size = LWS_ARRAY_SIZE(seq84) },
	{ .b = test85, .blen = sizeof(test85),
			.seq = seq85, .seq_size = LWS_ARRAY_SIZE(seq85) },
	{ .b = test86, .blen = sizeof(test86),
			.seq = seq86, .seq_size = LWS_ARRAY_SIZE(seq86) },
	{ .b = test87, .blen = sizeof(test87),
			.seq = seq87, .seq_size = LWS_ARRAY_SIZE(seq87) },
	{ .b = test88, .blen = sizeof(test88),
			.seq = seq88, .seq_size = LWS_ARRAY_SIZE(seq88) },
	{ .b = test89, .blen = sizeof(test89),
			.seq = seq89, .seq_size = LWS_ARRAY_SIZE(seq89) },
	{ .b = test90, .blen = sizeof(test90),
			.seq = seq90, .seq_size = LWS_ARRAY_SIZE(seq90) },
	{ .b = test91, .blen = sizeof(test91),
			.seq = seq91, .seq_size = LWS_ARRAY_SIZE(seq91) },
	{ .b = test92, .blen = sizeof(test92),
			.seq = seq92, .seq_size = LWS_ARRAY_SIZE(seq92) },
	{ .b = test93, .blen = sizeof(test93),
			.seq = seq93, .seq_size = LWS_ARRAY_SIZE(seq93) },
	{ .b = test94, .blen = sizeof(test94),
			.seq = seq94, .seq_size = LWS_ARRAY_SIZE(seq94) },
	{ .b = test95, .blen = sizeof(test95),
			.seq = seq95, .seq_size = LWS_ARRAY_SIZE(seq95) },
	{ .b = test96, .blen = sizeof(test96),
			.seq = seq96, .seq_size = LWS_ARRAY_SIZE(seq96) },
	{ .b = test97, .blen = sizeof(test97),
			.seq = seq97, .seq_size = LWS_ARRAY_SIZE(seq97) },
	{ .b = test98, .blen = sizeof(test98),
			.seq = seq98, .seq_size = LWS_ARRAY_SIZE(seq98) },
	{ .b = test99, .blen = sizeof(test99),
			.seq = seq99, .seq_size = LWS_ARRAY_SIZE(seq99) },
	{ .b = test100, .blen = sizeof(test100),
			.seq = seq100, .seq_size = LWS_ARRAY_SIZE(seq100) },
	{ .b = test101, .blen = sizeof(test101),
			.seq = seq101, .seq_size = LWS_ARRAY_SIZE(seq101) },
	{ .b = test102, .blen = sizeof(test102),
			.seq = seq102, .seq_size = LWS_ARRAY_SIZE(seq102) },
	{ .b = test103, .blen = sizeof(test103),
			.seq = seq103, .seq_size = LWS_ARRAY_SIZE(seq103) },
	{ .b = test104, .blen = sizeof(test104),
			.seq = seq104, .seq_size = LWS_ARRAY_SIZE(seq104) },
	{ .b = test105, .blen = sizeof(test105),
			.seq = seq105, .seq_size = LWS_ARRAY_SIZE(seq105) },
	{ .b = test106, .blen = sizeof(test106),
			.seq = seq106, .seq_size = LWS_ARRAY_SIZE(seq106) },
	{ .b = test107, .blen = sizeof(test107),
			.seq = seq107, .seq_size = LWS_ARRAY_SIZE(seq107) },
	{ .b = test108, .blen = sizeof(test108),
			.seq = seq108, .seq_size = LWS_ARRAY_SIZE(seq108) },
};

static const uint8_t
	w1[] = { 0x65, 0x68, 0x65, 0x6C,
		 0x6C, 0x6F },
	w2[] = { 0xc2 },
	w3[] = { 0x82, 0x63, 0x61, 0x62,
		 0x63, 0x63, 0x64, 0x65,
		 0x66 },
	w4[] = { 0xA2, 0x63, 0x67, 0x68,
		 0x69, 0x01, 0x63, 0x6A,
		 0x6B, 0x6C, 0x02 },
	w5[] = { 0xD8, 0x7B, 0xA2, 0x63,
		 0x67, 0x68, 0x69, 0x01,
		 0x63, 0x6A, 0x6B, 0x6C,
		 0x02 },
	w6[] = { 0xCC, 0xA2, 0x63, 0x67,
		 0x68, 0x69, 0x01, 0x63,
		 0x6A, 0x6B, 0x6C, 0x82,
		 0x61, 0x61, 0x61, 0x62 },
	w7[] = { 0x20, },
	w8[] = { 0x0c, },
	w13[] = { 0x18, 0x34 },
	w14[] = { 0x19, 0x12, 0x34 },
	w15[] = { 0x1a, 0x12, 0x34, 0x56, 0x78 },
	w16[] = { 0x1b, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0 },
	w17[] = { 0x65, 0x68, 0x65, 0x6C, 0x6C, 0x6F },
	w18[] = { 0x25 },
	w19[] = {
			0xd8, 0x7b, 0x58, 0xb7,
			0xd8, 0x62, 0x84, 0x43, 0xa1,
			0x03, 0x00, 0xa1, 0x07, 0x83,
			0x43, 0xa1, 0x01, 0x27, 0xa1,
			0x04, 0x42, 0x31, 0x31, 0x58,
			0x40, 0xb7, 0xca, 0xcb, 0xa2,
			0x85, 0xc4, 0xcd, 0x3e, 0xd2,
			0xf0, 0x14, 0x6f, 0x41, 0x98,
			0x86, 0x14, 0x4c, 0xa6, 0x38,
			0xd0, 0x87, 0xde, 0x12, 0x3d,
			0x40, 0x01, 0x67, 0x30, 0x8a,
			0xce, 0xab, 0xc4, 0xb5, 0xe5,
			0xc6, 0xa4, 0x0c, 0x0d, 0xe0,
			},
	w19a[] = {
			0xb7, 0x11, 0x67, 0xa3, 0x91,
			0x75, 0xea, 0x56, 0xc1, 0xfe,
			0x96, 0xc8, 0x9e, 0x5e, 0x7d,
			0x30, 0xda, 0xf2, 0x43, 0x8a,
			0x45, 0x61, 0x59, 0xa2, 0x0a,
			0x54, 0x54, 0x68, 0x69, 0x73,
			0x20, 0x69, 0x73, 0x20, 0x74,
			0x68, 0x65, 0x20, 0x63, 0x6f,
			0x6e, 0x74, 0x65, 0x6e, 0x74,
			0x2e, 0x81, 0x83, 0x43, 0xa1,
			0x01, 0x27, 0xa1, 0x04, 0x42,
			0x31, 0x31, 0x58, 0x40, 0x77,
			0xf3, 0xea, 0xcd, 0x11,},
	w19b[] = {
			0x85, 0x2c, 0x4b, 0xf9, 0xcb, 0x1d,
			0x72, 0xfa, 0xbe, 0x6b, 0x26,
			0xfb, 0xa1, 0xd7, 0x60, 0x92,
			0xb2, 0xb5, 0xb7, 0xec, 0x83,
			0xb8, 0x35, 0x57, 0x65, 0x22,
			0x64, 0xe6, 0x96, 0x90, 0xdb,
			0xc1, 0x17, 0x2d, 0xdc, 0x0b,
			0xf8, 0x84, 0x11, 0xc0, 0xd2,
			0x5a, 0x50, 0x7f, 0xdb, 0x24,
			0x7a, 0x20, 0xc4, 0x0d, 0x5e,
			0x24, 0x5f, 0xab, 0xd3, 0xfc,
			0x9e, 0xc1, 0x06 },
	w22[] = { 0xD8, 0x7B, 0x19, 0x01, 0xC8 },
	w24[] = { 0xDB, 0x12, 0x34, 0x56, 0x78, 0x9A,
			0xBC, 0xED, 0xF0, 0x19, 0x01, 0xC8},
	w25[] = { 0xF9, 0x3C, 0x00 },
	w26[] = { 0xF9, 0x3E, 0x00 },
	w27[] = { 0xFB, 0x3F, 0xF1, 0xF7, 0xCE, 0xD9, 0x16, 0x87, 0x2B },
	w28[] = { 0xA2, 0x61, 0x61, 0x01, 0x61, 0x62, 0x82, 0x02, 0x03 },
	w29[] = { 0x7F, 0x65, 0x68, 0x65, 0x6C, 0x6C, 0x6F, 0xFF
}
;

static const char * const tok[] = {
	"something",
};

struct priv {
	const struct cbort *cbt;
	size_t idx;
};

static int pass;

static signed char
test_cb(struct lecp_ctx *ctx, char reason)
{
	struct priv *priv = (struct priv *)ctx->user;
	size_t i = priv->idx++;

#if defined(VERBOSE)
	 lwsl_notice("%s: %s, ctx->path %s\n", __func__,
			 reason_names[(int)reason & 0x1f], ctx->path);
#endif

	// if (ctx->npos)
	//	lwsl_hexdump_notice(ctx->buf, ctx->npos);

	if (!priv->cbt->seq)
		return 0;

	if (i >= priv->cbt->seq_size) {
		lwsl_warn("%s: unexpected parse states\n", __func__);
		return 1;
	}

	if (priv->cbt->seq[i].reason != reason) {
		lwsl_warn("%s: reason mismatch\n", __func__);
		return 1;
	}

	if (priv->cbt->seq[i].buf &&
	    (priv->cbt->seq[i].buf_len != ctx->npos ||
	     memcmp(priv->cbt->seq[i].buf, ctx->buf, ctx->npos))) {
		lwsl_warn("%s: buf mismatch\n", __func__);
		lwsl_hexdump_notice(ctx->buf, (size_t)ctx->npos);
		return 1;
	}

	switch (reason) {
	case LECPCB_VAL_SIMPLE:
	case LECPCB_VAL_NUM_UINT:
	case LECPCB_VAL_NUM_INT:
		if (ctx->item.u.u64 != priv->cbt->seq[i].item.u.u64) {
			lwsl_warn("%s: number mismatch %llu %llu\n", __func__,
				(unsigned long long)ctx->item.u.u64,
				(unsigned long long)priv->cbt->seq[i].item.u.u64);
			return 1;
		}
		break;

	case LECPCB_VAL_FLOAT16:
		if (ctx->item.u.hf != priv->cbt->seq[i].item.u.hf) {
			lwsl_warn("%s: number mismatch %llu %llu\n", __func__,
				(unsigned long long)ctx->item.u.hf,
				(unsigned long long)priv->cbt->seq[i].item.u.hf);
			return 1;
		}
		break;
	case LECPCB_VAL_FLOAT32:
#if defined(LWS_WITH_CBOR_FLOAT)
		if (!isfinite(ctx->item.u.f) &&
		    !isfinite(priv->cbt->seq[i].item.u.f))
			break;
		if (isnan(ctx->item.u.f) &&
		    isnan(priv->cbt->seq[i].item.u.f))
			break;
#endif
		if (ctx->item.u.f != priv->cbt->seq[i].item.u.f) {
#if defined(LWS_WITH_CBOR_FLOAT)
			lwsl_warn("%s: number mismatch %f %f\n", __func__,
				ctx->item.u.f,
				priv->cbt->seq[i].item.u.f);
#else
			lwsl_warn("%s: f32 number mismatch %llu %llu\n", __func__,
				(unsigned long long)ctx->item.u.f,
				(unsigned long long)priv->cbt->seq[i].item.u.f);
#endif
			return 1;
		}
		break;
	case LECPCB_VAL_FLOAT64:
#if defined(LWS_WITH_CBOR_FLOAT)
		if (!isfinite(ctx->item.u.d) &&
		    !isfinite(priv->cbt->seq[i].item.u.d))
			break;
		if (isnan(ctx->item.u.d) &&
		    isnan(priv->cbt->seq[i].item.u.d))
			break;
#endif
		if (ctx->item.u.d != priv->cbt->seq[i].item.u.d) {
#if defined(LWS_WITH_CBOR_FLOAT)
			lwsl_warn("%s: f64 number mismatch %f %f\n", __func__,
				ctx->item.u.d,
				priv->cbt->seq[i].item.u.d);
#else
			lwsl_warn("%s: number mismatch %llu %llu\n", __func__,
				(unsigned long long)ctx->item.u.d,
				(unsigned long long)priv->cbt->seq[i].item.u.d);
#endif
			return 1;
		}
		break;

	case LECPCB_DESTRUCTED:
		pass++;
		break;
	}

	return 0;
}

int main(int argc, const char **argv)
{
	int n, m, e = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE,
			expected = (int)LWS_ARRAY_SIZE(cbor_tests) +
					29 /* <-- how many write tests */;
	struct lecp_ctx ctx;
	const char *p;

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS API selftest: LECP CBOR parser\n");

	for (m = 0; m < (int)LWS_ARRAY_SIZE(cbor_tests); m++) {

		struct priv priv;

		priv.cbt = &cbor_tests[m];
		priv.idx = 0;

		lwsl_notice("%s: ++++++++++++++++ test %d\n", __func__, m + 1);

		lecp_construct(&ctx, test_cb, &priv, tok, LWS_ARRAY_SIZE(tok));

		lwsl_hexdump_info(cbor_tests[m].b, cbor_tests[m].blen);

#if 0
		{
			char fn[128];
			int fd;

			lws_snprintf(fn, sizeof(fn), "/tmp/cbor-%d", m + 1);
			fd = open(fn, LWS_O_CREAT | LWS_O_TRUNC | LWS_O_WRONLY, 0600);
			if (fd != -1) {
				write(fd,  cbor_tests[m].b,
					   cbor_tests[m].blen);
				close(fd);
			}
		}
#endif

		n = lecp_parse(&ctx, cbor_tests[m].b,
				     cbor_tests[m].blen);

		lecp_destruct(&ctx);

		if (n < 0 && m + 1 != 46 /* expected to fail */) {
			lwsl_err("%s: test %d: CBOR decode failed %d '%s'\n",
					__func__, m + 1, n,
					lecp_error_to_string(n));
			e++;
		}
	}

	{
		lws_lec_pctx_t ctx;
		uint8_t buf[64];

		lws_lec_init(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "'hello'") !=
						LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w1) || memcmp(w1, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "2()") !=
						LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w2) || memcmp(w2, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "['abc','def']") !=
						LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w3) || memcmp(w3, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test4\n", __func__);

		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "{'ghi':1,'jkl':2}") !=
						LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w4) || memcmp(w4, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test5\n", __func__);

		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "123({'ghi':1,'jkl':2})") !=
						LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w5) || memcmp(w5, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test6\n", __func__);

		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "12({'ghi':1,'jkl':['a', 'b']})") !=
						LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w6) || memcmp(w6, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test7\n", __func__);

		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "%d", -1) !=
						LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w7) || memcmp(w7, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test8\n", __func__);

		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "%ld", -1l) !=
						LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w7) || memcmp(w7, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test9\n", __func__);

		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "%lld", -1ll) !=
						LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w7) || memcmp(w7, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test10\n", __func__);

		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "%u", 12) !=
						LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w8) || memcmp(w8, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test11\n", __func__);

		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "%ld", 12l) !=
						LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w8) || memcmp(w8, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test12\n", __func__);

		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "%lld", 12ll) !=
						LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w8) || memcmp(w8, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test13\n", __func__);

		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "%u", 0x34u) !=
						LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w13) || memcmp(w13, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test14\n", __func__);

		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "%ld", 0x1234ul) !=
						LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w14) || memcmp(w14, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test15\n", __func__);

		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "%lld", 0x12345678ull) !=
						LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w15) || memcmp(w15, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test16\n", __func__);

		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "%lld", 0x123456789abcdef0ull) !=
						LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w16) || memcmp(w16, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test17\n", __func__);
		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "%s", "hello") !=
						LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w17) || memcmp(w17, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test18\n", __func__);
		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "-6") !=
						LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w18) || memcmp(w18, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		/*
		 * A big binary blob is going to get emitted in 3 output
		 * buffers, by calling it two more times while still handling
		 * the same format object, format objects before that which
		 * were completed are skipped on the subsequent calls
		 */

		lwsl_user("%s: test19\n", __func__);
		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "123(%.*b)", (int)sizeof(test106), test106) !=
				LWS_LECPCTX_RET_AGAIN ||
		    ctx.used != sizeof(w19) || memcmp(w19, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test20\n", __func__);
		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "123(%.*b)", (int)sizeof(test106), test106) !=
				LWS_LECPCTX_RET_AGAIN ||
		    ctx.used != sizeof(w19a) || memcmp(w19a, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test21\n", __func__);
		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "123(%.*b)", (int)sizeof(test106), test106) !=
				LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w19b) || memcmp(w19b, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test22\n", __func__);
		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "%t(456)", 123) !=
				LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w22) || memcmp(w22, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test23\n", __func__);
		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "%lt(456)", 123ul) !=
				LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w22) || memcmp(w22, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test24\n", __func__);
		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "%llt(456)", 0x123456789abcedf0ull) !=
				LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w24) || memcmp(w24, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test25\n", __func__);
		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "%f", 1.0) !=
				LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w25) || memcmp(w25, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test26\n", __func__);
		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "%f", 1.5) !=
				LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w26) || memcmp(w26, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		lwsl_user("%s: test27\n", __func__);
		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "%f", 1.123) !=
				LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w27) || memcmp(w27, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;

		{
			int args[3] = { 1, 2, 3 };

			lwsl_user("%s: test28\n", __func__);
			lws_lec_setbuf(&ctx, buf, sizeof(buf));

			if (lws_lec_printf(&ctx, "{'a':%d,'b':[%d,%d]}",
						args[0], args[1], args[2]) !=
					LWS_LECPCTX_RET_FINISHED ||
			    ctx.used != sizeof(w28) ||
			    memcmp(w28, buf, ctx.used)) {
				lwsl_hexdump_notice(ctx.start, ctx.used);
				e++;
			} else
				pass++;
		}

		lwsl_user("%s: test29\n", __func__);
		lws_lec_setbuf(&ctx, buf, sizeof(buf));

		if (lws_lec_printf(&ctx, "<t'hello'>") !=
				LWS_LECPCTX_RET_FINISHED ||
		    ctx.used != sizeof(w29) || memcmp(w29, buf, ctx.used)) {
			lwsl_hexdump_notice(ctx.start, ctx.used);
			e++;
		} else
			pass++;
	}

	if (e)
		goto bail;

	if (pass != expected)
		goto bail;

	lwsl_user("Completed: PASS %d / %d\n", pass, expected);

	return 0;

bail:
	lwsl_user("Completed: FAIL, passed %d / %d (e %d)\n", pass,
				expected, e);

	return 1;
}
